// tumon_selftest: loads tumon_source.dll directly (no registration, no administrator rights, no Frame
// Server) and checks that the media source works: for every media type it offers, read frames through a
// Media Foundation source reader and verify count, size, motion and pacing.
//
//   tumon_selftest.exe [path\to\tumon_source.dll]
//
// Exit code 0 when every media type passes.
#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <atomic>
#include <cwctype>
#include <set>
#include <thread>
#include <string>
#include <vector>

#include "../include/tumon/ids.h"
#include "../src/channel/frame_channel.h"
#include "../src/source/formats.h"
#include "../src/source/frame_generator.h"

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "ole32")

using Microsoft::WRL::ComPtr;

namespace {

using GetClassObjectFn = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);

HRESULT CreateSource(HMODULE dll, ComPtr<IMFActivate>* activate, ComPtr<IMFMediaSource>* source) {
    auto getClassObject = reinterpret_cast<GetClassObjectFn>(GetProcAddress(dll, "DllGetClassObject"));
    if (!getClassObject) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    ComPtr<IClassFactory> factory;
    HRESULT hr = getClassObject(tumon::CLSID_TumonCameraSource, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;
    hr = factory->CreateInstance(nullptr, IID_PPV_ARGS(activate->GetAddressOf()));
    if (FAILED(hr)) return hr;
    return (*activate)->ActivateObject(IID_PPV_ARGS(source->GetAddressOf()));
}

struct TypeInfo {
    ComPtr<IMFMediaType> type;
    tumon::PixelFormat format{};
    UINT32 width = 0, height = 0;
};

HRESULT ListTypes(IMFMediaSource* source, std::vector<TypeInfo>* out) {
    ComPtr<IMFPresentationDescriptor> pd;
    HRESULT hr = source->CreatePresentationDescriptor(&pd);
    if (FAILED(hr)) return hr;
    BOOL selected = FALSE;
    ComPtr<IMFStreamDescriptor> sd;
    hr = pd->GetStreamDescriptorByIndex(0, &selected, &sd);
    if (FAILED(hr)) return hr;
    ComPtr<IMFMediaTypeHandler> handler;
    hr = sd->GetMediaTypeHandler(&handler);
    if (FAILED(hr)) return hr;
    DWORD count = 0;
    handler->GetMediaTypeCount(&count);
    for (DWORD i = 0; i < count; ++i) {
        TypeInfo info;
        if (FAILED(handler->GetMediaTypeByIndex(i, &info.type))) continue;
        GUID sub = GUID_NULL;
        info.type->GetGUID(MF_MT_SUBTYPE, &sub);
        if (!tumon::PixelFormatFromSubtype(sub, &info.format)) continue;
        MFGetAttributeSize(info.type.Get(), MF_MT_FRAME_SIZE, &info.width, &info.height);
        out->push_back(std::move(info));
    }
    return S_OK;
}

const wchar_t* FormatName(tumon::PixelFormat f) {
    switch (f) {
        case tumon::PixelFormat::YUY2: return L"YUY2";
        case tumon::PixelFormat::NV12: return L"NV12";
        case tumon::PixelFormat::RGB32: return L"RGB32";
        case tumon::PixelFormat::I420: return L"I420";
    }
    return L"?";
}

// ---------------------------------------------------------------------------------------------- live pictures

// Reads `count` pictures; keeps the last one in `last`. Returns how many were read.
int ReadFrames(IMFSourceReader* reader, int count, std::vector<uint8_t>* last, std::set<uint64_t>* distinct = nullptr) {
    int got = 0;
    for (int attempts = 0; attempts < count * 3 && got < count; ++attempts) {
        DWORD flags = 0;
        LONGLONG ts = 0;
        ComPtr<IMFSample> sample;
        const HRESULT hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, &flags, &ts, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
        if (!sample) continue;
        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;
        BYTE* data = nullptr;
        DWORD cur = 0;
        if (FAILED(buffer->Lock(&data, nullptr, &cur))) continue;
        last->assign(data, data + cur);
        if (distinct) {
            uint64_t h = 1469598103934665603ull;
            for (DWORD k = 0; k < cur; k += 61) h = (h ^ data[k]) * 1099511628211ull;
            distinct->insert(h);
        }
        buffer->Unlock();
        ++got;
    }
    return got;
}

// Creates the source and a reader set to media type 0 (YUY2, 640x480).
bool OpenYuy2Reader(HMODULE dll, ComPtr<IMFActivate>* activate, ComPtr<IMFMediaSource>* source, ComPtr<IMFSourceReader>* reader) {
    if (FAILED(CreateSource(dll, activate, source))) return false;
    std::vector<TypeInfo> types;
    ListTypes(source->Get(), &types);
    if (types.empty() || types[0].format != tumon::PixelFormat::YUY2 || types[0].width != 640 || types[0].height != 480) return false;
    if (FAILED(MFCreateSourceReaderFromMediaSource(source->Get(), nullptr, reader->GetAddressOf()))) return false;
    return SUCCEEDED((*reader)->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, types[0].type.Get()));
}

