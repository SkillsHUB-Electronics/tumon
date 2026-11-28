#include "idle_loader.h"

#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs")

using Microsoft::WRL::ComPtr;

namespace tumon {

namespace {

bool Decode(const BYTE* data, DWORD size, std::vector<uint8_t>* bgra, int* width, int* height) {
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return false;
    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(data), size))) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        return false;
    }
    UINT w = 0, h = 0;
    if (FAILED(converter->GetSize(&w, &h)) || w == 0 || h == 0 || w > 4096 || h > 4096) return false;
    bgra->resize(static_cast<size_t>(w) * h * 4);
    if (FAILED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(bgra->size()), bgra->data()))) return false;
    *width = static_cast<int>(w);
    *height = static_cast<int>(h);
    return true;
}

}  // namespace

bool LoadIdleArt(HMODULE module, std::vector<uint8_t>* bgra, int* width, int* height) {
    const HRSRC found = FindResourceW(module, L"IDLEART", RT_RCDATA);
    if (!found) return false;
    const HGLOBAL loaded = LoadResource(module, found);
    const DWORD size = SizeofResource(module, found);
    const auto* data = loaded ? static_cast<const BYTE*>(LockResource(loaded)) : nullptr;
    if (!data || size == 0) return false;

    // The Frame Server thread usually has COM already; if not, start it just for the decode.
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool ok = Decode(data, size, bgra, width, height);
    if (SUCCEEDED(com)) CoUninitialize();
    return ok;
}

}  // namespace tumon
