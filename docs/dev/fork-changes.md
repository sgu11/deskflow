# Input robustness fork

This branch is a source snapshot based on upstream commit
`217166c06b2225adcecd1b78d815d23da0b991bd`. It includes:

- macOS server/client cursor evacuation and balanced hiding at screen exits;
- the existing macOS client IME shortcut path and input relay changes;
- Windows server suppression of local Hangul/Kana toggles during remote input;
- focused regression tests and a macOS local app preparation script.

See `macos-dock-local-build.md`, `macos-ime-client.md`, and
`windows-korean-ime.md` for behavior and acceptance limits.

The inherited socket/buffering changes and mixed macOS keyboard injection
paths remain experimental. Publication does not establish that they improve
Wi-Fi performance, fix Korean composition, or resolve stuck modifiers.

The Windows routing policy tests pass on macOS, alongside ServerProxyTests
and KeyStateTests. A native Windows build and Korean type-3 keyboard acceptance
are still required; the intermittent half-width Katakana symptom is not yet
confirmed fixed. macOS Dock changes have passed focused tests and compilation
on arm64 and Intel; real Dock behavior remains to be validated.
