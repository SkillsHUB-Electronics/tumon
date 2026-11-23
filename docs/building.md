# Building from source

The native part (the Media Foundation camera and its tools) builds today. The Python app does not exist yet.

## Toolchain

| Need | For | Notes |
|---|---|---|
| Windows 11 (build 22000+) | Running and testing the virtual camera | Windows 10 has no `MFCreateVirtualCamera` |
| Visual Studio 2022 Build Tools, "Desktop development with C++" workload, with the Windows 11 SDK (10.0.26100 or newer) | The DLL and tools | Includes CMake. Install: `winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--passive --norestart --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"` (administrator) |
| CMake 3.24+ | Native build | The copy inside the Build Tools is enough |
| Python 3.12 | App and tests (not yet used) | Create the virtual environment **outside** OneDrive, for example under `%USERPROFILE%\.venvs\tumon` |
| Inno Setup 6 | Installer (not yet used) | Same as Tapis |
| Android Studio + adb | Phone app (Milestone 3) | |
| A code-signing certificate | Release builds | So Windows and antivirus accept the DLL and installer |

## Build the native code

Build **outside OneDrive**: its sync can lock object files and make the compiler hang. Replace the paths as needed
(the CMake path below is the one inside Build Tools).

```powershell
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S cpp -B "$env:USERPROFILE\build\tumon" -G "Visual Studio 17 2022" -A x64
& $cmake --build "$env:USERPROFILE\build\tumon" --config Release
```

Outputs are in `%USERPROFILE%\build\tumon\Release`: `tumon_source.dll`, `tumon_vcam_host.exe`, `tumon_selftest.exe`,
`tumon_mfinfo.exe`, `test_frame_generator.exe`. What each is: [cpp/README.md](../cpp/README.md).

## Tests

```powershell
& "$env:USERPROFILE\build\tumon\Release\test_frame_generator.exe"
& "$env:USERPROFILE\build\tumon\Release\tumon_selftest.exe" "$env:USERPROFILE\build\tumon\Release\tumon_source.dll"
```

or both through CTest: `ctest --test-dir "$env:USERPROFILE\build\tumon" -C Release --output-on-failure`.
`tumon_selftest` loads the DLL directly, so it needs neither registration nor administrator rights. Neither test
needs a camera or TeamTalk; testing in TeamTalk is manual (see [tools/test-camera](../tools/test-camera/README.md)).

## Diagnostic tools

- `tools/device-probe` (C#, built with the compiler that ships with Windows): DirectShow device and format list.
- `tumon_mfinfo.exe` (C++): Media Foundation device list with **native** formats; `--read NAME` reads frames.

## Testing a virtual camera safely

Registering a virtual camera changes the system (a registry key in HKEY_LOCAL_MACHINE and a folder in Program Files).
Always keep the matching unregister step at hand, and never leave a half-registered camera behind.
`tools/test-camera/uninstall-test-camera.ps1` undoes everything the install script does.
