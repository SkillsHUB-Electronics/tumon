// Reads the idle artwork (a PNG stored as a resource in the DLL) with the Windows Imaging Component.
#pragma once

#include <windows.h>

#include <cstdint>
#include <vector>

namespace tumon {

// Fills `bgra` (width * height * 4) from the resource "IDLEART" of `module`. False if anything goes wrong; the
// camera then shows its colour bars instead.
bool LoadIdleArt(HMODULE module, std::vector<uint8_t>* bgra, int* width, int* height);

}  // namespace tumon
