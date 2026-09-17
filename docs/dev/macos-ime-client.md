# macOS client IME and input relay branch

The `fix/macos-ime-client` branch routes Control+Space and Option+Space through
`CGEventPost`. Other keys continue through the existing HID path and its fallback.

The follow-up Wi-Fi tuning retains mouse compression, small packet write
coalescing, larger read buffers, and socket options. Two changes were withdrawn
after review:

- `pollActiveGroup()` queries TIS on each call. A 200ms cache could outlive an
  input-source change made by macOS or a shortcut, or a keyboard-group rebuild.
- Each successfully parsed normal protocol message receives a `CNOP` response.
  The 50ms rate limit discarded replies without scheduling a final response,
  leaving the existing delayed-ACK workaround inactive for those messages.

## Focused verification

Build `ServerProxyTests` and `OSXKeyStateTests` using the existing macOS build
configuration. Run `ServerProxyTests` in full. Its burst test requires one reply
per message, including the final message without subsequent input or a timer.

Run only `mapModifiersFromOSX_OSXMask` from `OSXKeyStateTests` for a check that
does not synthesize input. The other OSX key tests inject real keys into the
focused application and require a controlled test window.

Live acceptance must separately verify input-source switching followed by
immediate typing, actual Korean composition in the focused application, and
relay behavior on the affected Wi-Fi connection. Unit tests do not establish
these results. Updating the checkout does not update or restart a running core.
