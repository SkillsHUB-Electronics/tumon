// The engine that reads a real camera and feeds Tumon Camera through the frame channel. Used by the app
// (tumon.exe) and by the console tool (tumon_feeder.exe).
//
// It runs on its own thread. The real camera is opened only while some program is using Tumon Camera, so the
// camera's light is on only then, and only one feeder per channel can run at a time.
#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace tumon {

// Names of the cameras that can be used as a source (every Media Foundation camera except Tumon Camera itself).
// The calling thread must have COM and Media Foundation started.
std::vector<std::wstring> ListSourceCameras();

// True if Windows lists a camera called "Tumon Camera" (the virtual camera is installed).
bool IsTumonCameraInstalled();

struct FeederOptions {
    std::wstring camera;  // name, or part of it, as returned by ListSourceCameras()
    UINT32 width = 1280;
    UINT32 height = 720;
    UINT32 fps = 30;
    bool always = false;  // keep the real camera open even when nobody uses Tumon Camera
};

enum class FeederState {
    Stopped,
    Waiting,         // armed: no program is using Tumon Camera, so the real camera is closed
    Streaming,       // the real camera is open and pictures are being sent
    CameraProblem,   // the camera could not be opened or stopped; it will be tried again
    CameraMissing,   // no camera matches the chosen name; it will be looked for again
    AlreadyRunning,  // another feeder holds this channel; this one has ended
};

struct FeederStatus {
    FeederState state = FeederState::Stopped;
    std::wstring camera;  // the real camera's full name once opened
    UINT32 width = 0;
    UINT32 height = 0;
    double picturesPerSecond = 0;
    long consumers = 0;
    HRESULT error = S_OK;
    uint64_t sent = 0;
};

class CameraFeeder {
public:
    // Called on the feeder thread whenever the state changes, and about every two seconds while streaming.
    using Callback = std::function<void(const FeederStatus&)>;

    CameraFeeder() = default;
    ~CameraFeeder() { Stop(); }
    CameraFeeder(const CameraFeeder&) = delete;
    CameraFeeder& operator=(const CameraFeeder&) = delete;

    void Start(const FeederOptions& options, Callback callback);
    void Stop();  // waits for the thread to end
    bool Running() const { return running_; }

private:
    void Run(FeederOptions options, Callback callback);

    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
};

}  // namespace tumon
