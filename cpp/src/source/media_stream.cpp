#include <windows.h>
#include <initguid.h>  // define the GUIDs declared by the Media Foundation and kernel-streaming headers

#include "media_stream.h"

#include <ksmedia.h>
#include <mferror.h>

#include <algorithm>
#include <vector>

#include "formats.h"
#include "idle_loader.h"
#include "trace.h"
#include "yuy2_scaler.h"

using Microsoft::WRL::ComPtr;

namespace tumon {

namespace {

struct Mode {
    UINT32 width;
    UINT32 height;
};

// What the test camera offers. The first entry is the default type. The sizes are the standard ones
// TeamTalk users pick; the pixel formats are the ones we want to see TeamTalk accept or ignore.
constexpr Mode kModes[] = {{640, 480}, {320, 240}, {1280, 720}};
constexpr PixelFormat kFormats[] = {PixelFormat::YUY2, PixelFormat::NV12, PixelFormat::RGB32, PixelFormat::I420};
constexpr UINT32 kFps = 30;

HRESULT MakeType(PixelFormat format, UINT32 width, UINT32 height, IMFMediaType** out) {
    ComPtr<IMFMediaType> type;
    HRESULT hr = MFCreateMediaType(&type);
    if (FAILED(hr)) return hr;

    const int stride = DefaultStride(format, static_cast<int>(width));
    const UINT32 bytes = static_cast<UINT32>(FrameBytes(format, stride, static_cast<int>(height)));

    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    type->SetGUID(MF_MT_SUBTYPE, SubtypeFor(format));
    MFSetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, width, height);
    type->SetUINT32(MF_MT_DEFAULT_STRIDE, static_cast<UINT32>(stride));
    type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    type->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
    type->SetUINT32(MF_MT_SAMPLE_SIZE, bytes);
    MFSetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, kFps, 1);
    MFSetAttributeRatio(type.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    type->SetUINT32(MF_MT_AVG_BITRATE, bytes * 8 * kFps);

    *out = type.Detach();
    return S_OK;
}

constexpr HRESULT kSetNotFound = static_cast<HRESULT>(0x80070492);  // HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND)

}  // namespace

MediaStream::MediaStream() {
    ++g_objectCount;
    InitAttributes();
}

MediaStream::~MediaStream() {
    if (timer_) CloseHandle(timer_);
    --g_objectCount;
}

void MediaStream::SleepPrecisely(MFTIME hundredNs) {
    if (hundredNs <= 0) return;
    if (!timer_) {
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
        timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    }
    if (timer_) {
        LARGE_INTEGER due;
        due.QuadPart = -hundredNs;  // relative time
        if (SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(timer_, INFINITE);
            return;
        }
    }
    Sleep(static_cast<DWORD>(hundredNs / 10000));
}

HRESULT MediaStream::Initialize(IMFMediaSource* source, DWORD index) {
    if (!source) return E_POINTER;
    source_ = source;

    // The artwork lives in this DLL; find the module from the address of one of its functions.
    HMODULE module = nullptr;
    std::vector<uint8_t> art;
    int artWidth = 0, artHeight = 0;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&LoadIdleArt), &module) &&
        LoadIdleArt(module, &art, &artWidth, &artHeight)) {
        idle_.SetArt(std::move(art), artWidth, artHeight);
    }
    if (!idle_.Ready()) Trace(L"MediaStream: idle artwork not loaded, using colour bars");

    HRESULT hr = SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, PINNAME_VIDEO_CAPTURE);
    if (FAILED(hr)) return hr;
    hr = SetUINT32(MF_DEVICESTREAM_STREAM_ID, index);
    if (FAILED(hr)) return hr;
    hr = SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, 1);
    if (FAILED(hr)) return hr;
    hr = SetUINT32(MF_DEVICESTREAM_ATTRIBUTE_FRAMESOURCE_TYPES, MFFrameSourceTypes_Color);
    if (FAILED(hr)) return hr;
    hr = MFCreateEventQueue(&queue_);
    if (FAILED(hr)) return hr;

    std::vector<ComPtr<IMFMediaType>> owned;
    std::vector<IMFMediaType*> raw;
    for (const Mode& mode : kModes) {
        for (PixelFormat format : kFormats) {
            ComPtr<IMFMediaType> type;
            hr = MakeType(format, mode.width, mode.height, &type);
            if (FAILED(hr)) return hr;
            raw.push_back(type.Get());
            owned.push_back(std::move(type));
        }
    }

    hr = MFCreateStreamDescriptor(index, static_cast<DWORD>(raw.size()), raw.data(), &descriptor_);
    if (FAILED(hr)) return hr;

    ComPtr<IMFMediaTypeHandler> handler;
    hr = descriptor_->GetMediaTypeHandler(&handler);
    if (FAILED(hr)) return hr;
    return handler->SetCurrentMediaType(raw[0]);
}

