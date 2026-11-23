# Security policy

## Supported versions

Tumon has no release yet. Once released, only the latest version receives security fixes.

## Reporting a vulnerability

Please do **not** open a public issue. Use GitHub's private reporting:
**Security → Report a vulnerability** on the
[repository](https://github.com/SkillsHUB-Electronics/tumon/security). Include what you found, how to
reproduce it and the Windows build. You will get an answer as soon as the maintainer can; please allow
reasonable time before disclosing.

## Areas that matter most

- **The virtual camera runs inside a Windows system service** (Frame Server). A bug there can crash
  every app using a camera or run code with that service's rights. The frame channel from the app to
  the camera is treated as untrusted input.
- **The frame channel is shared memory that any interactively logged-on user's program may write to.** It is how a
  feeder program hands pictures to the camera inside the Frame Server. The source validates every field it reads
  and never trusts sizes from the writer, but another program running as a logged-on user could still show its own
  pictures through Tumon Camera (spoofing). Restricting writers to one signed feeder process is on the to-do list.
- **Installer and registration** change the system and must be signed and cleanly reversible.
- **Phone streaming (planned)** opens a local-network listener. It must authenticate the phone, avoid
  accepting video from strangers on the same network and never expose a port to the internet.

See also [PRIVACY.md](PRIVACY.md).
