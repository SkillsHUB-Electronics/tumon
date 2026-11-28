// Checks for the idle screen: scaling to cover the frame, the turning spinner, and bad arguments.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../src/source/idle_screen.h"

using tumon::IdleScreen;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

constexpr uint8_t kGuard = 0xA5;

// 16:9 artwork: light grey everywhere, a red band on the far left and right edges.
std::vector<uint8_t> Art(int w, int h) {
    std::vector<uint8_t> art(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t* p = &art[(static_cast<size_t>(y) * w + x) * 4];
            const bool edge = x < w / 16 || x >= w - w / 16;
            p[0] = edge ? 0 : 230;
            p[1] = edge ? 0 : 230;
            p[2] = edge ? 255 : 230;
            p[3] = 255;
        }
    }
    return art;
}

const uint8_t* Px(const std::vector<uint8_t>& f, int w, int x, int y) { return &f[(static_cast<size_t>(y) * w + x) * 4]; }

std::vector<uint8_t> Draw(IdleScreen& screen, int w, int h, uint64_t frame) {
    std::vector<uint8_t> out(static_cast<size_t>(w) * h * 4 + 64, kGuard);
    Check(screen.Draw(out.data(), w, h, frame), "Draw succeeds");
    bool intact = true;
    for (size_t i = static_cast<size_t>(w) * h * 4; i < out.size(); ++i) intact = intact && out[i] == kGuard;
    Check(intact, "nothing written past the frame");
    out.resize(static_cast<size_t>(w) * h * 4);
    return out;
}

}  // namespace

int main() {
    IdleScreen screen;
    std::vector<uint8_t> frame(640 * 360 * 4);
    Check(!screen.Draw(frame.data(), 640, 360, 0), "nothing is drawn before the artwork is set");
    Check(!screen.SetArt(std::vector<uint8_t>(10), 640, 360), "artwork of the wrong size is refused");
    Check(screen.SetArt(Art(640, 360), 640, 360), "artwork accepted");
    Check(screen.Ready(), "ready with artwork");

    // Same shape as the artwork: the edge bands are visible.
    {
        const auto f = Draw(screen, 640, 360, 0);
        Check(Px(f, 640, 2, 10)[2] == 255 && Px(f, 640, 2, 10)[1] == 0, "16:9 frame keeps the left edge");
        Check(Px(f, 640, 320, 10)[1] == 230, "middle is the background");
    }
    // A 4:3 frame covers by cropping the sides: no black bars, edge bands cut away.
    {
        const auto f = Draw(screen, 320, 240, 0);
        Check(Px(f, 320, 0, 120)[1] == 230 && Px(f, 320, 319, 120)[1] == 230, "4:3 frame crops the sides, no bars");
        Check(Px(f, 320, 0, 0)[1] == 230 && Px(f, 320, 319, 239)[1] == 230, "corners are picture, not black");
    }
    // The spinner turns: frames differ near it, and repeat after one turn (45 frames).
    {
        const auto a = Draw(screen, 640, 360, 0);
        const auto b = Draw(screen, 640, 360, 11);
        const auto c = Draw(screen, 640, 360, 45);
        Check(a != b, "spinner moves between frames");
        Check(a == c, "spinner repeats after one turn");
        // Only pixels around the spinner (centre 0.5 / 0.7837 of the frame) may differ.
        bool onlyThere = true;
        for (int y = 0; y < 360; ++y) {
            for (int x = 0; x < 640; ++x) {
                const bool near = x >= 295 && x <= 345 && y >= 258 && y <= 308;
                if (!near && Px(a, 640, x, y)[0] != Px(b, 640, x, y)[0]) onlyThere = false;
            }
        }
        Check(onlyThere, "only the spinner area changes");
    }
    Check(!screen.Draw(nullptr, 640, 360, 0), "null buffer refused");
    Check(!screen.Draw(frame.data(), 1, 1, 0), "tiny frame refused");

    if (g_failures == 0) std::printf("All idle screen checks passed.\n");
    return g_failures == 0 ? 0 : 1;
}
