# Changelog

All notable changes to Tumon are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/). Downloads and full release notes will be on the
[releases page](https://github.com/SkillsHUB-Electronics/tumon/releases).

## [Unreleased]

### Added
- Project scaffold: `README.md`, `CLAUDE.md`, `AGENTS.md`, `CHANGELOG.md`, `ROADMAP.md`,
  `CONTRIBUTING.md`, `SECURITY.md`, `PRIVACY.md`, `THIRD_PARTY_NOTICES.md`, `CODE_OF_CONDUCT.md`,
  `LICENSE` (MIT) and `.gitignore`.
- `.github/` issue forms (bug report, feature request), pull-request template and a placeholder CI
  workflow that checks the required project files exist; layout mirrors the Tapis repository.
- `docs/decisions/` (ADR index and template) and `docs/releasing.md`.
- `docs/` with an index, architecture overview, build notes, accessibility requirements and
  troubleshooting guide (all describing the *planned* design; no code exists yet).
- Research note `docs/research/teamtalk-iriun-findings.md` (2026-11-23) comparing how Iriun Webcam, OBS
  Virtual Camera and SplitCam register on Windows 11 and how TeamTalk 5.23 enumerates cameras.
- ADR 0001: use a Media Foundation virtual camera (not a DirectShow filter), which makes Windows 11 the
  minimum supported version.

- `tools/device-probe`: read-only C# tool (builds with the Windows-built-in compiler, no SDK) that lists DirectShow
  and Media Foundation video devices with every format and size they offer. First run recorded in
  `docs/research/probe-results-2026-11-23.md`.

- **`cpp/`: Milestone 0 test camera (C++17, CMake).** `tumon_source.dll` is a Media Foundation media source (activation
  object, `IMFMediaSourceEx`, one stream) offering 12 test media types (YUY2, NV12, RGB32, I420 at 640x480, 320x240,
  1280x720, 30 fps) with a CPU-drawn moving test picture. Plain COM, static C runtime, system DLLs only. Also
  `tumon_vcam_host.exe` (registers the camera with `MFCreateVirtualCamera` while it runs), `tumon_selftest.exe`
  (loads the DLL without registration and verifies all 12 types through a source reader: 20/20 frames, correct byte
  sizes, moving picture, 33.3 ms pacing: all pass), `tumon_mfinfo.exe` (native Media Foundation formats of every
  camera) and unit tests for the picture generator (CTest: 2 of 2 pass). Not yet registered or tried in TeamTalk.
- `tools/test-camera`: `install-test-camera.ps1` / `uninstall-test-camera.ps1` (administrator; copy the DLL to
  `C:\Program Files\Tumon` and register it in HKLM, and exactly reverse that) and a step-by-step README.
- Findings from `tumon_mfinfo` (observation 22): only Iriun offers YUY2 natively (16 YUY2 + 16 RGB24 types);
  the Surface camera and the Windows Virtual Camera offer only NV12; the SplitCam driver only RGB24. TeamTalk's list
  for Iriun has 16 entries, so it uses the YUY2 types only. Tumon will offer native YUY2 and convert NV12 from real
  cameras. `docs/building.md` now describes the real toolchain and build.

- **Milestone 1, first step: a real camera feeds Tumon Camera.**
  - `cpp/src/channel/frame_channel.*`: a shared-memory frame channel (`Global\TumonCameraFrames-v1`, private name
    override `TUMON_CHANNEL_NAME` for tests). Header plus three YUY2 slots (up to 1920x1080), sequence numbers
    instead of locks (a torn read is detected and skipped), a producer time-out of one second, and a count of running
    streams. The source creates it with an access list for the system, administrators, the service account and
    interactively logged-on users only; every field the feeder writes is validated before it is read.
  - The source now shows live pictures when a feeder is publishing (`yuy2_scaler`: fitted into the requested size
    with black bars, converted to every offered format) and the test pattern otherwise.
  - `tumon_feeder.exe --list | --device NAME | --index N`: reads a real camera through Media Foundation, converts
    to YUY2 with the Video Processor MFT, publishes into the channel. It opens the real camera **only while some
    program uses Tumon Camera** (the camera's light is off otherwise), reconnects, and refuses to read Tumon Camera itself.
  - Tests: unit tests for the scaler and the channel (CTest now 4 of 4), and an in-process live test in
    `tumon_selftest` with a fake feeder (picture content, letterboxing, stream count, fall back to the pattern after
    the feeder stops). `tumon_selftest --live N` shows what the source delivers while a separate feeder runs.
  - Run with the Surface camera (no administrator rights, private channel): 1280x720 at 30.0 pictures/s, the source
    switched to live pictures, the real camera closed when the reader stopped (research observation 28).
  - Needs the updated DLL installed by an administrator to appear in TeamTalk; not yet tried there.
- `cpp/tools/vcam_ctl.cpp` (`tumon_vcam_ctl.exe install | remove | status`): registers "Tumon Camera" as a permanent
  camera with `MFVirtualCameraLifetime_System` and `MFVirtualCameraAccess_AllUsers` (administrator), per Microsoft's
  documented behaviour that the same parameters reopen the same camera and `Remove` deletes it. `install-test-camera.ps1`
  gains `-Permanent`; `uninstall-test-camera.ps1` removes a permanent camera first. Built; `status` and the
  non-administrator refusal checked; the permanent install itself not yet run (needs the user's administrator shell).

### Verified
- **The live laptop camera appears in TeamTalk through Tumon Camera** (user-reported, with the permanent camera updated
  to the new DLL and `tumon_feeder` running): the Surface camera's picture shows and moves (research observation 29).
- **Permanent system camera works in TeamTalk 5.23 after a Windows restart** (user-reported): the camera's video was
  shown, no host program running. **Correction found later:** Windows' camera list order changed across the restart
  (Tumon from position 4 to 0) and TeamTalk now saves `deviceid=0`, so saved positions are fragile and it is not
  established that TeamTalk remapped by itself (research observations 26 and 27).

### Fixed
- `tumon_feeder.exe` now allows **only one running instance per channel** (a named mutex; the second one prints a
  message and exits with code 65). Two feeders at once fought over the real camera (every open failed with
  `0xC00D3704`) and wrote into the same slots. Found when the user had started two by accident. Verified: a second
  feeder is refused while the first keeps sending about 30 pictures per second.
- `install-test-camera.ps1` could not be run twice: copying over the installed DLL failed ("being used by another
  process") because the Frame Server holds it while the camera exists. It now skips the copy when the DLL is
  identical (hash check) and otherwise stops the Frame Server briefly, copies, and starts it again.
- `uninstall-test-camera.ps1` failed to delete the DLL ("access denied") because the Windows Frame Server keeps it
  loaded after the camera is unregistered. It now stops `FrameServer` and `FrameServerMonitor` briefly, deletes the
  DLL and starts them again. Found when the user ran the first uninstall.

### Changed
- **Milestone 0 passed.** In TeamTalk 5.23 the test camera is listed ("Tumon Camera"), fills *Video Resolution* by itself
  (640x480, 320x240, 1280x720 at FPS 30) and shows the test picture at 640x480 and at 1280x720 (user-confirmed);
  TeamTalk saved YUY2 (`fourcc=101`). ADR 0001 is now Accepted. New finding: TeamTalk stores the camera by its list
  position (`deviceid`), so cameras that come and go shift it; the plan adds a system-lifetime camera and a
  plain-text "TeamTalk setup" helper. README status updated.
- Probe fixed: the Media Foundation device list now works (wrong attribute key GUID replaced by the one found inside
  `TeamTalk5.dll`). It returns exactly the four cameras TeamTalk lists and not OBS or the recent SplitCam virtual
  camera, so TeamTalk's list is Media Foundation's video-capture enumeration. Reading native formats still fails.
- Research note: recorded TeamTalk's saved video settings (1280x720, 24 fps, YUY2, matching the Surface camera's only
  720p mode) and the report that the Surface camera needed a hand-typed custom format. Goal sharpened: Tumon must
  never require a custom format. Probe: still no Media Foundation listing (fails inside the assistant session even
  under a single-threaded apartment); now runs as STA and prints a diagnostic to stderr.
- Research note: recorded TeamTalk's resolution list for Iriun (8 sizes at FPS 29 and 60). The 60 fps entries do not
  exist in the DirectShow view of Iriun, so TeamTalk builds its list through Media Foundation; size does not filter
  cameras out (4K is listed), format does.
- Findings after TeamTalk's Video Capture preferences and error messages: TeamTalk builds its resolution list from
  formats it supports and falls back to "Custom video format" otherwise, where manual entry fails easily; starting
  needs a valid saved combination. Design now aims to make that list fill in by itself, and adds a plain-text
  "TeamTalk setup" helper to the roadmap. Next evidence requested: TeamTalk's Resolution and Image Format
  dropdowns for Surface Camera Front and Iriun.
- Findings after the TeamTalk device-list screenshot and the report that SplitCam's entry is listed but will not
  start: the RGB24 hypothesis is refuted for *listing* (Surface camera and Windows Virtual Camera are listed
  without it). New two-stage working model: TeamTalk lists real Windows cameras (not pure DirectShow filters), and a
  listed camera must also start with a format it can capture (probably YUY2). ADR 0001 is supported again but still
  *Proposed*; README, CLAUDE.md, roadmap and architecture updated to match.
- Findings after the probe (partly superseded by the entry above): devices reported to work all offer RGB24, devices reported to fail do not; OBS Virtual
  Camera offers no formats while stopped. ADR 0001 marked unproven; architecture and roadmap now plan for RGB24 and
  YUY2 and a Milestone 0 build that tests RGB24-only versus YUY2-only.
- Research note: added the user report that OBS becomes visible in TeamTalk through NDI, and marked it as
  unresolved evidence against a pure Media Foundation explanation.

- Research note: added the user report that older SplitCam versions worked in TeamTalk and recent ones do not,
  plus SplitCam's installed components; listed the competing explanations and the device-probe test.

- Research note and README: added the report that many recent laptop cameras are not listed in TeamTalk or fail
  to open; Tumon is now framed as a compatibility adapter (any camera in, TeamTalk-friendly format out), and a
  control test with the Surface camera was added.

- Research note and architecture: recorded the report that TeamTalk uses YUY2, the format names found in
  `TeamTalk5.dll` (with the caveat that they do not prove camera-path support), and a "YUY2 first, modest
  resolution" working assumption.

### Notes
- Version `0.0.0`: planning only. Nothing is installable.
