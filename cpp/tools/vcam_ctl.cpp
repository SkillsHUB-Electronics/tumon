// tumon_vcam_ctl: makes the "Tumon Camera" a permanent camera (it survives closing programs, sign-out and
// restarts) and removes it again.
//
//   tumon_vcam_ctl.exe install   register the camera for all users, system lifetime   (administrator)
//   tumon_vcam_ctl.exe remove    remove that camera                                     (administrator)
//   tumon_vcam_ctl.exe status    exit code 0 and "present" if Windows lists it, else 1 and "absent"
//
// Needs tumon_source.dll to be registered first (install-test-camera.ps1 does both steps).
//
// Why this matters: TeamTalk saves the camera as its position in Windows' camera list. A session camera
// (tumon_vcam_host.exe) comes and goes, so the position and the saved choice can break. A system camera
// is always there.
#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfvirtualcamera.h>
#include <wrl/client.h>

#include <cstdio>
#include <cwchar>
#include <string>

#include "../include/tumon/ids.h"

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "mfsensorgroup")
#pragma comment(lib, "ole32")

using Microsoft::WRL::ComPtr;

namespace {

void PrintError(const wchar_t* what, HRESULT hr) {
    wchar_t text[256] = L"";
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(hr), 0, text,
                   static_cast<DWORD>(_countof(text)), nullptr);
    wprintf(L"%s failed: 0x%08X %s\n", what, static_cast<unsigned>(hr), text);
    if (hr == E_ACCESSDENIED) wprintf(L"Run this in an Administrator PowerShell.\n");
}

// The parameters identify the camera: the same parameters always open the same camera.
HRESULT OpenCamera(ComPtr<IMFVirtualCamera>* camera) {
    return MFCreateVirtualCamera(MFVirtualCameraType_SoftwareCameraSource, MFVirtualCameraLifetime_System,
                                 MFVirtualCameraAccess_AllUsers, tumon::kCameraName, tumon::kClsidString, nullptr, 0,
                                 camera->GetAddressOf());
}

bool IsListed() {
    ComPtr<IMFAttributes> attrs;
    if (FAILED(MFCreateAttributes(&attrs, 1))) return false;
    attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    IMFActivate** devices = nullptr;
    UINT32 count = 0;
    if (FAILED(MFEnumDeviceSources(attrs.Get(), &devices, &count))) return false;
    bool found = false;
    for (UINT32 i = 0; i < count; ++i) {
        WCHAR* name = nullptr;
        UINT32 length = 0;
        if (SUCCEEDED(devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &length)) && name) {
            if (std::wstring(name).find(tumon::kCameraName) != std::wstring::npos) found = true;
            CoTaskMemFree(name);
        }
        devices[i]->Release();
    }
    CoTaskMemFree(devices);
    return found;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const std::wstring command = argc > 1 ? argv[1] : L"";
    if (command != L"install" && command != L"remove" && command != L"status") {
        wprintf(L"Usage: tumon_vcam_ctl.exe install | remove | status\n");
        return 64;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        PrintError(L"CoInitializeEx", hr);
        return 1;
    }
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        PrintError(L"MFStartup", hr);
        return 1;
    }

    int exitCode = 0;
    if (command == L"status") {
        const bool listed = IsListed();
        wprintf(L"%s: %s\n", tumon::kCameraName, listed ? L"present" : L"absent");
        exitCode = listed ? 0 : 1;
    } else {
        ComPtr<IMFVirtualCamera> camera;
        hr = OpenCamera(&camera);
        if (FAILED(hr)) {
            PrintError(L"MFCreateVirtualCamera", hr);
            exitCode = 2;
        } else if (command == L"install") {
            hr = camera->Start(nullptr);
            if (FAILED(hr)) {
                PrintError(L"Start (is tumon_source.dll registered, in C:\\Program Files\\Tumon?)", hr);
                exitCode = 3;
            } else {
                wprintf(L"%s installed as a permanent camera for all users.\n", tumon::kCameraName);
            }
        } else {
            hr = camera->Remove();
            if (FAILED(hr)) {
                PrintError(L"Remove", hr);
                exitCode = 4;
            } else {
                wprintf(L"%s removed.\n", tumon::kCameraName);
            }
        }
    }

    MFShutdown();
    CoUninitialize();
    return exitCode;
}
