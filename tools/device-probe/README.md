# Device probe

Read-only diagnostic. Lists the video capture devices that Windows exposes through **DirectShow** and **Media
Foundation**, and every media type (pixel format, resolution, frame rate) each one offers. It does not register,
install or change anything. Opening a device briefly to read its formats may light a camera LED.

Why it exists: see [docs/research/teamtalk-iriun-findings.md](../../docs/research/teamtalk-iriun-findings.md).
First results: [docs/research/probe-results-2026-11-23.md](../../docs/research/probe-results-2026-11-23.md).

## Build and run

Needs nothing beyond Windows: it is C# 5 and compiles with the `csc.exe` that ships with .NET Framework.

```powershell
cd "<folder where you cloned tumon>"
powershell -ExecutionPolicy Bypass -File tools\device-probe\build.ps1
tools\device-probe\bin\probe-x64.exe > probe-result.md
tools\device-probe\bin\probe-x86.exe > probe-result-x86.md
```

Run both builds: a 32-bit process only sees 32-bit filters and a 64-bit process only 64-bit ones, which matters for
virtual cameras that register only one of them.

## Output

Markdown: a summary table (device, formats, largest size, whether YUY2 is offered, whether only NV12 or MJPG is
offered) followed by per-device details. Phones linked through *Windows Virtual Camera* are named after their owner,
so the probe prints `<linked phone>` instead.

> **Superseded for Media Foundation:** use `tumon_mfinfo.exe` (C++, in `cpp/`), which reads each camera's native
> formats. This C# tool is kept for the DirectShow view (what Windows converts to) and still has the interop bug
> described below in its Media Foundation part.

## Known limits

- The Media Foundation **device list** works (an earlier failure came from a wrong attribute key GUID, now fixed). But
  reading each device's **native formats** still fails at `ActivateObject` with `0xC00D36E6` (attribute not found),
  both inside an assistant session and from an elevated terminal. So the Media Foundation rows show the device but no
  formats. The DirectShow rows show the formats Windows converts to, not the camera's native Media Foundation types,
  which are what TeamTalk appears to read. Fixing this is on the to-do list (ideas: create the source with
  `MFCreateDeviceSource` and an explicit symbolic-link attribute; use a source reader).
- A virtual camera that only offers formats while its producer runs (OBS Virtual Camera) reports no formats when
  idle. Start the producer, then probe again.
