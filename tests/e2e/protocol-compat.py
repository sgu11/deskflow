#!/usr/bin/env python3
"""Exercise the built core over TCP and X11 for protocol versions 1.6, 1.7 and 1.8.

Requires Linux, Xvfb, xev and xdotool. Always creates its own virtual display;
never injects input into an existing desktop. No installed service is changed.
"""

import argparse
import contextlib
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time


def packet(data):
    return struct.pack(">I", len(data)) + data


def receive(sock):
    def exact(size):
        data = b""
        while len(data) < size:
            chunk = sock.recv(size - len(data))
            if not chunk:
                raise RuntimeError("peer disconnected")
            data += chunk
        return data

    size = struct.unpack(">I", exact(4))[0]
    if not 4 <= size <= 65536:
        raise RuntimeError(f"invalid packet size: {size}")
    return exact(size)


def incoming(sock, seconds=10):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        sock.settimeout(min(5, max(0.01, end - time.monotonic())))
        yield receive(sock)
    raise TimeoutError("expected protocol message did not arrive")


@contextlib.contextmanager
def process(args, env, output):
    with output.open("w") as log:
        child = subprocess.Popen(args, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            yield child
        finally:
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=5)


def command(*args):
    return subprocess.check_output(args, env=environment, text=True, timeout=10).strip()


def settings(folder, port, mode):
    config = folder / "settings.ini"
    config.write_text(
        f"[core]\ncomputerName=protocol-{mode}\nport={port}\n"
        f"interface=127.0.0.1\ndisplay={environment['DISPLAY']}\n"
        "preventSleep=false\n[security]\ntlsEnabled=false\n"
        "[log]\nlevel=DEBUG\ntoFile=false\n"
        "[client]\nremoteHost=127.0.0.1\nlanguageSync=false\n"
        f"[server]\nexternalConfig=true\nexternalConfigFile={folder / 'server.conf'}\n"
        "enableClipboard=false\n"
    )
    return [str(core), mode, "--new-instance", "--settings", str(config)]


def test_client(minor, root):
    folder = root / f"client-1.{minor}"
    folder.mkdir()
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(15)
        with process(["stdbuf", "-oL", "xev", "-event", "keyboard", "-geometry", "600x400+10+10",
                      "-name", "DeskflowProtocolE2E"],
                     environment, folder / "xev.log"), process(
                settings(folder, listener.getsockname()[1], "client"), environment,
                folder / "core.log"):
            with listener.accept()[0] as peer:
                peer.settimeout(5)
                window = command("xdotool", "search", "--sync", "--name", "^DeskflowProtocolE2E$").splitlines()[0]
                command("xdotool", "windowfocus", "--sync", window)
                peer.sendall(packet(b"Barrier" + struct.pack(">HH", 1, minor)))
                hello = receive(peer)
                assert hello[:11] == b"Barrier" + struct.pack(">HH", 1, minor), hello
                peer.sendall(packet(b"QINF"))
                assert receive(peer).startswith(b"DINF")
                peer.sendall(packet(b"CIAK") + packet(b"CROP") + packet(b"DSOP" + struct.pack(">I", 0)))
                peer.sendall(packet(b"CINN" + struct.pack(">HHIH", 100, 100, 1, 0)))
                for reply in incoming(peer):
                    if reply == b"CNOP":
                        break
                # Enter restores the pre-capture focus. Focus the observer afterward.
                command("xdotool", "windowfocus", "--sync", window)
                language = struct.pack(">I", 2) + b"en" if minor == 8 else b""
                messages = [
                    (b"DKDL" if minor == 8 else b"DKDN") + struct.pack(">HHH", 0x61, 0, 38) + language,
                    b"DKRP" + struct.pack(">HHHH", 0x61, 0, 3, 38) + language,
                    b"DKUP" + struct.pack(">HHH", 0x61, 0, 38),
                ] + [b"CNOP"] * 32
                peer.sendall(b"".join(packet(message) for message in messages))
                replies = []
                for reply in incoming(peer):
                    if reply == b"CNOP":
                        replies.append(reply)
                    elif reply == b"CALV":
                        peer.sendall(packet(reply))
                    elif reply.startswith((b"EBAD", b"CBYE")):
                        raise RuntimeError(f"client rejected input: {reply!r}")
                    if len(replies) == len(messages):
                        break
                # X11 generates autorepeat locally; KeyMap does not inject DKRP
                # events on Linux. Verify the following key-up still arrives,
                # and use the CNOP replies above to detect stream desynchronization.
                end = time.monotonic() + 3
                while time.monotonic() < end:
                    events = (folder / "xev.log").read_text()
                    if events.count("KeyPress event") >= 1 and events.count("KeyRelease event") >= 1:
                        break
                    time.sleep(0.05)
                else:
                    raise RuntimeError(f"1.{minor}: missing X11 key events after DKRP\n{events[-4000:]}")
                assert events.count("keysym 0x61, a") >= 2, events
    print(f"PASS client 1.{minor}: X11 key down/up across DKRP and {len(replies)} CNOP replies", flush=True)


