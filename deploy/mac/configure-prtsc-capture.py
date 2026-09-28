#!/usr/bin/env python3
"""Use macOS F13/Print Screen for full-screen capture; preserve region capture."""

import datetime
import pathlib
import plistlib
import subprocess
import sys


DOMAIN = "com.apple.symbolichotkeys"
KEY = "AppleSymbolicHotKeys"
ACTIVATE = "/System/Library/PrivateFrameworks/SystemAdministration.framework/Resources/activateSettings"


def read_hotkeys():
    data = subprocess.check_output(["defaults", "export", DOMAIN, "-"])
    return plistlib.loads(data).get(KEY, {})


def main():
    if sys.platform != "darwin":
        raise SystemExit("This setup requires macOS.")
    before = read_hotkeys()
    target = {
        "enabled": True,
        "value": {"parameters": [65535, 105, 0], "type": "standard"},
    }
    if before.get("28") == target:
        print("Full-screen capture already uses F13 / Print Screen.")
        return

    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    backup = pathlib.Path.home() / "Library/Application Support/Deskflow/key-mapping-backups" / stamp
    backup.mkdir(parents=True, mode=0o700)
    # Store only the owned shortcut, not the user's unrelated shortcuts.
    old = {"present": "28" in before}
    if old["present"]:
        old["value"] = before["28"]
    (backup / "full-screen-capture.plist").write_bytes(plistlib.dumps(old))

    subprocess.run(
        ["defaults", "write", DOMAIN, KEY, "-dict-add", "28", plistlib.dumps(target).decode()],
        check=True,
    )
    after = read_hotkeys()
    assert after.get("28") == target, "Full-screen capture shortcut did not persist"
    assert {k: v for k, v in before.items() if k != "28"} == {
        k: v for k, v in after.items() if k != "28"
    }, "An unrelated shortcut changed during setup"
    subprocess.run([ACTIVATE, "-u"], check=True)
    print(f"Full-screen capture: F13 / Print Screen. Backup: {backup}")
    print("Region capture and other shortcuts preserved; physical-key acceptance is still required.")


if __name__ == "__main__":
    main()
