# 0002: Build the app as a native Windows program in C++

- Status: Accepted (user's choice, 2026-11-27)
- Date: 2026-11-27

## Context

The plan was to follow Tapis: a Python app with PySide6. Neither PySide6 nor PyInstaller is installed on the
development PC, the camera code and the feeder are already C++, and TeamTalk is widely used by blind and low-vision
people, so the interface must work well with screen readers. The goal now is a first release soon.

## Decision

`Tumon.exe` is a native Win32 program: one dialog made of standard controls, a notification-area icon, and the
feeder engine (`cpp/src/feeder`) on its own thread. English and Bahasa Indonesia strings live in `cpp/app/strings.h`.
Status changes raise a UI Automation notification so screen readers announce them.

## Consequences

- Standard controls give correct roles and names to Narrator, NVDA and JAWS (checked through MSAA).
- No runtime to bundle: small download, one engine shared by the app and the console tool.
- Tumon looks and is built differently from Tapis; shared UI code between them is not possible.
- Richer features (live preview, effects with many settings) are more work than with Qt. Revisit if needed.
