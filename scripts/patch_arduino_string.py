"""Apply the Arduino-ESP32 empty-String move fix to the installed core.

Arduino-ESP32 2.0.x has a bug in String::move(): assigning an empty string to a
String that already owns a buffer calls memmove(dst, nullptr, 1) and panics with
LoadProhibited. WebServer::_parseRequest() does exactly that for POST bodies, so
a malformed/empty hook payload can reboot the device.

Upstream fix: https://github.com/espressif/arduino-esp32/issues/10938
The 3.x cores contain it, but their firmware does not fit the 1.875 MB OTA slots
used by the M5Stack profile. Patch the 2.0.x source before compiling; the change
is idempotent and survives PlatformIO package updates.
"""

Import("env")

from pathlib import Path

MARKER = "AgentFace32: guard empty String move"


def patch_core():
    try:
        package = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    except Exception as exc:
        print("[string-patch] framework not installed yet: %s" % exc)
        return
    path = Path(package) / "cores" / "esp32" / "WString.cpp"
    if not path.exists():
        print("[string-patch] not found: %s" % path)
        return
    text = path.read_text(encoding="utf-8", errors="replace")
    if MARKER in text:
        return

    old = """        if(capacity() >= rhs.len()) {
            memmove(wbuffer(), rhs.buffer(), rhs.length() + 1);
            setLen(rhs.len());"""
    new = """        if(capacity() >= rhs.len()) {
            // AgentFace32: guard empty String move
            if (rhs.len()) {
                memmove(wbuffer(), rhs.buffer(), rhs.length() + 1);
            }
            setLen(rhs.len());"""
    if old not in text:
        print("[string-patch] pattern not found in %s" % path)
        return
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    print("[string-patch] patched %s" % path)


patch_core()
