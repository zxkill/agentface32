# Media checklist for the GitHub page

The repository intentionally ships without the current prototype photo because development screenshots can expose local IP addresses and other workstation details. Before publishing, add one sanitized device photo and, ideally, a short demo GIF/video. This will make the project much easier to understand at a glance.

## Recommended 20–30 second demo

Record the device fairly close, with the screen readable. A good sequence is:

1. Claude receives a prompt → READING.
2. Claude edits a file → EDITING with path.
3. Codex starts a command in parallel.
4. Press C → show the two-agent dashboard.
5. Codex completes → background DONE indicator.
6. Trigger a real/manual permission request → NEEDS YOU.
7. Show AUTO mode switching to the most recently active agent.

Keep terminal text secondary; the device should be the subject.

## GIF

- 8–15 seconds
- 720p-ish source is enough
- crop around the device
- reduce to ~10–15 FPS for repository size
- target under ~10 MB if practical
- save as `docs/images/demo.gif`

Then add near the top of `README.md`:

```html
<p align="center">
  <img src="docs/images/demo.gif" width="520" alt="AgentFace32 reacting to Claude Code and Codex">
</p>
```

## Video

For YouTube/PeerTube/etc., use a 30–60 second walkthrough and link it near the top of the README. Suggested title:

> AgentFace32 — an ESP32 desk display for Claude Code & Codex

## GitHub social preview

GitHub's repository social preview works well at **1280×640**. Suggested composition:

- dark background
- large `AgentFace32`
- subtitle `ESP32 status display for Claude Code & Codex`
- a close, bright photo of the device on the right
- no more than 2–3 short text elements

Keep provider logos optional; product names in plain text age better and avoid making the project look officially affiliated.
