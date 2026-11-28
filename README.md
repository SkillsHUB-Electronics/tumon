<p align="center">
  <img src="assets/logo/tumon-logo.png" width="200" alt="Tumon logo: a dark blue rounded tile with a white and light blue letter T and a camera lens shape, above the word tumon">
</p>

<h1 align="center">Tumon</h1>

<p align="center">
  <b>A virtual camera for Windows that TeamTalk can actually see.</b>
</p>

<p align="center">
  Tumon takes video from a USB webcam, a laptop camera or a phone, and presents it to other apps as
  a camera called <i>Tumon Camera</i>. It is built so that <b>TeamTalk 5</b> lists it, which
  OBS Virtual Camera and recent SplitCam do not.<br>
  <i>By SkillsHUB Electronics. Sister project of <a href="https://github.com/SkillsHUB-Electronics/tapis">Tapis</a>, the microphone noise filter.</i>
</p>

> **Status: early (v0.0.0).** Milestone 0 passed on 2026-11-23: a Media Foundation test camera is listed by
> TeamTalk 5.23, fills its resolution list by itself and shows video. There is **no installable product yet**; the
> repository holds the research, that test camera (`cpp/`), the design and the project documentation. See the
> [roadmap](ROADMAP.md) and the [changelog](CHANGELOG.md).

## The app (built, not released yet)

`Tumon.exe` lets you pick a camera (laptop or USB webcam) and press **Start**; TeamTalk then sees it as "Tumon Camera"
with its resolution list filled in. The real camera turns on only while some program is using Tumon Camera. It has a
notification-area icon, can start with Windows, speaks English and Bahasa Indonesia, and uses standard Windows controls
for screen readers. An installer script exists; there is no downloadable release yet.

## Why Tumon

TeamTalk is popular with blind and low-vision users, and it is picky about cameras. Virtual cameras that
work in Teams, Zoom or Meet are often missing from its video device list, and many recent laptop cameras are
not listed or fail to open. Iriun Webcam is still listed. Early measurements (see the
[device probe results](docs/research/probe-results-2026-11-23.md)) suggest two hurdles: TeamTalk **lists** real
Windows cameras (such as Iriun) but not pure DirectShow filters (OBS, recent SplitCam), and a listed camera must
still **start** with a format and size TeamTalk can capture (probably YUY2 at a modest resolution).
Tumon will therefore be a real Windows camera that presents TeamTalk-friendly formats and sizes, converting from
whatever the real camera gives.
On top of that it adds what Iriun does not: use of any local camera, phone as a camera, simple effects, and an
interface that works with screen readers. This is a hypothesis until the Milestone 0 tests confirm it.

The evidence is in [docs/research/teamtalk-iriun-findings.md](docs/research/teamtalk-iriun-findings.md); the first
architecture decision, still unproven, is in
[docs/decisions/0001-media-foundation-virtual-camera.md](docs/decisions/0001-media-foundation-virtual-camera.md).

## Planned features

| Area | Plan |
|---|---|
| **Virtual camera** | "Tumon Camera" registered as a Windows camera, visible to TeamTalk, Teams, Zoom, Meet, Discord, browsers |
| **Sources** | USB webcam, laptop webcam, Android phone (Wi-Fi and USB), later iPhone, screen or window |
| **Processing** | Resolution and frame-rate choice, flip and rotate, crop, brightness and contrast, optional background blur |
| **Interface** | Full keyboard navigation, screen-reader labels, English and Bahasa Indonesia, light and dark themes |
| **Control** | Command line and MCP server, same approach as Tapis |
| **Privacy** | Video stays on your PC and your local network; nothing is uploaded |

Nothing in this table is implemented yet. Status per feature lives in the [roadmap](ROADMAP.md).

## Requirements (planned)

- Windows 11 is the target. Whether Windows 10 can be supported depends on the camera technology chosen in
  Milestone 0 (see [ADR 0001](docs/decisions/0001-media-foundation-virtual-camera.md)).
- TeamTalk 5 (tested target: 5.23).

## Documentation

| Document | What it covers |
|---|---|
| [docs/README.md](docs/README.md) | Index of all documentation |
| [docs/architecture.md](docs/architecture.md) | Components, data flow, folder layout |
| [docs/research/teamtalk-iriun-findings.md](docs/research/teamtalk-iriun-findings.md) | What we measured on Iriun, OBS, SplitCam and TeamTalk |
| [docs/decisions/](docs/decisions/) | Architecture decision records |
| [docs/building.md](docs/building.md) | Toolchain and build steps (planned) |
| [docs/accessibility.md](docs/accessibility.md) | Screen-reader and keyboard requirements |
| [docs/troubleshooting.md](docs/troubleshooting.md) | Camera not listed, black picture, and similar |
| [CONTRIBUTING.md](CONTRIBUTING.md) · [SECURITY.md](SECURITY.md) · [PRIVACY.md](PRIVACY.md) | Project policies |
| [AGENTS.md](AGENTS.md) · [CLAUDE.md](CLAUDE.md) | Instructions for AI agents |

## License

MIT. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Tumon is not
affiliated with Iriun, OBS, SplitCam or BearWare.dk (TeamTalk). Those names are used only to say
what Tumon works with or was compared against.
