// The picture shown while nothing is sending a camera: the artwork, scaled to cover the frame (a 4:3 frame loses a
// little at the left and right, never the middle), with a turning spinner so the receiver sees the video is alive.
// No Windows headers, so it can be unit-tested anywhere.
#pragma once

#include <cstdint>
#include <vector>

namespace tumon {

class IdleScreen {
public:
    // `bgra` is width * height * 4 bytes. Returns false (and stays not ready) if the sizes do not match.
    bool SetArt(std::vector<uint8_t> bgra, int width, int height);
    bool Ready() const { return !art_.empty(); }

    // Draws frame number `frame` into `dst` (width * height * 4 bytes, B G R X). False if not ready or bad size.
    bool Draw(uint8_t* dst, int width, int height, uint64_t frame);

private:
    void BuildBase(int width, int height);
    void DrawSpinner(uint8_t* dst, int width, int height, uint64_t frame) const;

    std::vector<uint8_t> art_;
    int artWidth_ = 0;
    int artHeight_ = 0;
    std::vector<uint8_t> base_;  // the artwork already scaled to the last frame size
    int baseWidth_ = 0;
    int baseHeight_ = 0;
};

}  // namespace tumon
