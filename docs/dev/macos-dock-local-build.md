# macOS Dock transition candidate

The linked screen edge, including its last pixel behind the Dock, uses the
normal screen-switch rules. Existing corner locks still apply. No Dock
exclusion zone, dwell time, or predictive cursor movement is added.

On a confirmed leave, the macOS server or client immediately requests cursor
hiding and posts a tagged, zero-delta move to the center of the display being
left. The local event tap lets this move reach the Dock without forwarding it
to the remote screen. A balanced show/hide pair reasserts hiding after the tap
acknowledges the move. Server mouse capture remains active throughout.

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

## Native acceptance still required

Test slow crossings through an expanded Dock and fast crossings before
magnification starts, with magnification both enabled and disabled. Confirm
the Dock hover clears, the local cursor disappears, and the remote cursor
receives no center jump. Reverse direction immediately, repeat transitions,
disconnect, and shut down normally to check stale events and capture release.
Also test the display arrangements and Dock positions actually in use.

Unit tests establish ordering, balanced hide ownership, failure handling,
stale completion rejection, and capture lifetime. They do not establish
native Dock behavior or compatibility with a particular third-party client.
