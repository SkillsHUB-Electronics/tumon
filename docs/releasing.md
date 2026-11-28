# Releasing

1. **Bump the version** in `pyproject.toml` and the Python package `__init__` (both once they exist).
   The installer reads the version from `pyproject.toml`.
2. **Update [CHANGELOG.md](../CHANGELOG.md)**: move the *Unreleased* entries under the new version with
   today's date, and add the compare link at the bottom.
3. **Merge** the pull request into `main`. CI must be green.
4. **Test on a clean Windows 11 machine**: install, check Tumon Camera appears in the Windows Camera app and
   in TeamTalk, uninstall, check it is gone. Record anything unexpected in the changelog.
5. **Publish a GitHub release** with tag `vX.Y.Z` on `main`, title `Tumon vX.Y.Z`, and the changelog
   section as the notes (in English). Releases are published by a maintainer, not by agents.
6. The installer and DLL must be **code-signed** before they are attached.

## Building the installer

```powershell
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake --build "$env:USERPROFILE\build\tumon" --config Release
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" "/DBuildDir=$env:USERPROFILE\build\tumon\Release" packaging\windows\installer.iss
```

The version is `0.1.0` in `cpp/CMakeLists.txt`, `cpp/app/resource.h`, `cpp/app/app.manifest` and
`packaging/windows/installer.iss`: change them together.

## Checklist for the first release

- Install on a Windows 11 PC that never had Tumon; start Tumon; choose a camera; see it in TeamTalk.
- Install the same version again (upgrade path), then uninstall and check that the camera, the files and the "run at
  sign-in" entry are gone.
- Release notes state: unsigned (SmartScreen warns), the frame channel can be written by other programs of signed-in
  users, the TeamTalk phone app swaps red and blue, some lag on phones.

## Versioning

[Semantic Versioning](https://semver.org/): a new feature bumps the minor version (0.1.0 → 0.2.0), a fix
bumps the patch version (0.1.0 → 0.1.1). 1.0.0 will mark the end of the beta. The latest release is `0.1.0` (preview)
.
