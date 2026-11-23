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
#include <set>
#include <string>
#include <vector>

#include "../include/tumon/ids.h"
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
    if (argc > 1) path = argv[1];

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

    wprintf(L"%s\n", failures == 0 ? L"RESULT: all media types pass." : L"RESULT: FAILURES, see above.");
    MFShutdown();
    CoUninitialize();
    return failures == 0 ? 0 : 5;
}
