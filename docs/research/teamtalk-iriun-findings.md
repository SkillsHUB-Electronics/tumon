# Research: how TeamTalk, Iriun, OBS and SplitCam relate (2026-11-23)

Question: why does TeamTalk list Iriun's camera but not OBS Virtual Camera or SplitCam?

All observations are from one Windows 11 PC (build 10.0.26300) and are **static**: registry, device list
and strings in a DLL. Nothing was tested at runtime inside TeamTalk yet.

## Versions examined

| Product | Version |
|---|---|
| TeamTalk 5 (BearWare.dk), 64-bit | 5.23.0.5209 |
| Iriun Webcam | 2.9.1 |
| OBS Studio | 32.2.2 |
| SplitCam | 10.9.2 |

## Observations

| # | Observation | How obtained |
|---|---|---|
| 1 | DirectShow video-input devices registered: **OBS Virtual Camera**, **SplitCam Virtual Camera**. Iriun is **not** among them. | Registry: instances of the video-input device category `{860BB310-5D01-11d0-BD3B-00A0C911CE86}` (native and WOW6432Node) |
| 2 | Iriun appears as PnP device *Iriun Webcam*, class **Camera**, a software device, status OK, plus a *MEDIA* device and a virtual microphone. | `Get-PnpDevice` |
| 3 | Iriun's COM class `{596419B9-519E-495A-9DB9-48E9648A5A56}` has `InprocServer32` pointing at `Iriunv.dll` inside the Windows driver store (`iriunv0.inf`). | Registry |
| 4 | The Iriun installer folder holds the app, an FFmpeg decoder, adb and `devcon`, but no DirectShow filter DLL. | File listing |
| 5 | `TeamTalk5.dll` contains the names `mfplat`, `mfreadwrite`, `mf.dll` and `MFEnumDeviceSources`, and does not contain DirectShow device-enumeration names (`ICreateDevEnum`, `CLSID_VideoInputDeviceCategory`, `strmiids`). The word "DirectShow" appears three times (likely constant or comment text). | Byte-string search of the DLL |
| 6 | Reported by the user: TeamTalk lists Iriun but not OBS Virtual Camera or SplitCam. | User report (not independently re-tested) |

