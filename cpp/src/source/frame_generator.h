// Generates a moving test picture (colour bars, a moving black line and a frame counter) and writes
// it in the pixel formats the test camera offers. Pure CPU, no Direct3D, no dependencies.
// Kept free of Windows headers so it can be unit-tested anywhere.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tumon {

enum class PixelFormat {
    YUY2,   // packed 4:2:2, Y0 U Y1 V
    NV12,   // planar Y, then interleaved UV (4:2:0)
    RGB32,  // B G R X, top-down
    I420,   // planar Y, U, V (4:2:0)
};

// Bytes in one row of the main (luma or packed) plane when the buffer is tightly packed.
int DefaultStride(PixelFormat format, int width);

// Bytes needed for a whole frame whose main plane has the given stride (bytes per row).
// Planar formats store the chroma planes after the luma plane:
//   NV12: UV plane has the same stride as luma.   I420: U and V planes have stride/2.
size_t FrameBytes(PixelFormat format, int stride, int height);

class FrameGenerator {
public:
    // Draws frame number `frame` at width x height and converts it into `dst`.
    // `dst` must hold FrameBytes(format, stride, height) bytes. Width and height must be even.
    // Returns false (and writes nothing) if the arguments are invalid.
    bool Write(PixelFormat format, uint8_t* dst, int stride, int width, int height, uint64_t frame);

private:
    void Draw(int width, int height, uint64_t frame);

    std::vector<uint8_t> bgra_;  // width * height * 4
};

}  // namespace tumon
