# macOS special keys

macOS's ordinary keyboard-event path aliases Print Screen to the F13 virtual
keycode. Use a separate function key, such as F20, when a keyboard needs both
Print Screen and an independent function-key action.

## Optional local screenshot shortcut

Run `python3 deploy/mac/configure-prtsc-capture.py` to assign F13 / Print Screen
to macOS's full-screen screenshot action (symbolic hotkey 28). The script
backs up only that shortcut under
`~/Library/Application Support/Deskflow/key-mapping-backups/`, preserves the
other symbolic hotkeys, and asks macOS to reload the setting.

To restore the default, set the full-screen screenshot shortcut to
Command-Shift-3 in System Settings > Keyboard > Keyboard Shortcuts > Screenshots.
A prior custom value, if present, is in the saved `full-screen-capture.plist`.

With input on the Mac, verify that Print Screen creates a full-screen capture,
the existing region shortcut still works, and the independent function key
does neither. Preference checks alone do not establish native key acceptance.

## Remote function keys

This revision includes upstream macOS F17-F20 mappings. Client keymaps and
compositor shortcuts still determine the resulting remote action. Verify
key down/up and held-key release through the actual connection. Check remote
Print Screen against the client's screenshot bindings separately.
