#include <windows.h>
#include <initguid.h>  // define the GUIDs declared by the Media Foundation and kernel-streaming headers

#include "media_source.h"

#include <ksmedia.h>
#include <mferror.h>
#include <propvarutil.h>

#include "media_stream.h"
#include "trace.h"

#pragma comment(lib, "mfsensorgroup")

using Microsoft::WRL::ComPtr;

namespace tumon {

namespace {

constexpr HRESULT kSetNotFound = static_cast<HRESULT>(0x80070492);  // HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND)

MediaStream* AsStream(IMFMediaStream2* stream) { return static_cast<MediaStream*>(stream); }

}  // namespace

MediaSource::MediaSource() {
    ++g_objectCount;
    InitAttributes();
}

MediaSource::~MediaSource() { --g_objectCount; }

HRESULT MediaSource::Initialize(IMFAttributes* attributes) {
    if (attributes) {
        HRESULT hr = attributes->CopyAllItems(static_cast<IMFAttributes*>(this));
        if (FAILED(hr)) return hr;
    }

    // Sensor profiles tell the Frame Server which stream configurations exist. Same shape as the
    // reference virtual camera: a legacy profile (up to 30 fps) and a high-frame-rate profile.
    ComPtr<IMFSensorProfileCollection> collection;
    HRESULT hr = MFCreateSensorProfileCollection(&collection);
    if (FAILED(hr)) return hr;

    ComPtr<IMFSensorProfile> profile;
    hr = MFCreateSensorProfile(KSCAMERAPROFILE_Legacy, 0, nullptr, &profile);
    if (FAILED(hr)) return hr;
    hr = profile->AddProfileFilter(0, L"((RES==;FRT<=30,1;SUT==))");
    if (FAILED(hr)) return hr;
    hr = collection->AddProfile(profile.Get());
    if (FAILED(hr)) return hr;

    profile.Reset();
    hr = MFCreateSensorProfile(KSCAMERAPROFILE_HighFrameRate, 0, nullptr, &profile);
    if (FAILED(hr)) return hr;
    hr = profile->AddProfileFilter(0, L"((RES==;FRT>=60,1;SUT==))");
    if (FAILED(hr)) return hr;
    hr = collection->AddProfile(profile.Get());
    if (FAILED(hr)) return hr;
    hr = SetUnknown(MF_DEVICEMFT_SENSORPROFILE_COLLECTION, collection.Get());
    if (FAILED(hr)) return hr;

    // The shared block through which the feeder program hands over pictures from a real camera. If it cannot be
    // created the camera still works and simply shows the test pattern.
    channel_ = std::make_shared<FrameChannelServer>();
    if (!channel_->Create()) Trace(L"MediaSource: no frame channel (test pattern only)");

    // One video stream.
    auto* stream = new (std::nothrow) MediaStream();
    if (!stream) return E_OUTOFMEMORY;
    ComPtr<IMFMediaStream2> streamHolder;
    streamHolder.Attach(static_cast<IMFMediaStream2*>(stream));
    hr = stream->Initialize(static_cast<IMFMediaSourceEx*>(this), 0);
    if (FAILED(hr)) return hr;
    stream->SetChannel(channel_);
    streams_.push_back(streamHolder);

    std::vector<ComPtr<IMFStreamDescriptor>> descriptors(streams_.size());
    std::vector<IMFStreamDescriptor*> raw(streams_.size());
    for (size_t i = 0; i < streams_.size(); ++i) {
        hr = streams_[i]->GetStreamDescriptor(&descriptors[i]);
        if (FAILED(hr)) return hr;
        raw[i] = descriptors[i].Get();
    }
    hr = MFCreatePresentationDescriptor(static_cast<DWORD>(raw.size()), raw.data(), &descriptor_);
    if (FAILED(hr)) return hr;
    // A new presentation descriptor has every stream deselected. A live camera starts with its stream
    // selected, so clients that start the source with a clone of this descriptor get the video stream.
    for (DWORD i = 0; i < raw.size(); ++i) {
        hr = descriptor_->SelectStream(i);
        if (FAILED(hr)) return hr;
    }
    return MFCreateEventQueue(&queue_);
}

int MediaSource::StreamIndexById(DWORD id) {
    for (size_t i = 0; i < streams_.size(); ++i) {
        ComPtr<IMFStreamDescriptor> desc;
        if (FAILED(streams_[i]->GetStreamDescriptor(&desc))) return -1;
        DWORD streamId = 0;
        if (FAILED(desc->GetStreamIdentifier(&streamId))) return -1;
        if (streamId == id) return static_cast<int>(i);
    }
    return -1;
}

// IUnknown
STDMETHODIMP MediaSource::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == __uuidof(IMFMediaEventGenerator) || riid == __uuidof(IMFMediaSource) ||
        riid == __uuidof(IMFMediaSourceEx)) {
        *ppv = static_cast<IMFMediaSourceEx*>(this);
    } else if (riid == __uuidof(IMFAttributes)) {
        *ppv = static_cast<IMFAttributes*>(this);
    } else if (riid == __uuidof(IMFGetService)) {
        *ppv = static_cast<IMFGetService*>(this);
    } else if (riid == __uuidof(IKsControl)) {
        *ppv = static_cast<IKsControl*>(this);
    } else if (riid == __uuidof(IMFSampleAllocatorControl)) {
        *ppv = static_cast<IMFSampleAllocatorControl*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

// IMFMediaEventGenerator
STDMETHODIMP MediaSource::BeginGetEvent(IMFAsyncCallback* callback, IUnknown* state) {
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_) return MF_E_SHUTDOWN;
    return queue_->BeginGetEvent(callback, state);
}

