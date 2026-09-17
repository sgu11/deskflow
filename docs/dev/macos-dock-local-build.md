# macOS Dock transition candidate

The linked screen edge, including its last pixel behind the Dock, uses the
normal screen-switch rules. Existing corner locks still apply. No Dock
exclusion zone, dwell time, or predictive cursor movement is added.

On a confirmed server leave, request cursor hiding without warping or posting
another mouse-motion event. On subsequent physical motion, save the incoming
X/Y deltas for the remote peer, then rewrite only the local event to a
zero-delta move at a fixed point inside the display being left. Convert local
drags to ordinary moves; buttons, keys and wheel remain suppressed locally.
This lets Dock observe hover exit without forwarding relocation to the peer.

The first rewritten move acknowledges the transition. A balanced show/hide
pair reasserts hiding afterward. Do not replace that pair with hide/show:
native testing found persistent cursor and Dock residue with that ordering.
The server does not depend on foreground cursor disassociation. The macOS
client retains its tagged posted parking move and acknowledgement path.

The common monotonic clock now preserves fractional seconds. Previously,
integer division truncated time to whole seconds, delaying the 10ms hide
refresh until the next whole-second tick. Regression tests cover both clock
precision and actual short-timer delivery.

Reentry or disable invalidates the move token, cancels pending settling,
releases the owned hide request, and releases capture. Late events not yet
past the tap are discarded; events already delivered to the OS cannot be
recalled. The tap acknowledgement does not prove that Dock rendering finished.

`DESKFLOW_MACOS_HIDE_DELAY_MS` controls only the final reassertion, not the
initial hide. Default: 10 ms; accepted range: 0..2000 ms. This is an experiment
parameter, not a guaranteed animation-completion deadline.

## Prepare a local app without installation

Requirements: macOS, Command Line Tools, CMake, and Homebrew formulae
`qtbase`, `qttools`, `qtsvg`, `qttranslations`, and `openssl@3`.

From a clean committed checkout:

```sh
bash deploy/mac/prepare-local.sh
```

The script builds the GUI and core, runs focused non-injecting tests, bundles
Qt, signs ad hoc, and writes an app, zip, and revision/hash receipt in `dist/`.
It does not install or launch the server/client. Ad-hoc signing is not
notarization and does not preserve another app's Accessibility approval.

Before a later installation, preserve the complete old app and private
settings/TLS material outside Git. Stop the old process and automatic restart,
then replace the complete app bundle; do not mix a new core with old Qt
libraries. Retain TLS trust and verify macOS permissions. Roll back by restoring
the complete old bundle and its previous launch configuration.

## Validation and remaining acceptance

Local macOS arm64 testing with a directly attached mouse/trackpad and a
Waynergy client confirmed normal round trips and cursor/Dock residue resolving
almost immediately. The non-injecting test suite passed all 26 test groups.
This does not establish the same result on every OS version or display layout.

If motion stops exactly at the crossing, hover dismissal waits for the next
physical motion; immediate dismissal in that case remains unverified.

Test slow crossings through an expanded Dock and fast crossings before
magnification starts, with magnification both enabled and disabled. Confirm
the Dock hover clears, the local cursor disappears, and the remote cursor
receives no center jump. Reverse direction immediately, repeat transitions,
disconnect, and shut down normally to check stale events and capture release.
Also test the display arrangements and Dock positions actually in use.

Unit tests establish ordering, balanced hide ownership, failure handling,
stale completion rejection, physical/local motion separation, and short timers.
They do not establish native Dock behavior or compatibility with a particular
third-party client.

## Menu bar application

The macOS bundle starts as an `LSUIElement` application. Restoring the settings
window keeps the accessory activation policy, so the menu bar icon remains the
entry point without adding a normal Dock tile. The input core disables Qt's
foreground application transformation before constructing `QApplication`; it no
longer changes process type from its worker thread.

On macOS 27, the system may separately display a background-activity Dock item.
In local verification, quitting that existing item and relaunching left only the
menu bar icon, with the input client connected. This observation does not establish
that the system background indicator can never recur.
