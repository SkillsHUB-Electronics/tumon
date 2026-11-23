// Checks for the shared-memory frame channel: the source's side (server) and the feeder's side (client) in one
// process, on a private name so it never touches a real Tumon Camera.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <vector>

#include "../src/channel/frame_channel.h"

using tumon::FrameChannelClient;
using tumon::FrameChannelServer;
using tumon::FrameInfo;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

std::vector<uint8_t> Picture(int w, int h, uint8_t luma) {
    std::vector<uint8_t> f(static_cast<size_t>(w) * h * 2);
    for (size_t i = 0; i < f.size(); i += 2) {
        f[i] = luma;
        f[i + 1] = 128;
    }
    return f;
}

}  // namespace

int main() {
    SetEnvironmentVariableW(L"TUMON_CHANNEL_NAME", L"Local\\TumonCameraFrames-UnitTest");

    FrameChannelClient early;
    Check(!early.Open(), "client cannot open before the source created the block");

    FrameChannelServer server;
    Check(server.Create(), "server creates the block");
    Check(server.IsOpen(), "server is open");

    FrameChannelClient client;
    Check(client.Open(), "client opens the block");

    // Nothing published yet.
    uint64_t last = 0;
    std::vector<uint8_t> data;
    FrameInfo info;
    Check(!server.ReadLatest(&last, &data, &info), "no picture before the first write");
    Check(!server.ProducerAlive(), "no producer before the first write");

    // One picture.
    const auto a = Picture(64, 48, 100);
    Check(client.Write(a.data(), 64, 48, 128), "client writes a picture");
    Check(server.ReadLatest(&last, &data, &info), "server reads it");
    Check(info.width == 64 && info.height == 48 && info.stride == 128 && info.format == tumon::kFormatYuy2, "picture info is right");
    Check(data.size() == 128u * 48u && data[0] == 100 && data[data.size() - 2] == 100, "picture bytes are right");
    Check(last == 1, "picture number is 1");
    Check(!server.ReadLatest(&last, &data, &info), "the same picture is not delivered twice");
    Check(server.ProducerAlive(), "producer is alive right after a write");

    // A newer picture replaces it, and padded source rows are repacked.
    std::vector<uint8_t> padded(static_cast<size_t>(160) * 48, 55);  // stride 160 > 128
    Check(client.Write(padded.data(), 64, 48, 160), "client writes a picture with padded rows");
    Check(server.ReadLatest(&last, &data, &info), "server reads the second picture");
    Check(last == 2 && info.stride == 128 && data[0] == 55, "second picture is repacked to a tight stride");

    // Many pictures: the reader always gets the newest one.
    for (int i = 0; i < 10; ++i) client.Write(Picture(64, 48, static_cast<uint8_t>(10 + i)).data(), 64, 48, 128);
    Check(server.ReadLatest(&last, &data, &info) && last == 12 && data[0] == 19, "reader gets the newest of many pictures");

    // The biggest allowed picture.
    const auto big = Picture(1920, 1080, 77);
    Check(client.Write(big.data(), 1920, 1080, 3840), "1920x1080 fits");
    Check(server.ReadLatest(&last, &data, &info) && info.width == 1920 && data.size() == 3840u * 1080u, "1920x1080 is read back");

    // Things that must be refused.
    Check(!client.Write(nullptr, 64, 48, 128), "null picture refused");
    Check(!client.Write(a.data(), 63, 48, 128), "odd width refused");
    Check(!client.Write(a.data(), 64, 48, 100), "stride smaller than a row refused");
    Check(!client.Write(big.data(), 1922, 1080, 3844), "too wide refused");
    Check(!client.Write(big.data(), 1920, 1082, 3840), "too tall refused");

    // Consumers are counted for the feeder.
    Check(client.Consumers() == 0, "no consumers at first");
    server.AddConsumer();
    server.AddConsumer();
    Check(client.Consumers() == 2, "two consumers visible to the client");
    server.RemoveConsumer();
    Check(client.Consumers() == 1, "one consumer left");

    // The producer is declared gone after a second without pictures.
    Sleep(1200);
    Check(!server.ProducerAlive(), "producer is considered gone after a second of silence");

    // A server going away takes its own consumers with it.
    {
        FrameChannelServer second;
        Check(second.Create(), "a second server shares the existing block");
        second.AddConsumer();
        Check(client.Consumers() == 2, "second server's consumer is counted");
    }
    Check(client.Consumers() == 1, "a destroyed server removes its consumers");

    if (g_failures == 0) std::printf("All frame channel checks passed.\n");
    return g_failures == 0 ? 0 : 1;
}
