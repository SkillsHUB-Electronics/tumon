// tumon_vcam_host: registers the "Tumon Camera" virtual camera for as long as this program runs.
//
//   tumon_vcam_host.exe              run until you press Enter (or Ctrl+C)
//   tumon_vcam_host.exe --seconds N  run for N seconds, then remove the camera
//
// Requires tumon_source.dll to be registered first (install-test-camera.ps1, as administrator): the camera
// exists only while this program is alive (a "session" camera). Closing the program removes it.
#include <windows.h>

#include <mfapi.h>
#include <mfvirtualcamera.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include "../include/tumon/ids.h"

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfsensorgroup")
#pragma comment(lib, "ole32")

namespace {

HANDLE g_stop = nullptr;

BOOL WINAPI OnCtrl(DWORD) {
    SetEvent(g_stop);
    return TRUE;
}

void PrintError(const wchar_t* what, HRESULT hr) {
    wchar_t text[256] = L"";
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(hr), 0, text,
                   static_cast<DWORD>(_countof(text)), nullptr);
    wprintf(L"%s failed: 0x%08X %s\n", what, static_cast<unsigned>(hr), text);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    int seconds = 0;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--seconds") == 0 && i + 1 < argc) seconds = _wtoi(argv[++i]);
    }

    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    SetConsoleCtrlHandler(OnCtrl, TRUE);

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
    {
        Microsoft::WRL::ComPtr<IMFVirtualCamera> camera;
        hr = MFCreateVirtualCamera(MFVirtualCameraType_SoftwareCameraSource, MFVirtualCameraLifetime_Session,
                                   MFVirtualCameraAccess_CurrentUser, tumon::kCameraName, tumon::kClsidString,
                                   nullptr, 0, &camera);
        if (FAILED(hr)) {
            PrintError(L"MFCreateVirtualCamera (is tumon_source.dll registered?)", hr);
            exitCode = 2;
        } else {
            hr = camera->Start(nullptr);
            if (FAILED(hr)) {
                PrintError(L"Start (access denied usually means the DLL is in a folder the Frame Server cannot read)", hr);
                exitCode = 3;
            } else {
                wprintf(L"%s is running. ", tumon::kCameraName);
                if (seconds > 0) {
                    wprintf(L"It will stop in %d seconds.\n", seconds);
                    WaitForSingleObject(g_stop, static_cast<DWORD>(seconds) * 1000);
                } else {
                    wprintf(L"Press Enter or Ctrl+C to stop it.\n");
                    HANDLE handles[2] = {g_stop, GetStdHandle(STD_INPUT_HANDLE)};
                    for (;;) {
                        const DWORD r = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
                        if (r == WAIT_OBJECT_0) break;
                        INPUT_RECORD rec;
                        DWORD n = 0;
                        if (ReadConsoleInputW(handles[1], &rec, 1, &n) && rec.EventType == KEY_EVENT &&
                            rec.Event.KeyEvent.bKeyDown && rec.Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
                            break;
                        }
                    }
                }
                hr = camera->Remove();  // do not call Shutdown first: it would shut the source down twice
                wprintf(L"Camera removed (0x%08X).\n", static_cast<unsigned>(hr));
            }
        }
    }

    MFShutdown();
    CoUninitialize();
    return exitCode;
}
