# Contributing to Tumon

Thanks for helping. Tumon is small and early; the most valuable contributions right now are test
reports (does it show up in your app?) and accessibility feedback.

## Before you start

1. Read the [README](README.md), [docs/architecture.md](docs/architecture.md) and the
   [roadmap](ROADMAP.md).
2. For anything non-trivial, open an issue first so we agree on the approach.
3. Read the [code of conduct](CODE_OF_CONDUCT.md).

## Workflow

- Default branch is `main`. Create a feature branch (`feat/...`, `fix/...`, `docs/...`) and open a pull request.
- Keep a change focused: one topic per pull request.
- Add a [CHANGELOG](CHANGELOG.md) entry under `[Unreleased]` (Added, Changed, Fixed, Removed).
- Update the docs that describe what you changed. Mark unimplemented things as *planned*.
- Commit messages: short imperative subject (`Add shared-memory ring buffer`), details in the body.

## Standards

- **Languages:** native parts in C++17 (CMake), app in Python 3.10+ (PySide6). Details in
  [docs/building.md](docs/building.md).
- **Strings:** English and Bahasa Indonesia together.
- **Accessibility:** keyboard path and accessible name for every control
  ([docs/accessibility.md](docs/accessibility.md)).
- **Tests:** add or update tests, and say in the pull request which you ran and which you could not
  (hardware-dependent tests are noted, not skipped silently).
- **Decisions:** hard-to-reverse choices get an ADR in [docs/decisions/](docs/decisions/).
- **No third-party code without a compatible license** and an entry in
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Do not copy code, names or assets from Iriun,
  OBS, SplitCam or any other product.

## Reporting a problem

Include: Windows build, Tumon version, the app that cannot see or use the camera (with version), the
camera source, and what you expected. Do not attach video that shows other people.
Security problems go through [SECURITY.md](SECURITY.md), not public issues.

## License

By contributing you agree that your work is released under the [MIT license](LICENSE).