STDMETHODIMP MediaSource::EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** event) {
    if (!event) return E_POINTER;
    *event = nullptr;
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_) return MF_E_SHUTDOWN;
    return queue_->EndGetEvent(result, event);
}

STDMETHODIMP MediaSource::GetEvent(DWORD flags, IMFMediaEvent** event) {
    if (!event) return E_POINTER;
    *event = nullptr;
    ComPtr<IMFMediaEventQueue> queue;
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (!queue_) return MF_E_SHUTDOWN;
        queue = queue_;
    }
    return queue->GetEvent(flags, event);  // may block, so not under the lock
}

STDMETHODIMP MediaSource::QueueEvent(MediaEventType type, REFGUID extendedType, HRESULT status, const PROPVARIANT* value) {
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_) return MF_E_SHUTDOWN;
    return queue_->QueueEventParamVar(type, extendedType, status, value);
}

// IMFMediaSource
STDMETHODIMP MediaSource::GetCharacteristics(DWORD* characteristics) {
    if (!characteristics) return E_POINTER;
    *characteristics = MFMEDIASOURCE_IS_LIVE;
    return S_OK;
}

STDMETHODIMP MediaSource::CreatePresentationDescriptor(IMFPresentationDescriptor** descriptor) {
    if (!descriptor) return E_POINTER;
    *descriptor = nullptr;
    std::lock_guard<std::mutex> guard(lock_);
    if (!descriptor_) return MF_E_SHUTDOWN;
    return descriptor_->Clone(descriptor);
}

STDMETHODIMP MediaSource::Start(IMFPresentationDescriptor* descriptor, const GUID* timeFormat, const PROPVARIANT* startPosition) {
    Trace(L"MediaSource::Start descriptor=%p timeFormat=%p startPosition=%p", descriptor, timeFormat, startPosition);
    if (!descriptor || !startPosition) return E_POINTER;
    if (timeFormat && *timeFormat != GUID_NULL) return E_INVALIDARG;
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_ || !descriptor_) return MF_E_SHUTDOWN;

    DWORD count = 0;
    HRESULT hr = descriptor->GetStreamDescriptorCount(&count);
    if (FAILED(hr)) return hr;
    if (count != streams_.size()) return E_INVALIDARG;

    for (DWORD i = 0; i < count; ++i) {
        ComPtr<IMFStreamDescriptor> desc;
        BOOL selected = FALSE;
        hr = descriptor->GetStreamDescriptorByIndex(i, &selected, &desc);
        if (FAILED(hr)) return hr;

        DWORD id = 0;
        hr = desc->GetStreamIdentifier(&id);
        if (FAILED(hr)) return hr;
        const int index = StreamIndexById(id);
        if (index < 0) return E_FAIL;

        MF_STREAM_STATE state = MF_STREAM_STATE_STOPPED;
        streams_[index]->GetStreamState(&state);
        Trace(L"MediaSource::Start stream %d selected=%d state=%d", index, static_cast<int>(selected), static_cast<int>(state));

        if (selected) {
            hr = descriptor_->SelectStream(index);
            if (FAILED(hr)) return hr;

            // New stream the first time, an update when it was already running.
            ComPtr<IUnknown> streamUnknown;
            hr = streams_[index].As(&streamUnknown);
            if (FAILED(hr)) return hr;
            hr = queue_->QueueEventParamUnk(state == MF_STREAM_STATE_STOPPED ? MENewStream : MEUpdatedStream,
                                            GUID_NULL, S_OK, streamUnknown.Get());
            if (FAILED(hr)) return hr;

            ComPtr<IMFMediaTypeHandler> handler;
            ComPtr<IMFMediaType> type;
            hr = desc->GetMediaTypeHandler(&handler);
            if (SUCCEEDED(hr)) hr = handler->GetCurrentMediaType(&type);
            if (FAILED(hr)) return hr;
            hr = AsStream(streams_[index].Get())->Start(type.Get());
            if (FAILED(hr)) return hr;
        } else {
            descriptor_->DeselectStream(index);
            AsStream(streams_[index].Get())->Stop();
        }
    }

    PROPVARIANT time;
    PropVariantInit(&time);
    hr = InitPropVariantFromInt64(MFGetSystemTime(), &time);
    if (FAILED(hr)) return hr;
    hr = queue_->QueueEventParamVar(MESourceStarted, GUID_NULL, S_OK, &time);
    PropVariantClear(&time);
    return hr;
}

