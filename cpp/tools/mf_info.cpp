// tumon_mfinfo: lists every video capture device Media Foundation knows, with each device's native media
// types, and can read frames from one of them.
//
//   tumon_mfinfo.exe                         list devices and their native formats
//   tumon_mfinfo.exe --read NAME [FRAMES]    read FRAMES (default 60) frames from the first device whose name
//                                            contains NAME, using the first native type; report the rate
//   tumon_mfinfo.exe --read NAME FRAMES --type N   use native type number N instead (as numbered by the listing)
//
// Read-only. Opening a camera may light its LED.
#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <map>
#include <set>
#include <string>
#include <vector>

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "ole32")

using Microsoft::WRL::ComPtr;

namespace {

std::wstring SubtypeName(const GUID& g) {
    // Subtypes built from a FOURCC share one GUID tail; RGB variants use small numbers.
    static const GUID base = {0x00000000, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};
    GUID tail = g;
    tail.Data1 = 0;
    if (memcmp(&tail, &base, sizeof(GUID)) != 0) {
        wchar_t buf[64];
        StringFromGUID2(g, buf, 64);
        return buf;
    }
    switch (g.Data1) {
        case 20: return L"RGB24";
        case 21: return L"ARGB32";
        case 22: return L"RGB32";
        default: break;
    }
    std::wstring name;
    for (int i = 0; i < 4; ++i) {
        const wchar_t c = static_cast<wchar_t>((g.Data1 >> (8 * i)) & 0xFF);
        if (c < 0x20 || c > 0x7E) {
            wchar_t buf[16];
            swprintf_s(buf, L"FMT_%u", g.Data1);
            return buf;
        }
        name += c;
    }
    return name;
}

struct NativeType {
    std::wstring subtype;
    UINT32 width = 0, height = 0;
    double fps = 0;
    ComPtr<IMFMediaType> type;
};

struct Device {
    std::wstring name;
    ComPtr<IMFActivate> activate;
};

std::wstring GetName(IMFActivate* a) {
    WCHAR* s = nullptr;
    UINT32 len = 0;
    std::wstring name = L"(unnamed)";
    if (SUCCEEDED(a->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &s, &len)) && s) {
        name = s;
        CoTaskMemFree(s);
    }
    return name;
}

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
        d.name = GetName(list[i]);
        d.activate.Attach(list[i]);
        devices.push_back(std::move(d));
    }
    CoTaskMemFree(list);
    return devices;
}

HRESULT ReadNativeTypes(IMFMediaSource* source, std::vector<NativeType>* out) {
    ComPtr<IMFPresentationDescriptor> pd;
    HRESULT hr = source->CreatePresentationDescriptor(&pd);
    if (FAILED(hr)) return hr;
    DWORD streams = 0;
    pd->GetStreamDescriptorCount(&streams);
    for (DWORD s = 0; s < streams; ++s) {
        BOOL selected = FALSE;
        ComPtr<IMFStreamDescriptor> sd;
        if (FAILED(pd->GetStreamDescriptorByIndex(s, &selected, &sd))) continue;
        ComPtr<IMFMediaTypeHandler> handler;
        if (FAILED(sd->GetMediaTypeHandler(&handler))) continue;
        GUID major = GUID_NULL;
        handler->GetMajorType(&major);
        if (major != MFMediaType_Video) continue;
        DWORD count = 0;
        handler->GetMediaTypeCount(&count);
        for (DWORD t = 0; t < count; ++t) {
            ComPtr<IMFMediaType> type;
            if (FAILED(handler->GetMediaTypeByIndex(t, &type))) continue;
            NativeType n;
            GUID sub = GUID_NULL;
            if (SUCCEEDED(type->GetGUID(MF_MT_SUBTYPE, &sub))) n.subtype = SubtypeName(sub);
            MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &n.width, &n.height);
            UINT32 num = 0, den = 1;
            if (SUCCEEDED(MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &num, &den)) && den) n.fps = static_cast<double>(num) / den;
            n.type = type;
            out->push_back(std::move(n));
        }
    }
    return S_OK;
}

HRESULT OpenSource(IMFActivate* act, ComPtr<IMFMediaSource>* source) {
    return act->ActivateObject(IID_PPV_ARGS(source->GetAddressOf()));
}

void ListDevices() {
    std::vector<Device> devices = EnumerateDevices();
    wprintf(L"# Media Foundation video capture devices (%zu)\n\n", devices.size());
    for (size_t d = 0; d < devices.size(); ++d) {
        wprintf(L"## %s\n", devices[d].name.c_str());
        ComPtr<IMFMediaSource> source;
        HRESULT hr = OpenSource(devices[d].activate.Get(), &source);
        if (FAILED(hr)) {
            wprintf(L"- could not open: 0x%08X\n\n", static_cast<unsigned>(hr));
            continue;
        }
        std::vector<NativeType> types;
        hr = ReadNativeTypes(source.Get(), &types);
        if (FAILED(hr)) {
            wprintf(L"- could not read types: 0x%08X\n\n", static_cast<unsigned>(hr));
        } else {
            // Group by subtype, keep numbering stable for --type.
            std::map<std::wstring, std::vector<size_t>> bySubtype;
            for (size_t i = 0; i < types.size(); ++i) bySubtype[types[i].subtype].push_back(i);
            for (auto& entry : bySubtype) {
                wprintf(L"- **%s**:", entry.first.c_str());
                for (size_t i : entry.second) {
                    wprintf(L" #%zu %ux%u@%.0f", i, types[i].width, types[i].height, types[i].fps);
                }
                wprintf(L"\n");
            }
            if (types.empty()) wprintf(L"- (no video types)\n");
            wprintf(L"\n");
        }
        source->Shutdown();
        devices[d].activate->ShutdownObject();
    }
}

