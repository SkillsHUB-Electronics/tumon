<p align="center">
  <img src="assets/logo/tumon-logo.png" width="200" alt="Tumon logo: a dark blue rounded tile with a white and light blue letter T and a camera lens shape, above the word tumon">
</p>

<h1 align="center">Tumon</h1>

<p align="center">
  <b>A virtual camera for Windows that TeamTalk can actually see.</b>
</p>

<p align="center">
  Tumon takes the picture from your laptop camera or USB webcam and presents it as a camera called
  <i>Tumon Camera</i>, which <b>TeamTalk 5</b> lists and sets up by itself.<br>
  <i>By SkillsHUB Electronics. Sister project of <a href="https://github.com/SkillsHUB-Electronics/tapis">Tapis</a>, the microphone noise filter.</i>
</p>

<p align="center">
  <a href="https://github.com/SkillsHUB-Electronics/tumon/releases"><b>Download (0.1.0 preview)</b></a>
</p>

> **Status: early preview (0.1.0).** Tried on one Windows 11 PC with TeamTalk 5.23 and its built-in camera. The
> installer is **not code-signed**. Read the [known limitations](docs/release-notes/v0.1.0.md#known-limitations)
> before using it. Bahasa Indonesia: lihat [catatan rilis](docs/release-notes/v0.1.0.md#bahasa-indonesia).

## Use it

1. Download `Tumon-Setup-0.1.0.exe` from the [releases page](https://github.com/SkillsHUB-Electronics/tumon/releases)
   and run it (Windows 11, administrator).
2. Open **Tumon**, choose your camera, press **Start**.
3. In TeamTalk, *Preferences › Video Capture*: device **Tumon Camera**, **640x480, FPS 30**, image format **YUY2**.
   Then turn on video transmission.

The real camera turns on only while some program is using Tumon Camera.

## Why Tumon

TeamTalk is popular with blind and low-vision users, and it is picky about cameras:

- It lists only real Windows cameras (Media Foundation). OBS Virtual Camera and recent SplitCam are DirectShow
  filters, so TeamTalk does not show them.
- It fills its resolution list only from a camera's **YUY2** formats. Many laptop cameras offer only NV12, so you have
  to type a "custom video format" and it fails if one number is wrong.

Tumon is a real Windows camera that offers YUY2 and converts whatever your camera gives. The measurements behind this
are in [docs/research](docs/research/teamtalk-iriun-findings.md) and the decision in
[ADR 0001](docs/decisions/0001-media-foundation-virtual-camera.md).

## Features

| Area | In 0.1.0 | Planned |
|---|---|---|
| **Virtual camera** | "Tumon Camera" as a permanent Windows camera, YUY2, NV12, RGB32 and I420 at 320x240, 640x480, 1280x720 | |
| **Sources** | Laptop and USB cameras (USB not yet tested) | Phone as a camera, screen or window |
| **Processing** | Fits the picture into the chosen size without stretching | Flip, rotate, crop, brightness, background blur |
| **Interface** | Standard Windows controls (names and roles for screen readers), English and Bahasa Indonesia, tray icon, start with Windows | Dark theme, shortcuts help |
| **Control** | Console tools | Command line and MCP server, as in Tapis |
| **Privacy** | Video stays on your PC; the camera is on only while used | |

Status per item: [roadmap](ROADMAP.md). History: [changelog](CHANGELOG.md).

## Documentation

| Document | What it covers |
|---|---|
| [docs/README.md](docs/README.md) | Index of all documentation |
| [docs/troubleshooting.md](docs/troubleshooting.md) | Camera not listed, colours on a phone, uninstall |
| [docs/building.md](docs/building.md) | Toolchain and build steps |
| [docs/architecture.md](docs/architecture.md) | Components, data flow, folder layout |
| [docs/accessibility.md](docs/accessibility.md) | Screen-reader and keyboard requirements |
| [CONTRIBUTING.md](CONTRIBUTING.md) · [SECURITY.md](SECURITY.md) · [PRIVACY.md](PRIVACY.md) | Project policies |
| [AGENTS.md](AGENTS.md) · [CLAUDE.md](CLAUDE.md) | Instructions for AI agents |

## License

MIT. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Tumon is not
affiliated with Iriun, OBS, SplitCam or BearWare.dk (TeamTalk). Those names are used only to say
what Tumon works with or was compared against.
