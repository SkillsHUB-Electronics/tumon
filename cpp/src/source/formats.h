// Mapping between Media Foundation video subtypes and the generator's pixel formats.
#pragma once

#include <mfapi.h>

#include "frame_generator.h"

namespace tumon {

inline const GUID& SubtypeFor(PixelFormat format) {
    switch (format) {
        case PixelFormat::YUY2: return MFVideoFormat_YUY2;
        case PixelFormat::NV12: return MFVideoFormat_NV12;
        case PixelFormat::RGB32: return MFVideoFormat_RGB32;
        case PixelFormat::I420: return MFVideoFormat_I420;
    }
    return GUID_NULL;
}

inline bool PixelFormatFromSubtype(const GUID& subtype, PixelFormat* format) {
    for (PixelFormat f : {PixelFormat::YUY2, PixelFormat::NV12, PixelFormat::RGB32, PixelFormat::I420}) {
        if (subtype == SubtypeFor(f)) {
            *format = f;
            return true;
        }
    }
    return false;
}

}  // namespace tumon
