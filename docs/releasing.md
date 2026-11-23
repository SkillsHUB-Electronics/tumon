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

## Versioning

[Semantic Versioning](https://semver.org/): a new feature bumps the minor version (0.1.0 → 0.2.0), a fix
bumps the patch version (0.1.0 → 0.1.1). 1.0.0 will mark the end of the beta. The repository is at `0.0.0`
(documentation only).
