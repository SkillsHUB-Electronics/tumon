// Unit checks for the test-picture generator (no Windows APIs needed).
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../src/source/frame_generator.h"

using tumon::DefaultStride;
using tumon::FrameBytes;
using tumon::FrameGenerator;
using tumon::PixelFormat;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

constexpr uint8_t kGuard = 0xA5;
constexpr size_t kGuardBytes = 64;

const PixelFormat kAll[] = {PixelFormat::YUY2, PixelFormat::NV12, PixelFormat::RGB32, PixelFormat::I420};

void TestSizes() {
    Check(FrameBytes(PixelFormat::YUY2, 1280, 480) == 614400, "YUY2 640x480 size");
    Check(FrameBytes(PixelFormat::RGB32, 2560, 480) == 1228800, "RGB32 640x480 size");
    Check(FrameBytes(PixelFormat::NV12, 640, 480) == 460800, "NV12 640x480 size");
    Check(FrameBytes(PixelFormat::I420, 640, 480) == 460800, "I420 640x480 size");
    Check(DefaultStride(PixelFormat::YUY2, 640) == 1280, "YUY2 stride");
    Check(DefaultStride(PixelFormat::NV12, 640) == 640, "NV12 stride");
}

// Writing must never touch memory outside FrameBytes(), for tight and for padded strides.
void TestNoOverrun() {
    const int sizes[][2] = {{640, 480}, {320, 240}, {1280, 720}, {64, 48}};
    for (PixelFormat f : kAll) {
        for (const auto& s : sizes) {
            for (int pad : {0, 64}) {
                const int stride = DefaultStride(f, s[0]) + pad;
                const size_t bytes = FrameBytes(f, stride, s[1]);
                std::vector<uint8_t> buf(bytes + kGuardBytes, kGuard);
                FrameGenerator gen;
                Check(gen.Write(f, buf.data(), stride, s[0], s[1], 7), "Write returns true");
                bool intact = true;
                for (size_t i = 0; i < kGuardBytes; ++i) intact = intact && buf[bytes + i] == kGuard;
                Check(intact, "no write past the end of the frame");
            }
        }
    }
}

void TestPicture() {
    const int w = 640, h = 480;
    // Frame 1 puts the moving line at x = 6..13, so x = 20, y = 10 is inside the white bar.
    {
        FrameGenerator gen;
        std::vector<uint8_t> buf(FrameBytes(PixelFormat::YUY2, w * 2, h));
        gen.Write(PixelFormat::YUY2, buf.data(), w * 2, w, h, 1);
        Check(buf[10 * w * 2 + 20 * 2] == 235, "YUY2 white bar has luma 235");
        Check(buf[10 * w * 2 + 20 * 2 + 1] == 128, "YUY2 white bar has neutral chroma");
    }
    {
        FrameGenerator gen;
        std::vector<uint8_t> buf(FrameBytes(PixelFormat::NV12, w, h));
        gen.Write(PixelFormat::NV12, buf.data(), w, w, h, 1);
        Check(buf[10 * w + 20] == 235, "NV12 white bar has luma 235");
    }
    {
        FrameGenerator gen;
        std::vector<uint8_t> buf(FrameBytes(PixelFormat::RGB32, w * 4, h));
        gen.Write(PixelFormat::RGB32, buf.data(), w * 4, w, h, 1);
        const uint8_t* p = &buf[10 * w * 4 + 20 * 4];
        Check(p[0] == 255 && p[1] == 255 && p[2] == 255, "RGB32 white bar is white");
    }
    // Consecutive frames differ (the picture moves).
    for (PixelFormat f : kAll) {
        FrameGenerator gen;
        const int stride = DefaultStride(f, w);
        std::vector<uint8_t> a(FrameBytes(f, stride, h)), b(a.size());
        gen.Write(f, a.data(), stride, w, h, 0);
        gen.Write(f, b.data(), stride, w, h, 1);
        Check(a != b, "consecutive frames differ");
    }
}

void TestBadArguments() {
    FrameGenerator gen;
    std::vector<uint8_t> buf(1 << 20);
    Check(!gen.Write(PixelFormat::YUY2, nullptr, 1280, 640, 480, 0), "null buffer rejected");
    Check(!gen.Write(PixelFormat::YUY2, buf.data(), 1280, 641, 480, 0), "odd width rejected");
    Check(!gen.Write(PixelFormat::YUY2, buf.data(), 1280, 640, 479, 0), "odd height rejected");
    Check(!gen.Write(PixelFormat::YUY2, buf.data(), 100, 640, 480, 0), "stride smaller than a row rejected");
    Check(!gen.Write(PixelFormat::YUY2, buf.data(), 1280, 8, 8, 0), "tiny frame rejected");
}

}  // namespace

int main() {
    TestSizes();
    TestNoOverrun();
    TestPicture();
    TestBadArguments();
    if (g_failures == 0) std::printf("All frame generator checks passed.\n");
    return g_failures == 0 ? 0 : 1;
}
