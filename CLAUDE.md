# CLAUDE.md

Instructions for Claude and other AI agents working in this repository are in [AGENTS.md](AGENTS.md).
Read it first, then [CHANGELOG.md](CHANGELOG.md) for what changed recently and
[docs/research/teamtalk-iriun-findings.md](docs/research/teamtalk-iriun-findings.md) for why the design is what it is.

Key facts that are easy to miss:

- **The app is called Tumon.** It is a Windows virtual camera by SkillsHUB Electronics, sister project of
  [Tapis](https://github.com/SkillsHUB-Electronics/tapis) (microphone noise filter). Repository:
  `SkillsHUB-Electronics/tumon`. Default branch: `main`.
- **The goal is that TeamTalk 5 lists and opens the camera** (and also fixes laptop cameras TeamTalk cannot open).
  What TeamTalk needs is **not fully known**. Working model (2026-11-23, from a TeamTalk device-list screenshot,
  user reports and the [device probe](docs/research/probe-results-2026-11-23.md)), in two stages:
  1. **Listing:** TeamTalk lists real Windows cameras (kernel driver, PnP, Media Foundation) such as Iriun and the
     Windows Virtual Camera, and does **not** list pure DirectShow filters (OBS Virtual Camera, recent SplitCam).
  2. **Starting:** a listed camera can still fail to start (the RGB24-only SplitCam Video Driver does). Probably
     TeamTalk needs a format and size it can capture, likely YUY2 at a modest resolution.
  RGB24 was an earlier hypothesis and is refuted for listing. See [ADR 0001](docs/decisions/0001-media-foundation-virtual-camera.md)
  (Media Foundation virtual camera, **Accepted**).
  **Refinement from native-format data (observation 22):** TeamTalk fills its Video Resolution list only from a
  camera's native **YUY2** types. Iriun is the only camera here offering YUY2 natively (32 native types: 16 YUY2 +
  16 RGB24 → 16 list entries). The Surface camera and the Windows Virtual Camera offer only NV12, the SplitCam driver
  only RGB24, so those fall back to "Custom video format". Tumon therefore offers native YUY2 and converts NV12
  from real cameras to YUY2.
- **Native code exists now** (`cpp/`, Milestone 0 test camera): a Media Foundation media source DLL with 12 test
  media types, a host that registers it, a self-test that verifies it without installing anything, and
  `tumon_mfinfo` for native formats. It builds with VS 2022 Build Tools and CMake, **outside OneDrive**. Registering it
  changes the system (HKLM key + `C:\Program Files\Tumon`): only with the user's say-so, always with the uninstall
  script. **Tested in TeamTalk 5.23 (2026-11-23): listed, resolution list filled by itself (3 entries), YUY2 chosen, video shown** (research observations 23 and 24; ADR 0001 accepted). TeamTalk saves the camera as its **list position** (`deviceid`), so cameras appearing or disappearing shift it. **Permanent system camera verified (2026-11-23):** installed with `install-test-camera.ps1 -Permanent` (`tumon_vcam_ctl`), it works with no program running; TeamTalk showed it after a Windows restart, but the list order changed across the restart (Tumon 4 to 0), so a saved position is fragile (observations 26 and 27). **Live pictures work in-process (observation 28):** `tumon_feeder` reads a real camera, converts to YUY2 and feeds the source through a shared-memory channel (`cpp/src/channel`); the real camera is opened only while Tumon Camera is in use. **In TeamTalk the live Surface picture shows and moves (user-confirmed, observation 29).** Run exactly one feeder: a second one is refused (two fight over the camera, 0xC00D3704). **A second TeamTalk client on another PC also sees the video** (user-reported, observation 30). Tumon 0.1.0 is installed on the author's PC by the installer (it replaced the earlier test-camera install).
- **Verify constants and GUIDs against authoritative sources, never memory.** A GUID typed from memory broke the
  first probe; the right one was read from `TeamTalk5.dll`.
- **Milestone 0 passed, but all evidence is from one PC** (Windows 11 build 26300, TeamTalk 5.23, four other cameras).
  Re-check on other PCs and cameras; the [roadmap](ROADMAP.md) lists the follow-ups. Update the research note and
  ADR when new evidence changes the model.
- **Windows 11 only** (build 22000+): `MFCreateVirtualCamera` (ADR 0001) does not exist on Windows 10.
- **Stack:** everything is C++17 + CMake. The app is native Win32 (`cpp/app`, [ADR 0002](docs/decisions/0002-native-win32-app.md),
  the user chose this over the earlier Python + PySide6 plan), the feeder engine is `cpp/src/feeder` (shared with the
  console tool), the installer is Inno Setup (`packaging/windows/installer.iss`; compiled, first install and upgrade
  verified on the author's PC, **uninstall not yet tested**, unsigned, no release published). CI (`.github/workflows/ci.yml`)
  builds on Windows and runs all four CTest tests. Built: camera source DLL, tools, app, installer. Planned: effects, phone camera. Do not describe planned things as existing. App strings go
  into both tables in `cpp/app/strings.h` (English and Bahasa Indonesia).
- **Do not copy code, names or assets from Iriun, OBS or SplitCam.** Study behaviour, write our own.
  Microsoft's VCamSample (MIT) is the permitted reference for the Media Foundation pattern; credit it
  in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) if code is adapted.
- **Accessibility is a requirement, not polish.** See [docs/accessibility.md](docs/accessibility.md).
- **Private information.** Keep personal details (names, emails, local paths, machine names) out of
  commits, docs and issue text. Findings are written with product versions only.
