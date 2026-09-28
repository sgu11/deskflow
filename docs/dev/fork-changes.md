# Input robustness fork

This branch integrates upstream through commit
`be5c51405`. It includes:

- macOS server motion isolation, Dock hover dismissal, and balanced cursor hiding;
- subsecond monotonic clock precision for short timers;
- the existing macOS client IME shortcut path and input relay changes;
- Windows server suppression of local Hangul/Kana toggles during remote input;
- macOS session-lock synchronization through Waynergy's screensaver callback;
- focused regression tests, protocol compatibility E2E checks, and a macOS
  local app preparation script.

See `macos-dock-local-build.md`, `macos-ime-client.md`, and
`windows-korean-ime.md`, `macos-lock-sync.md`, and `upstream-sync.md` for behavior
and acceptance limits.

The inherited socket/buffering changes and mixed macOS keyboard injection
paths remain experimental. Publication does not establish that they improve
Wi-Fi performance, fix Korean composition, or resolve stuck modifiers.

The Windows routing policy tests pass on macOS, alongside ServerProxyTests
and KeyStateTests. A native Windows build and Korean type-3 keyboard acceptance
are still required; the intermittent half-width Katakana symptom is not yet
confirmed fixed. The September 28 source passed 29 non-injecting CTest groups
on macOS arm64 and the read-only modifier mapping case. Linux and protocol E2E
results in `upstream-sync.md` apply to the earlier September 22 revision.
Native macOS-server/Waynergy-client testing confirmed normal round trips and
almost immediate cursor/Dock cleanup. Exact-edge stops without further motion
and other display arrangements still require acceptance.

The September 28 update includes upstream screen-settings and clipboard fixes,
macOS F17-F20 mapping and client display wake, plus an optional Print Screen
setup helper documented in `macos-special-keys.md`.
