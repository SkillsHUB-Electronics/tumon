// tumon_feeder: reads a real camera (a USB webcam or the laptop's built-in camera) and hands its pictures to the
// "Tumon Camera", so TeamTalk (or any app) shows what that camera sees, in the YUY2 format TeamTalk understands.
//
//   tumon_feeder.exe --list                         list the cameras Windows knows
//   tumon_feeder.exe --device NAME                  feed from the first camera whose name contains NAME
//   tumon_feeder.exe --index N                      feed from camera number N of the --list output
//   options: --width W --height H --fps F           preferred picture (default 1280x720 at 30)
//            --seconds N                            stop after N seconds
//            --always                               keep the camera on even when nobody uses Tumon Camera
//
// The real camera is opened only while some program is actually using Tumon Camera (so the camera's light is on
// only then) and is closed again when it stops. Ctrl+C ends the program. See docs/architecture.md.
#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

#include "../src/channel/frame_channel.h"

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "ole32")

using Microsoft::WRL::ComPtr;

namespace {

std::atomic<bool> g_stop{false};

BOOL WINAPI OnCtrl(DWORD) {
    g_stop = true;
    return TRUE;
}

struct Options {
    std::wstring device;
    int index = -1;
    UINT32 width = 1280;
    UINT32 height = 720;
    UINT32 fps = 30;
    int seconds = 0;
    bool always = false;
    bool list = false;
};

bool ContainsNoCase(const std::wstring& haystack, const std::wstring& needle) {
    auto lower = [](std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return s;
    };
    return lower(haystack).find(lower(needle)) != std::wstring::npos;
}

struct Device {
    std::wstring name;
    ComPtr<IMFActivate> activate;
};

std::vector<Device> EnumerateDevices() {
    std::vector<Device> devices;
    ComPtr<IMFAttributes> attrs;
    if (FAILED(MFCreateAttributes(&attrs, 1))) return devices;
    attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    IMFActivate** list = nullptr;
    UINT32 count = 0;
    if (FAILED(MFEnumDeviceSources(attrs.Get(), &list, &count))) return devices;
    for (UINT32 i = 0; i < count; ++i) {
        Device d;
        WCHAR* name = nullptr;
        UINT32 length = 0;
        d.name = L"(unnamed)";
        if (SUCCEEDED(list[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &length)) && name) {
            d.name = name;
            CoTaskMemFree(name);
        }
        d.activate.Attach(list[i]);
        devices.push_back(std::move(d));
    }
    CoTaskMemFree(list);
    return devices;
}

// An open real camera whose pictures arrive as YUY2.
struct Camera {
    ComPtr<IMFActivate> activate;
    ComPtr<IMFMediaSource> source;
    ComPtr<IMFSourceReader> reader;
    std::wstring name;
    UINT32 width = 0;
    UINT32 height = 0;

    bool IsOpen() const { return reader != nullptr; }

    void Close() {
        reader.Reset();
        if (source) source->Shutdown();
        if (activate) activate->ShutdownObject();
        source.Reset();
        activate.Reset();
    }
};

const DWORD kVideoStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);

// Picks the native type closest to what was asked for (never larger than the channel allows).
bool ChooseNativeType(IMFSourceReader* reader, const Options& want, ComPtr<IMFMediaType>* chosen, UINT32* w, UINT32* h, UINT32* fpsNum, UINT32* fpsDen) {
    long bestScore = -1;
    for (DWORD i = 0;; ++i) {
        ComPtr<IMFMediaType> type;
        const HRESULT hr = reader->GetNativeMediaType(kVideoStream, i, &type);
        if (hr == MF_E_NO_MORE_TYPES || FAILED(hr)) break;

        UINT32 tw = 0, th = 0;
        MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &tw, &th);
        if (tw == 0 || th == 0 || (tw & 1) || tw > tumon::kChannelMaxWidth || th > tumon::kChannelMaxHeight) continue;
        UINT32 num = 0, den = 1;
        MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &num, &den);
        const double fps = den ? static_cast<double>(num) / den : 0.0;

        GUID subtype = GUID_NULL;
        type->GetGUID(MF_MT_SUBTYPE, &subtype);
        long score = std::labs(static_cast<long>(tw) - static_cast<long>(want.width)) + std::labs(static_cast<long>(th) - static_cast<long>(want.height));
        if (fps < want.fps - 1) score += static_cast<long>((want.fps - fps) * 20);  // too slow
        if (fps > 31) score += static_cast<long>(fps - 30);                          // needlessly fast
        if (subtype == MFVideoFormat_MJPG) score += 5;                               // costs a decode

        if (bestScore < 0 || score < bestScore) {
            bestScore = score;
            *chosen = type;
            *w = tw;
            *h = th;
            *fpsNum = num ? num : 30;
            *fpsDen = den ? den : 1;
        }
    }
    return bestScore >= 0;
}