| 7 | Reported by the user (2026-11-23, after the first version of this note): with the NDI plugin for OBS and NDI's Windows webcam component installed, OBS output **is** visible in TeamTalk. Not independently re-tested; NDI is not installed on the examined PC. | User report |
| 8 | Reported by the user (2026-11-23): **older SplitCam versions were visible in TeamTalk; versions from roughly four months earlier (about July 2026) onward are not.** The change happened inside SplitCam (or possibly coincided with a Windows update). Not re-tested. | User report |
| 9 | SplitCam 10.9.2 ships `virtualcam.dll` / `virtualcam_x64.dll` (the DirectShow "SplitCam Virtual Camera") and also a 2021 kernel driver `splitcam_hd_driver` ("SplitCam Virtual Video Driver"), but that driver is class **MEDIA**, not **Camera**, and no SplitCam device is listed in the Camera class. | `pnputil /enum-drivers`, `Get-PnpDevice`, file listing |
| 10 | Reported by the user (2026-11-23): **many recent laptop built-in cameras are not listed in TeamTalk, and some that are listed fail with an error**, which the user suspects is a resolution that is too high or a pixel format/encoding TeamTalk does not handle. Not re-tested. | User report |
| 11 | The examined PC has a Microsoft Surface front camera (USB, with a separate IR camera) and a Logitech C920 (currently unplugged). The Surface camera is a candidate control: a real recent laptop camera. | `Get-PnpDevice` |
| 12 | Reported by the user (2026-11-23): **TeamTalk works with YUY2 video** (written as "YUY"). Not yet confirmed from TeamTalk's own SDK documentation. | User report |
| 13 | `TeamTalk5.dll` contains the format names `YUY2`, `YUYV`, `I420`, `NV12`, `MJPG`, `UYVY`, `YV12` and `RGB32` (`NV12` and `MJPG` included). They sit next to each other in comparison code, so they probably come from a bundled conversion library and **do not show which formats the camera capture path accepts**. | Byte-string search of the DLL |
| 14 | **TeamTalk's own video device list (user screenshot, 2026-11-23)** shows exactly four entries: *Surface Camera Front*, *SplitCam Video Driver*, *Iriun Webcam* and the linked phone's *Windows Virtual Camera*. It does **not** show *OBS Virtual Camera* or *SplitCam Virtual Camera*. The screenshot shows only the dropdown, so it is assumed to be TeamTalk's list; whether each listed camera also *opens* was not shown. | User screenshot |
| 15 | Reported by the user (2026-11-23), after the screenshot: **SplitCam's entry is listed in TeamTalk, but starting it does not work** (the entry is the *SplitCam Video Driver*; whether the SplitCam app was running at the time was not stated). | User report |
| 16 | **TeamTalk 5.23 › Preferences › Video Capture** (user screenshot, 2026-11-23) has: *Video Capture Device*, *Video Resolution* (a dropdown, with a **"…" button for a custom format**), *Image Format* (shown: **YUY2**), a *Test Selected* button, and codec settings (WebM VP8, 256 kbps). With *SplitCam Video Driver* selected the resolution box read **"Custom video format"**. | User screenshot |
| 17 | Exact messages (user screenshots and report): *Test Selected* with SplitCam Video Driver, custom format, YUY2 → **"Failed to initialize video device"**. After the user pressed *Default*, *Test Selected* worked (a video window with the user's picture appeared; which device the Default button selected was not stated). After OK, starting the camera from the main window → **"Video device hasn't been configured properly. Check settings in 'Preferences'"**. The user adds that TeamTalk **cannot detect the resolution of the virtual camera by itself**, and a wrongly typed resolution also errors. | User screenshots and report |
| 18 | **TeamTalk's Video Resolution list for Iriun Webcam** (user screenshot, 2026-11-23): 16 entries, each a size and a frame rate: 640x360, 640x480, 960x540, 1280x720, 1280x960, 1920x1080, 2560x1440 and 3840x2160, each at **FPS 29** and at **FPS 60**, plus "Custom video format" at the end. The Image Format dropdown was hidden behind the list. | User screenshot |
| 19 | **TeamTalk's saved video settings** (`TeamTalk5.ini`, section `[videocapture]`, read 2026-11-23; only that section was read): `deviceid=0`, `resolution=1280x720`, `fps=24/1`, `fourcc=101`. `fourcc=101` is YUY2 (the numbering I recall from the SDK is I420=100, YUY2=101, RGB32=102). The probe shows the Surface camera offers 1280x720 **only at 24 fps** and YUY2 there, so this is very likely the Surface camera's custom setting. | File on the user's PC |
| 20 | Reported by the user (2026-11-23): the **Surface camera needed a "custom" video format** and stopped working when that setting was lost, because the user no longer remembered the values. Even this camera, listed and working, was not filled in automatically. | User report |
| 21 | **Media Foundation video-capture enumeration (`MFEnumDeviceSources`) returns exactly four devices: Surface Camera Front, SplitCam Video Driver, Iriun Webcam and the linked phone's Windows Virtual Camera.** OBS Virtual Camera and SplitCam Virtual Camera are not in it. That is the same four that TeamTalk lists (observation 14). The probe's first attempts failed only because the attribute key GUID I typed from memory was wrong; the correct one, `c60ac5fe-252a-478f-a0ef-bc8fa5f7cad3`, was read from the bytes of `TeamTalk5.dll`, where it sits next to the friendly-name key, the video-capture value (`8ac3587a-…`) and the `IMFMediaSource` interface ID, in the order an enumerate-name-activate routine uses them. | Probe run on the PC; GUID read from `TeamTalk5.dll` |
| 22 | **Native Media Foundation media types of the four cameras** (C++ tool `tumon_mfinfo`, 2026-11-23). **Surface Camera Front: NV12 only** (1280x720 at 24, 30 and 15 fps; 1920x1080; 800x800; 640x480; 640x360). **SplitCam Video Driver: RGB24 only** (27 sizes at 30 and 60 fps). **Iriun Webcam: 16 YUY2 types and 16 RGB24 types**, i.e. 8 sizes (640x360 to 3840x2160) at 30 and 60 fps in each format. **Linked phone (Windows Virtual Camera): NV12 only** (1280x720 at 30). No camera here offers MJPEG. | `tumon_mfinfo` run on the PC |
| 23 | **Milestone 0 test (user screenshots, 2026-11-23): the Tumon test camera works in TeamTalk 5.23.** It is listed as "Tumon Camera (Windows Virtual Camera)". *Video Resolution* filled in **by itself** with three entries: 640x480 FPS 30, 320x240 FPS 30, 1280x720 FPS 30 (plus "Custom video format"). The camera's 12 media types (four formats x three sizes) collapsed into those three entries, one per size. TeamTalk's Video tab showed the test picture (colour bars, moving black line, frame counter). Tumon Camera was the 5th device in Media Foundation's list. | User screenshots, `tumon_mfinfo` |
| 24 | After OK, TeamTalk saved `deviceid=4`, `resolution=640x480`, `fps=30/1`, `fourcc=101` (YUY2), `enable=true` (only that section of the settings file was read). `deviceid=4` is the camera's **position (zero-based) in the Media Foundation list**: Surface 0, SplitCam driver 1, Iriun 2, linked phone 3, Tumon 4. | `TeamTalk5.ini` |
| 25 | Reported by the user with a second screenshot (2026-11-23): **1280x720 also works**; TeamTalk's Video tab then showed the same test picture in a wider (16:9) frame. 320x240 has not been reported yet. | User screenshot |
| 26 | **Permanent camera confirmed in TeamTalk (user report, 2026-11-23).** With the camera installed as a system camera (`tumon_vcam_ctl install`, no host program), TeamTalk 5.23 still had Tumon Camera selected and showed the video **after a Windows restart**. Before that, Windows itself read 60 of 60 frames at 30 fps from it with no host running, at list position 4. | User report, `tumon_mfinfo` |

## Interpretation

- Observations 2 and 3 say Iriun 2.9.1 is a Windows 11 **Media Foundation virtual camera** (software device
  plus a media source COM class in the driver store), not a DirectShow filter.
- Observations 1 and 5 say OBS and SplitCam register DirectShow filters, while TeamTalk enumerates via
  Media Foundation.
- With observation 6 this suggests TeamTalk 5.23 sees **Media Foundation cameras only**, which is why DirectShow-only
  virtual cameras are missing. This corrects our first assumption (that TeamTalk is "old DirectShow").

## Observation 10 strengthens the format explanation

Real laptop cameras are normal Windows cameras that Media Foundation enumerates, yet TeamTalk misses or fails on
many of them. That is hard to explain by "TeamTalk sees only Media Foundation" (explanation (a) below) and fits
"TeamTalk only works with formats and sizes it supports" (explanation (b)): recent cameras often offer only NV12
or MJPEG at high resolutions. **Decisive control test (no tools needed):** if the Surface front camera is *absent*
from TeamTalk's video list while it is present in Windows Settings › Cameras, explanation (a) is ruled out.

Product consequence: Tumon should not only be a virtual camera, it should be a **compatibility adapter**: open
any real camera in whatever format it offers, convert and scale, and present only formats and sizes TeamTalk
accepts (to be established by the probe). That also fixes the laptop-camera problem, not just the virtual-camera one.

## Update: TeamTalk's own list (observation 14) supersedes the RGB24 reading below

Comparing the TeamTalk list with the probe:

| Device | How Windows knows it | Offers RGB24? | In TeamTalk's list |
|---|---|---|---|
| Surface Camera Front | real PnP camera (class Camera) | no (NV12, YUY2) | **yes** |
| SplitCam Video Driver | kernel (WDM) video driver | yes (RGB24 only) | **yes** |
| Iriun Webcam | Media Foundation virtual camera (PnP, class Camera) | yes | **yes** |
| Windows Virtual Camera (linked phone) | Media Foundation virtual camera made by Windows | no (NV12, YUY2) | **yes** |
| SplitCam Virtual Camera 10.9.2 | pure DirectShow filter (`virtualcam*.dll`) | no | no |
| OBS Virtual Camera | pure DirectShow filter | no formats while stopped | no |

What this says:

- **RGB24 is not what decides *listing*.** The Surface camera and the Windows Virtual Camera are listed without it.
- **The split is how the camera is registered.** Everything listed is a real Windows camera (PnP/kernel/Media
  Foundation). Both cameras that are missing are **pure DirectShow filters**. That brings back the first idea
  (TeamTalk enumerates through Media Foundation, as `TeamTalk5.dll` suggested) and it is now backed by a TeamTalk
  screenshot, not only by DLL strings.
- **It explains old versus new SplitCam.** Old SplitCam presumably fed the kernel driver ("SplitCam Video Driver",
  still listed); since about July 2026 it presents its DirectShow filter instead. Not verified.
- **A Media Foundation virtual camera made through `MFCreateVirtualCamera` is accepted by TeamTalk.** Two existing
  examples (Iriun, the Windows Virtual Camera) are listed. That supports ADR 0001.
- **Listing and opening are two separate hurdles (observation 15).** The SplitCam Video Driver is listed but
  does not start. Two candidate reasons, neither tested: (1) no producer feeds that driver any more (recent SplitCam
  uses its DirectShow filter instead), or (2) the driver offers **RGB24 only** and TeamTalk cannot start capture
  with it, which would fit the user's report that TeamTalk uses **YUY2** (Iriun, the Surface camera and the Windows
  Virtual Camera all offer YUY2). That would also explain "detected but error" on laptop cameras (format or
  resolution rejected when starting).
