# Trying the Tumon test camera

The test camera shows the Tumon logo with a turning circle, offered in YUY2, NV12, RGB32 and I420 at 640x480,
320x240 and 1280x720. It started as a colour-bar test picture, which remains only as the fallback if the artwork
cannot be loaded. It exists to answer one question: **does TeamTalk list it, fill its
resolution list by itself, and show the picture?**

> Registering a camera changes the system (a registry key under HKEY_LOCAL_MACHINE and a folder in Program Files).
> Both are removed by `uninstall-test-camera.ps1`. Windows 11 only.

## 1. Build (once)

See [docs/building.md](../../docs/building.md). The scripts look for `tumon_source.dll` in `cpp\build\Release` or in
`%USERPROFILE%\build\tumon\Release`.

## 2. Check the DLL without installing anything

```powershell
& "$env:USERPROFILE\build\tumon\Release\tumon_selftest.exe" "$env:USERPROFILE\build\tumon\Release\tumon_source.dll"
```

Expect `RESULT: all media types pass.`

## 3. Install (Administrator PowerShell)

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\test-camera\install-test-camera.ps1
```

## 4. Start the camera (normal PowerShell, leave the window open)

```powershell
& "$env:USERPROFILE\build\tumon\Release\tumon_vcam_host.exe"
```

Press Enter in that window to stop the camera. Use `--seconds 60` to stop after a minute on its own.

### Permanent camera instead of steps 3 and 4 (recommended for daily use)

TeamTalk saves the camera as its **position in Windows' camera list**, so a camera that comes and goes (the host
program above) can break the saved choice. With `-Permanent` the camera is a system camera for all users: it is
listed all the time, even with no program running and after restarts. Administrator PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\test-camera\install-test-camera.ps1 -Permanent
```

Check at any time (normal PowerShell): `& "$env:ProgramFiles\Tumon\tumon_vcam_ctl.exe" status` prints `present` or
`absent`. `uninstall-test-camera.ps1` removes the permanent camera first, then everything else.

## 5. Test

- Windows **Camera** app: "Tumon Camera" should show the Tumon logo and turning circle.
- **TeamTalk 5 › Preferences › Video Capture**: is "Tumon Camera" in the device list? Does *Video Resolution* fill in
  by itself (not just "Custom video format")? What does the *Image Format* dropdown offer? Does *Test Selected* show
  the picture? Does starting video from the main window work?
- Check from a terminal what Windows sees: `tumon_mfinfo.exe --read "Tumon" 60`.

## 6. Uninstall (Administrator PowerShell, after stopping the host)

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\test-camera\uninstall-test-camera.ps1
```

## Show a real camera instead of the logo picture (feeder)

After installing the **current** build (run `install-test-camera.ps1 -Permanent` again as administrator: it updates the
DLL and restarts the Frame Server for a moment), start the feeder in a normal PowerShell window and leave it open:

```powershell
& "$env:USERPROFILE\build\tumon\Release\tumon_feeder.exe" --device "Surface Camera Front"
```

`--list` shows the camera names (for a USB webcam use its name, or `--index N`). Then select Tumon Camera in
TeamTalk. The feeder opens the real camera only while Tumon Camera is in use and closes it afterwards. Without a
feeder, Tumon Camera shows the logo picture. Ctrl+C stops the feeder.

## Test with a second TeamTalk client (does someone else see the video?)

This is the test that matters in real use. You need a TeamTalk server both clients can join, and a second client on
another device (another PC, or the TeamTalk app on a phone). Do not point other people's cameras or faces at a
public server without their consent.

1. Client A (this PC): feeder running (one only), TeamTalk connected, in a channel, Tumon Camera selected,
   video transmission **on** (the camera button in the toolbar, or *Me › Enable Video Transmission*).
2. Client B (other device): join the same channel and open the **Video** tab.
3. Record, for each size you try (640x480, 320x240, 1280x720):
   - B sees A's video at all? (and A's own preview shows it)
   - colours right (no green or pink cast), picture upright, shape right
   - smooth or choppy, and roughly how many seconds of delay
   - feeder window: pictures per second and "1 program(s) using Tumon Camera"
4. Compare once with a camera TeamTalk lists natively (for example Iriun) if you have one, to see whether Tumon looks
   different to the receiver. TeamTalk encodes with the codec and bitrate set in *Video Capture* (default VP8, 256 kbps).

## If something goes wrong

- Uninstall says `Access to the path ... tumon_source.dll is denied`: the Windows Frame Server still has the DLL
  loaded. The registration is already removed; the script stops the Frame Server for a moment, deletes the DLL and
  starts it again. Close TeamTalk and other camera apps first. A restart of Windows also releases the file.
- `MFCreateVirtualCamera failed`: the DLL is not registered. Run step 3 again.
- `Start failed ... access denied`: the Frame Server cannot read the DLL. It must be in `C:\Program Files\Tumon`,
  not under a user profile; step 3 does this.
- No picture in an app: close and reopen the app (many list cameras only at start), then check step 5's
  `tumon_mfinfo.exe` line.
