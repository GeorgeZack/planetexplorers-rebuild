# Private native input supervisor

**October7 correction:** the original `-displayfd` startup collided with the desktop Xwayland socket. [Incident and recovery](PRIVATE_DISPLAY_COLLISION.md) supersedes the earlier claim of complete desktop isolation. Both tracked launchers now use explicit, checked high display numbers. Do not reuse older local prototypes.

[rebuild/tools/native_private_input.py](../../rebuild/tools/native_private_input.py) supports bounded ordinary keyboard/mouse acceptance on a newly launched, isolated native-game display. It never connects to the user's desktop, drives Unity/the Steam original, generates a gameplay fixture, patches a save, or uses smoke/test gameplay modes. It is an operator tool, not an unattended task planner. Only one agent/operator should control a session.

The tracked helper replaces an earlier local prototype for **future sessions**. It does not adopt that prototype's running process or registry. The tests described below do not launch any display or game; an existing acceptance session can continue independently with its original operator.

## Local prerequisites and ownership

Build the normal `rebuild/build/planet_explorers` executable and install its usual recovered assets first. Xvfb must be privately available at `rebuild/build/private-xvfb/usr/bin/Xvfb`; see the package provenance and extraction recipe in [the UI capture audit](SHOP_QUANTITY_AND_ITEM_DETAILS.md). This helper does not download/install dependencies, change system display configuration or fall back to an existing X server. Input uses the already available `libX11.so.6` and `libXtst.so.6`; screenshots require `ffmpeg` with X11 capture. Linux `/proc`, `prctl(PR_SET_PDEATHSIG)` and Python pidfd support are required for lifecycle/process checks and explicit stop. The local native executable and privately extracted Xvfb are ordinary unprivileged executables; this is not a launcher for setuid/setgid or file-capability programs.

`start` requires an existing native PECS checkpoint and a **new, nonexistent output directory**. It copies the checkpoint unchanged to read-only `baseline.pesave`, records its SHA256, and creates a separate writable `progress.pesave`. The original checkpoint is never opened for writing. The normal game launches with `--sandbox --load progress.pesave --save progress.pesave`; saving requires ordinary game input. Neither stopping nor this tool's lifetime timeout fabricates a save.

The foreground supervisor owns both children: private Xvfb and the normal production game. The shared launcher selects an explicit display100–999 under a per-user startup lock, excluding inherited DISPLAY, existing X lock/socket names (including XN_) and filesystem/abstract listeners in `/proc/net/unix`. Xvfb receives that explicit number; the game receives only that `DISPLAY`, no inherited `WAYLAND_DISPLAY`, and a unique private-session token. Software rendering is requested through `LIBGL_ALWAYS_SOFTWARE=1` and `GALLIUM_DRIVER=llvmpipe`. The existing native combat trace is enabled for evidence. Startup polls the owned child and verifies the listener's PID/UID with SO_PEERCRED before launching clients. It never removes foreign X paths, and fails closed on unavailable displays or startup failures. There is no displayfd autodiscovery. Forced server teardown can leave stale high-display files; later launches skip them rather than deleting them.

Every command validates registry schema/directory, session lifetime, both child PIDs/start times/executable paths, supervisor PID/start time/executable, child parentage, exact private-server command, exact normal-game load/save arguments, and the game's matching private DISPLAY/token binding. Baseline and executable hashes must remain unchanged. A rebuilt/replaced binary, reused PID, changed display, orphaned child or expired session fails closed. Do not rebuild the executable during an acceptance session. Input also requires a uniquely titled native window with matching `_NET_WM_PID` and confirmed focus on the owned display. There is no default-display fallback.

`command.lock` serializes commands. Input controls are released in `finally` on ordinary exceptions and handled SIGINT/SIGTERM/SIGHUP. The command and supervisor request SIGHUP if their launching parent dies; both game and Xvfb request kernel SIGKILL if their owning supervisor dies, including an uncatchable supervisor termination. Registration checks the parent again to close the fork/registration race. The supervisor should remain attached to its launching terminal/tool session.