bool ContainsNoCase(const std::wstring& haystack, const std::wstring& needle) {
    auto lower = [](std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return s;
    };
    return lower(haystack).find(lower(needle)) != std::wstring::npos;
}

int ReadFrames(const std::wstring& name, int frames, int typeIndex) {
    std::vector<Device> devices = EnumerateDevices();
    Device* chosen = nullptr;
    for (Device& d : devices) {
        if (ContainsNoCase(d.name, name)) {
            chosen = &d;
            break;
        }
    }
    if (!chosen) {
        wprintf(L"No video capture device whose name contains \"%s\".\n", name.c_str());
        return 2;
    }
    wprintf(L"Device: %s\n", chosen->name.c_str());

    ComPtr<IMFMediaSource> source;
    HRESULT hr = OpenSource(chosen->activate.Get(), &source);
    if (FAILED(hr)) {
        wprintf(L"Could not open the device: 0x%08X\n", static_cast<unsigned>(hr));
        return 3;
    }
    std::vector<NativeType> types;
    ReadNativeTypes(source.Get(), &types);
    if (types.empty() || typeIndex < 0 || static_cast<size_t>(typeIndex) >= types.size()) {
        wprintf(L"Native type #%d does not exist (device offers %zu).\n", typeIndex, types.size());
        return 4;
    }
    const NativeType& want = types[typeIndex];
    wprintf(L"Native type #%d: %s %ux%u@%.0f\n", typeIndex, want.subtype.c_str(), want.width, want.height, want.fps);

    ComPtr<IMFSourceReader> reader;
    hr = MFCreateSourceReaderFromMediaSource(source.Get(), nullptr, &reader);
    if (FAILED(hr)) {
        wprintf(L"MFCreateSourceReaderFromMediaSource failed: 0x%08X\n", static_cast<unsigned>(hr));
        return 5;
    }
    hr = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, want.type.Get());
    if (FAILED(hr)) {
        wprintf(L"SetCurrentMediaType failed: 0x%08X\n", static_cast<unsigned>(hr));
        return 6;
    }

    std::set<uint64_t> distinct;
    int got = 0;
    DWORD bytes = 0;
    LONGLONG firstTs = 0, lastTs = 0;
    const ULONGLONG t0 = GetTickCount64();
    for (int i = 0; i < frames * 4 && got < frames; ++i) {
        DWORD flags = 0;
        LONGLONG ts = 0;
        ComPtr<IMFSample> sample;
        hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, &flags, &ts, &sample);
        if (FAILED(hr)) {
            wprintf(L"ReadSample failed: 0x%08X\n", static_cast<unsigned>(hr));
            return 7;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;

        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;
        BYTE* data = nullptr;
        DWORD max = 0, cur = 0;
        if (FAILED(buffer->Lock(&data, &max, &cur))) continue;
        uint64_t h = 1469598103934665603ull;  // FNV-1a over a sparse subset of the frame
        for (DWORD k = 0; k < cur; k += 61) h = (h ^ data[k]) * 1099511628211ull;
        distinct.insert(h);
        bytes = cur;
        buffer->Unlock();

        if (got == 0) firstTs = ts;
        lastTs = ts;
        ++got;
    }
    const double seconds = (GetTickCount64() - t0) / 1000.0;
    const double span = (lastTs - firstTs) / 10000000.0;
    wprintf(L"Frames read: %d, %lu bytes each, %zu distinct pictures\n", got, bytes, distinct.size());
    wprintf(L"Wall time: %.2f s (%.1f fps), sample timestamps span %.2f s (%.1f fps)\n", seconds,
            seconds > 0 ? got / seconds : 0.0, span, span > 0 ? (got - 1) / span : 0.0);
    wprintf(L"%s\n", (got >= frames && distinct.size() > 1) ? L"RESULT: OK, frames arrive and the picture changes."
                                                          : L"RESULT: PROBLEM, too few frames or the picture never changes.");

    source->Shutdown();
    chosen->activate->ShutdownObject();
    return got >= frames && distinct.size() > 1 ? 0 : 8;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 1;
    if (FAILED(MFStartup(MF_VERSION))) return 1;

    int rc = 0;
    if (argc >= 3 && wcscmp(argv[1], L"--read") == 0) {
        int frames = 60;
        int typeIndex = 0;
        for (int i = 3; i < argc; ++i) {
            if (wcscmp(argv[i], L"--type") == 0 && i + 1 < argc) {
                typeIndex = _wtoi(argv[++i]);
            } else if (iswdigit(argv[i][0])) {
                frames = _wtoi(argv[i]);
            }
        }
        rc = ReadFrames(argv[2], frames, typeIndex);
    } else {
        ListDevices();
    }

    MFShutdown();
    CoUninitialize();
    return rc;
}
