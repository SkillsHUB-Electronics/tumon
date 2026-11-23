# Third-party notices

Tumon currently bundles **no third-party code**.

## Reference only (not copied)

| Project | License | How it is used |
|---|---|---|
| [VCamSample](https://github.com/smourier/VCamSample) (Simon Mourier, MIT, Copyright (c) 2024-2026 Simon Mourier) | MIT | The pattern for a Media Foundation virtual camera (activation object, `IMFMediaSourceEx`, stream, sensor profiles, allocator control, HKLM registration) that `cpp/src/source` was written against. **No source was copied**; Tumon's code is its own (plain COM, no WIL or C++/WinRT, CPU-only drawing). If code is ever adapted, it moves to the next section with this copyright notice. |

## Bundled or adapted

*None yet.* Candidate libraries (for example a video decoder for phone streams, or a UI toolkit) will be
listed here with name, version, license and copyright before they are released.

## Trademarks

Iriun, OBS, SplitCam, TeamTalk, BearWare.dk, Microsoft, Windows and Media Foundation are trademarks of
their owners. They are mentioned only to describe compatibility or comparison. Tumon is not affiliated
with or endorsed by them.
