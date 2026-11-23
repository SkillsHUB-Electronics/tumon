# Roadmap

Status key: **planned**, **in progress**, **done**. Nothing below is done yet. Dates are not promised;
milestones are ordered by risk, riskiest first.

## Milestone 0: find out what TeamTalk accepts (passed 2026-11-23; follow-ups below)

The project rests on what TeamTalk 5 needs to list and start a camera. Working model (from a TeamTalk device-list
screenshot, user reports and the [probe results](docs/research/probe-results-2026-11-23.md)): it **lists** real Windows
cameras but not pure DirectShow filters, and a listed camera must **start** with a format and size it can capture
(probably YUY2, modest resolution). See the [findings](docs/research/teamtalk-iriun-findings.md).

- [x] Device probe tool written and run (`tools/device-probe`).
- [x] TeamTalk's device list captured (lists Surface camera, SplitCam driver, Iriun, Windows Virtual Camera; not OBS or recent SplitCam).
- [x] Recorded TeamTalk's Video Capture preferences and error messages (SplitCam driver: "Custom video format", "Failed to initialize video device").
- [x] TeamTalk's *Video Resolution* list for Iriun captured (8 sizes × FPS 29 and 60: built from Media Foundation types).
- [ ] Capture TeamTalk's *Image Format* dropdown, and the *Video Resolution* and *Image Format* lists for Surface Camera Front; check the TeamTalk SDK header for its supported pixel formats.
- [ ] Record which listed cameras actually start and show video (Surface camera, Iriun, Windows Virtual Camera, SplitCam driver).
- [x] Media Foundation device list from the probe: identical to TeamTalk's list (the four real Windows cameras).
- [ ] Probe OBS Virtual Camera while started; read each camera's native Media Foundation formats (`ActivateObject` still fails in the probe) to see why the Surface camera needs a custom format.
- [x] Read every camera's **native** Media Foundation formats (`tumon_mfinfo`): only Iriun offers YUY2 natively and only Iriun fills TeamTalk's list.
- [x] Test-pattern Media Foundation camera written, built, unit-tested and verified through a source reader (`tumon_selftest`: 12 media types, 30 fps, correct sizes).
- [x] **Test camera registered and tried in TeamTalk 5.23 (2026-11-23): listed, resolution list filled by itself (640x480, 320x240, 1280x720 at FPS 30), YUY2 chosen, video shown. Milestone 0 passed; ADR 0001 accepted.**
- [x] 640x480 and 1280x720 confirmed in TeamTalk (user screenshots); 320x240 still to be confirmed.
- [ ] Capture TeamTalk's *Image Format* dropdown for Tumon Camera (it was hidden behind the resolution list).
- [ ] Check that a second TeamTalk client receives and decodes the video.
- [x] Decision: a *system* camera (`MFVirtualCameraLifetime_System` + `MFVirtualCameraAccess_AllUsers`) so it persists across restarts (TeamTalk saves the camera as its list index). Tool written: `tumon_vcam_ctl install|remove|status`; `install-test-camera.ps1 -Permanent` wires it up.
- [x] Permanent camera installed on the test PC and verified from Windows with no host program running: listed at position 4, 60 of 60 frames at 30 fps (2026-11-23).
- [x] **Permanent camera verified in TeamTalk after a Windows restart (user, 2026-11-23).**
- [ ] (done, kept for the record) Try the permanent camera in TeamTalk: install with `-Permanent`, select it in TeamTalk, close all programs, restart Windows, check `tumon_vcam_ctl status` and that TeamTalk still finds it at the same position.

- [ ] Build a minimal `IMFMediaSource` that outputs a test pattern (based on Microsoft's VCamSample pattern).
- [ ] Register it with `MFCreateVirtualCamera`; confirm it appears in the Windows Camera app.
- [ ] Confirm it appears in TeamTalk's video device list and that the picture is sent to another client.
- [ ] Record the result in the research note and ADR 0001 (confirm or reverse the decision).

## Milestone 1: local cameras (planned)

- [ ] App feeds frames from a USB or laptop webcam to the virtual camera through shared memory.
- [ ] Resolution and frame-rate selection; safe fallback picture when no source is active.
- [ ] Installer (Inno Setup): registers and unregisters the camera cleanly; code-signed.
- [ ] Keyboard-only and Narrator-tested interface, English and Bahasa Indonesia.

- [ ] A plain-text "TeamTalk setup" panel stating the exact device, resolution and image format to pick (accessible).

## Milestone 2: effects and control (planned)

- [ ] Flip, rotate, crop, brightness, contrast.
- [ ] Optional background blur (CPU first).
- [ ] `tumonctl` command line and MCP server, mirroring `tapisctl`.

## Milestone 3: phone as a camera (planned)

- [ ] Android app streaming over Wi-Fi, discovery on the local network.
- [ ] USB connection through adb port forwarding.
- [ ] Protocol decision recorded as an ADR (H.264 vs MJPEG, latency target, encryption).
- [ ] iPhone app (stretch).

## Milestone 4: polish and release (planned)

- [ ] Screen or window source.
- [ ] Documentation with screenshots, troubleshooting from real reports.
- [ ] First public release `0.1.0`.

## Out of scope for now

- Windows 10 support (the virtual camera API is Windows 11 only; a DirectShow fallback could be reconsidered
  in a future ADR if demand is clear).
- macOS and Linux.
- Video filters that need a GPU.