def test_server(minor, root):
    folder = root / f"server-1.{minor}"
    folder.mkdir()
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    (folder / "server.conf").write_text(
        "section: screens\n  protocol-server:\n  protocol-peer:\nend\n"
        "section: links\n  protocol-server:\n    right = protocol-peer\n"
        "  protocol-peer:\n    left = protocol-server\nend\n"
        "section: options\n  keystroke(F1) = switchToScreen(protocol-peer)\nend\n"
    )
    with process(settings(folder, port, "server"), environment, folder / "core.log") as child:
        end = time.monotonic() + 15
        while True:
            try:
                peer = socket.create_connection(("127.0.0.1", port), timeout=2)
                break
            except OSError:
                if child.poll() is not None or time.monotonic() > end:
                    raise RuntimeError("core server did not start")
                time.sleep(0.1)
        with peer:
            peer.settimeout(5)
            hello = receive(peer)
            assert hello[:7] in (b"Barrier", b"Synergy"), hello
            name = b"protocol-peer"
            peer.sendall(packet(hello[:7] + struct.pack(">HHI", 1, minor, len(name)) + name))
            for message in incoming(peer):
                if message == b"QINF":
                    peer.sendall(packet(b"DINF" + struct.pack(">7H", 0, 0, 800, 600, 0, 400, 300)))
                elif message.startswith(b"DSOP"):
                    break
            # Select the peer through a real captured hotkey. XWarpPointer does
            # not generate the XI2 raw motion used for physical edge crossing.
            command("xdotool", "key", "F1")
            for message in incoming(peer):
                if message.startswith(b"CINN"):
                    break
                if message == b"CALV":
                    peer.sendall(packet(message))

            repeats = []
            down = None
            with process(["xdotool", "keydown", "a", "sleep", "1.2", "keyup", "a"],
                         environment, folder / "sender.log") as sender:
                for message in incoming(peer):
                    if message.startswith((b"DKDN", b"DKDL")):
                        down = message
                    elif message.startswith(b"DKRP"):
                        repeats.append(message)
                    elif message.startswith(b"DKUP") and down:
                        break
                    elif message == b"CALV":
                        peer.sendall(packet(message))
                assert sender.wait(timeout=5) == 0
            assert down and repeats, (down, repeats)
            assert down[:4] == (b"DKDL" if minor == 8 else b"DKDN"), down
            for message in repeats:
                if minor < 8:
                    assert len(message) == 12, message
                else:
                    size = struct.unpack(">I", message[12:16])[0]
                    assert len(message) == 16 + size, message
    print(f"PASS server 1.{minor}: real held key produced {len(repeats)} version-correct DKRP messages", flush=True)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("core", type=Path, help="built Linux deskflow-core")
args = parser.parse_args()
core = args.core.resolve(strict=True)
with tempfile.TemporaryDirectory(prefix="deskflow-protocol-e2e-") as directory:
    root = Path(directory)
    environment = os.environ.copy()
    environment.pop("WAYLAND_DISPLAY", None)
    environment.update(QT_QPA_PLATFORM="xcb", XDG_CONFIG_HOME=str(root / "config"))
    with (root / "display").open("w+") as display:
        xvfb = subprocess.Popen(["Xvfb", "-displayfd", str(display.fileno()), "-screen", "0", "800x600x24",
                                 "-nolisten", "tcp"], pass_fds=(display.fileno(),), stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
        try:
            for _ in range(100):
                display.seek(0)
                number = display.read().strip()
                if number:
                    break
                time.sleep(0.05)
            else:
                raise RuntimeError("Xvfb did not start")
            environment["DISPLAY"] = ":" + number
            for version in (6, 7, 8):
                test_client(version, root)
                test_server(version, root)
        except Exception:
            for log in root.glob("*/core.log"):
                print(f"--- {log.parent.name} ---\n{log.read_text()[-6000:]}", flush=True)
            raise
        finally:
            xvfb.terminate()
            xvfb.wait(timeout=5)
