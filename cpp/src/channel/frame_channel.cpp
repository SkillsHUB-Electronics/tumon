#include "frame_channel.h"

#include <sddl.h>

#include <cstring>
#include <string>

#pragma comment(lib, "advapi32")

namespace tumon {

namespace {

// System, administrators, the service account (Local Service), and interactively logged-on users may read and
// write; nobody else.
constexpr wchar_t kSddl[] = L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;LS)(A;;GRGW;;;IU)";

std::wstring g_override;
std::wstring g_global;
std::wstring g_local;

bool LoadOverride() {
    wchar_t buffer[200];
    const DWORD n = GetEnvironmentVariableW(L"TUMON_CHANNEL_NAME", buffer, 200);
    if (n == 0 || n >= 200) return false;
    g_override = buffer;
    return true;
}

}  // namespace

const wchar_t* ChannelNameGlobal() {
    if (g_global.empty()) g_global = LoadOverride() ? g_override : L"Global\\TumonCameraFrames-v1";
    return g_global.c_str();
}

const wchar_t* ChannelNameLocal() {
    if (g_local.empty()) g_local = LoadOverride() ? g_override : L"Local\\TumonCameraFrames-v1";
    return g_local.c_str();
}

// ---------------------------------------------------------------- server

FrameChannelServer::~FrameChannelServer() {
    if (header_ && myConsumers_ > 0) InterlockedAdd(&header_->consumers, -myConsumers_);
    if (view_) UnmapViewOfFile(view_);
    if (mapping_) CloseHandle(mapping_);
}

bool FrameChannelServer::Create() {
    if (header_) return true;

    const wchar_t* names[2] = {ChannelNameGlobal(), ChannelNameLocal()};
    for (const wchar_t* name : names) {
        SECURITY_ATTRIBUTES attributes{};
        attributes.nLength = sizeof(attributes);
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        const bool isGlobal = wcsncmp(name, L"Global\\", 7) == 0;
        if (isGlobal) {
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(kSddl, SDDL_REVISION_1, &descriptor, nullptr)) continue;
            attributes.lpSecurityDescriptor = descriptor;
        }

        HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, isGlobal ? &attributes : nullptr, PAGE_READWRITE,
                                            static_cast<DWORD>(static_cast<ULONGLONG>(kChannelBytes) >> 32),
                                            static_cast<DWORD>(kChannelBytes & 0xFFFFFFFF), name);
        const DWORD error = GetLastError();
        if (descriptor) LocalFree(descriptor);
        if (!mapping) continue;

        auto* view = static_cast<uint8_t*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, kChannelBytes));
        if (!view) {
            CloseHandle(mapping);
            continue;
        }

        auto* header = reinterpret_cast<ChannelHeader*>(view);
        if (error != ERROR_ALREADY_EXISTS || header->magic != kChannelMagic) {
            // Fresh block (zero-filled by the system): initialise it. Another process that created it first keeps ownership.
            header->version = kChannelVersion;
            header->slotCount = kChannelSlots;
            header->slotBytes = kChannelSlotBytes;
            header->latestSlot = -1;
            MemoryBarrier();
            header->magic = kChannelMagic;
        }
        mapping_ = mapping;
        view_ = view;
        header_ = header;
        name_ = name;
        return true;
    }
    return false;
}

void FrameChannelServer::AddConsumer() {
    if (!header_) return;
    InterlockedIncrement(&header_->consumers);
    ++myConsumers_;
}

void FrameChannelServer::RemoveConsumer() {
    if (!header_ || myConsumers_ <= 0) return;
    InterlockedDecrement(&header_->consumers);
    --myConsumers_;
}

bool FrameChannelServer::ProducerAlive() const {
    if (!header_ || header_->magic != kChannelMagic) return false;
    const LONG64 tick = InterlockedCompareExchange64(const_cast<volatile LONG64*>(&header_->lastWriteTick), 0, 0);
    if (tick == 0) return false;
    return GetTickCount64() - static_cast<ULONGLONG>(tick) < kProducerTimeoutMs;
}