inline int Luma640(const std::vector<uint8_t>& yuy2, int x, int y) { return yuy2[static_cast<size_t>(y) * 1280 + static_cast<size_t>(x) * 2]; }

// A stand-in for the real feeder: publishes 1280x720 pictures, left half dark (Y=60), right half bright (Y=200).
void FakeFeeder(std::atomic<bool>* stop) {
    tumon::FrameChannelClient client;
    for (int i = 0; i < 100 && !client.Open(); ++i) Sleep(20);
    if (!client.IsOpen()) return;
    const int w = 1280, h = 720;
    std::vector<uint8_t> frame(static_cast<size_t>(w) * h * 2);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; x += 2) {
            const uint8_t luma = x < w / 2 ? 60 : 200;
            uint8_t* p = &frame[(static_cast<size_t>(y) * w + x) * 2];
            p[0] = luma;
            p[1] = 128;
            p[2] = luma;
            p[3] = 128;
        }
    }
    while (!*stop) {
        client.Write(frame.data(), w, h, w * 2);
        Sleep(33);
    }
}

bool TestLiveFrames(HMODULE dll) {
    wprintf(L"Live pictures through the frame channel:\n");
    ComPtr<IMFActivate> activate;
    ComPtr<IMFMediaSource> source;
    ComPtr<IMFSourceReader> reader;
    if (!OpenYuy2Reader(dll, &activate, &source, &reader)) {
        wprintf(L"  cannot open the source for the live test\n");
        return false;
    }
    tumon::FrameChannelClient observer;
    if (!observer.Open()) {
        wprintf(L"  the source did not create the frame channel\n");
        return false;
    }

    bool ok = true;
    auto check = [&](bool condition, const wchar_t* what) {
        wprintf(L"  %-62s %s\n", what, condition ? L"ok" : L"FAIL");
        if (!condition) ok = false;
    };

    std::vector<uint8_t> frame;
    std::atomic<bool> stop{false};
    std::thread feeder(FakeFeeder, &stop);

    // 1. With a feeder: the 1280x720 picture is fitted into 640x480 (black bars above and below).
    const int got = ReadFrames(reader.Get(), 45, &frame);  // the first few may still be the test pattern
    check(got == 45, L"45 pictures read while the feeder runs");
    if (frame.size() == 640u * 480u * 2u) {
        check(std::abs(Luma640(frame, 160, 240) - 60) <= 8, L"left half shows the feeder's dark picture (Y about 60)");
        check(std::abs(Luma640(frame, 480, 240) - 200) <= 8, L"right half shows the feeder's bright picture (Y about 200)");
        check(Luma640(frame, 100, 2) <= 20 && Luma640(frame, 500, 477) <= 20, L"picture is letterboxed: black bars top and bottom");
    } else {
        check(false, L"picture has the expected size");
    }
    check(observer.Consumers() == 1, L"the source reports one running stream to the feeder");

    // 2. The feeder stops: after the time-out the test pattern comes back.
    stop = true;
    feeder.join();
    Sleep(1400);
    ReadFrames(reader.Get(), 10, &frame);
    int brightest = 0;
    if (frame.size() == 640u * 480u * 2u) {
        for (int x = 0; x < 640; x += 4) brightest = std::max(brightest, Luma640(frame, x, 2));
    }
    check(brightest > 150, L"after the feeder stops the test pattern returns");

    source->Shutdown();
    activate->ShutdownObject();
    reader.Reset();
    check(observer.Consumers() == 0, L"no running streams reported after shutdown");
    return ok;
}