- **Working model, to be tested (revised after observations 16 and 17):**
  1. *Listing* needs a real Windows camera (kernel, PnP or Media Foundation), not a pure DirectShow filter.
  2. TeamTalk then **builds its Video Resolution list from the formats the camera reports that TeamTalk itself
     supports**. When none match, it shows **"Custom video format"** and leaves the user to type size and format
     by hand, which fails if anything is slightly wrong ("Failed to initialize video device"). The SplitCam
     Video Driver offers only RGB24 (27 sizes), so a "no supported format, so custom format" result is exactly
     what we would expect if TeamTalk does not support RGB24.
  3. *Starting* from the main window needs a saved, valid device-plus-format combination ("Video device hasn't
     been configured properly" otherwise).
  From memory (to be checked against the TeamTalk SDK header, which is not installed here), the SDK's pixel formats
  are I420, YUY2 and RGB32; NV12, MJPEG and RGB24 are not among them. That would also explain recent laptop
  cameras that offer only NV12 or MJPEG.
  Tumon must therefore (a) be a real Windows camera and (b) offer formats in TeamTalk's set so the resolution list
  **fills in by itself**, because manual entry is fragile.
- **Observation 18 supports the Media Foundation reading.** The DirectShow view of Iriun in the probe shows 30 fps only,
  yet TeamTalk offers **29 and 60 fps** for the same eight sizes. Frame rates that DirectShow does not report can only
  come from Media Foundation's own media types, so TeamTalk's list is built through Media Foundation. TeamTalk does
  not filter by size (4K is listed), so size is not what makes a camera fail; the **format** is.
  It lists size and frame rate as pairs and the pixel format separately (*Image Format*), so which of Iriun's two
  formats (YUY2 or RGB24) TeamTalk uses is still unknown.
- **Observation 21 settles the listing question: TeamTalk's camera list is Media Foundation's video-capture
  enumeration.** The two lists match device for device, and the one entry type that differs (pure DirectShow
  filters) is absent from both. So a camera appears in TeamTalk if and only if Media Foundation enumerates it,
  as far as this PC shows. This is now supported by three independent facts: the DLL's imports and constants,
  the 29/60 fps entries, and this list match. Still untested: that a Tumon camera registered with
  `MFCreateVirtualCamera` is enumerated the same way (Iriun and the Windows Virtual Camera are, which is strong
  circumstantial evidence).
- **Native formats, now read (observation 22).** The C++ tool activates each enumerated source without trouble; the
  C# probe's `ActivateObject` failure was a bug in that interop code. (Old note follows, superseded:) activating each enumerated source (`ActivateObject`) fails with `0xC00D36E6` in the
  probe, so we still cannot see the Surface camera's native Media Foundation types. TeamTalk evidently can.
- **Conclusion from observation 22: TeamTalk builds its automatic list from the camera's native YUY2 types.**
  Iriun offers 32 native types (16 YUY2, 16 RGB24) but TeamTalk's list for Iriun has exactly 16 entries (8 sizes x 2
  frame rates), so it kept one format and dropped the other. The Surface camera and the linked phone offer only NV12;
  the SplitCam driver offers only RGB24. None of those has a YUY2 type, so none can fill the list, and TeamTalk shows
  "Custom video format". Not yet shown: whether TeamTalk keeps YUY2 or RGB24 for Iriun (the saved setting
  `fourcc=101` is YUY2 and its Image Format box read YUY2). Also: Iriun's 30 fps is probably 30000/1001, which
  TeamTalk shows as "FPS 29" (30 rounded down).
