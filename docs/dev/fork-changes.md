# Input robustness fork

This branch is a source snapshot based on upstream commit
`217166c06b2225adcecd1b78d815d23da0b991bd`. It includes:

- macOS server motion isolation, Dock hover dismissal, and balanced cursor hiding;
- subsecond monotonic clock precision for short timers;
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
confirmed fixed. The latest macOS Dock changes passed 26 non-injecting test
groups on arm64.
Native macOS-server/Waynergy-client testing confirmed normal round trips and
almost immediate cursor/Dock cleanup. Exact-edge stops without further motion
and other display arrangements still require acceptance.
