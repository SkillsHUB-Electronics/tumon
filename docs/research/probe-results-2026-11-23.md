# Device probe results, 2026-11-23

Output of [`tools/device-probe`](../../tools/device-probe/README.md) on the examined PC (Windows 11 build 26300.9974,
26H2). The 32-bit and 64-bit builds returned identical device lists. A phone linked through *Windows Virtual
Camera* is shown as `<linked phone>`; the probe hides that name on purpose.

Read with the [findings note](teamtalk-iriun-findings.md). Nothing here says what TeamTalk shows; that comes from
the user's reports and from the control tests listed at the end.

## Update: what TeamTalk actually lists

The user's TeamTalk video device list (screenshot) shows **Surface Camera Front, SplitCam Video Driver, Iriun Webcam
and the linked phone's Windows Virtual Camera**, and does **not** show OBS Virtual Camera or SplitCam Virtual Camera.
So the table's last column and the "RGB24" reading below were too hasty: the Surface camera and the Windows Virtual
Camera are listed without RGB24. What the missing two have in common is that they are **pure DirectShow filters**,
while everything listed is a real Windows camera (kernel driver, PnP, or Media Foundation). See the
[findings](teamtalk-iriun-findings.md#update-teamtalks-own-list-observation-14-supersedes-the-rgb24-reading-below).
Whether the listed cameras also open and show video is still unreported.

## Update 2: the Media Foundation list now works

After correcting the attribute key GUID (it was wrong in the first version of the probe; the right one was read
from `TeamTalk5.dll`), `MFEnumDeviceSources` returns **Surface Camera Front, SplitCam Video Driver, Iriun Webcam and
the linked phone's Windows Virtual Camera**: exactly the list TeamTalk shows. OBS Virtual Camera and SplitCam
Virtual Camera are not enumerated. Reading each source's *native* formats still fails at `ActivateObject`
(`0xC00D36E6`), so the tables below remain the DirectShow view.

## Update 3: native formats (C++ tool, `tumon_mfinfo`)

The C# probe could not read native formats (a bug in its interop code). The C++ tool can. Native Media Foundation
types, which are what TeamTalk reads (a phone linked through Windows Virtual Camera is shown as `<linked phone>`):

| Device | Native types |
|---|---|
| Surface Camera Front | **NV12 only**: 1280x720 @24/30/15, 1920x1080 @30/15, 800x800 @30, 640x480 @30/15, 640x360 @30/15 |
| SplitCam Video Driver | **RGB24 only**: 27 sizes (320x180 up to 2048x1536) at 30 and 60 fps |
| Iriun Webcam | **YUY2** (16 types) **and RGB24** (16 types): 640x360, 640x480, 960x540, 1280x720, 1280x960, 1920x1080, 2560x1440, 3840x2160, each at 30 and 60 fps |
| `<linked phone>` (Windows Virtual Camera) | **NV12 only**: 1280x720 @30 |

Only Iriun offers YUY2 natively, and only Iriun fills TeamTalk's resolution list by itself (16 entries = its 16 YUY2
types). The DirectShow tables below show formats Windows converts to (for example YUY2 for the Surface camera),
which is why they differ from this table. See the [findings](teamtalk-iriun-findings.md).

## Summary

| API | Device | Formats (number of media types) | Largest size | YUY2 | RGB24 | NV12 or MJPG only |
|---|---|---|---|---|---|---|
| DirectShow | SplitCam Video Driver (2021 kernel driver) | RGB24 (27) | 2048x1536 | no | **yes** | no |
| DirectShow | Iriun Webcam | RGB24 (8), YUY2 (8) | 3840x2160 | yes | **yes** | no |
| DirectShow | Surface Camera Front (laptop camera) | NV12 (5), YUY2 (5) | 1920x1080 | yes | no | no |
| DirectShow | `<linked phone>` (Windows Virtual Camera) | NV12 (1), YUY2 (1) | 1280x720 | yes | no | no |
| DirectShow | OBS Virtual Camera (not started) | none | none | no | no | no formats at all |
| DirectShow | SplitCam Virtual Camera (10.9.2) | I420 (19), NV12 (19), YUY2 (19) | 3840x2160 | yes | no | no |