- **Why the custom YUY2 setting worked for the Surface camera (observation 19).** TeamTalk then asks Media Foundation
  for YUY2 from a camera whose native type is NV12, and Windows converts. So a typed format can work where
  the automatic list fails; it simply has to be known and typed. The same custom YUY2 failed for the RGB24-only
  SplitCam driver ("Failed to initialize video device"), cause not established.
- **Consequence for Tumon (the design now rests on data):** (1) be a real Media Foundation camera so it is listed;
  (2) offer **native YUY2 types** at standard sizes so TeamTalk fills its list by itself; (3) for real cameras that
  deliver NV12 (most laptops), the adapter converts to YUY2 before presenting them. Both are testable now with the
  Milestone 0 test camera, which offers YUY2 first.
- **Milestone 0 passed (observations 23 and 24).** A Media Foundation virtual camera that offers native YUY2 is listed
  by TeamTalk, fills its resolution list without any typing, picks YUY2 (`fourcc=101`) and shows video. This confirms
  the two-stage model: real Windows camera for *listing*, native YUY2 for the *automatic list*. ADR 0001 is accepted.
- **New risk: TeamTalk stores the camera as its list position (`deviceid`).** Any camera that appears or disappears
  before ours in the list (a phone linked through Windows Virtual Camera, a session camera such as OBS-style tools,
  an unplugged USB webcam) shifts the index, so the saved choice points at a different camera or at none, which
  matches "Video device hasn't been configured properly" and the earlier Surface trouble. Tumon cannot control
  other cameras, but it can (a) be a stable, always-present *system* camera instead of a session camera, and (b) tell
  the user, in plain text, which entry to re-select after the order changes.
