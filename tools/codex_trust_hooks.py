#!/usr/bin/env python3
"""Re-trust Codex lifecycle hooks after editing hooks.json.

Codex stores the approved hash for every non-managed hook in
`$CODEX_HOME/config.toml` under `[hooks.state.'<key>']`. Editing a hook (even
adding a `commandWindows` variant) changes its hash and silently disables it
in `codex app-server` sessions, which is what T3 Code and the IDE extensions
use. The interactive TUI offers a review prompt; app-server clients do not.

This tool asks a short-lived `codex app-server` for the current hook hashes
(`hooks/list`), writes them into the existing `hooks.state` entries, and keeps
a backup of config.toml. It only updates hooks that Codex already knows about;
it never adds a new hook or invents a trust decision for a file Codex has not
seen.

Standard library only.

  python tools/codex_trust_hooks.py --check
  python tools/codex_trust_hooks.py
  python tools/codex_trust_hooks.py --cwd C:\\path\\to\\project
"""

import argparse
import json
import os
import queue
import re
import shutil
import subprocess
import sys
import threading
import time


def log(message):
    print(message, flush=True)


def codex_home(explicit=None):
    if explicit:
        return os.path.abspath(os.path.expanduser(explicit))
    return os.path.abspath(os.path.expanduser(os.environ.get("CODEX_HOME", "~/.codex")))


def find_codex(explicit=None):
    if explicit:
        return explicit
    for name in ("codex", "codex.exe"):
        found = shutil.which(name)
        if found:
            return found
    raise SystemExit("codex binary not found in PATH; pass --codex")


class AppServer:
    def __init__(self, binary, cwd):
        self.proc = subprocess.Popen(
            [binary, "app-server", "--listen", "stdio://"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=cwd,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )
        self.lines = queue.Queue()
        self.next_id = 1
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        for line in self.proc.stdout:
            self.lines.put(line)

    def send(self, payload):
        self.proc.stdin.write(json.dumps(payload) + "\n")
        self.proc.stdin.flush()

    def request(self, method, params, timeout_s=20):
        request_id = self.next_id
        self.next_id += 1
        self.send({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params})
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            try:
                line = self.lines.get(timeout=0.25)
            except queue.Empty:
                continue
            if not line.strip():
                continue
            try:
                message = json.loads(line)
            except json.JSONDecodeError:
                continue
            if message.get("id") == request_id:
                if "error" in message:
                    raise RuntimeError("%s failed: %s" % (method, message["error"]))
                return message.get("result", {})
        raise RuntimeError("%s timed out" % method)

    def close(self):
        try:
            self.proc.kill()
            self.proc.wait(timeout=5)
        except Exception:
            pass


def read_hook_status(binary, cwd):
    server = AppServer(binary, cwd)
    try:
        server.request("initialize", {
            "clientInfo": {"name": "agentface32-trust", "title": "AgentFace32", "version": "1.0"},
            "capabilities": {"experimentalApi": True},
        })
        server.send({"jsonrpc": "2.0", "method": "initialized", "params": {}})
        result = server.request("hooks/list", {"cwds": [cwd]})
    finally:
        server.close()

    hooks = []
    for entry in result.get("data", []):
        for hook in entry.get("hooks", []):
            hooks.append(hook)
    return hooks


def patch_config(config_path, hooks, dry_run=False):
    if not os.path.exists(config_path):
        raise SystemExit("config not found: %s" % config_path)

    mapping = {}
    for hook in hooks:
        key = hook.get("key")
        current = hook.get("currentHash")
        if key and current:
            mapping[key] = current

    lines = open(config_path, encoding="utf-8").read().splitlines()
    output = []
    current_key = None
    updated = 0
    skipped = []

    for line in lines:
        section = re.match(r"^\[hooks\.state\.'(.*)'\]$", line.strip())
        if section:
            current_key = section.group(1).replace("''", "'")
        if current_key in mapping and re.match(r"^\s*trusted_hash\s*=", line):
            new_line = 'trusted_hash = "%s"' % mapping[current_key]
            if new_line != line:
                updated += 1
            line = new_line
        output.append(line)

    for key in mapping:
        if "[hooks.state.'%s']" % key not in "\n".join(lines):
            skipped.append(key)

    if dry_run:
        return updated, skipped, mapping

    if updated:
        backup = config_path + ".before-hook-trust-" + time.strftime("%Y%m%d-%H%M%S")
        shutil.copy2(config_path, backup)
        tmp = config_path + ".tmp"
        with open(tmp, "w", encoding="utf-8", newline="\n") as handle:
            handle.write("\n".join(output) + "\n")
        os.replace(tmp, config_path)
    return updated, skipped, mapping


def main():
    parser = argparse.ArgumentParser(description="Re-trust Codex hooks for app-server/T3 Code")
    parser.add_argument("--codex", help="path to the codex binary")
    parser.add_argument("--codex-home", help="CODEX_HOME (default: $CODEX_HOME or ~/.codex)")
    parser.add_argument("--cwd", default=os.getcwd(), help="project directory Codex should inspect")
    parser.add_argument("--check", action="store_true", help="only print trust status")
    parser.add_argument("--dry-run", action="store_true", help="show what would change")
    args = parser.parse_args()

    binary = find_codex(args.codex)
    home = codex_home(args.codex_home)
    cwd = os.path.abspath(args.cwd)
    config_path = os.path.join(home, "config.toml")

    hooks = read_hook_status(binary, cwd)
    if not hooks:
        log("No hooks found for %s (check %s)." % (cwd, os.path.join(home, "hooks.json")))
        return 1

    for hook in hooks:
        log("%-18s %-9s %s" % (hook.get("eventName"), hook.get("trustStatus"), hook.get("key")))

    if args.check:
        modified = [h for h in hooks if h.get("trustStatus") in ("modified", "untrusted")]
        return 1 if modified else 0

    updated, skipped, _ = patch_config(config_path, hooks, dry_run=args.dry_run)
    if args.dry_run:
        log("Would update %d hook hashes in %s" % (updated, config_path))
    else:
        log("Updated %d hook hashes in %s" % (updated, config_path))
    if skipped:
        log("Not in hooks.state (trust them once in the Codex TUI): %s" % ", ".join(skipped))
    log("Restart Codex / start a new T3 Code thread so app-server reloads the config.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
