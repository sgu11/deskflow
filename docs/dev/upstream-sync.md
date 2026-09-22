# Upstream integration, 2026-09-22

Merged `deskflow/deskflow` master at
`49e6293a4d147d0829a5acead8f61dcbbc01afce`. This brings in global-shortcut
portal support, corrected log rotation, EI locked-modifier synchronization,
and version-specific key-down/key-repeat parsing and serialization.

The `ServerProxy.h` conflict preserves the fork's socket-batch mouse handling
while accepting upstream's protocol 1.6/1.7/1.8 subclasses. The existing burst
acknowledgement test now calls `handleMessage`, where upstream moved the CNOP
reply. That method is protected so the existing test adapter can reach it.
The macOS cursor, IME and session-lock changes remain present.

## Verification

The merged source built successfully on macOS arm64 and Linux x86_64. The
following macOS checks passed: 28 CTest groups and the read-only modifier
mapping case. Clipboard and native key-injection groups were excluded to avoid
changing the active desktop.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_TESTS=ON -DSKIP_BUILD_TESTS=ON -DBUILD_INSTALLER=OFF
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build/src/unittests \
  --output-on-failure -E '^(OSXClipboardTests|OSXKeyStateTests)$'
QT_QPA_PLATFORM=offscreen build/src/unittests/platform/OSXKeyStateTests \
  mapModifiersFromOSX_OSXMask
```

Linux validation used a disposable Debian trixie container with Xvfb, libei
1.3.901 and libportal 0.9.1. All 30 CTest groups passed, including EI modifier
and X11 clipboard checks. A real-core TCP/X11 E2E test passed all six cases:
client and server roles for protocols 1.6, 1.7 and 1.8.

```sh
xvfb-run -a env QT_QPA_PLATFORM=minimal \
  ctest --test-dir build/src/unittests --output-on-failure
python3 tests/e2e/protocol-compat.py build/bin/deskflow-core
```

The E2E script requires `Xvfb`, `xev`, `xdotool`, `stdbuf` and Python 3. It
creates a private virtual display and connects a protocol peer to the actual
core over loopback TCP. The client cases verify real key-down/up delivery
across a key-repeat message and all 35 replies in a burst. X11 generates
autorepeat locally; it does not inject each received DKRP as another key press.
The server cases select the peer with a captured hotkey and hold a real virtual
key to verify the version-specific DKRP wire format. Peers are protocol
fixtures, not installed historical Deskflow or Waynergy binaries.

The optional global-shortcuts path requires `xdp_global_shortcut_new`, which
the tested libportal lacks, so that new path was not compiled or exercised.
Native Wayland portal interaction and a native Windows build remain untested.
No installed app or service was updated or restarted for this merge. Prior
native lock-sync evidence applies to the pre-merge installation; this merged
revision has not been deployed or given native desktop acceptance.
