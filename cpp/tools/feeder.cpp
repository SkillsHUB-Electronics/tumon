// tumon_feeder: the console version of the Tumon feeder. Reads a real camera and hands its pictures to the
// "Tumon Camera". The app (tumon.exe) does the same with a window; the engine is src/feeder/camera_feeder.cpp.
//
//   tumon_feeder.exe --list                         list the cameras that can be used
//   tumon_feeder.exe --device NAME                  feed from the camera whose name contains NAME
//   tumon_feeder.exe --index N                      feed from camera number N of the --list output
//   options: --width W --height H --fps F           preferred picture (default 1280x720 at 30)
//            --seconds N                            stop after N seconds
//            --always                               keep the camera on even when nobody uses Tumon Camera
//
// The real camera is opened only while some program is using Tumon Camera. Ctrl+C ends the program.
#include <windows.h>

#include <mfapi.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../src/feeder/camera_feeder.h"

namespace {

std::atomic<bool> g_stop{false};

BOOL WINAPI OnCtrl(DWORD) {
    g_stop = true;
    return TRUE;
}

void Print(const tumon::FeederStatus& s) {
    switch (s.state) {
        case tumon::FeederState::Waiting: wprintf(L"Waiting for a program to use Tumon Camera (real camera closed).\n"); break;
        case tumon::FeederState::Streaming:
            if (s.picturesPerSecond > 0) {
                wprintf(L"  %.1f pictures/s sent, %ld program(s) using Tumon Camera\n", s.picturesPerSecond, s.consumers);
            } else {
                wprintf(L"Camera opened: %s, %ux%u (converted to YUY2)\n", s.camera.c_str(), s.width, s.height);
            }
            break;
        case tumon::FeederState::CameraProblem:
            wprintf(L"The camera could not be used (0x%08X); will try again. Check Windows privacy settings and other apps.\n",
                    static_cast<unsigned>(s.error));
            break;
        case tumon::FeederState::CameraMissing: wprintf(L"No camera matches that name; still looking.\n"); break;
        case tumon::FeederState::AlreadyRunning:
            wprintf(L"Another tumon_feeder or the Tumon app is already feeding this camera channel. One is enough: close it first.\n");
            break;
        case tumon::FeederState::Stopped: wprintf(L"Stopped. %llu pictures sent.\n", s.sent); break;
    }
    fflush(stdout);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    tumon::FeederOptions options;
    bool list = false;
    int index = -1;
    int seconds = 0;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        auto next = [&]() -> const wchar_t* { return i + 1 < argc ? argv[++i] : L""; };
        if (a == L"--list") list = true;
        else if (a == L"--device") options.camera = next();
        else if (a == L"--index") index = _wtoi(next());
        else if (a == L"--width") options.width = static_cast<UINT32>(_wtoi(next()));
        else if (a == L"--height") options.height = static_cast<UINT32>(_wtoi(next()));
        else if (a == L"--fps") options.fps = static_cast<UINT32>(_wtoi(next()));
        else if (a == L"--seconds") seconds = _wtoi(next());
        else if (a == L"--always") options.always = true;
        else {
            list = false;
            options.camera.clear();
            index = -1;
            break;
        }
    }
    if (!list && options.camera.empty() && index < 0) {
        wprintf(L"Usage: tumon_feeder.exe --list | --device NAME | --index N  [--width W --height H --fps F --seconds N --always]\n");
        return 64;
    }
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) || FAILED(MFStartup(MF_VERSION))) {
        wprintf(L"Could not start COM or Media Foundation.\n");
        return 1;
    }

    const std::vector<std::wstring> cameras = tumon::ListSourceCameras();
    if (list) {
        for (size_t i = 0; i < cameras.size(); ++i) wprintf(L"%zu: %s\n", i, cameras[i].c_str());
        MFShutdown();
        CoUninitialize();
        return 0;
    }
    if (index >= 0) {
        if (static_cast<size_t>(index) >= cameras.size()) {
            wprintf(L"There is no camera number %d. Use --list.\n", index);
            MFShutdown();
            CoUninitialize();
            return 2;
        }
        options.camera = cameras[index];
    }
    MFShutdown();
    CoUninitialize();

    SetConsoleCtrlHandler(OnCtrl, TRUE);
    std::atomic<bool> refused{false};
    tumon::CameraFeeder feeder;
    feeder.Start(options, [&](const tumon::FeederStatus& s) {
        if (s.state == tumon::FeederState::AlreadyRunning) refused = true;
        Print(s);
    });

    const ULONGLONG deadline = seconds > 0 ? GetTickCount64() + static_cast<ULONGLONG>(seconds) * 1000 : 0;
    while (!g_stop && !refused && (deadline == 0 || GetTickCount64() < deadline)) Sleep(100);
    feeder.Stop();
    return refused ? 65 : 0;
}