STDMETHODIMP MediaSource::Stop() {
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_ || !descriptor_) return MF_E_SHUTDOWN;

    for (DWORD i = 0; i < streams_.size(); ++i) {
        HRESULT hr = AsStream(streams_[i].Get())->Stop();
        if (FAILED(hr)) return hr;
        descriptor_->DeselectStream(i);
    }

    PROPVARIANT time;
    PropVariantInit(&time);
    HRESULT hr = InitPropVariantFromInt64(MFGetSystemTime(), &time);
    if (FAILED(hr)) return hr;
    hr = queue_->QueueEventParamVar(MESourceStopped, GUID_NULL, S_OK, &time);
    PropVariantClear(&time);
    return hr;
}

STDMETHODIMP MediaSource::Pause() { return MF_E_INVALID_STATE_TRANSITION; }

STDMETHODIMP MediaSource::Shutdown() {
    std::vector<ComPtr<IMFMediaStream2>> streams;
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (!queue_) return MF_E_SHUTDOWN;
        queue_->Shutdown();
        queue_.Reset();
        descriptor_.Reset();
        streams.swap(streams_);
    }
    for (auto& stream : streams) AsStream(stream.Get())->Shutdown();  // breaks the source <-> stream reference cycle
    channel_.reset();
    ReleaseAttributes();
    return S_OK;
}

// IMFMediaSourceEx
STDMETHODIMP MediaSource::GetSourceAttributes(IMFAttributes** attributes) {
    if (!attributes) return E_POINTER;
    *attributes = static_cast<IMFAttributes*>(this);
    AddRef();
    return S_OK;
}

STDMETHODIMP MediaSource::GetStreamAttributes(DWORD streamId, IMFAttributes** attributes) {
    if (!attributes) return E_POINTER;
    *attributes = nullptr;
    std::lock_guard<std::mutex> guard(lock_);
    if (streamId >= streams_.size()) return E_INVALIDARG;
    return streams_[streamId].CopyTo(IID_PPV_ARGS(attributes));
}

// The test camera draws on the CPU only, so a Direct3D manager from the client is ignored.
STDMETHODIMP MediaSource::SetD3DManager(IUnknown* manager) {
    if (!manager) return E_POINTER;
    return S_OK;
}

// IMFGetService
STDMETHODIMP MediaSource::GetService(REFGUID, REFIID, LPVOID*) { return MF_E_UNSUPPORTED_SERVICE; }

// IMFSampleAllocatorControl
STDMETHODIMP MediaSource::SetDefaultAllocator(DWORD outputStreamId, IUnknown* allocator) {
    if (!allocator) return E_POINTER;
    std::lock_guard<std::mutex> guard(lock_);
    const int index = StreamIndexById(outputStreamId);
    if (index < 0) return E_FAIL;
    return AsStream(streams_[index].Get())->SetAllocator(allocator);
}

STDMETHODIMP MediaSource::GetAllocatorUsage(DWORD outputStreamId, DWORD* inputStreamId, MFSampleAllocatorUsage* usage) {
    if (!inputStreamId || !usage) return E_POINTER;
    std::lock_guard<std::mutex> guard(lock_);
    const int index = StreamIndexById(outputStreamId);
    if (index < 0) return E_FAIL;
    *inputStreamId = outputStreamId;
    *usage = AsStream(streams_[index].Get())->GetAllocatorUsage();
    return S_OK;
}

// IKsControl: no camera-control properties are exposed.
STDMETHODIMP MediaSource::KsProperty(PKSPROPERTY property, ULONG, LPVOID, ULONG, ULONG* bytesReturned) {
    if (!property || !bytesReturned) return E_POINTER;
    return kSetNotFound;
}

STDMETHODIMP MediaSource::KsMethod(PKSMETHOD method, ULONG, LPVOID, ULONG, ULONG* bytesReturned) {
    if (!method || !bytesReturned) return E_POINTER;
    return kSetNotFound;
}

STDMETHODIMP MediaSource::KsEvent(PKSEVENT, ULONG, LPVOID, ULONG, ULONG* bytesReturned) {
    if (!bytesReturned) return E_POINTER;
    return kSetNotFound;
}

}  // namespace tumon
