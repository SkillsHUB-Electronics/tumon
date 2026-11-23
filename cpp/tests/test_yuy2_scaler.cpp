// Unit checks for the YUY2 fit-and-scale routine (no Windows APIs needed).
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../src/source/yuy2_scaler.h"

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

// A solid YUY2 picture.
std::vector<uint8_t> Solid(int w, int h, uint8_t y, uint8_t u, uint8_t v) {
    std::vector<uint8_t> f(static_cast<size_t>(w) * h * 2);
    for (size_t i = 0; i < f.size(); i += 4) {
        f[i] = y;
        f[i + 1] = u;
        f[i + 2] = y;
        f[i + 3] = v;
    }
    return f;
}

const uint8_t* Px(const std::vector<uint8_t>& bgra, int w, int x, int y) { return &bgra[(static_cast<size_t>(y) * w + x) * 4]; }

void TestIdentitySize() {
    // White (Y=235) at the same size fills the canvas.
    const auto src = Solid(64, 32, 235, 128, 128);
    std::vector<uint8_t> dst(64 * 32 * 4, 7);
    Check(tumon::FitYuy2ToBgra(src.data(), 64, 32, 128, dst.data(), 64, 32), "identity call succeeds");
    const uint8_t* p = Px(dst, 64, 10, 10);
    Check(p[0] > 250 && p[1] > 250 && p[2] > 250 && p[3] == 255, "white stays white");
    p = Px(dst, 64, 0, 0);
    Check(p[0] > 250, "corner is picture, not border");
}

void TestLetterbox() {
    // A 2:1 picture shown in a square canvas: black bars above and below, picture in the middle.
    const auto src = Solid(64, 32, 235, 128, 128);
    std::vector<uint8_t> dst(64 * 64 * 4, 7);
    Check(tumon::FitYuy2ToBgra(src.data(), 64, 32, 128, dst.data(), 64, 64), "letterbox call succeeds");
    const uint8_t* top = Px(dst, 64, 32, 2);
    const uint8_t* mid = Px(dst, 64, 32, 32);
    const uint8_t* bottom = Px(dst, 64, 32, 62);
    Check(top[0] == 0 && top[1] == 0 && top[2] == 0 && top[3] == 255, "top bar is black");
    Check(bottom[0] == 0 && bottom[1] == 0 && bottom[2] == 0, "bottom bar is black");
    Check(mid[0] > 250 && mid[1] > 250 && mid[2] > 250, "middle shows the picture");
}

void TestPillarbox() {
    // A tall picture in a wide canvas: black bars left and right.
    const auto src = Solid(32, 64, 235, 128, 128);
    std::vector<uint8_t> dst(64 * 32 * 4, 7);
    Check(tumon::FitYuy2ToBgra(src.data(), 32, 64, 64, dst.data(), 64, 32), "pillarbox call succeeds");
    Check(Px(dst, 64, 1, 16)[0] == 0, "left bar is black");
    Check(Px(dst, 64, 62, 16)[2] == 0, "right bar is black");
    Check(Px(dst, 64, 32, 16)[1] > 250, "middle shows the picture");
}

void TestColours() {
    // Y=81, U=90, V=240 is (about) pure red in BT.601; Y=41,U=240,V=110 is (about) pure blue.
    const auto red = Solid(16, 16, 81, 90, 240);
    std::vector<uint8_t> dst(16 * 16 * 4);
    tumon::FitYuy2ToBgra(red.data(), 16, 16, 32, dst.data(), 16, 16);
    const uint8_t* p = Px(dst, 16, 8, 8);
    Check(p[2] > 230 && p[1] < 25 && p[0] < 25, "red decodes as red");
    const auto blue = Solid(16, 16, 41, 240, 110);
    tumon::FitYuy2ToBgra(blue.data(), 16, 16, 32, dst.data(), 16, 16);
    p = Px(dst, 16, 8, 8);
    Check(p[0] > 230 && p[1] < 25 && p[2] < 25, "blue decodes as blue");
}

void TestPaddedStrideAndBadArguments() {
    // Source rows 16 bytes longer than needed (padding must be skipped).
    const int w = 32, h = 16, stride = w * 2 + 16;
    std::vector<uint8_t> src(static_cast<size_t>(stride) * h, 99);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w * 2; x += 4) {
            uint8_t* p = &src[static_cast<size_t>(y) * stride + x];
            p[0] = 235; p[1] = 128; p[2] = 235; p[3] = 128;
        }
    }
    std::vector<uint8_t> dst(32 * 16 * 4);
    Check(tumon::FitYuy2ToBgra(src.data(), w, h, stride, dst.data(), 32, 16), "padded stride accepted");
    Check(Px(dst, 32, 31, 8)[0] > 250, "padding bytes are not drawn");

    Check(!tumon::FitYuy2ToBgra(nullptr, w, h, stride, dst.data(), 32, 16), "null source rejected");
    Check(!tumon::FitYuy2ToBgra(src.data(), 31, h, stride, dst.data(), 32, 16), "odd source width rejected");
    Check(!tumon::FitYuy2ToBgra(src.data(), w, h, 10, dst.data(), 32, 16), "stride smaller than a row rejected");
    Check(!tumon::FitYuy2ToBgra(src.data(), w, h, stride, dst.data(), 1, 16), "tiny canvas rejected");
}

}  // namespace

int main() {
    TestIdentitySize();
    TestLetterbox();
    TestPillarbox();
    TestColours();
    TestPaddedStrideAndBadArguments();
    if (g_failures == 0) std::printf("All scaler checks passed.\n");
    return g_failures == 0 ? 0 : 1;
}
