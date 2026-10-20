#!/usr/bin/env python3
"""AgentFace32 device tool: Wi-Fi firmware upload, logs and crash analysis.

Standard library only. Device address: --host, else $AGENTFACE_HOST, else
agentface32.local. Admin password (if set on the device): --password or
$AGENTFACE_PASSWORD.

  python tools/agentface.py health
  python tools/agentface.py ota .pio/build/m5stack-basic/firmware.bin
  python tools/agentface.py logs            # whole ring buffer
  python tools/agentface.py logs -f         # follow
  python tools/agentface.py crash --env m5stack-basic   # decode backtrace
  python tools/agentface.py coredump -o core.bin
  python tools/agentface.py reboot
"""

import argparse
import base64
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
import uuid

DEFAULT_HOST = "agentface32.local"


class Device:
    def __init__(self, host, password=None, timeout=10):
        host = host.strip().rstrip("/")
        if not host.startswith("http://") and not host.startswith("https://"):
            host = "http://" + host
        self.base = host
        self.password = password
        self.timeout = timeout

    def request(self, path, data=None, headers=None, method=None, timeout=None):
        req = urllib.request.Request(self.base + path, data=data, method=method)
        for k, v in (headers or {}).items():
            req.add_header(k, v)
        if self.password:
            token = base64.b64encode(("admin:" + self.password).encode()).decode()
            req.add_header("Authorization", "Basic " + token)
        return urllib.request.urlopen(req, timeout=timeout or self.timeout)

    def json(self, path, data=None, method=None, timeout=None):
        headers = {"Content-Type": "application/json"} if data is not None else {}
        body = json.dumps(data).encode() if data is not None else None
        with self.request(path, body, headers, method, timeout) as r:
            return json.loads(r.read().decode("utf-8", "replace"))

    def health(self, timeout=None):
        return self.json("/health", timeout=timeout)


def log(msg):
    print(msg, flush=True)


def wait_for_device(dev, old_uptime_ms, timeout_s=90):
    """Wait until the device answers again after a restart."""
    deadline = time.time() + timeout_s
    time.sleep(3)
    while time.time() < deadline:
        try:
            h = dev.health(timeout=4)
            if old_uptime_ms is None or h.get("uptime_ms", 0) < old_uptime_ms:
                return h
        except Exception:
            pass
        time.sleep(2)
    return None


def upload_firmware(host, firmware, password=None, wait=True):
    """Upload firmware over HTTP. Returns 0 on success (used by pio_ota.py)."""
    dev = Device(host, password, timeout=120)
    with open(firmware, "rb") as f:
        image = f.read()
    md5 = hashlib.md5(image).hexdigest()

    before = None
    try:
        before = dev.health(timeout=6)
        log("Device %s: v%s on %s, slot %s" % (dev.base, before.get("version"),
                                              before.get("hardware"), before.get("app_partition")))
    except Exception as e:
        log("Warning: /health failed before upload (%s), trying anyway" % e)

    boundary = "----agentface" + uuid.uuid4().hex
    head = ("--%s\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"firmware.bin\"\r\n"
            "Content-Type: application/octet-stream\r\n\r\n" % boundary).encode()
    tail = ("\r\n--%s--\r\n" % boundary).encode()
    body = head + image + tail
    log("Uploading %s (%d bytes, md5 %s)..." % (firmware, len(image), md5))
    started = time.time()
    try:
        with dev.request("/update?md5=" + md5, body,
                         {"Content-Type": "multipart/form-data; boundary=" + boundary}, "POST") as r:
            result = json.loads(r.read().decode() or "{}")
    except urllib.error.HTTPError as e:
        log("Upload failed: HTTP %d %s" % (e.code, e.read().decode("utf-8", "replace")))
        return 1
    except Exception as e:
        log("Upload failed: %s" % e)
        return 1
    if not result.get("ok"):
        log("Upload failed: %s" % result)
        return 1
    log("Upload done in %.1fs, device is restarting" % (time.time() - started))
    if not wait:
        return 0

    h = wait_for_device(dev, before.get("uptime_ms") if before else None)
    if not h:
        log("Device did not come back within 90s. Check power/serial; it may have rolled back.")
        return 1
    log("Back online: v%s build %s, slot %s, last reset %s" % (
        h.get("version"), h.get("build"), h.get("app_partition"), h.get("last_reset")))
    if before and h.get("app_partition") == before.get("app_partition"):
        log("ERROR: still running the old slot - the new image was rejected/rolled back.")
        log("Read the logs: python tools/agentface.py logs")
        return 1
    # The image is confirmed after ~20s of healthy running.
    deadline = time.time() + 45
    while h.get("ota_pending_verify") and time.time() < deadline:
        time.sleep(3)
        try:
            h = dev.health(timeout=4)
        except Exception:
            pass
    if h.get("ota_pending_verify"):
        log("Warning: new firmware not confirmed yet (it rolls back if it crashes before that).")
    else:
        log("New firmware confirmed.")
    return 0