HRESULT MediaStream::SetAllocator(IUnknown* allocator) {
    if (!allocator) return E_POINTER;
    std::lock_guard<std::mutex> guard(lock_);
    allocator_.Reset();
    allocatorReady_ = false;
    return allocator->QueryInterface(IID_PPV_ARGS(&allocator_));
}

void MediaStream::SetChannel(std::shared_ptr<FrameChannelServer> channel) {
    std::lock_guard<std::mutex> guard(lock_);
    channel_ = std::move(channel);
}

// Tells the feeder, through the shared header, how many streams are running, so it only opens the real camera
// while somebody is actually using Tumon Camera. Call with the lock held.
void MediaStream::CountConsumer(bool running) {
    if (!channel_) return;
    if (running && !consumerCounted_) {
        channel_->AddConsumer();
        consumerCounted_ = true;
    } else if (!running && consumerCounted_) {
        channel_->RemoveConsumer();
        consumerCounted_ = false;
    }
}

bool MediaStream::Render(uint8_t* dst, int stride, int width, int height) {
    bool live = false;
    if (channel_ && channel_->IsOpen()) {
        const bool gotNew = channel_->ReadLatest(&lastLiveFrame_, &live_, &liveInfo_);
        if (gotNew) haveLive_ = true;
        live = haveLive_ && channel_->ProducerAlive();
        if (!live) haveLive_ = false;  // the feeder stopped: forget its last picture
        if (live && !gotNew) ++reusedLive_;
    }

    if (live) {
        uint8_t* canvas = generator_.Canvas(width, height);
        if (FitYuy2ToBgra(live_.data(), static_cast<int>(liveInfo_.width), static_cast<int>(liveInfo_.height),
                          static_cast<int>(liveInfo_.stride), canvas, width, height)) {
            if (!lastWasLive_) Trace(L"MediaStream: showing live pictures (%ux%u)", liveInfo_.width, liveInfo_.height);
            lastWasLive_ = true;
            ++liveFrames_;
            return generator_.WriteCanvas(format_, dst, stride, width, height);
        }
    }
    if (lastWasLive_) Trace(L"MediaStream: feeder stopped, back to the test pattern");
    lastWasLive_ = false;
    ++patternFrames_;
    if (idle_.Ready() && idle_.Draw(generator_.Canvas(width, height), width, height, frame_)) {
        return generator_.WriteCanvas(format_, dst, stride, width, height);
    }
    return generator_.Write(format_, dst, stride, width, height, frame_);
}

