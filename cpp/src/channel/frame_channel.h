// The frame channel: how a program that reads a real camera (the feeder) hands pictures to the Tumon camera
// source, which lives inside a Windows service (the Frame Server) in another session.
//
// One named shared-memory block holds a header and three picture slots. The feeder writes the newest
// picture into a free slot and publishes it; the source copies the newest complete picture when TeamTalk (or
// any app) asks for a frame. There is no locking: each slot carries a sequence number that is 0 while it is
// being written, so a reader can tell if it copied a torn picture and simply skips it.
//
// Pictures are YUY2, at most 1920x1080. The block is created by the source (the service has the right to
// create objects in the Global namespace); the feeder opens it. The access list allows only the system,
// administrators, the service account and the interactively logged-on users.
#pragma once

#include <windows.h>

#include <cstdint>
#include <vector>

namespace tumon {

constexpr uint32_t kChannelMagic = 0x434E4D54;  // "TMNC"
constexpr uint32_t kChannelVersion = 1;
constexpr int kChannelSlots = 3;
constexpr uint32_t kChannelMaxWidth = 1920;
constexpr uint32_t kChannelMaxHeight = 1080;
constexpr uint32_t kChannelSlotBytes = kChannelMaxWidth * kChannelMaxHeight * 2;
constexpr uint32_t kFormatYuy2 = 1;
constexpr ULONGLONG kProducerTimeoutMs = 1000;  // no new picture for this long = no producer

struct alignas(8) ChannelHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t slotCount;
    uint32_t slotBytes;
    volatile LONG consumers;      // streams currently running in the source
    volatile LONG latestSlot;     // newest complete slot, -1 before the first picture
    volatile LONG64 latestFrame;  // number of the newest picture (0 before the first)
    volatile LONG64 lastWriteTick;  // GetTickCount64() when the feeder last published
    uint8_t reserved[24];
};

struct alignas(8) SlotHeader {
    volatile LONG64 sequence;  // 0 while being written, otherwise the picture number
    uint32_t width;
    uint32_t height;
    uint32_t stride;  // bytes per row
    uint32_t format;  // kFormatYuy2
    uint8_t reserved[40];
};

static_assert(sizeof(ChannelHeader) == 64, "ChannelHeader layout");
static_assert(sizeof(SlotHeader) == 64, "SlotHeader layout");

constexpr size_t kChannelBytes = sizeof(ChannelHeader) + kChannelSlots * (sizeof(SlotHeader) + kChannelSlotBytes);

struct FrameInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    uint32_t format = 0;
};

// The names the channel uses. If the environment variable TUMON_CHANNEL_NAME is set, only that name is used
// (tests and development; the Frame Server never has it).
const wchar_t* ChannelNameGlobal();
const wchar_t* ChannelNameLocal();

// The source's side: creates the block, counts running streams, reads the newest picture.
class FrameChannelServer {
public:
    FrameChannelServer() = default;
    ~FrameChannelServer();
    FrameChannelServer(const FrameChannelServer&) = delete;
    FrameChannelServer& operator=(const FrameChannelServer&) = delete;

    bool Create();  // Global name first (service), then Local (a normal program, for tests)
    bool IsOpen() const { return header_ != nullptr; }
    const wchar_t* Name() const { return name_; }

    void AddConsumer();
    void RemoveConsumer();

    // Copies the newest complete picture if its number is greater than *lastFrame.
    bool ReadLatest(uint64_t* lastFrame, std::vector<uint8_t>* data, FrameInfo* info);
    bool ProducerAlive() const;

private:
    HANDLE mapping_ = nullptr;
    uint8_t* view_ = nullptr;
    ChannelHeader* header_ = nullptr;
    const wchar_t* name_ = L"";
    LONG myConsumers_ = 0;
};

// The feeder's side: opens the block and publishes pictures.
class FrameChannelClient {
public:
    FrameChannelClient() = default;
    ~FrameChannelClient();
    FrameChannelClient(const FrameChannelClient&) = delete;
    FrameChannelClient& operator=(const FrameChannelClient&) = delete;

    bool Open();
    void Close();
    bool IsOpen() const { return header_ != nullptr; }
    const wchar_t* Name() const { return name_; }

    LONG Consumers() const { return header_ ? header_->consumers : 0; }
    uint64_t FrameCount() const { return header_ ? static_cast<uint64_t>(header_->latestFrame) : 0; }

    // Publishes a YUY2 picture (rows are `stride` bytes apart in `yuy2`). False if it does not fit.
    bool Write(const uint8_t* yuy2, int width, int height, int stride);

private:
    HANDLE mapping_ = nullptr;
    uint8_t* view_ = nullptr;
    ChannelHeader* header_ = nullptr;
    const wchar_t* name_ = L"";
};

// Pointer to slot `index` in a mapped block.
inline SlotHeader* SlotAt(uint8_t* base, int index) {
    return reinterpret_cast<SlotHeader*>(base + sizeof(ChannelHeader) + static_cast<size_t>(index) * (sizeof(SlotHeader) + kChannelSlotBytes));
}
inline uint8_t* SlotData(SlotHeader* slot) { return reinterpret_cast<uint8_t*>(slot) + sizeof(SlotHeader); }

}  // namespace tumon
