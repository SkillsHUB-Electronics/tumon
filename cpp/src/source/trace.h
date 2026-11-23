// Developer tracing. Does nothing unless the environment variable TUMON_TRACE_FILE names a file, so the
// copy that runs inside the Windows Frame Server (which never has that variable) stays silent and never
// writes to disk. Used with tools\selftest.
#pragma once

#include <windows.h>

#include <cstdarg>
#include <cstdio>

namespace tumon {

inline void Trace(const wchar_t* format, ...) {
    wchar_t path[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"TUMON_TRACE_FILE", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"a") != 0 || !file) return;
    va_list args;
    va_start(args, format);
    vfwprintf(file, format, args);
    va_end(args);
    fputwc(L'\n', file);
    fclose(file);
}

}  // namespace tumon
