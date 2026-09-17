# Korean type-3 keyboard on a Windows server

While a Windows server controls a remote screen, the keyboard hook forwards
events to Deskflow and suppresses their local delivery. Caps Lock, Num Lock,
and Scroll Lock remain exceptions to maintain their indicator state.

Hangul/Kana (Win32 virtual key 0x15) is no longer an exception. Korean type-3
Shift+Space can arrive as this virtual key with Space's scan code (0x39).
The existing Korean mapping forwards that combination as Space with its
modifier mask; the original event must not also toggle the server's mode.
Local screen input and Deskflow's explicit fake-local-input bypass retain
their previous behavior. Both hook implementations use the same policy.

This is a candidate fix for unintended server input-mode changes, not proof
that it resolves intermittent half-width Katakana. It does not reset an
already affected IME or change keyboard layouts, modifier tracking, or the
ToUnicode/dead-key path.

Portable routing tests cover local input, remote IME/ordinary keys, preserved
LED keys, and returning to local input. Native Windows validation must check:

- Local Shift+Space still toggles Korean input.
- Remote Shift+Space reaches the client without changing the server's mode.
- Releasing Shift before Space, and the reverse order, leaves no stuck keys.
- Crossing either screen boundary while the combination is held is safe.
- Repeated transitions do not reproduce half-width Katakana in local apps.

Capture layout identifiers, IME transition events and modifier state when
diagnosing failures; avoid recording ordinary typed content. A native Windows
build and live keyboard acceptance remain separate from portable test results.