def cmd_health(dev, args):
    print(json.dumps(dev.health(), indent=2, ensure_ascii=False))


def cmd_ota(dev, args):
    sys.exit(upload_firmware(dev.base, args.firmware, dev.password, wait=not args.no_wait))


def fetch_logs(dev, since):
    with dev.request("/logs?since=%d" % since) as r:
        text = r.read().decode("utf-8", "replace")
        nxt = int(r.headers.get("X-Log-Next", since))
    return text, nxt


def cmd_logs(dev, args):
    pos = args.since
    text, pos = fetch_logs(dev, pos)
    sys.stdout.write(text)
    sys.stdout.flush()
    while args.follow:
        time.sleep(args.interval)
        try:
            text, nxt = fetch_logs(dev, pos)
        except Exception as e:
            sys.stderr.write("[agentface] %s, retrying\n" % e)
            continue
        if nxt < pos:
            sys.stdout.write("\n[agentface] device log restarted (reboot or power cycle)\n")
        sys.stdout.write(text)
        sys.stdout.flush()
        pos = nxt


def find_addr2line(elf):
    root = os.path.join(os.path.expanduser("~"), ".platformio", "packages")
    chip = "esp32s3" if "esp32s3" in elf.replace("\\", "/") else "esp32"
    exe = "xtensa-%s-elf-addr2line" % chip
    for cand in (os.path.join(root, "toolchain-xtensa-" + chip, "bin", exe),
                 os.path.join(root, "toolchain-xtensa-" + chip, "bin", exe + ".exe")):
        if os.path.exists(cand):
            return cand
    return shutil.which(exe)


def cmd_crash(dev, args):
    info = dev.json("/api/crash")
    print(json.dumps(info, indent=2))
    if not info.get("coredump"):
        return
    elf = args.elf or (os.path.join(".pio", "build", args.env, "firmware.elf") if args.env else None)
    if not elf:
        print("\nPass --env <pio env> or --elf <firmware.elf> to decode the backtrace.")
        return
    tool = find_addr2line(elf)
    if not tool or not os.path.exists(elf):
        print("\nCannot decode: addr2line=%s elf=%s" % (tool, elf))
        return
    with open(elf, "rb") as f:
        elf_sha = hashlib.sha256(f.read()).hexdigest()
    dump_sha = info.get("app_elf_sha256", "")
    if dump_sha and not elf_sha.startswith(dump_sha):
        print("\nWARNING: dump is from another build (dump %s, elf %s); decoded names will be wrong."
              % (dump_sha, elf_sha[:len(dump_sha)]))
    addrs = [info.get("pc")] + info.get("backtrace", [])
    addrs = [a for a in addrs if a]
    print("\nDecoded with %s (must be the ELF of the crashed build):" % elf)
    out = subprocess.run([tool, "-pfiaC", "-e", elf] + addrs, capture_output=True, text=True)
    print(out.stdout or out.stderr)


def cmd_coredump(dev, args):
    with dev.request("/coredump", timeout=60) as r:
        data = r.read()
    with open(args.output, "wb") as f:
        f.write(data)
    print("Saved %d bytes to %s" % (len(data), args.output))
    print("Analyze: python -m esp_coredump info_corefile -t raw -c %s .pio/build/<env>/firmware.elf"
          % args.output)


def cmd_reboot(dev, args):
    print(dev.json("/api/reboot", data={}, method="POST"))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default=os.environ.get("AGENTFACE_HOST", DEFAULT_HOST))
    p.add_argument("--password", default=os.environ.get("AGENTFACE_PASSWORD"))
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("health").set_defaults(fn=cmd_health)
    o = sub.add_parser("ota")
    o.add_argument("firmware")
    o.add_argument("--no-wait", action="store_true")
    o.set_defaults(fn=cmd_ota)
    lg = sub.add_parser("logs")
    lg.add_argument("-f", "--follow", action="store_true")
    lg.add_argument("--since", type=int, default=0)
    lg.add_argument("--interval", type=float, default=1.5)
    lg.set_defaults(fn=cmd_logs)
    c = sub.add_parser("crash")
    c.add_argument("--env")
    c.add_argument("--elf")
    c.set_defaults(fn=cmd_crash)
    cd = sub.add_parser("coredump")
    cd.add_argument("-o", "--output", default="coredump.bin")
    cd.set_defaults(fn=cmd_coredump)
    sub.add_parser("reboot").set_defaults(fn=cmd_reboot)
    args = p.parse_args()
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    dev = Device(args.host, args.password)
    try:
        args.fn(dev, args)
    except urllib.error.HTTPError as e:
        sys.exit("HTTP %d: %s" % (e.code, e.read().decode("utf-8", "replace")))
    except urllib.error.URLError as e:
        sys.exit("Cannot reach %s: %s" % (dev.base, e.reason))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