// `tumon_selftest --live N`: shows, once a second for N seconds, what the source delivers while a separate
// tumon_feeder.exe (started with the same TUMON_CHANNEL_NAME) feeds it from a real camera. Numbers only.
int RunLive(HMODULE dll, int seconds) {
    ComPtr<IMFActivate> activate;
    ComPtr<IMFMediaSource> source;
    ComPtr<IMFSourceReader> reader;
    if (!OpenYuy2Reader(dll, &activate, &source, &reader)) {
        wprintf(L"Cannot open the source.\n");
        return 3;
    }
    tumon::FrameChannelClient observer;
    if (!observer.Open()) {
        wprintf(L"The source did not create the frame channel.\n");
        return 4;
    }
    wprintf(L"Reading Tumon Camera (YUY2 640x480) for %d seconds. Start tumon_feeder.exe with TUMON_CHANNEL_NAME=%s\n", seconds, observer.Name());
    std::vector<uint8_t> frame;
    for (int s = 0; s < seconds; ++s) {
        std::set<uint64_t> distinct;
        const ULONGLONG t0 = GetTickCount64();
        const int got = ReadFrames(reader.Get(), 30, &frame, &distinct);
        double sum = 0, sumSquares = 0;
        size_t n = 0;
        for (size_t i = 0; i < frame.size(); i += 2) {
            sum += frame[i];
            sumSquares += static_cast<double>(frame[i]) * frame[i];
            ++n;
        }
        const double mean = n ? sum / n : 0;
        const double sd = n ? std::sqrt(std::max(0.0, sumSquares / n - mean * mean)) : 0;
        wprintf(L"  second %2d: %2d pictures in %4llu ms, %2zu distinct, brightness mean %5.1f sd %5.1f, feeder pictures so far %llu, streams %ld\n",
                s + 1, got, GetTickCount64() - t0, distinct.size(), mean, sd, observer.FrameCount(), observer.Consumers());
    }
    source->Shutdown();
    activate->ShutdownObject();
    return 0;
}

