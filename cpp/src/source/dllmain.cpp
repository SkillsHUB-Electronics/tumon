#include <windows.h>

#include <atomic>
#include <new>
#include <string>

#include "../../include/tumon/ids.h"
#include "activator.h"
#include "com_impl.h"

namespace tumon {
std::atomic<long> g_objectCount{0};
}

namespace {

HMODULE g_module = nullptr;

class ClassFactory final : public IClassFactory {
public:
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }   // static object
    ULONG STDMETHODCALLTYPE Release() override { return 1; }  // static object
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;

        auto* activator = new (std::nothrow) tumon::Activator();
        if (!activator) return E_OUTOFMEMORY;
        HRESULT hr = activator->Initialize();
        if (SUCCEEDED(hr)) hr = activator->QueryInterface(riid, ppv);
        activator->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        if (lock) {
            ++tumon::g_objectCount;
        } else {
            --tumon::g_objectCount;
        }
        return S_OK;
    }
};

ClassFactory g_factory;

std::wstring ClsidKeyPath() {
    return std::wstring(L"Software\\Classes\\CLSID\\") + tumon::kClsidString;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow() { return tumon::g_objectCount.load() == 0 ? S_OK : S_FALSE; }

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (clsid != tumon::CLSID_TumonCameraSource) return CLASS_E_CLASSNOTAVAILABLE;
    return g_factory.QueryInterface(riid, ppv);
}

// A virtual camera source must be registered in HKEY_LOCAL_MACHINE (not HKEY_CURRENT_USER): the Windows
// Frame Server services load it, so regsvr32 has to run elevated and the DLL must sit in a folder those
// services can read (for example C:\Program Files\Tumon), not under a user profile.
STDAPI DllRegisterServer() {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(g_module, path, MAX_PATH)) return HRESULT_FROM_WIN32(GetLastError());

    HKEY clsidKey = nullptr;
    LSTATUS status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, ClsidKeyPath().c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &clsidKey, nullptr);
    if (status != ERROR_SUCCESS) return HRESULT_FROM_WIN32(status);
    RegSetValueExW(clsidKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(L"Tumon Camera Source"),
                   static_cast<DWORD>((wcslen(L"Tumon Camera Source") + 1) * sizeof(wchar_t)));
    RegCloseKey(clsidKey);

    HKEY serverKey = nullptr;
    status = RegCreateKeyExW(HKEY_LOCAL_MACHINE, (ClsidKeyPath() + L"\\InprocServer32").c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &serverKey, nullptr);
    if (status != ERROR_SUCCESS) return HRESULT_FROM_WIN32(status);
    RegSetValueExW(serverKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(path),
                   static_cast<DWORD>((wcslen(path) + 1) * sizeof(wchar_t)));
    RegSetValueExW(serverKey, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(L"Both"),
                   static_cast<DWORD>((wcslen(L"Both") + 1) * sizeof(wchar_t)));
    RegCloseKey(serverKey);
    return S_OK;
}

STDAPI DllUnregisterServer() {
    const LSTATUS status = RegDeleteTreeW(HKEY_LOCAL_MACHINE, ClsidKeyPath().c_str());
    if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND) return S_OK;
    return HRESULT_FROM_WIN32(status);
}