HRESULT MediaStream::Start(IMFMediaType* type) {
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_ || !descriptor_) return MF_E_SHUTDOWN;

    ComPtr<IMFMediaType> current;
    if (!type) {
        ComPtr<IMFMediaTypeHandler> handler;
        HRESULT hr = descriptor_->GetMediaTypeHandler(&handler);
        if (SUCCEEDED(hr)) hr = handler->GetCurrentMediaType(&current);
        if (FAILED(hr)) return hr;
        type = current.Get();
    }

    GUID subtype = GUID_NULL;
    HRESULT hr = type->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr)) return hr;
    if (!PixelFormatFromSubtype(subtype, &format_)) return MF_E_INVALIDMEDIATYPE;

    UINT32 w = 0, h = 0;
    hr = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &w, &h);
    if (FAILED(hr) || w == 0 || h == 0) return MF_E_INVALIDMEDIATYPE;
    width_ = w;
    height_ = h;

    UINT32 num = kFps, den = 1;
    if (SUCCEEDED(MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &num, &den)) && den != 0 && num != 0) {
        fps_ = std::max<UINT32>(1, num / den);
    } else {
        fps_ = kFps;
    }

    if (allocator_) {
        if (allocatorReady_) allocator_->UninitializeSampleAllocator();
        allocatorReady_ = false;
        hr = allocator_->InitializeSampleAllocator(10, type);
        if (FAILED(hr)) return hr;
        allocatorReady_ = true;
    }

    frame_ = 0;
    startTime_ = MFGetSystemTime();
    hr = queue_->QueueEventParamVar(MEStreamStarted, GUID_NULL, S_OK, nullptr);
    if (FAILED(hr)) return hr;
    state_ = MF_STREAM_STATE_RUNNING;
    liveFrames_ = 0;
    patternFrames_ = 0;
    lastLiveFrame_ = 0;
    haveLive_ = false;
    CountConsumer(true);
    Trace(L"MediaStream::Start ok: format %d %ux%u @%u fps, allocator %s", static_cast<int>(format_), width_, height_, fps_,
          allocator_ ? L"provided" : L"none");
    return S_OK;
}

HRESULT MediaStream::Stop() {
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_) return MF_E_SHUTDOWN;

    if (allocator_ && allocatorReady_) allocator_->UninitializeSampleAllocator();
    allocatorReady_ = false;
    const HRESULT hr = queue_->QueueEventParamVar(MEStreamStopped, GUID_NULL, S_OK, nullptr);
    state_ = MF_STREAM_STATE_STOPPED;
    CountConsumer(false);
    Trace(L"MediaStream::Stop: %llu live pictures, %llu test-pattern pictures", liveFrames_, patternFrames_);
    if (requests_ > 0) {
        Trace(L"MediaStream::Stop: %llu requests, average wait %.1f ms, average fill %.1f ms, %llu live pictures repeated, %llu waits timed out",
              requests_, waitMsTotal_ / static_cast<double>(requests_), fillMsTotal_ / static_cast<double>(requests_), reusedLive_, waitTimeouts_);
    }
    return hr;
}

void MediaStream::Shutdown() {
    std::lock_guard<std::mutex> guard(lock_);
    CountConsumer(false);
    channel_.reset();
    if (queue_) {
        queue_->Shutdown();
        queue_.Reset();
    }
    descriptor_.Reset();
    source_.Reset();
    allocator_.Reset();
    allocatorReady_ = false;
    ReleaseAttributes();
}

HRESULT MediaStream::FillSample(IMFSample* sample) {
    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = sample->ConvertToContiguousBuffer(&buffer);
    if (FAILED(hr)) return hr;

    const int w = static_cast<int>(width_);
    const int h = static_cast<int>(height_);

    ComPtr<IMF2DBuffer> buffer2d;
    if (SUCCEEDED(buffer.As(&buffer2d))) {
        BYTE* scanline0 = nullptr;
        LONG pitch = 0;
        hr = buffer2d->Lock2D(&scanline0, &pitch);
        if (FAILED(hr)) return hr;

        DWORD maxLength = 0;
        buffer->GetMaxLength(&maxLength);
        const size_t needed = pitch > 0 ? FrameBytes(format_, pitch, h) : 0;
        const bool ok = pitch > 0 && maxLength >= needed &&
                        Render(scanline0, pitch, w, h);
        buffer2d->Unlock2D();
        if (!ok) return E_FAIL;
        buffer->SetCurrentLength(static_cast<DWORD>(needed));
        return S_OK;
    }

    BYTE* data = nullptr;
    DWORD maxLength = 0;
    hr = buffer->Lock(&data, &maxLength, nullptr);
    if (FAILED(hr)) return hr;
    const int stride = DefaultStride(format_, w);
    const size_t needed = FrameBytes(format_, stride, h);
    const bool ok = maxLength >= needed && Render(data, stride, w, h);
    buffer->Unlock();
    if (!ok) return E_FAIL;
    return buffer->SetCurrentLength(static_cast<DWORD>(needed));
}