bool OpenCamera(const Options& options, Camera* camera) {
    std::vector<Device> devices = EnumerateDevices();
    Device* chosen = nullptr;
    if (options.index >= 0) {
        if (static_cast<size_t>(options.index) < devices.size()) chosen = &devices[options.index];
    } else {
        for (Device& d : devices) {
            if (ContainsNoCase(d.name, options.device)) {
                chosen = &d;
                break;
            }
        }
    }
    if (!chosen) {
        wprintf(L"No camera found for that name or number.\n");
        return false;
    }
    if (ContainsNoCase(chosen->name, L"Tumon")) {
        wprintf(L"Refusing to read Tumon Camera itself (it would feed on its own picture).\n");
        return false;
    }

    ComPtr<IMFMediaSource> source;
    HRESULT hr = chosen->activate->ActivateObject(IID_PPV_ARGS(&source));
    if (FAILED(hr)) {
        wprintf(L"Cannot open \"%s\": 0x%08X (is the camera blocked in Windows privacy settings or used exclusively?)\n",
                chosen->name.c_str(), static_cast<unsigned>(hr));
        return false;
    }

    ComPtr<IMFAttributes> attrs;
    MFCreateAttributes(&attrs, 1);
    // The Video Processor MFT lets Windows convert whatever the camera delivers (NV12, MJPEG...) to YUY2. The basic
    // option (MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING) could not: SetCurrentMediaType failed with 0xC00D5212.
    attrs->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
    ComPtr<IMFSourceReader> reader;
    hr = MFCreateSourceReaderFromMediaSource(source.Get(), attrs.Get(), &reader);
    if (FAILED(hr)) {
        wprintf(L"Cannot read from \"%s\": 0x%08X\n", chosen->name.c_str(), static_cast<unsigned>(hr));
        source->Shutdown();
        return false;
    }

    ComPtr<IMFMediaType> native;
    UINT32 w = 0, h = 0, num = 30, den = 1;
    if (!ChooseNativeType(reader.Get(), options, &native, &w, &h, &num, &den)) {
        wprintf(L"\"%s\" offers no usable video size.\n", chosen->name.c_str());
        source->Shutdown();
        return false;
    }
    hr = reader->SetCurrentMediaType(kVideoStream, nullptr, native.Get());
    if (SUCCEEDED(hr)) {
        ComPtr<IMFMediaType> out;
        MFCreateMediaType(&out);
        out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
        MFSetAttributeSize(out.Get(), MF_MT_FRAME_SIZE, w, h);
        MFSetAttributeRatio(out.Get(), MF_MT_FRAME_RATE, num, den);
        out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        hr = reader->SetCurrentMediaType(kVideoStream, nullptr, out.Get());
    }
    if (FAILED(hr)) {
        wprintf(L"Cannot get YUY2 pictures from \"%s\": 0x%08X\n", chosen->name.c_str(), static_cast<unsigned>(hr));
        source->Shutdown();
        return false;
    }

    camera->activate = chosen->activate;
    camera->source = source;
    camera->reader = reader;
    camera->name = chosen->name;
    camera->width = w;
    camera->height = h;
    wprintf(L"Camera opened: %s, %ux%u at %.0f fps (converted to YUY2)\n", chosen->name.c_str(), w, h, den ? static_cast<double>(num) / den : 0.0);
    return true;
}

// Reads one picture and publishes it. Returns false if the camera failed (so the caller reopens it).
bool Pump(Camera* camera, tumon::FrameChannelClient* channel, uint64_t* sent) {
    DWORD flags = 0;
    LONGLONG timestamp = 0;
    ComPtr<IMFSample> sample;
    const HRESULT hr = camera->reader->ReadSample(kVideoStream, 0, nullptr, &flags, &timestamp, &sample);
    if (FAILED(hr) || (flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM))) {
        wprintf(L"The camera stopped delivering pictures (0x%08X); will try again.\n", static_cast<unsigned>(hr));
        return false;
    }
    if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
        ComPtr<IMFMediaType> current;
        if (SUCCEEDED(camera->reader->GetCurrentMediaType(kVideoStream, &current))) {
            MFGetAttributeSize(current.Get(), MF_MT_FRAME_SIZE, &camera->width, &camera->height);
        }
    }
    if (!sample) return true;

    ComPtr<IMFMediaBuffer> buffer;
    if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) return true;
    const int w = static_cast<int>(camera->width);
    const int h = static_cast<int>(camera->height);

    ComPtr<IMF2DBuffer> buffer2d;
    if (SUCCEEDED(buffer.As(&buffer2d))) {
        BYTE* scanline0 = nullptr;
        LONG pitch = 0;
        if (SUCCEEDED(buffer2d->Lock2D(&scanline0, &pitch))) {
            if (pitch > 0 && channel->Write(scanline0, w, h, pitch)) ++*sent;
            buffer2d->Unlock2D();
        }
    } else {
        BYTE* data = nullptr;
        DWORD length = 0;
        if (SUCCEEDED(buffer->Lock(&data, nullptr, &length))) {
            if (length >= static_cast<DWORD>(w) * h * 2 && channel->Write(data, w, h, w * 2)) ++*sent;
            buffer->Unlock();
        }
    }
    return true;
}

