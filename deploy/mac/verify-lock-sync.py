#!/usr/bin/env python3
"""Native macOS -> Omarchy lock acceptance; the operator authenticates locally."""

import argparse
import ctypes
import json
import subprocess
import time


def mac_locked():
    session = cg.CGSessionCopyCurrentDictionary()
    if not session:
        raise RuntimeError("Quartz session unavailable")
    try:
        value = cf.CFDictionaryGetValue(session, lock_key)
        return bool(value and cf.CFBooleanGetValue(value))
    finally:
        cf.CFRelease(session)


def remote(command):
    return subprocess.check_output(
        ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5",
         "-o", "ControlMaster=auto", "-o", "ControlPersist=600",
         "-o", "ControlPath=/tmp/deskflow-lock-%C", args.host, command],
        text=True, timeout=15,
    ).strip()


def linux_secure():
    state = json.loads(remote(
        "systemd-run --user --quiet --wait --pipe "
        "/usr/share/omarchy/bin/omarchy-shell lock status"
    ))
    return state["sessionLocked"] and state["secure"]


def wait_for(predicate, seconds, failure):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(0.25)
    raise RuntimeError(failure)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("host", help="SSH alias of the Omarchy Waynergy client")
parser.add_argument("--timeout", type=int, default=180, help="seconds per operator action")
parser.add_argument("--reconnect", action="store_true", help="restart Waynergy while the Mac is locked")
args = parser.parse_args()
if args.host.startswith("-") or args.timeout <= 0:
    parser.error("invalid host or timeout")

cf = ctypes.CDLL("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")
cg = ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
cg.CGSessionCopyCurrentDictionary.restype = ctypes.c_void_p
cf.CFStringCreateWithCString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32]
cf.CFStringCreateWithCString.restype = ctypes.c_void_p
cf.CFDictionaryGetValue.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
cf.CFDictionaryGetValue.restype = ctypes.c_void_p
cf.CFBooleanGetValue.argtypes = [ctypes.c_void_p]
cf.CFBooleanGetValue.restype = ctypes.c_bool
cf.CFRelease.argtypes = [ctypes.c_void_p]
lock_key = cf.CFStringCreateWithCString(None, b"CGSSessionScreenIsLocked", 0x08000100)

try:
    if mac_locked() or linux_secure():
        raise RuntimeError("Start with both desktops unlocked")
    print("READY: lock the Mac; keep it locked until the reconnect check finishes.", flush=True)
    wait_for(mac_locked, args.timeout, "No native macOS lock observed")
    started = time.monotonic()
    wait_for(linux_secure, 5, "Linux did not acquire a secure session lock")
    print(f"PASS: native Mac lock -> secure Linux lock ({time.monotonic() - started:.2f}s observed)", flush=True)

    if args.reconnect:
        remote("systemctl --user restart waynergy.service")

        def lock_replayed():
            if not mac_locked():
                raise RuntimeError("Mac unlocked before reconnect verification")
            # Limit the evidence to the new process, never an earlier callback.
            output = remote(
                'invocation=$(systemctl --user show waynergy.service -p InvocationID --value); '
                'journalctl --user "_SYSTEMD_INVOCATION_ID=$invocation" --no-pager -o cat '
                '| grep -F "Screensaver callback state start command #0 completed" || true'
            )
            return bool(output) and linux_secure()

        wait_for(lock_replayed, 15, "Reconnected Waynergy did not receive the active lock")
        print("PASS: Waynergy reconnect received the active lock", flush=True)

    print("READY: unlock only the Mac; leave Linux locked.", flush=True)
    wait_for(lambda: not mac_locked(), args.timeout, "No native macOS unlock observed")
    # Give the server time to deliver CSEC off, then check the actual compositor.
    end = time.monotonic() + 3
    while time.monotonic() < end:
        if not linux_secure():
            raise RuntimeError("Linux unlocked without local authentication")
        time.sleep(0.25)
    print("PASS: Mac unlock preserved Linux authentication. Unlock Linux locally.", flush=True)
finally:
    cf.CFRelease(lock_key)