// IUnknown
STDMETHODIMP MediaStream::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == __uuidof(IMFMediaEventGenerator) || riid == __uuidof(IMFMediaStream) ||
        riid == __uuidof(IMFMediaStream2)) {
        *ppv = static_cast<IMFMediaStream2*>(this);
    } else if (riid == __uuidof(IMFAttributes)) {
        *ppv = static_cast<IMFAttributes*>(this);
    } else if (riid == __uuidof(IKsControl)) {
        *ppv = static_cast<IKsControl*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

// IMFMediaEventGenerator
STDMETHODIMP MediaStream::BeginGetEvent(IMFAsyncCallback* callback, IUnknown* state) {
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_) return MF_E_SHUTDOWN;
    return queue_->BeginGetEvent(callback, state);
}

STDMETHODIMP MediaStream::EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** event) {
    if (!event) return E_POINTER;
    *event = nullptr;
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_) return MF_E_SHUTDOWN;
    return queue_->EndGetEvent(result, event);
}

STDMETHODIMP MediaStream::GetEvent(DWORD flags, IMFMediaEvent** event) {
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

STDMETHODIMP MediaStream::QueueEvent(MediaEventType type, REFGUID extendedType, HRESULT status, const PROPVARIANT* value) {
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_) return MF_E_SHUTDOWN;
    return queue_->QueueEventParamVar(type, extendedType, status, value);
}

// IMFMediaStream
STDMETHODIMP MediaStream::GetMediaSource(IMFMediaSource** source) {
    if (!source) return E_POINTER;
    *source = nullptr;
    std::lock_guard<std::mutex> guard(lock_);
    if (!source_) return MF_E_SHUTDOWN;
    return source_.CopyTo(source);
}

STDMETHODIMP MediaStream::GetStreamDescriptor(IMFStreamDescriptor** descriptor) {
    if (!descriptor) return E_POINTER;
    *descriptor = nullptr;
    std::lock_guard<std::mutex> guard(lock_);
    if (!descriptor_) return MF_E_SHUTDOWN;
    return descriptor_.CopyTo(descriptor);
}

