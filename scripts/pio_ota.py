# PlatformIO extra script: adds the "ota" target that builds the firmware and
# uploads it over Wi-Fi to a running AgentFace32.
#
#   pio run -e m5stack-basic -t ota                       # host from $AGENTFACE_HOST
#   pio run -e m5stack-basic -t ota --upload-port 192.168.1.50
#
# Default host: agentface32.local. Admin password: $AGENTFACE_PASSWORD.

import os
import sys

Import("env")  # noqa: F821  (provided by PlatformIO)

TOOLS_DIR = os.path.join(env.subst("$PROJECT_DIR"), "tools")  # noqa: F821


def _looks_like_serial(port):
    p = port.upper()
    return p.startswith("COM") or p.startswith("/DEV/")


def _ota_action(target, source, env):
    if TOOLS_DIR not in sys.path:
        sys.path.insert(0, TOOLS_DIR)
    from agentface import DEFAULT_HOST, upload_firmware

    port = env.subst("$UPLOAD_PORT")
    host = os.environ.get("AGENTFACE_HOST") or (port if port and not _looks_like_serial(port) else DEFAULT_HOST)
    firmware = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    rc = upload_firmware(host, firmware, os.environ.get("AGENTFACE_PASSWORD"))
    if rc:
        env.Exit(rc)


env.AddCustomTarget(  # noqa: F821
    name="ota",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=[_ota_action],
    title="Upload over Wi-Fi",
    description="Build and upload firmware to AgentFace32 over HTTP",
)
