#include "frame_generator.h"

#include <algorithm>

namespace tumon {

namespace {

struct Rgb {
    uint8_t r, g, b;
};

constexpr Rgb kBars[7] = {
    {255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0}, {255, 0, 255}, {255, 0, 0}, {0, 0, 255},
};

// BT.601 limited range.
inline uint8_t ToY(int r, int g, int b) { return static_cast<uint8_t>(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16); }
inline uint8_t ToU(int r, int g, int b) { return static_cast<uint8_t>(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128); }
inline uint8_t ToV(int r, int g, int b) { return static_cast<uint8_t>(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128); }

}  // namespace

int DefaultStride(PixelFormat format, int width) {
    switch (format) {
        case PixelFormat::YUY2: return width * 2;
        case PixelFormat::RGB32: return width * 4;
        case PixelFormat::NV12:
        case PixelFormat::I420: return width;
    }
    return 0;
}

size_t FrameBytes(PixelFormat format, int stride, int height) {
    const size_t s = static_cast<size_t>(stride);
    const size_t h = static_cast<size_t>(height);
    switch (format) {
        case PixelFormat::YUY2:
        case PixelFormat::RGB32: return s * h;
        case PixelFormat::NV12: return s * h + s * (h / 2);
        case PixelFormat::I420: return s * h + 2 * ((s / 2) * (h / 2));
    }
    return 0;
}

void FrameGenerator::Draw(int width, int height, uint64_t frame) {
    bgra_.resize(static_cast<size_t>(width) * height * 4);

    const int barsHeight = height * 2 / 3;
    const int tickerTop = height - std::max(8, height / 12);
    const int lineWidth = std::max(2, width / 80);
    const int lineX = static_cast<int>((frame * 6) % static_cast<uint64_t>(width));

    for (int y = 0; y < height; ++y) {
        uint8_t* row = &bgra_[static_cast<size_t>(y) * width * 4];
        for (int x = 0; x < width; ++x) {
            Rgb c;
            if (y >= tickerTop) {
                // Frame counter: 16 blocks, one per bit of the frame number (white = 1).
                const int bit = 15 - (x * 16 / width);
                const bool on = ((frame >> bit) & 1u) != 0;
                c = on ? Rgb{255, 255, 255} : Rgb{0, 0, 0};
            } else if (y < barsHeight) {
                c = kBars[x * 7 / width];
            } else {
                const uint8_t gray = static_cast<uint8_t>(x * 255 / (width - 1));
                c = {gray, gray, gray};
            }
            // Moving black line (not over the ticker, so the counter stays readable).
            if (y < tickerTop && x >= lineX && x < lineX + lineWidth) c = {0, 0, 0};

            row[x * 4 + 0] = c.b;
            row[x * 4 + 1] = c.g;
            row[x * 4 + 2] = c.r;
            row[x * 4 + 3] = 255;
        }
    }
}

bool FrameGenerator::Write(PixelFormat format, uint8_t* dst, int stride, int width, int height, uint64_t frame) {
    if (!dst || width < 16 || height < 16 || (width & 1) || (height & 1)) return false;
    if (stride < DefaultStride(format, width)) return false;
    Draw(width, height, frame);
    return WriteCanvas(format, dst, stride, width, height);
}

uint8_t* FrameGenerator::Canvas(int width, int height) {
    bgra_.resize(static_cast<size_t>(width) * height * 4);
    return bgra_.data();
}

bool FrameGenerator::WriteCanvas(PixelFormat format, uint8_t* dst, int stride, int width, int height) {
    if (!dst || width < 16 || height < 16 || (width & 1) || (height & 1)) return false;
    if (stride < DefaultStride(format, width)) return false;
    if (bgra_.size() != static_cast<size_t>(width) * height * 4) return false;
    const uint8_t* src = bgra_.data();
    auto px = [&](int x, int y, int& r, int& g, int& b) {
        const uint8_t* p = src + (static_cast<size_t>(y) * width + x) * 4;
        b = p[0];
        g = p[1];
        r = p[2];
    };

    switch (format) {
        case PixelFormat::RGB32:
            for (int y = 0; y < height; ++y) {
                std::copy_n(src + static_cast<size_t>(y) * width * 4, static_cast<size_t>(width) * 4,
                            dst + static_cast<size_t>(y) * stride);
            }
            return true;

        case PixelFormat::YUY2:
            for (int y = 0; y < height; ++y) {
                uint8_t* out = dst + static_cast<size_t>(y) * stride;
                for (int x = 0; x < width; x += 2) {
                    int r0, g0, b0, r1, g1, b1;
                    px(x, y, r0, g0, b0);
                    px(x + 1, y, r1, g1, b1);
                    out[x * 2 + 0] = ToY(r0, g0, b0);
                    out[x * 2 + 1] = ToU((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2);
                    out[x * 2 + 2] = ToY(r1, g1, b1);
                    out[x * 2 + 3] = ToV((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2);
                }
            }
            return true;

        case PixelFormat::NV12:
        case PixelFormat::I420: {
            uint8_t* yPlane = dst;
            for (int y = 0; y < height; ++y) {
                uint8_t* out = yPlane + static_cast<size_t>(y) * stride;
                for (int x = 0; x < width; ++x) {
                    int r, g, b;
                    px(x, y, r, g, b);
                    out[x] = ToY(r, g, b);
                }
            }
            uint8_t* chroma = dst + static_cast<size_t>(stride) * height;
            const int chromaStride = (format == PixelFormat::NV12) ? stride : stride / 2;
            uint8_t* uPlane = chroma;
            uint8_t* vPlane = chroma + static_cast<size_t>(chromaStride) * (height / 2);
            for (int y = 0; y < height; y += 2) {
                for (int x = 0; x < width; x += 2) {
                    int r = 0, g = 0, b = 0;
                    for (int dy = 0; dy < 2; ++dy) {
                        for (int dx = 0; dx < 2; ++dx) {
                            int rr, gg, bb;
                            px(x + dx, y + dy, rr, gg, bb);
                            r += rr; g += gg; b += bb;
                        }
                    }
                    r /= 4; g /= 4; b /= 4;
                    const int cy = y / 2;
                    const int cx = x / 2;
                    if (format == PixelFormat::NV12) {
                        uint8_t* out = chroma + static_cast<size_t>(cy) * chromaStride;
                        out[cx * 2 + 0] = ToU(r, g, b);
                        out[cx * 2 + 1] = ToV(r, g, b);
                    } else {
                        uPlane[static_cast<size_t>(cy) * chromaStride + cx] = ToU(r, g, b);
                        vPlane[static_cast<size_t>(cy) * chromaStride + cx] = ToV(r, g, b);
                    }
                }
            }
            return true;
        }
    }
    return false;
}

}  // namespace tumon
