// The single video stream of the Tumon test camera.
#pragma once

#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <ks.h>
#include <ksproxy.h>
#include <wrl/client.h>

#include <atomic>
#include <mutex>

#include "attributes.h"
#include "com_impl.h"
#include "frame_generator.h"

namespace tumon {

class MediaStream final : public AttributesBase<IMFAttributes>, public IMFMediaStream2, public IKsControl {
public:
    MediaStream();
    ~MediaStream();

    HRESULT Initialize(IMFMediaSource* source, DWORD index);
    HRESULT SetAllocator(IUnknown* allocator);
    MFSampleAllocatorUsage GetAllocatorUsage() const { return MFSampleAllocatorUsage_UsesProvidedAllocator; }
    HRESULT Start(IMFMediaType* type);
    HRESULT Stop();
    void Shutdown();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;

    // IMFMediaEventGenerator
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* callback, IUnknown* state) override;
    STDMETHODIMP EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** event) override;
    STDMETHODIMP GetEvent(DWORD flags, IMFMediaEvent** event) override;
    STDMETHODIMP QueueEvent(MediaEventType type, REFGUID extendedType, HRESULT status, const PROPVARIANT* value) override;

    // IMFMediaStream
    STDMETHODIMP GetMediaSource(IMFMediaSource** source) override;
    STDMETHODIMP GetStreamDescriptor(IMFStreamDescriptor** descriptor) override;
    STDMETHODIMP RequestSample(IUnknown* token) override;

    // IMFMediaStream2
    STDMETHODIMP SetStreamState(MF_STREAM_STATE value) override;
    STDMETHODIMP GetStreamState(MF_STREAM_STATE* value) override;

    // IKsControl
    STDMETHODIMP KsProperty(PKSPROPERTY property, ULONG length, LPVOID data, ULONG dataLength, ULONG* bytesReturned) override;
    STDMETHODIMP KsMethod(PKSMETHOD method, ULONG length, LPVOID data, ULONG dataLength, ULONG* bytesReturned) override;
    STDMETHODIMP KsEvent(PKSEVENT event, ULONG length, LPVOID data, ULONG dataLength, ULONG* bytesReturned) override;

private:
    HRESULT FillSample(IMFSample* sample);

    std::mutex lock_;
    MF_STREAM_STATE state_ = MF_STREAM_STATE_STOPPED;
    FrameGenerator generator_;
    PixelFormat format_ = PixelFormat::YUY2;
    UINT32 width_ = 640;
    UINT32 height_ = 480;
    UINT32 fps_ = 30;
    uint64_t frame_ = 0;
    MFTIME startTime_ = 0;
    Microsoft::WRL::ComPtr<IMFStreamDescriptor> descriptor_;
    Microsoft::WRL::ComPtr<IMFMediaEventQueue> queue_;
    Microsoft::WRL::ComPtr<IMFMediaSource> source_;
    Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> allocator_;
    bool allocatorReady_ = false;

    TUMON_COM_REFCOUNT()
};

}  // namespace tumon
