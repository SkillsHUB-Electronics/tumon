#include "camera_feeder.h"

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdlib>
#include <cwctype>

#include "../channel/frame_channel.h"

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "ole32")

using Microsoft::WRL::ComPtr;

namespace tumon {

namespace {

const DWORD kVideoStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
constexpr wchar_t kOwnName[] = L"Tumon Camera";

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
        d.name = L"(unnamed)";
        WCHAR* name = nullptr;
        UINT32 length = 0;
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

// Picks the native type closest to what was asked for (never larger than the channel allows).
bool ChooseNativeType(IMFSourceReader* reader, const FeederOptions& want, ComPtr<IMFMediaType>* chosen, UINT32* w, UINT32* h,
                      UINT32* fpsNum, UINT32* fpsDen) {
    long bestScore = -1;
    for (DWORD i = 0;; ++i) {
        ComPtr<IMFMediaType> type;
        const HRESULT hr = reader->GetNativeMediaType(kVideoStream, i, &type);
        if (hr == MF_E_NO_MORE_TYPES || FAILED(hr)) break;

        UINT32 tw = 0, th = 0;
        MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &tw, &th);
        if (tw == 0 || th == 0 || (tw & 1) || tw > kChannelMaxWidth || th > kChannelMaxHeight) continue;
        UINT32 num = 0, den = 1;
        MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &num, &den);
        const double fps = den ? static_cast<double>(num) / den : 0.0;

        GUID subtype = GUID_NULL;
        type->GetGUID(MF_MT_SUBTYPE, &subtype);
        long score = std::labs(static_cast<long>(tw) - static_cast<long>(want.width)) +
                     std::labs(static_cast<long>(th) - static_cast<long>(want.height));
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

HRESULT OpenCamera(const FeederOptions& options, Camera* camera, bool* missing) {
    *missing = false;
    std::vector<Device> devices = EnumerateDevices();
    Device* chosen = nullptr;
    // Exact name first, then a part of the name. Never Tumon Camera itself: it would feed on its own picture.
    for (Device& d : devices) {
        if (d.name == options.camera && !ContainsNoCase(d.name, kOwnName)) chosen = &d;
    }
    if (!chosen) {
        for (Device& d : devices) {
            if (ContainsNoCase(d.name, options.camera) && !ContainsNoCase(d.name, kOwnName)) {
                chosen = &d;
                break;
            }
        }
    }
    if (!chosen) {
        *missing = true;
        return MF_E_NOT_FOUND;
    }

    ComPtr<IMFMediaSource> source;
    HRESULT hr = chosen->activate->ActivateObject(IID_PPV_ARGS(&source));
    if (FAILED(hr)) return hr;

    // The Video Processor MFT lets Windows convert whatever the camera delivers (NV12, MJPEG...) to YUY2. The basic
    // option (MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING) could not: SetCurrentMediaType failed with 0xC00D5212.
    ComPtr<IMFAttributes> attrs;
    MFCreateAttributes(&attrs, 1);
    attrs->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
    ComPtr<IMFSourceReader> reader;
    hr = MFCreateSourceReaderFromMediaSource(source.Get(), attrs.Get(), &reader);

    ComPtr<IMFMediaType> native;
    UINT32 w = 0, h = 0, num = 30, den = 1;
    if (SUCCEEDED(hr) && !ChooseNativeType(reader.Get(), options, &native, &w, &h, &num, &den)) hr = MF_E_INVALIDMEDIATYPE;
    if (SUCCEEDED(hr)) hr = reader->SetCurrentMediaType(kVideoStream, nullptr, native.Get());
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
        reader.Reset();
        source->Shutdown();
        chosen->activate->ShutdownObject();
        return hr;
    }

    camera->activate = chosen->activate;
    camera->source = source;
    camera->reader = reader;
    camera->name = chosen->name;
    camera->width = w;
    camera->height = h;
    return S_OK;
}

// Reads one picture and publishes it.
HRESULT Pump(Camera* camera, FrameChannelClient* channel, uint64_t* sent) {
    DWORD flags = 0;
    LONGLONG timestamp = 0;
    ComPtr<IMFSample> sample;
    const HRESULT hr = camera->reader->ReadSample(kVideoStream, 0, nullptr, &flags, &timestamp, &sample);
    if (FAILED(hr)) return hr;
    if (flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM)) return E_FAIL;
    if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
        ComPtr<IMFMediaType> current;
        if (SUCCEEDED(camera->reader->GetCurrentMediaType(kVideoStream, &current))) {
            MFGetAttributeSize(current.Get(), MF_MT_FRAME_SIZE, &camera->width, &camera->height);
        }
    }
    if (!sample) return S_OK;

    ComPtr<IMFMediaBuffer> buffer;
    if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) return S_OK;
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
    return S_OK;
}

}  // namespace