- **A system camera survives restarts (observation 26).** A camera registered with `MFVirtualCameraLifetime_System`
  and `MFVirtualCameraAccess_AllUsers` is listed all the time, and TeamTalk's saved choice still worked after
  restarting Windows. It only stays valid while the cameras *before* it keep their order, which Tumon cannot control.
- **Observations 19 and 20 show why custom formats hurt.** The Surface camera's 720p mode runs only at 24 fps; TeamTalk
  did not offer it in its list, so the user had to find and type `1280x720`, `24` fps and `YUY2` by hand, and forgot
  them. Reason (confirmed by observation 22): TeamTalk fills its list from the camera's *native* Media Foundation types
  (NV12 only for the Surface camera, a format TeamTalk's list does not use), while the DirectShow view in
  the probe shows YUY2 only because Windows converts. Observation 22 confirms it: the Surface camera's native types
  are NV12 only. For Tumon this is the goal: **never need a custom format**.
  The settings also show TeamTalk stores the format as `resolution`, `fps` (a fraction such as `24/1`) and a numeric
  `fourcc`, so a setup helper can state them exactly.
- **Next evidence needed from the user (small and cheap):** the **Image Format** dropdown (open it for Iriun), and the
  **Video Resolution** and **Image Format** lists for **Surface Camera Front**. Also: which device did the *Default*
  button select, and was the SplitCam app running during the failed test? NDI Webcam Input is still unexamined.

## Probe results (2026-11-23) and the RGB24 hypothesis (superseded by the update above)

The [device probe](probe-results-2026-11-23.md) changed the picture again:

- **Every device reported to work offers RGB24** (Iriun: RGB24 + YUY2; the old SplitCam driver: RGB24 only).
- **Every device reported to fail lacks RGB24** (recent SplitCam: NV12/I420/YUY2; Surface laptop camera: NV12/YUY2).
- **OBS Virtual Camera offers no media types at all while not started.**
- The old SplitCam driver offers no YUY2 yet reportedly worked, so "TeamTalk needs YUY2" (observation 12) is less
  likely than "TeamTalk needs RGB24, or captures through RGB24".
- Iriun is also visible as a DirectShow device (Windows bridges it), so "DirectShow vs Media Foundation" no longer
  separates the devices cleanly. ADR 0001 is therefore **not yet justified**; see its status.

This is a correlation across five devices and relies on user reports. The control tests and the Milestone 0 build
(RGB24-only camera, then YUY2-only camera) will decide it.

## Working design assumption

Until tested: present **YUY2 and RGB24** (both are offered by cameras TeamTalk lists; neither is proven required),
at **modest resolutions**
(for example 640×480 and 320×240, 15 to 30 fps), and never offer only NV12 or MJPEG. The adapter converts whatever
the real camera gives (often NV12 or MJPEG at 1080p or more) into that.

## Limits and what would change the conclusion

- **Observation 8 is also unexplained.** If TeamTalk rejected every DirectShow-only camera, old SplitCam (a
  DirectShow filter as far as we know) would never have worked. So the discriminator may be finer than "DirectShow
  vs Media Foundation": what a given SplitCam release changed (pixel formats, resolutions, registration, bitness)
  is the open question. The 2021 driver is unchanged, so the change is more likely in `virtualcam*.dll`.
- **Competing explanations to test**, none confirmed: (a) TeamTalk lists only devices that Media Foundation
  enumerates; (b) TeamTalk drops devices that offer no format it supports; (c) a Windows update changed behaviour.
  A **device probe** (list DirectShow and Media Foundation devices and every media type each offers) run against
  Iriun, OBS, SplitCam, NDI and a real webcam would separate (a) from (b).

- **Observation 7 is unresolved and weakens the Media Foundation-only reading.** Whether NDI's Windows webcam
  component registers as DirectShow, as a Media Foundation camera, or both is unknown (NDI's public docs do
  not say; the NDI SDK does ship a DirectShow filter). Installing NDI Tools on the examined PC and repeating
  observations 1 to 3 would show which kind of device TeamTalk accepts. Until then ADR 0001 stays *Proposed*.

- Older Iriun versions may have used DirectShow; this machine has 2.9.1 only.
- The `MFEnumDeviceSources` name could belong to code that is not on TeamTalk's camera path. The runtime test is the proof.
- Windows' Frame Server can expose some DirectShow cameras to Media Foundation apps; why OBS and SplitCam do
  not appear needs checking (for example pixel format, or the filters being registered only for DirectShow).

## Next step: Milestone 0 test

1. Build a minimal Media Foundation virtual camera that outputs a test pattern.
2. Check it in the Windows Camera app, then in TeamTalk's video device list, then send video to a second client.
3. Update this note and [ADR 0001](../decisions/0001-media-foundation-virtual-camera.md) with the result.

A quick check the maintainer can do today: open TeamTalk's video preferences and compare the device list
with *Settings › Bluetooth & devices › Cameras*. Iriun should be in both; OBS Virtual Camera should be in
neither if the interpretation holds.
