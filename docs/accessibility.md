# Accessibility

TeamTalk is widely used by blind and low-vision people, so Tumon's interface is held to these
requirements from the first version. They are acceptance criteria, not polish.

## Requirements

1. **Everything works with the keyboard alone.** Logical Tab order, no keyboard traps, visible focus,
   shortcuts for start/stop and for choosing the source, listed in the app.
2. **Every control has an accessible name and role** (UI Automation), including sliders (with value
   and unit), toggles (with state) and the device list.
3. **Status is announced, not only shown.** Source connected or lost, camera started or stopped, and
   errors use accessible live regions or notifications, not colour or an icon alone.
4. **No information by colour alone**; contrast of at least 4.5:1 in light and dark themes; respects
   Windows high-contrast mode and text scaling up to 200%.
5. **A live preview is optional.** The app must be fully usable without looking at the preview;
   anything the preview tells you (is the camera on, is anyone in frame) is also available as text.
6. **Plain messages.** Errors say what happened and what to do, in English and Bahasa Indonesia, with a
   button to the relevant Windows setting where one exists (same approach as Tapis's microphone help).
7. **Installer and uninstaller** are keyboard- and screen-reader-operable.

## How to test

- Narrator (built in): walk through every screen with the keyboard only.
- NVDA (free) as a second reader; JAWS when a licensed machine is available.
- Windows high-contrast themes and 200% text scaling.
- Accessibility Insights for Windows to inspect the UI Automation tree.
- Ask blind TeamTalk users to try each milestone build; their reports outrank feature requests.

## Known risks

- Qt's accessibility support is good but custom-drawn widgets (spectrum-style plots, sliders) need
  explicit accessible names and values. Tapis's custom widgets must be checked before reuse.
