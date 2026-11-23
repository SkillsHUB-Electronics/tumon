#include <windows.h>
#include <initguid.h>  // define the GUIDs declared by the Media Foundation headers

#include "activator.h"

#include <mfapi.h>
#include <mferror.h>
#include <mftransform.h>
#include <mfvirtualcamera.h>

#include <new>

#include "../../include/tumon/ids.h"
#include "media_source.h"

using Microsoft::WRL::ComPtr;

namespace tumon {

Activator::Activator() {
    ++g_objectCount;
    InitAttributes();
}

Activator::~Activator() { --g_objectCount; }

HRESULT Activator::Initialize() {
    HRESULT hr = SetUINT32(MF_VIRTUALCAMERA_PROVIDE_ASSOCIATED_CAMERA_SOURCES, 1);
    if (FAILED(hr)) return hr;
    hr = SetGUID(MFT_TRANSFORM_CLSID_Attribute, CLSID_TumonCameraSource);
    if (FAILED(hr)) return hr;

    auto* source = new (std::nothrow) MediaSource();
    if (!source) return E_OUTOFMEMORY;
    source_.Attach(static_cast<IMFMediaSourceEx*>(source));  // we own the creation reference
    return source->Initialize(this);
}

STDMETHODIMP Activator::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == __uuidof(IMFActivate)) {
        *ppv = static_cast<IMFActivate*>(this);
    } else if (riid == __uuidof(IMFAttributes)) {
        *ppv = static_cast<IMFAttributes*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP Activator::ActivateObject(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (!source_) return MF_E_SHUTDOWN;
    return source_->QueryInterface(riid, ppv);
}

STDMETHODIMP Activator::ShutdownObject() { return S_OK; }

STDMETHODIMP Activator::DetachObject() {
    source_.Reset();
    return S_OK;
}

}  // namespace tumon
