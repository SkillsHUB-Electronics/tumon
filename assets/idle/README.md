# Idle picture

`tumon-idle.png` is the artwork the maintainer supplied for the picture Tumon Camera shows while no source is running.

The camera does not use this file directly. `python tools/make_idle.py` (needs Pillow) paints out the drawn spinner,
scales the picture to 1280x720 and writes `cpp/src/source/idle.png`, which is built into the camera DLL. The camera
then draws its own turning circle in the same place; if the artwork changes, check the spinner position constants in
both `tools/make_idle.py` and `cpp/src/source/idle_screen.cpp`.
