# Troubleshooting

> Tumon is an early preview. This page lists checks to try; some have not been needed yet. Users and support can
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

- Start a source in the Tumon app. With no source running, the camera shows the fallback picture on purpose: the
  Tumon logo with a turning circle (version 0.1.0 shows colour bars instead).
- Another app may be using the physical webcam exclusively; close it, or choose a different source.

## The picture is delayed or choppy

- Lower the resolution or frame rate in Tumon. TeamTalk sends small video anyway.
- For a phone source, prefer USB or a 5 GHz Wi-Fi network.

## Colours look wrong on someone's phone (orange looks purple, red and blue swapped)

Seen with the TeamTalk phone app, while the same video was correct on a laptop receiver. Test: press Stop in Tumon
(or stop the feeder) so Tumon Camera shows its fallback picture, which is blue (logo and waves along the bottom),
and look at the receiver. If a laptop shows it blue and the phone shows it orange or brown, the phone app is swapping
red and blue when it draws the picture; Tumon's output is correct (the format conversion is unit-tested). With
version 0.1.0 the fallback is colour bars (white, yellow, cyan, green, magenta, red, blue from the left); a swapped
receiver shows white, cyan, yellow, green, magenta, blue, red. Check
whether the same phone shows other cameras the same way, and report it to the TeamTalk developers with the app
version. Do not try to fix it in Tumon: swapping the colours there would break every correct receiver.

## Uninstall

Use *Apps › Installed apps › Tumon › Uninstall*. This removes the virtual camera. If a camera named
Tumon still appears afterwards, report it with your Windows build.

## Still stuck

Open an issue using the bug-report form and include the Windows build, the Tumon version, the app and
its version, and the source you used.