An atomic `input-lease.json` is published before connecting for input and retained until releases and display close succeed. Setup is limited to5s; the armed lease permits the requested action duration (plus the click's0.12s positioning delay) and1s release grace. Failed commands leave their lease, and later commands cannot overwrite it. The supervisor polls every50ms, checking command PID/start time/executable and the lease deadline. An absent/reused command, malformed lease or expired lease aborts both owned children without waiting on a possibly abandoned input lock. Lease clocks are sampled after reading the snapshot; any fault is rechecked against the same complete lease so successful removal or concurrent arming does not falsely abort the session.

SIGKILL or a fatal Xlib exit cannot run Python releases. The lease watchdog instead destroys the owned game/display, clearing their input state. The requested hold limit is5s; watchdog poll/release-grace bounds are not a hard real-time guarantee under arbitrary OS scheduling or process suspension. This is deliberate session abort, not successful gameplay or graceful save. The supervisor also enforces a finite lifetime, default1800s and maximum3600s. Teardown signals both owned children before bounded waits and leaves any stale X paths excluded from subsequent selection. Explicit `stop` first signals only the verified game through a pidfd; the supervisor then owns server cleanup. No timeout, signal or watchdog invents a save.

## Commands

Run the supervisor in its own terminal/tool session and retain the printed directory/registry:

```sh
python rebuild/tools/native_private_input.py start /absolute/new/acceptance-directory /absolute/path/to/normal-game-checkpoint.pesave --lifetime 1800
```

Subsequent commands from the sole operator use that exact directory:

```sh
python rebuild/tools/native_private_input.py status /absolute/new/acceptance-directory
python rebuild/tools/native_private_input.py key /absolute/new/acceptance-directory w 0.4
python rebuild/tools/native_private_input.py key /absolute/new/acceptance-directory Shift_L+w 0.4
python rebuild/tools/native_private_input.py button /absolute/new/acceptance-directory 1 0.12
python rebuild/tools/native_private_input.py burst /absolute/new/acceptance-directory 2 1
python rebuild/tools/native_private_input.py look /absolute/new/acceptance-directory 40 -10
python rebuild/tools/native_private_input.py click /absolute/new/acceptance-directory 640 400 1 0.12
python rebuild/tools/native_private_input.py shot /absolute/new/acceptance-directory after-input-001.png
python rebuild/tools/native_private_input.py stop /absolute/new/acceptance-directory
```

**Captured-camera actions use `button` or `burst`, never `click`.** These button-only paths never warp the pointer. `click` intentionally moves to an absolute coordinate and is reserved for visible-pointer UI/free-pointer interactions. In the prototype, warping the virtual pointer before a captured-camera attack changed yaw; a button pulse must not silently do that. `look` uses bounded relative motion only.

Key/button holds and bursts are limited to5s, key chords to four distinct supported keys, relative look to1000 pixels per axis, and UI click coordinates to the fixed1280×720 private display. Only left/right buttons are enabled. Screenshots must use a new simple PNG filename in the owned output directory; captures never overwrite an existing file. Near-expiry sessions reject new input. Invalid arguments and guard failures produce ordinary exceptions/exit1 diagnostics, not assertion aborts.

`inputs.jsonl` records command begin/completion/failure, per-key/button transitions, relative/absolute motions, wall/monotonic timestamps, session identity and final baseline integrity. `session.json`, `native.log`, `xvfb.log`, screenshots and the writable progress checkpoint remain local to the new directory. The registry contains only the necessary process/display fields, not a dump of the user's environment.

## Verification and evidence limits

[Twenty-four focused tests](../../rebuild/tests/test_native_private_input.py) pass with zero skips using synthetic process records, mocked input/children and one short non-graphical Linux subprocess probe. They cover argument bounds, forbidden/duplicate chords, screenshot paths, valid ownership, PID reuse, wrong executable/parent/display/token, smoke-argument rejection, expiry, interrupted key/button release, journal-failure release, button bursts without pointer warps, exact relative motion totals, fresh-directory enforcement, unchanged baseline/progress copies and owned child cleanup. High-display regressions cover range/inherited/occupied exclusions, filesystem and abstract listeners, startup failure/timeout, exact listener ownership and stripped server display environment. Lifecycle coverage also includes all three interruption handlers, HUP release with retained abort lease, successful lease removal, failed-lease overwrite rejection, missing/reused/expired/malformed owners, deterministic publication/removal/replacement races, watchdog operation while the command lock is held, finite lifetime and signal-both-before-wait cleanup. The subprocess probe kills its own short Python supervisor and verifies/reaps its detached sleep child after kernel parent-death SIGKILL; its subreaper setting is confined to the probe wrapper. Tests do not touch the running prototype, native game, or any live display.

```sh
python rebuild/tests/test_native_private_input.py
```

The tracked helper has now driven ordinary native input in [the earned hunt session](HUNT_ORDINARY_ACCEPTANCE.md). That session began before the last watchdog race fixes; a fresh final-helper session separately checked normal completed-save load, Pause/menu input, owned screenshots and explicit stop, with baseline unchanged. Input window selection is intentionally limited to the sandbox title; after returning to the main menu, use `stop` rather than attempting further menu input. No final-helper crash-injection graphics test is claimed. Source/guard tests and the non-graphical lifecycle probe qualify the stated checks; they do not establish live Xlib failure recovery or real-time scheduling bounds; actual native movement, collision, UI, save/load and combat outcomes require their own input journal, screenshots/logs and observed result. Normal input can still fail because of a real game bug or missing assets. A captured screenshot does not establish hidden quest IDs, exact animation timing, four real kills or original-game parity. Distinguish those observations from both staged render fixtures and source-derived contracts.

After the collision fix, a real shared-launcher regression selected `:100`, initialized/drew/closed a hidden raylib window on that private server, and verified the desktop X0/X0_/lock inode and modification timestamps were unchanged. XOpenDisplay on the desktop `:0` still passed after teardown. This is display-isolation qualification, not a repeated hunt run.
