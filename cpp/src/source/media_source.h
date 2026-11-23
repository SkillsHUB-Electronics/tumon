// The Tumon test camera as a Media Foundation media source (what the Windows Frame Server loads).
#pragma once

#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <ks.h>
#include <ksproxy.h>
#include <wrl/client.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "../channel/frame_channel.h"
#include "attributes.h"
#include "com_impl.h"

namespace tumon {

class MediaSource final : public AttributesBase<IMFAttributes>,
                          public IMFMediaSourceEx,
                          public IMFGetService,
                          public IKsControl,
                          public IMFSampleAllocatorControl {
public:
    MediaSource();
    ~MediaSource();

    // Call once, after the creator holds its reference. `attributes` are the activation object's.
    HRESULT Initialize(IMFAttributes* attributes);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;

    // IMFMediaEventGenerator
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* callback, IUnknown* state) override;
    STDMETHODIMP EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** event) override;
    STDMETHODIMP GetEvent(DWORD flags, IMFMediaEvent** event) override;
    STDMETHODIMP QueueEvent(MediaEventType type, REFGUID extendedType, HRESULT status, const PROPVARIANT* value) override;

    // IMFMediaSource
    STDMETHODIMP GetCharacteristics(DWORD* characteristics) override;
    STDMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor** descriptor) override;
    STDMETHODIMP Start(IMFPresentationDescriptor* descriptor, const GUID* timeFormat, const PROPVARIANT* startPosition) override;
    STDMETHODIMP Stop() override;
    STDMETHODIMP Pause() override;
    STDMETHODIMP Shutdown() override;

    // IMFMediaSourceEx
    STDMETHODIMP GetSourceAttributes(IMFAttributes** attributes) override;
    STDMETHODIMP GetStreamAttributes(DWORD streamId, IMFAttributes** attributes) override;
    STDMETHODIMP SetD3DManager(IUnknown* manager) override;

    // IMFGetService
    STDMETHODIMP GetService(REFGUID service, REFIID riid, LPVOID* object) override;

    // IMFSampleAllocatorControl
    STDMETHODIMP SetDefaultAllocator(DWORD outputStreamId, IUnknown* allocator) override;
    STDMETHODIMP GetAllocatorUsage(DWORD outputStreamId, DWORD* inputStreamId, MFSampleAllocatorUsage* usage) override;

    // IKsControl
    STDMETHODIMP KsProperty(PKSPROPERTY property, ULONG length, LPVOID data, ULONG dataLength, ULONG* bytesReturned) override;
    STDMETHODIMP KsMethod(PKSMETHOD method, ULONG length, LPVOID data, ULONG dataLength, ULONG* bytesReturned) override;
    STDMETHODIMP KsEvent(PKSEVENT event, ULONG length, LPVOID data, ULONG dataLength, ULONG* bytesReturned) override;

private:
    int StreamIndexById(DWORD id);

    std::mutex lock_;
    std::shared_ptr<FrameChannelServer> channel_;
    std::vector<Microsoft::WRL::ComPtr<IMFMediaStream2>> streams_;
    Microsoft::WRL::ComPtr<IMFMediaEventQueue> queue_;
    Microsoft::WRL::ComPtr<IMFPresentationDescriptor> descriptor_;

    TUMON_COM_REFCOUNT()
};

}  // namespace tumon
