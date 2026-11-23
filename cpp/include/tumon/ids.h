// Identifiers shared by the Tumon media source DLL and the tools that register it.
#pragma once

#include <guiddef.h>

namespace tumon {

// CLSID of the Tumon camera media source (the COM class that the Windows Frame Server loads).
// {38035696-DAA4-41E0-95BC-C1FC73185CFE}
inline constexpr GUID CLSID_TumonCameraSource = {
    0x38035696, 0xdaa4, 0x41e0, {0x95, 0xbc, 0xc1, 0xfc, 0x73, 0x18, 0x5c, 0xfe}};

// Same CLSID as the string MFCreateVirtualCamera expects.
inline constexpr wchar_t kClsidString[] = L"{38035696-DAA4-41E0-95BC-C1FC73185CFE}";

// Name shown to applications such as TeamTalk and the Windows Camera app.
inline constexpr wchar_t kCameraName[] = L"Tumon Camera";

}  // namespace tumon
