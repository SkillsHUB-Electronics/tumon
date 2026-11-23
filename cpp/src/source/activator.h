// The COM class registered under the Tumon CLSID. MFCreateVirtualCamera makes the Frame Server create one
// of these; it hands out the media source.
#pragma once

#include <windows.h>

#include <mfidl.h>
#include <wrl/client.h>

#include "attributes.h"
#include "com_impl.h"

namespace tumon {

class Activator final : public AttributesBase<IMFActivate> {
public:
    Activator();
    ~Activator();

    HRESULT Initialize();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;

    // IMFActivate
    STDMETHODIMP ActivateObject(REFIID riid, void** ppv) override;
    STDMETHODIMP ShutdownObject() override;
    STDMETHODIMP DetachObject() override;

private:
    Microsoft::WRL::ComPtr<IMFMediaSourceEx> source_;

    TUMON_COM_REFCOUNT()
};

}  // namespace tumon