std::vector<std::wstring> ListSourceCameras() {
    std::vector<std::wstring> names;
    for (const Device& d : EnumerateDevices()) {
        if (!ContainsNoCase(d.name, kOwnName)) names.push_back(d.name);
    }
    return names;
}

bool IsTumonCameraInstalled() {
    for (const Device& d : EnumerateDevices()) {
        if (ContainsNoCase(d.name, kOwnName)) return true;
    }
    return false;
}

void CameraFeeder::Start(const FeederOptions& options, Callback callback) {
    Stop();
    stop_ = false;
    running_ = true;
    thread_ = std::thread(&CameraFeeder::Run, this, options, std::move(callback));
}

void CameraFeeder::Stop() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    running_ = false;
}

void CameraFeeder::Run(FeederOptions options, Callback callback) {
    FeederStatus status;
    auto report = [&](FeederState state) {
        status.state = state;
        if (callback) callback(status);
    };

    const bool comStarted = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    const HRESULT mf = MFStartup(MF_VERSION);
    if (!comStarted || FAILED(mf)) {
        status.error = FAILED(mf) ? mf : E_FAIL;
        report(FeederState::CameraProblem);
        if (SUCCEEDED(mf)) MFShutdown();
        if (comStarted) CoUninitialize();
        running_ = false;
        return;
    }

    // Only one feeder per channel: two of them fight over the real camera (each open fails with 0xC00D3704) and write
    // into the same picture slots. The name includes the channel name so tests use their own.
    std::wstring mutexName = L"Local\\TumonFeeder-";
    for (const wchar_t* c = ChannelNameGlobal(); *c; ++c) mutexName += (*c == L'\\') ? L'_' : *c;
    HANDLE singleInstance = CreateMutexW(nullptr, FALSE, mutexName.c_str());
    if (!singleInstance || GetLastError() == ERROR_ALREADY_EXISTS) {
        report(FeederState::AlreadyRunning);
        if (singleInstance) CloseHandle(singleInstance);
        MFShutdown();
        CoUninitialize();
        running_ = false;
        return;
    }

    FrameChannelClient channel;
    Camera camera;
    uint64_t sentAtLastReport = 0;
    ULONGLONG lastReport = GetTickCount64();
    FeederState last = FeederState::Stopped;
    auto enter = [&](FeederState state) {
        if (state != last) {
            last = state;
            report(state);
        }
    };

    enter(FeederState::Waiting);
    // Wait in short steps so Stop() returns quickly.
    auto pause = [&](int milliseconds) {
        for (int waited = 0; waited < milliseconds && !stop_; waited += 50) Sleep(50);
    };

    while (!stop_) {
        if (!channel.IsOpen() && !channel.Open()) {
            enter(FeederState::Waiting);  // nobody has the camera open yet, so there is no frame channel
            pause(250);
            continue;
        }

        status.consumers = channel.Consumers();
        if (!options.always && status.consumers <= 0) {
            if (camera.IsOpen()) camera.Close();
            status.picturesPerSecond = 0;
            enter(FeederState::Waiting);
            pause(200);
            continue;
        }

        if (!camera.IsOpen()) {
            bool missing = false;
            const HRESULT hr = OpenCamera(options, &camera, &missing);
            if (FAILED(hr)) {
                status.error = hr;
                enter(missing ? FeederState::CameraMissing : FeederState::CameraProblem);
                pause(1000);
                continue;
            }
            status.camera = camera.name;
            status.width = camera.width;
            status.height = camera.height;
            status.error = S_OK;
            sentAtLastReport = status.sent;
            lastReport = GetTickCount64();
            enter(FeederState::Streaming);
        }

        const HRESULT hr = Pump(&camera, &channel, &status.sent);
        if (FAILED(hr)) {
            camera.Close();
            status.error = hr;
            enter(FeederState::CameraProblem);
            pause(500);
            continue;
        }
        status.width = camera.width;
        status.height = camera.height;

        const ULONGLONG now = GetTickCount64();
        if (now - lastReport >= 2000) {
            status.picturesPerSecond =
                static_cast<double>(status.sent - sentAtLastReport) * 1000.0 / static_cast<double>(now - lastReport);
            sentAtLastReport = status.sent;
            lastReport = now;
            report(FeederState::Streaming);
        }
    }

    camera.Close();
    report(FeederState::Stopped);
    CloseHandle(singleInstance);
    MFShutdown();
    CoUninitialize();
    running_ = false;
}

}  // namespace tumon
