# macOS session lock to Waynergy

The macOS server polls the current Quartz session every 250 ms on the core event
thread. A transition of `CGSSessionScreenIsLocked` emits the existing primary
screensaver event and CSEC message. This replaces ScreenSaverEngine process
tracking: modern screen locking does not require that process, and merely
showing a screensaver does not establish an authenticated lock.

Monitoring runs only while the primary screen is enabled. An unavailable Quartz
session does not count as an unlock. Starting the server while already locked
publishes the active state on its first poll. Duplicate transitions are ignored;
disconnecting the previously active client preserves the lock state so a new
connection receives CSEC on. Polling can be delayed by core event-loop load or
system sleep; synchronization resumes when the server can run and connect.

The lock dictionary key is provided by macOS but is not a public SDK constant.
Run native acceptance after macOS upgrades. Neither compiled code nor a fake
notification proves the operating system's actual lock state.

## Omarchy / Waynergy client

Waynergy 0.0.17 already supports `screensaver/start` and `screensaver/stop`.
Configure an executable helper through `screensaver/start` in a Waynergy
`config.ini.d` snippet, with an empty `screensaver/stop` action. The wire protocol
is unchanged. Native testing exposed
Waynergy's `SA_NOCLDWAIT`/`system()` interaction: a successful callback reported
`ECHILD` (-1). Native acceptance used a separate Waynergy patch that enables
child waiting around this callback and logs completion from the Waynergy
process itself. That client patch is not included in this Deskflow repository.
Child stdout may lack the parent service's journal InvocationID. The E2E test's
`--reconnect` mode therefore requires the patched parent-process log message
`Screensaver callback state start command #0 completed`; stock Waynergy does
not provide that verification signal.

The helper requests `omarchy-shell lock lock` and checks the reply. Waynergy's
restricted service PATH requires the helper to include `/usr/share/omarchy/bin`.
The stop action is empty: macOS unlock does not authenticate or unlock Linux.
Each device keeps its own authentication. Other distributions need an appropriate
local lock command in their Waynergy start hook. Deskflow Linux clients are not
changed by this work; their `Screen::screensaver()` remains a no-op.

## Build and native acceptance

Build with `deploy/mac/prepare-local.sh` from a clean committed checkout. Install
the complete prepared bundle with the existing backup and dependency-verification
procedure from [macOS local build](macos-dock-local-build.md).

With the changed server installed and Waynergy connected, start with both
desktops unlocked:

```sh
python3 deploy/mac/verify-lock-sync.py CLIENT_SSH_HOST --reconnect
```

The E2E check waits for an actual Mac lock, requires Omarchy's compositor-backed
`sessionLocked=true` and `secure=true`, restarts Waynergy while the Mac is locked,
and checks that the new process receives the active lock. Then unlock only the
Mac; Linux must remain securely locked. Authenticate on Linux separately after
the test. The script does not read or enter passwords or unlock either machine.

Also exercise locking while input is on the Linux screen and reconnecting that
client while locked, since its prior active-screen pointer must not erase the
server's lock state. Confirm normal input after both local authentications.

Rollback uses the previous complete Mac bundle and a backup of the client settings.
Moving its `lock-sync.ini` out of `config.ini.d` and restarting Waynergy disables
the callback. This does not unlock a session that is already locked.
