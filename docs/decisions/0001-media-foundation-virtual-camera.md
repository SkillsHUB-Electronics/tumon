# 0001: Use a Media Foundation virtual camera, not a DirectShow filter

- Status: **Accepted (2026-11-23)**: the Milestone 0 test camera was listed by TeamTalk 5.23, filled its resolution list by itself and showed video (research observation 23).
- Date: 2026-11-23

> **Update 5 (2026-11-23):** the camera was also installed as a **system-lifetime** camera for all users and TeamTalk
> showed it after a Windows restart. Caveat: the camera list order changed across the restart (position 4 to 0), so
> TeamTalk's position-based saved choice is fragile; see research observation 27.
>
> **Update 4 (2026-11-23, Milestone 0 passed; this ADR is now Accepted):** TeamTalk 5.23 lists the Tumon test camera,
> fills *Video Resolution* automatically (640x480, 320x240, 1280x720 at FPS 30), picks YUY2 and shows the picture.
> Still to verify: that another TeamTalk client receives the video, and a camera that survives restarts (system
> lifetime) because TeamTalk saves the camera by its list position.
>
> **Update 3 (2026-11-23, native formats and a working test camera):** the C++ tool `tumon_mfinfo` shows TeamTalk's list
> is built from native YUY2 types of Media Foundation cameras (only Iriun has them), and a Media Foundation test camera
> now builds and passes a source-reader self-test. Still *Proposed* until TeamTalk lists and starts that camera
> (Milestone 0, step "register the test camera").
>
> **Update 2 (2026-11-23, TeamTalk device-list screenshot):** TeamTalk lists Iriun and
> the Windows Virtual Camera (both Media Foundation virtual cameras), the Surface camera and the SplitCam kernel
> driver, and does not list OBS or the recent SplitCam (pure DirectShow filters). That supports this decision again.
> It is still *Proposed* until a Tumon test camera is listed and opens in TeamTalk (Milestone 0).
>
> **Earlier update 2026-11-23:** user reports and the [device probe](../research/probe-results-2026-11-23.md) weaken
> the case for this decision. Old SplitCam worked, NDI works, Iriun is also visible as a DirectShow device, and the
> devices that fail share a missing RGB24 format rather than a missing Media Foundation registration. Treat this
> ADR as **unproven** until Milestone 0 shows what TeamTalk actually needs; the choice between a Media Foundation
> and a DirectShow implementation may turn out to matter less than the formats offered.

## Context

Tumon must appear in TeamTalk 5's video device list. The initial assumption was that TeamTalk uses
legacy DirectShow and that Iriun Webcam stays visible because it ships a DirectShow filter.

Measurements on one Windows 11 PC (details in
[the research note](../research/teamtalk-iriun-findings.md)) contradict that assumption:

- Iriun Webcam 2.9.1 is **not** in the DirectShow video-input category. It is a PnP device of class
  *Camera* (a software device) whose COM class is `Iriunv.dll` in the Windows driver store. That is how a
  Media Foundation virtual camera looks.
- OBS Virtual Camera and SplitCam Virtual Camera **are** registered as DirectShow filters, and the user
  reports TeamTalk does not list them.
- TeamTalk 5.23's `TeamTalk5.dll` references `mfplat`, `mfreadwrite`, `mf.dll` and `MFEnumDeviceSources`.

The three observations fit one explanation: TeamTalk enumerates cameras through Media Foundation, so
Media Foundation cameras appear and DirectShow-only ones do not. This is inferred from static analysis and
the user's report; it has not yet been tested at runtime.

## Decision

Build Tumon's virtual camera with `MFCreateVirtualCamera` and a custom `IMFMediaSource` DLL (pattern from
Microsoft's VCamSample), not as a DirectShow filter.

## Consequences

- **Windows 11 (build 22000+) only.** `MFCreateVirtualCamera` does not exist on Windows 10. Accepted: the
  target user runs Windows 11.
- The DLL runs inside the Frame Server service, so it must be small, hardened and signed. Frames arrive
  through a shared-memory channel treated as untrusted ([architecture](../architecture.md)).
- Apps that only enumerate DirectShow cameras will not see Tumon unless Windows' Frame Server also exposes
  the camera to them (to be checked in Milestone 0). A DirectShow fallback can be added later by a new ADR.
- **Revisit** if the Milestone 0 test shows TeamTalk does not list a minimal Media Foundation camera. In
  that case this record is superseded and the DirectShow format investigation (pin media types, bitness)
  becomes the plan again.
