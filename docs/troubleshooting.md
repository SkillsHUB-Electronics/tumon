# Troubleshooting

> Tumon is not released yet. This page lists the checks planned to be needed, so users and support can
> use them from day one. It will grow from real reports.

## "Tumon Camera" is not in my app's camera list

1. Windows 11 is required (build 22000 or newer). Run `winver` to check.
2. Open the Windows **Camera** app and look for Tumon Camera. If it is missing there, the camera is not
   registered: reinstall Tumon or use *Repair* in the installer.
3. Close and reopen the app (many apps list cameras only at start). In TeamTalk, reopen
   *Preferences › Video* after Tumon is running.
4. Settings › Privacy & security › Camera: *Let desktop apps access your camera* must be on.
5. Antivirus or "controlled folder" tools can block the camera component; check their log.

## The picture is black or shows the fallback image

- Start a source in the Tumon app. With no source running, the camera shows the fallback picture on purpose.
- Another app may be using the physical webcam exclusively; close it, or choose a different source.

## The picture is delayed or choppy

- Lower the resolution or frame rate in Tumon. TeamTalk sends small video anyway.
- For a phone source, prefer USB or a 5 GHz Wi-Fi network.

## Uninstall

Use *Apps › Installed apps › Tumon › Uninstall*. This removes the virtual camera. If a camera named
Tumon still appears afterwards, report it with your Windows build.

## Still stuck

Open an issue using the bug-report form and include the Windows build, the Tumon version, the app and
its version, and the source you used.
