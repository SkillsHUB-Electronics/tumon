#include "yuy2_scaler.h"

#include <algorithm>
#include <vector>

namespace tumon {

namespace {

inline uint8_t Clamp(int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }

}  // namespace

bool FitYuy2ToBgra(const uint8_t* src, int srcWidth, int srcHeight, int srcStride, uint8_t* dst, int dstWidth, int dstHeight) {
    if (!src || !dst || srcWidth < 2 || srcHeight < 2 || (srcWidth & 1) || srcStride < srcWidth * 2 || dstWidth < 2 || dstHeight < 2) {
        return false;
    }

    // Largest size with the source's shape that fits in the destination, centred.
    int fitWidth, fitHeight;
    if (static_cast<int64_t>(srcWidth) * dstHeight <= static_cast<int64_t>(srcHeight) * dstWidth) {
        fitHeight = dstHeight;
        fitWidth = static_cast<int>(static_cast<int64_t>(srcWidth) * dstHeight / srcHeight);
    } else {
        fitWidth = dstWidth;
        fitHeight = static_cast<int>(static_cast<int64_t>(srcHeight) * dstWidth / srcWidth);
    }
    fitWidth = std::max(2, std::min(fitWidth, dstWidth));
    fitHeight = std::max(2, std::min(fitHeight, dstHeight));
    const int offsetX = (dstWidth - fitWidth) / 2;
    const int offsetY = (dstHeight - fitHeight) / 2;

    // Black everywhere, then the picture on top.
    for (size_t i = 0; i < static_cast<size_t>(dstWidth) * dstHeight; ++i) {
        dst[i * 4 + 0] = 0;
        dst[i * 4 + 1] = 0;
        dst[i * 4 + 2] = 0;
        dst[i * 4 + 3] = 255;
    }

    std::vector<int> columns(static_cast<size_t>(fitWidth));
    for (int x = 0; x < fitWidth; ++x) columns[x] = static_cast<int>(static_cast<int64_t>(x) * srcWidth / fitWidth);

    for (int y = 0; y < fitHeight; ++y) {
        const int sy = static_cast<int>(static_cast<int64_t>(y) * srcHeight / fitHeight);
        const uint8_t* row = src + static_cast<size_t>(sy) * srcStride;
        uint8_t* out = dst + (static_cast<size_t>(y + offsetY) * dstWidth + offsetX) * 4;
        for (int x = 0; x < fitWidth; ++x) {
            const int sx = columns[x];
            const int pair = sx & ~1;
            const int Y = row[sx * 2] - 16;
            const int U = row[pair * 2 + 1] - 128;
            const int V = row[pair * 2 + 3] - 128;
            out[x * 4 + 0] = Clamp((298 * Y + 516 * U + 128) >> 8);            // B
            out[x * 4 + 1] = Clamp((298 * Y - 100 * U - 208 * V + 128) >> 8);  // G
            out[x * 4 + 2] = Clamp((298 * Y + 409 * V + 128) >> 8);            // R
            out[x * 4 + 3] = 255;
        }
    }
    return true;
}

}  // namespace tumon
