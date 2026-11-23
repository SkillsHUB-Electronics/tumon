// Small helpers for the hand-written COM objects (no ATL, no WRL, no C++/WinRT, no WIL).
#pragma once

#include <windows.h>

#include <atomic>

namespace tumon {

// Number of live COM objects in this DLL; DllCanUnloadNow reports S_OK only when it is zero.
extern std::atomic<long> g_objectCount;

}  // namespace tumon

// AddRef/Release for a class whose first line of construction sets m_ref to 1 (the creator owns that
// reference). Place inside the class body. Works with multiple inheritance: one override replaces the
// IUnknown methods of every base.
#define TUMON_COM_REFCOUNT()                                              \
public:                                                                   \
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }         \
    ULONG STDMETHODCALLTYPE Release() override {                          \
        const ULONG remaining = --m_ref;                                  \
        if (remaining == 0) delete this;                                  \
        return remaining;                                                 \
    }                                                                     \
private:                                                                  \
    std::atomic<ULONG> m_ref{1};