STDMETHODIMP MediaStream::RequestSample(IUnknown* token) {
    std::lock_guard<std::mutex> guard(lock_);
    if (!queue_) return MF_E_SHUTDOWN;
    if (state_ != MF_STREAM_STATE_RUNNING) {
        Trace(L"MediaStream::RequestSample refused: state %d", static_cast<int>(state_));
        return MF_E_INVALIDREQUEST;
    }

    // Pacing. A real camera is driven by its sensor; here the request itself waits (at most 100 ms). Replace with a
    // timer-driven queue before shipping.
    LARGE_INTEGER frequency, t0, t1, t2;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&t0);
    const auto ms = [&](LARGE_INTEGER a, LARGE_INTEGER b) { return (b.QuadPart - a.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart); };

    const MFTIME duration = 10000000 / static_cast<MFTIME>(fps_);
    MFTIME due;
    if (channel_ && channel_->IsOpen() && channel_->ProducerAlive()) {
        // A feeder is publishing: follow the camera's own rhythm. Answer each request with the *next new* picture
        // instead of sampling on our own clock, which was out of step with the camera and repeated about a third of
        // the pictures (20 different pictures in 30). Never faster than a quarter above the stream's frame rate.
        const MFTIME earliest = lastEmit_ + duration * 3 / 4;
        MFTIME now = MFGetSystemTime();
        if (now < earliest) SleepPrecisely(earliest - now);
        const ULONGLONG deadline = GetTickCount64() + 100;
        while (channel_->LatestFrame() <= lastLiveFrame_ && GetTickCount64() < deadline) SleepPrecisely(10000);  // 1 ms
        if (channel_->LatestFrame() <= lastLiveFrame_) ++waitTimeouts_;
        due = MFGetSystemTime();
    } else {
        const MFTIME now = MFGetSystemTime();
        due = startTime_ + static_cast<MFTIME>(frame_) * duration;
        if (now - due > 5 * duration) {
            startTime_ = now - static_cast<MFTIME>(frame_) * duration;  // fell behind: restart the schedule
            due = now;
        } else if (due > now) {
            SleepPrecisely(std::min<MFTIME>(due - now, 1000000));
        }
    }
    lastEmit_ = due;
    QueryPerformanceCounter(&t1);

    ComPtr<IMFSample> sample;
    HRESULT hr;
    if (allocator_ && allocatorReady_) {
        hr = allocator_->AllocateSample(&sample);
    } else {
        hr = MFCreateSample(&sample);
        if (SUCCEEDED(hr)) {
            ComPtr<IMFMediaBuffer> buffer;
            const size_t bytes = FrameBytes(format_, DefaultStride(format_, static_cast<int>(width_)), static_cast<int>(height_));
            hr = MFCreateMemoryBuffer(static_cast<DWORD>(bytes), &buffer);
            if (SUCCEEDED(hr)) hr = sample->AddBuffer(buffer.Get());
        }
    }
    if (FAILED(hr)) return hr;

    hr = FillSample(sample.Get());
    if (FAILED(hr)) return hr;
    QueryPerformanceCounter(&t2);
    ++requests_;
    waitMsTotal_ += ms(t0, t1);
    fillMsTotal_ += ms(t1, t2);
    sample->SetSampleTime(due);
    sample->SetSampleDuration(duration);
    if (token) sample->SetUnknown(MFSampleExtension_Token, token);

    hr = queue_->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, sample.Get());
    if (FAILED(hr)) return hr;
    ++frame_;
    return S_OK;
}

// IMFMediaStream2
STDMETHODIMP MediaStream::SetStreamState(MF_STREAM_STATE value) {
    MF_STREAM_STATE current;
    {
        std::lock_guard<std::mutex> guard(lock_);
        current = state_;
    }
    if (current == value) return S_OK;

    switch (value) {
        case MF_STREAM_STATE_PAUSED:
            if (current != MF_STREAM_STATE_RUNNING) return MF_E_INVALID_STATE_TRANSITION;
            {
                std::lock_guard<std::mutex> guard(lock_);
                state_ = value;
            }
            return S_OK;
        case MF_STREAM_STATE_RUNNING: return Start(nullptr);
        case MF_STREAM_STATE_STOPPED: return Stop();
        default: return MF_E_INVALID_STATE_TRANSITION;
    }
}

STDMETHODIMP MediaStream::GetStreamState(MF_STREAM_STATE* value) {
    if (!value) return E_POINTER;
    std::lock_guard<std::mutex> guard(lock_);
    *value = state_;
    return S_OK;
}

// IKsControl: no camera-control properties are exposed (exposure, focus, and so on).
STDMETHODIMP MediaStream::KsProperty(PKSPROPERTY property, ULONG, LPVOID, ULONG, ULONG* bytesReturned) {
    if (!property || !bytesReturned) return E_POINTER;
    return kSetNotFound;
}

STDMETHODIMP MediaStream::KsMethod(PKSMETHOD method, ULONG, LPVOID, ULONG, ULONG* bytesReturned) {
    if (!method || !bytesReturned) return E_POINTER;
    return kSetNotFound;
}

STDMETHODIMP MediaStream::KsEvent(PKSEVENT, ULONG, LPVOID, ULONG, ULONG* bytesReturned) {
    if (!bytesReturned) return E_POINTER;
    return kSetNotFound;
}

}  // namespace tumon
