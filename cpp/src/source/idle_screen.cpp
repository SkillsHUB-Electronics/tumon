#include "idle_screen.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tumon {

namespace {

// Where the spinner sits in the artwork, as fractions of its size (see tools/make_idle.py, which paints the
// drawn one out): centre x and y, outer radius and ring thickness as fractions of the height.
constexpr double kSpinnerX = 0.5;
constexpr double kSpinnerY = 0.7837;
constexpr double kSpinnerRadius = 0.0542;
constexpr double kSpinnerThickness = 0.0117;
constexpr double kFramesPerTurn = 45.0;  // 1.5 seconds at 30 fps
constexpr double kPi = 3.14159265358979323846;
constexpr double kArc = 2.0 * kPi * 300.0 / 360.0;  // the ring has a gap of 60 degrees

// Scale so the artwork covers the frame, and the offset of the cropped part.
struct Cover {
    double scale;
    double offsetX;
    double offsetY;
};

Cover CoverFor(int artWidth, int artHeight, int width, int height) {
    const double scale = std::max(static_cast<double>(width) / artWidth, static_cast<double>(height) / artHeight);
    return {scale, (artWidth * scale - width) / 2.0, (artHeight * scale - height) / 2.0};
}

}  // namespace

bool IdleScreen::SetArt(std::vector<uint8_t> bgra, int width, int height) {
    art_.clear();
    base_.clear();
    baseWidth_ = baseHeight_ = 0;
    if (width < 2 || height < 2 || bgra.size() != static_cast<size_t>(width) * height * 4) return false;
    art_ = std::move(bgra);
    artWidth_ = width;
    artHeight_ = height;
    return true;
}

void IdleScreen::BuildBase(int width, int height) {
    base_.resize(static_cast<size_t>(width) * height * 4);
    baseWidth_ = width;
    baseHeight_ = height;
    const Cover c = CoverFor(artWidth_, artHeight_, width, height);
    for (int y = 0; y < height; ++y) {
        // Bilinear sampling at the middle of each destination pixel.
        const double sy = std::clamp((y + c.offsetY + 0.5) / c.scale - 0.5, 0.0, artHeight_ - 1.0);
        const int y0 = static_cast<int>(sy);
        const int y1 = std::min(y0 + 1, artHeight_ - 1);
        const double fy = sy - y0;
        for (int x = 0; x < width; ++x) {
            const double sx = std::clamp((x + c.offsetX + 0.5) / c.scale - 0.5, 0.0, artWidth_ - 1.0);
            const int x0 = static_cast<int>(sx);
            const int x1 = std::min(x0 + 1, artWidth_ - 1);
            const double fx = sx - x0;
            uint8_t* out = &base_[(static_cast<size_t>(y) * width + x) * 4];
            for (int channel = 0; channel < 3; ++channel) {
                const auto at = [&](int ax, int ay) { return static_cast<double>(art_[(static_cast<size_t>(ay) * artWidth_ + ax) * 4 + channel]); };
                const double top = at(x0, y0) * (1 - fx) + at(x1, y0) * fx;
                const double bottom = at(x0, y1) * (1 - fx) + at(x1, y1) * fx;
                out[channel] = static_cast<uint8_t>(top * (1 - fy) + bottom * fy + 0.5);
            }
            out[3] = 255;
        }
    }
}

void IdleScreen::DrawSpinner(uint8_t* dst, int width, int height, uint64_t frame) const {
    const Cover c = CoverFor(artWidth_, artHeight_, width, height);
    const double cx = kSpinnerX * artWidth_ * c.scale - c.offsetX;
    const double cy = kSpinnerY * artHeight_ * c.scale - c.offsetY;
    const double outer = kSpinnerRadius * artHeight_ * c.scale;
    const double thickness = std::max(2.0, kSpinnerThickness * artHeight_ * c.scale);
    const double middle = outer - thickness / 2.0;
    const double head = std::fmod(static_cast<double>(frame % static_cast<uint64_t>(kFramesPerTurn)) / kFramesPerTurn * 2.0 * kPi, 2.0 * kPi);

    const int left = std::max(0, static_cast<int>(cx - outer - 2));
    const int right = std::min(width - 1, static_cast<int>(cx + outer + 2));
    const int top = std::max(0, static_cast<int>(cy - outer - 2));
    const int bottom = std::min(height - 1, static_cast<int>(cy + outer + 2));
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const double dx = x + 0.5 - cx;
            const double dy = y + 0.5 - cy;
            const double coverage = std::clamp(thickness / 2.0 - std::fabs(std::sqrt(dx * dx + dy * dy) - middle) + 0.5, 0.0, 1.0);
            if (coverage <= 0.0) continue;
            // How far behind the head this pixel is, going clockwise (screen y points down).
            double behind = head - std::atan2(dy, dx);
            behind = std::fmod(behind, 2.0 * kPi);
            if (behind < 0) behind += 2.0 * kPi;
            if (behind > kArc) continue;
            const double t = behind / kArc;  // 0 at the head, 1 at the tail
            const double alpha = coverage * (1.0 - 0.9 * t);
            // From deep blue at the head to cyan at the tail (B, G, R).
            const double colour[3] = {210.0 + 45.0 * t, 87.0 + 133.0 * t, 0.0 + 30.0 * t};
            uint8_t* out = &dst[(static_cast<size_t>(y) * width + x) * 4];
            for (int channel = 0; channel < 3; ++channel) {
                out[channel] = static_cast<uint8_t>(out[channel] * (1.0 - alpha) + colour[channel] * alpha + 0.5);
            }
        }
    }
}

bool IdleScreen::Draw(uint8_t* dst, int width, int height, uint64_t frame) {
    if (!Ready() || !dst || width < 2 || height < 2) return false;
    if (width != baseWidth_ || height != baseHeight_) BuildBase(width, height);
    std::memcpy(dst, base_.data(), base_.size());
    DrawSpinner(dst, width, height, frame);
    return true;
}

}  // namespace tumon
