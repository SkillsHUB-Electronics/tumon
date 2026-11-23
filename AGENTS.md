# Instructions for AI agents

See also [CLAUDE.md](CLAUDE.md) for the key facts. Read [CONTRIBUTING.md](CONTRIBUTING.md) and
[docs/architecture.md](docs/architecture.md) first. The rules there apply to agents too. The points
that are easy to miss:

- **Default branch** is `main`. Work on a feature branch and open a pull request. Agents do not push
  to `main`, create tags or publish releases; a maintainer does.
- **Record what changed.** Every change gets a [CHANGELOG](CHANGELOG.md) entry under `[Unreleased]`, so
  the maintainer's other devices and the next agent session know what happened without reading the diff.
- **Docs match reality.** Mark anything not implemented as *planned*. When behaviour changes, update
  [README.md](README.md), the relevant file in [docs/](docs/README.md) and the [roadmap](ROADMAP.md) in
  the same change.
- **Decisions are written down.** A choice that is hard to reverse (camera API, IPC, phone protocol,
  installer) gets a numbered record in [docs/decisions/](docs/decisions/) using the template there.
- **Research is evidence.** Findings go in [docs/research/](docs/research/) with the tool or command used
  and the product versions. Say whether a finding was observed at runtime or only inferred.
- **Two languages.** Every user-visible string goes into both English and Bahasa Indonesia resource files
  (same convention as Tapis).
- **Accessibility.** Every control needs a keyboard path and an accessible name; see
  [docs/accessibility.md](docs/accessibility.md). Test with Narrator at minimum.
- **Legal files.** `LICENSE`, `PRIVACY.md` and `THIRD_PARTY_NOTICES.md` change only together with what
  they describe. New data leaving the PC must be added to the privacy policy first.
- **Native code.** The virtual camera runs inside the Windows Frame Server service. Keep it small, no
  network access, no logging of frames, and never crash on bad input from the shared-memory channel.
- **Machine changes.** Registering a virtual camera or installing drivers changes the user's system.
  Do not do it without the user's say-so, and always provide the matching uninstall step.
- **Report only what ran.** Say which tests you ran and which you could not run (for example anything
  needing a physical camera, a phone or TeamTalk), and rely on CI for the rest.
- **No copying from other products.** See the note in [CLAUDE.md](CLAUDE.md).