## What stands out (written before the TeamTalk screenshot; point 4 and the control-test table are superseded)

1. **OBS Virtual Camera offers no media types while it is not started.** A camera with nothing to negotiate is a
   natural reason for TeamTalk to ignore it. (The probe has not yet been run with OBS's virtual camera started.)
2. **The recent SplitCam virtual camera and the Surface laptop camera offer YUY2, NV12 and I420 but no RGB24.**
   The recent SplitCam is reported *not* to work in TeamTalk and many laptop cameras are reported to fail.
3. **Iriun offers RGB24 and YUY2. The old-style SplitCam driver offers RGB24 only.** Iriun is reported to work, and
   older SplitCam versions are reported to have worked.
4. So far, **every device reported to work offers RGB24, and every device reported to fail lacks it.** That is a
   pattern, not proof; it fits the report that old SplitCam worked even though its driver offers no YUY2, which
   makes "TeamTalk needs YUY2" less likely than "TeamTalk needs RGB24 (or uses it as its capture format)".
5. Iriun also shows up here as a *DirectShow* device. Windows bridges its camera into DirectShow, so
   "DirectShow versus Media Foundation" is no longer a clean way to tell the devices apart.
6. Sizes up to 4K are offered by Iriun and the new SplitCam; resolution alone does not separate working from
   failing devices.

## Not working in the probe

- The Media Foundation enumeration call `MFEnumDeviceSources` failed with `0xC00D36E6` (attribute not found), so
  there is no native Media Foundation list. The Windows *Camera* device class (Iriun, Surface Camera Front,
  Surface IR Camera Front, plus a Logitech C920 that is unplugged) is the closest stand-in. The SplitCam driver is
  class *MEDIA*, not *Camera*, and OBS and the recent SplitCam are not PnP devices at all.

## Details

### SplitCam Video Driver (DirectShow)
- **RGB24**: 320x180@30, 320x240@30, 400x225@30, 400x300@30, 512x384@30, 640x360@30, 640x480@30, 800x600@30, 960x540@30, 1024x768@60, 1280x720@60, 1280x960@60, 1600x900@60, 1400x1050@60, 1600x1200@60, 1920x1080@60, 1920x1440@60, 2048x1536@60

### Iriun Webcam (DirectShow)
- **YUY2**: 640x360@30, 640x480@30, 960x540@30, 1280x720@30, 1280x960@30, 1920x1080@30, 2560x1440@30, 3840x2160@30
- **RGB24**: 640x360@30, 640x480@30, 960x540@30, 1280x720@30, 1280x960@30, 1920x1080@30, 2560x1440@30, 3840x2160@30

### Surface Camera Front (DirectShow)
- **YUY2**: 640x360@30, 640x480@30, 800x800@30, 1280x720@24, 1920x1080@30
- **NV12**: 640x360@30, 640x480@30, 800x800@30, 1280x720@24, 1920x1080@30

### `<linked phone>` (Windows Virtual Camera) (DirectShow)
- **YUY2**: 1280x720@30
- **NV12**: 1280x720@30

### OBS Virtual Camera (DirectShow)
- (no media types reported)

### SplitCam Virtual Camera (DirectShow)
- **NV12, I420, YUY2**, each: 320x180, 320x240, 400x225, 400x300, 512x384, 640x360, 640x480, 800x600, 960x540, 1024x768, 1280x720, 1280x960, 1600x900, 1400x1050, 1600x1200, 1920x1080, 1920x1440, 2048x1536, 3840x2160, all at 30 fps

## Control tests that would settle it (cheap, from the user)

Open TeamTalk's video device list and note which of these appear. The pattern above predicts the right-hand column.

| Device | Offers RGB24? | Predicted in TeamTalk if "RGB24 is required" |
|---|---|---|
| Iriun Webcam (phone connected or not) | yes | listed |
| SplitCam Video Driver (if TeamTalk lists it) | yes | listed |
| Surface Camera Front | no | **not listed, or fails** |
| SplitCam Virtual Camera | no | not listed |
| OBS Virtual Camera, started | unknown (probe again) | depends on its types once started |
| Windows Virtual Camera (linked phone) | no | not listed |

A decisive build test follows: a minimal virtual camera that offers **RGB24 only** (Milestone 0), then **YUY2 only**.