bool ParseOptions(int argc, wchar_t** argv, Options* o) {
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        auto next = [&]() -> const wchar_t* { return i + 1 < argc ? argv[++i] : L""; };
        if (a == L"--list") o->list = true;
        else if (a == L"--device") o->device = next();
        else if (a == L"--index") o->index = _wtoi(next());
        else if (a == L"--width") o->width = static_cast<UINT32>(_wtoi(next()));
        else if (a == L"--height") o->height = static_cast<UINT32>(_wtoi(next()));
        else if (a == L"--fps") o->fps = static_cast<UINT32>(_wtoi(next()));
        else if (a == L"--seconds") o->seconds = _wtoi(next());
        else if (a == L"--always") o->always = true;
        else return false;
    }
    return o->list || !o->device.empty() || o->index >= 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    Options options;
    if (!ParseOptions(argc, argv, &options)) {
        wprintf(L"Usage: tumon_feeder.exe --list | --device NAME | --index N  [--width W --height H --fps F --seconds N --always]\n");
        return 64;
    }
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) || FAILED(MFStartup(MF_VERSION))) {
        wprintf(L"Could not start COM or Media Foundation.\n");
        return 1;
    }

    if (options.list) {
        std::vector<Device> devices = EnumerateDevices();
        for (size_t i = 0; i < devices.size(); ++i) wprintf(L"%zu: %s\n", i, devices[i].name.c_str());
        MFShutdown();
        CoUninitialize();
        return 0;
    }

    // Only one feeder per channel: two of them fight over the real camera (each open fails with 0xC00D3704) and
    // write into the same pictures slots. The name includes the channel name so tests use their own.
    std::wstring mutexName = L"Local\\TumonFeeder-";
    for (const wchar_t* c = tumon::ChannelNameGlobal(); *c; ++c) mutexName += (*c == L'\\') ? L'_' : *c;
    HANDLE singleInstance = CreateMutexW(nullptr, TRUE, mutexName.c_str());
    if (!singleInstance || GetLastError() == ERROR_ALREADY_EXISTS) {
        wprintf(L"Another tumon_feeder is already running for this camera channel. One is enough: close it first.\n");
        MFShutdown();
        CoUninitialize();
        return 65;
    }

    SetConsoleCtrlHandler(OnCtrl, TRUE);
    const ULONGLONG deadline = options.seconds > 0 ? GetTickCount64() + static_cast<ULONGLONG>(options.seconds) * 1000 : 0;

    tumon::FrameChannelClient channel;
    Camera camera;
    uint64_t sent = 0, sentAtLastReport = 0;
    ULONGLONG lastReport = GetTickCount64();
    bool announcedWaiting = false;

    wprintf(L"Waiting for programs that use Tumon Camera. Press Ctrl+C to stop.\n");
    while (!g_stop && (deadline == 0 || GetTickCount64() < deadline)) {
        if (!channel.IsOpen()) {
            if (!channel.Open()) {
                if (!announcedWaiting) wprintf(L"Tumon Camera is not in use yet (no frame channel).\n");
                announcedWaiting = true;
                Sleep(250);
                continue;
            }
            announcedWaiting = false;
            wprintf(L"Connected to the Tumon Camera channel (%s).\n", channel.Name());
        }

        const bool wanted = options.always || channel.Consumers() > 0;
        if (!wanted) {
            if (camera.IsOpen()) {
                camera.Close();
                wprintf(L"Nobody is using Tumon Camera now: real camera closed.\n");
            }
            Sleep(200);
            continue;
        }

        if (!camera.IsOpen()) {
            wprintf(L"Tumon Camera is in use: opening the real camera...\n");
            if (!OpenCamera(options, &camera)) {
                Sleep(1000);
                continue;
            }
        }
        if (!Pump(&camera, &channel, &sent)) {
            camera.Close();
            Sleep(500);
        }

        const ULONGLONG now = GetTickCount64();
        if (now - lastReport >= 2000) {
            wprintf(L"  %.1f pictures/s sent, %ld program(s) using Tumon Camera\n",
                    static_cast<double>(sent - sentAtLastReport) * 1000.0 / static_cast<double>(now - lastReport), channel.Consumers());
            sentAtLastReport = sent;
            lastReport = now;
        }
    }

    camera.Close();
    CloseHandle(singleInstance);
    wprintf(L"Stopped. %llu pictures sent.\n", sent);
    MFShutdown();
    CoUninitialize();
    return 0;
}
