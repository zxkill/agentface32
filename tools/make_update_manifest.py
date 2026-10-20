#!/usr/bin/env python3
"""Generate update.json from packaged OTA binaries.

The firmware downloads this small manifest instead of the GitHub releases API
(which is ~27 KB and rate-limited). Run it after `pio run` and before creating a
GitHub release:

  python tools/make_update_manifest.py --version 0.9.0 --dist dist/v0.9.0

Then commit update.json and publish the release. The manifest URL is
raw.githubusercontent.com/<repo>/main/update.json.
"""

import argparse
import json
import os
import re
import sys

DEFAULT_REPO = "zxkill/agentface32"


def main():
    parser = argparse.ArgumentParser(description="Build update.json for AgentFace32")
    parser.add_argument("--version", required=True, help="version without or with v, e.g. 0.9.0")
    parser.add_argument("--dist", required=True, help="directory with *-ota.bin files")
    parser.add_argument("--out", default="update.json", help="output path (default: update.json)")
    parser.add_argument("--repo", default=DEFAULT_REPO, help="owner/name (default: %s)" % DEFAULT_REPO)
    args = parser.parse_args()

    version = args.version[1:] if args.version.startswith("v") else args.version
    pattern = re.compile(r"^agentface32-v%s-(.+)-ota\.bin$" % re.escape(version))
    assets = {}
    for name in sorted(os.listdir(args.dist)):
        match = pattern.match(name)
        if not match:
            continue
        path = os.path.join(args.dist, name)
        assets[match.group(1)] = {"file": name, "size": os.path.getsize(path)}

    if not assets:
        print("No agentface32-v%s-*-ota.bin files in %s" % (version, args.dist), file=sys.stderr)
        return 1

    manifest = {
        "version": version,
        "release": "https://github.com/%s/releases/tag/v%s" % (args.repo, version),
        "assets": assets,
    }
    with open(args.out, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(manifest, handle, indent=2)
        handle.write("\n")

    print("Wrote %s (%d assets) for v%s" % (args.out, len(assets), version))
    return 0


if __name__ == "__main__":
    sys.exit(main())
