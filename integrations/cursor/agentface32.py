#!/usr/bin/env python3
"""Forward one Cursor hook payload to AgentFace32.

Cursor spawns hook commands and writes a JSON object to stdin. This script
re-sends that object to POST /hook/cursor and always allows the action to
continue, so the display can never block your agent.

Set AGENTFACE_URL to override the default device address, for example:
  export AGENTFACE_URL=http://192.168.1.50
"""

import json
import os
import sys
import urllib.request

DEFAULT_URL = "http://agentface32.local"
TIMEOUT_SECONDS = 1.5


def main() -> int:
    base = os.environ.get("AGENTFACE_URL", DEFAULT_URL).rstrip("/")
    raw = sys.stdin.read()

    try:
        payload = json.loads(raw) if raw.strip() else {}
    except json.JSONDecodeError:
        payload = {"raw": raw}
    if not isinstance(payload, dict):
        payload = {"raw": raw}

    request = urllib.request.Request(
        base + "/hook/cursor",
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
            response.read()
    except Exception:
        # The device may be off or on another network. Cursor must continue.
        pass

    # Cursor validates the stdout JSON per hook. Permission hooks block the
    # action on an invalid response, so answer their schema explicitly.
    event = payload.get("hook_event_name", "")
    if event in ("preToolUse", "subagentStart"):
        print(json.dumps({"permission": "allow"}))
    elif event == "beforeSubmitPrompt":
        print(json.dumps({"continue": True}))
    else:
        print("{}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
