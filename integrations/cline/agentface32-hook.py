#!/usr/bin/env python3
"""Forward one Cline hook payload to AgentFace32.

Cline runs executable files from .clinerules/hooks/ (or the global hooks
directory) and writes JSON to stdin. The same script serves every event; the
firmware reads hookName from the payload.

This hook never blocks Cline: it prints {"cancel": false} even when the device
is unreachable.

Set AGENTFACE_URL to override the default device address.
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
        base + "/hook/cline",
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
            response.read()
    except Exception:
        pass

    print(json.dumps({"cancel": False}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
