// Draws a YUY2 picture into a BGRA canvas, scaled to fit (the picture keeps its shape; the rest is black).
// No Windows headers, so it can be unit-tested anywhere.
#pragma once

#include <cstdint>

namespace tumon {

// `dst` holds dstWidth * dstHeight * 4 bytes (B, G, R, 255). Returns false for bad arguments (nothing written).
bool FitYuy2ToBgra(const uint8_t* src, int srcWidth, int srcHeight, int srcStride, uint8_t* dst, int dstWidth, int dstHeight);

}  // namespace tumon