// Reads `frames` frames of one media type from a fresh source. Returns true when everything checks out.
bool TestType(HMODULE dll, size_t index, const TypeInfo& wanted, int frames) {
    ComPtr<IMFActivate> activate;
    ComPtr<IMFMediaSource> source;
    HRESULT hr = CreateSource(dll, &activate, &source);
    if (FAILED(hr)) {
        wprintf(L"  type %zu: cannot create the source: 0x%08X\n", index, static_cast<unsigned>(hr));
        return false;
    }

    std::vector<TypeInfo> types;
    ListTypes(source.Get(), &types);
    if (index >= types.size()) {
        wprintf(L"  type %zu: missing\n", index);
        return false;
    }

    ComPtr<IMFSourceReader> reader;
    hr = MFCreateSourceReaderFromMediaSource(source.Get(), nullptr, &reader);
    if (FAILED(hr)) {
        wprintf(L"  type %zu: source reader failed: 0x%08X\n", index, static_cast<unsigned>(hr));
        return false;
    }
    hr = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, types[index].type.Get());
    if (FAILED(hr)) {
        wprintf(L"  type %zu: SetCurrentMediaType failed: 0x%08X\n", index, static_cast<unsigned>(hr));
        return false;
    }

    const size_t expectedBytes = tumon::FrameBytes(wanted.format, tumon::DefaultStride(wanted.format, static_cast<int>(wanted.width)),
                                                   static_cast<int>(wanted.height));
    std::set<uint64_t> distinct;
    std::vector<LONGLONG> stamps;
    DWORD bytes = 0;
    HRESULT lastHr = S_OK;
    DWORD lastFlags = 0;
    for (int attempts = 0; attempts < frames * 3 && static_cast<int>(stamps.size()) < frames; ++attempts) {
        DWORD flags = 0;
        LONGLONG ts = 0;
        ComPtr<IMFSample> sample;
        hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, &flags, &ts, &sample);
        lastHr = hr;
        lastFlags = flags;
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
        if (!sample) continue;
        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;
        BYTE* data = nullptr;
        DWORD cur = 0;
        if (FAILED(buffer->Lock(&data, nullptr, &cur))) continue;
        uint64_t h = 1469598103934665603ull;
        for (DWORD k = 0; k < cur; k += 31) h = (h ^ data[k]) * 1099511628211ull;
        distinct.insert(h);
        bytes = cur;
        buffer->Unlock();
        stamps.push_back(ts);
    }

    double intervalMs = 0;
    if (stamps.size() > 1) intervalMs = (stamps.back() - stamps.front()) / 10000.0 / static_cast<double>(stamps.size() - 1);

    const bool countOk = static_cast<int>(stamps.size()) == frames;
    const bool sizeOk = bytes == expectedBytes;
    const bool motionOk = distinct.size() > 1;
    const bool paceOk = std::fabs(intervalMs - 33.3) < 10.0;
    const bool ok = countOk && sizeOk && motionOk && paceOk;
    wprintf(L"  %-5s %4ux%-4u  frames %2zu/%d  bytes %8lu (want %zu)  distinct %2zu  interval %5.1f ms  %s\n",
            FormatName(wanted.format), wanted.width, wanted.height, stamps.size(), frames, bytes, expectedBytes,
            distinct.size(), intervalMs, ok ? L"ok" : L"FAIL");

    if (!ok) wprintf(L"        last ReadSample: hr=0x%08X flags=0x%X\n", static_cast<unsigned>(lastHr), lastFlags);

    source->Shutdown();
    activate->ShutdownObject();
    return ok;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring path = L"tumon_source.dll";
    bool live = false;
    int liveSeconds = 10;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--live") == 0) {
            live = true;
            if (i + 1 < argc && iswdigit(argv[i + 1][0])) liveSeconds = _wtoi(argv[++i]);
        } else {
            path = argv[i];
        }
    }
    // Private channel names: tests never touch a real Tumon Camera, and --live and a separate feeder agree on one.
    SetEnvironmentVariableW(L"TUMON_CHANNEL_NAME", live ? L"Local\\TumonCameraFrames-LiveTest" : L"Local\\TumonCameraFrames-SelfTest");

    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) || FAILED(MFStartup(MF_VERSION))) {
        wprintf(L"Could not start COM or Media Foundation.\n");
        return 1;
    }

    HMODULE dll = LoadLibraryW(path.c_str());
    if (!dll) {
        wprintf(L"Cannot load %s (error %lu)\n", path.c_str(), GetLastError());
        return 2;
    }
    wprintf(L"Loaded %s\n", path.c_str());
    if (live) return RunLive(dll, liveSeconds);

    int failures = 0;
    {
        ComPtr<IMFActivate> activate;
        ComPtr<IMFMediaSource> source;
        HRESULT hr = CreateSource(dll, &activate, &source);
        if (FAILED(hr)) {
            wprintf(L"Cannot create the source: 0x%08X\n", static_cast<unsigned>(hr));
            return 3;
        }
        DWORD characteristics = 0;
        source->GetCharacteristics(&characteristics);
        wprintf(L"Characteristics: 0x%X (live source: %s)\n", characteristics, (characteristics & MFMEDIASOURCE_IS_LIVE) ? L"yes" : L"NO");
        if (!(characteristics & MFMEDIASOURCE_IS_LIVE)) ++failures;

        std::vector<TypeInfo> types;
        hr = ListTypes(source.Get(), &types);
        if (FAILED(hr) || types.empty()) {
            wprintf(L"No media types: 0x%08X\n", static_cast<unsigned>(hr));
            return 4;
        }
        wprintf(L"Media types offered: %zu\n", types.size());
        source->Shutdown();
        activate->ShutdownObject();

        for (size_t i = 0; i < types.size(); ++i) {
            if (!TestType(dll, i, types[i], 20)) ++failures;
        }
    }

    if (!TestLiveFrames(dll)) ++failures;

    wprintf(L"%s\n", failures == 0 ? L"RESULT: all media types and the live-picture test pass." : L"RESULT: FAILURES, see above.");
    MFShutdown();
    CoUninitialize();
    return failures == 0 ? 0 : 5;
}
