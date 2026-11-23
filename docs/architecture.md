# Architecture (planned)

> **Built so far:** the Media Foundation source DLL (test camera), its host, self-test and diagnostic tools in
> [`cpp/`](../cpp/README.md). Everything else (app, effects, phone, installer) is planned. This page is the design that [ADR 0001](decisions/0001-media-foundation-virtual-camera.md)
> and the [research](research/teamtalk-iriun-findings.md) point to. It will be revised after Milestone 0.

## Data flow

```
 Sources                      Tumon app (user session)            Windows Frame Server        Apps
 ───────                      ────────────────────────            ────────────────────        ────
 USB / laptop webcam ─┐       capture → decode → effects →         Tumon media source          TeamTalk
 Phone (Wi-Fi / USB)  ├────►  convert to NV12/YUY2/RGB32  ──────►  (IMFMediaSource DLL)  ────►  Teams, Zoom
 Screen or window     ┘       write to shared memory ring          reads shared memory         Meet, browsers
                                      ▲                                  ▲
                                      └── control pipe (start/stop, format negotiation, status)
```

## Components

| Component | Language | Runs in | Job |
|---|---|---|---|
| **Media source DLL** (`tumon_source`) | C++17 | Frame Server service (LocalService) | Implements `IMFMediaSource` / `IMFMediaStream`; exposes the formats apps ask for; reads frames from shared memory; shows a fallback picture when no producer is active. Must be tiny and never crash on bad input. |
| **Registrar** | C++17 | Installer / app (elevated) | `MFCreateVirtualCamera` registration, COM registration of the DLL, clean unregister. |
| **App** (`tumon`) | Python 3.10+, PySide6 | User session | UI, source selection, effects pipeline, writes frames to shared memory, tray icon. |
| **Native core** (`tumon-core`) | C++17 | In the app process | Fast pixel work: colour conversion, scaling, flip/rotate/crop, blur. Python reference kept for parity tests, as in Tapis. |
| **Phone app** | Kotlin (Android first) | Phone | Captures camera, encodes, streams to the PC. |
| **Control** | Python | App | `tumonctl` command line and MCP server over a per-user named pipe (mirrors `tapisctl`). |

## Key design points

- **Be a real Windows camera, then make TeamTalk's resolution list fill in by itself.** TeamTalk lists Media
  Foundation, PnP and kernel cameras and not pure DirectShow filters. It then builds its *Video Resolution* list
  only from formats it supports; otherwise it shows "Custom video format" and asks the user to type values, which
  fails easily (the RGB24-only SplitCam Video Driver ends up there). So offer YUY2 (plus I420 and RGB32 if the SDK
  confirms them) at standard sizes such as 320x240, 352x288, 640x480 and 1280x720, and always convert from whatever
  the real camera provides ([research](research/teamtalk-iriun-findings.md)). To be confirmed in Milestone 0.
- **TeamTalk setup helper (idea).** Because manual entry is fragile, the Tumon app should state, in plain text that
  a screen reader can read, the exact device name, resolution and image format to choose in TeamTalk's Video
  Capture preferences. It must not edit TeamTalk's settings without the user's consent.
- **Formats.** Offer several resolutions (for example 320×240, 640×480, 1280×720) and pixel formats. TeamTalk
  sends small video, so low resolutions matter more than 4K. Exact set is decided in Milestone 0 from what
  TeamTalk and the Windows Camera app negotiate.
- **Shared memory channel: built (v1), see `cpp/src/channel/frame_channel.h`.** Named block
  `Global\TumonCameraFrames-v1` created by the source (a service may create `Global` objects; a normal program may
  not), opened by the feeder. Header + 3 YUY2 slots (max 1920x1080). No locks: each slot has a sequence number, 0
  while being written, so a reader that copied a torn picture notices and skips it. A count of running streams lets
  the feeder open the real camera only while Tumon Camera is in use (the camera light stays off otherwise). If the
  feeder is silent for one second the source falls back to its test pattern. Access list: system, administrators,
  Local Service, interactively logged-on users. The older sketch follows.
- **Shared memory channel (original plan).** A named memory-mapped ring buffer with a header (version, width, height, format,
  frame counter) plus an event for new-frame signalling. The DLL treats it as untrusted: validate every field,
  bound every copy. Access limited to the signed-in user.
- **No producer.** The camera stays listed and outputs a clear fallback picture, so apps never fail to open it.
- **Accessibility.** The app UI is built for keyboard and screen readers ([accessibility.md](accessibility.md)).
- **Privacy.** No network code in the DLL. Phone streaming terminates in the app, on the local network only
  ([PRIVACY.md](../PRIVACY.md)).

## Planned folder layout

```
tumon/
├─ .github/                  issue and PR templates, CI
├─ cpp/                      CMake project: source DLL, registrar, core, tests, third_party/
├─ tumon/                    Python package: ui/, capture/, effects/, ipc/, i18n/, legal/
├─ android/                  phone app (Gradle project)
├─ packaging/windows/        Inno Setup script, signing notes
├─ tests/                    pytest (including native parity)
├─ tools/                    CI helpers, screenshot generator
├─ docs/                     this folder; research/ and decisions/ inside
└─ README.md CHANGELOG.md ROADMAP.md CONTRIBUTING.md SECURITY.md PRIVACY.md …
```

Folders are created when they get their first real file; empty placeholders are not committed.

## Open questions

- Does TeamTalk accept the formats we offer? (Milestone 0.)
- Frame Server restrictions on shared-memory access from a LocalService process: which security descriptor works?
- Phone protocol: H.264 vs MJPEG, authentication, discovery (Milestone 3, will get its own ADR).