bool FrameChannelServer::ReadLatest(uint64_t* lastFrame, std::vector<uint8_t>* data, FrameInfo* info) {
    if (!header_ || header_->magic != kChannelMagic || header_->version != kChannelVersion) return false;
    const LONG index = header_->latestSlot;
    if (index < 0 || index >= kChannelSlots) return false;

    SlotHeader* slot = SlotAt(view_, index);
    const LONG64 before = InterlockedCompareExchange64(&slot->sequence, 0, 0);
    if (before == 0 || static_cast<uint64_t>(before) <= *lastFrame) return false;

    FrameInfo copy;
    copy.width = slot->width;
    copy.height = slot->height;
    copy.stride = slot->stride;
    copy.format = slot->format;
    if (copy.format != kFormatYuy2 || copy.width < 2 || copy.height < 2 || copy.width > kChannelMaxWidth ||
        copy.height > kChannelMaxHeight || copy.stride < copy.width * 2 ||
        static_cast<uint64_t>(copy.stride) * copy.height > kChannelSlotBytes) {
        return false;  // never trust the writer: a bad header is skipped, not copied
    }

    data->resize(static_cast<size_t>(copy.stride) * copy.height);
    memcpy(data->data(), SlotData(slot), data->size());
    MemoryBarrier();
    const LONG64 after = InterlockedCompareExchange64(&slot->sequence, 0, 0);
    if (after != before) return false;  // overwritten while copying

    *lastFrame = static_cast<uint64_t>(before);
    *info = copy;
    return true;
}

// ---------------------------------------------------------------- client

FrameChannelClient::~FrameChannelClient() { Close(); }

void FrameChannelClient::Close() {
    if (view_) UnmapViewOfFile(view_);
    if (mapping_) CloseHandle(mapping_);
    view_ = nullptr;
    mapping_ = nullptr;
    header_ = nullptr;
}

bool FrameChannelClient::Open() {
    if (header_) return true;
    const wchar_t* names[2] = {ChannelNameGlobal(), ChannelNameLocal()};
    for (const wchar_t* name : names) {
        HANDLE mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name);
        if (!mapping) continue;
        auto* view = static_cast<uint8_t*>(MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, kChannelBytes));
        if (!view) {
            CloseHandle(mapping);
            continue;
        }
        auto* header = reinterpret_cast<ChannelHeader*>(view);
        if (header->magic != kChannelMagic || header->version != kChannelVersion || header->slotCount != kChannelSlots ||
            header->slotBytes != kChannelSlotBytes) {
            UnmapViewOfFile(view);
            CloseHandle(mapping);
            continue;
        }
        mapping_ = mapping;
        view_ = view;
        header_ = header;
        name_ = name;
        return true;
    }
    return false;
}

bool FrameChannelClient::Write(const uint8_t* yuy2, int width, int height, int stride) {
    if (!header_ || !yuy2 || width < 2 || height < 2 || (width & 1) || stride < width * 2) return false;
    if (static_cast<uint32_t>(width) > kChannelMaxWidth || static_cast<uint32_t>(height) > kChannelMaxHeight) return false;

    const int next = (header_->latestSlot + 1) % kChannelSlots;
    SlotHeader* slot = SlotAt(view_, next);

    InterlockedExchange64(&slot->sequence, 0);  // mark "being written"
    slot->width = static_cast<uint32_t>(width);
    slot->height = static_cast<uint32_t>(height);
    slot->stride = static_cast<uint32_t>(width * 2);
    slot->format = kFormatYuy2;
    uint8_t* out = SlotData(slot);
    for (int y = 0; y < height; ++y) {
        memcpy(out + static_cast<size_t>(y) * width * 2, yuy2 + static_cast<size_t>(y) * stride, static_cast<size_t>(width) * 2);
    }
    MemoryBarrier();

    const LONG64 frame = header_->latestFrame + 1;
    InterlockedExchange64(&slot->sequence, frame);  // publish the slot
    InterlockedExchange(&header_->latestSlot, next);
    InterlockedExchange64(&header_->latestFrame, frame);
    InterlockedExchange64(&header_->lastWriteTick, static_cast<LONG64>(GetTickCount64()));
    return true;
}

}  // namespace tumon
