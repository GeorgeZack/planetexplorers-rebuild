# Private Xvfb / desktop Xwayland collision

October7,2026. The user reported an X11 `:0` error when starting the rebuild on Hyprland/Wayland. This was caused by the test launcher, not a switch of the desktop to Xorg.

## Confirmed failure and recovery

Hyprland1157 and its Xwayland1250 remained alive. Both retained the original listener descriptors for `/tmp/.X11-unix/X0` and `X0_`; the desktop `.X0-lock` still named Hyprland. However, the filesystem X0 entry had been replaced during the private Xvfb runs on`:0`. Connecting to X0 returned ECONNREFUSED, while X0_ completed an X11 protocol handshake. XOpenDisplay(`:0`) failed. The old `-displayfd` discovery was therefore not a safe isolation mechanism on this session.

[Hyprland's server implementation](https://github.com/hyprwm/Hyprland/blob/main/src/xwayland/Server.cpp) describes its two equivalent listening sockets when abstract sockets are disabled. Live `/proc` descriptor inspection and the successful alternate handshake established the actual local state. Recovery preserved the stale X0 entry, staged a same-owner hardlink to the functioning X0_ socket, then atomically replaced X0 after checking its original inode. XOpenDisplay succeeded immediately. Hyprland/Xwayland and existing applications were not restarted, and no session variables, configuration or packages changed. Local recovery details are in `rebuild/build/xwayland-recovery-20261007.json`; this is evidence of a one-time repair, not a general socket-repair script.

A hidden160×120 raylib window subsequently initialized, drew and closed with exit0. Its log identifies GLFW/X11, NVIDIA Quadro K1200 and OpenGL3.3. This confirms the installed game library's Xwayland path works again. It does not add a native Wayland backend or constitute a full gameplay launch. Normal game commands and saves are unchanged.

## Permanent test-launcher correction

Both [ordinary input](../../rebuild/tools/native_private_input.py) and [draw-only capture](../../rebuild/tests/run_private_ui_capture.py) now share one startup path:

- Use explicit displays100–999; reject lower numbers and the inherited desktop display.
- Reject every existing XN/XN_ socket and X lock path, including stale paths, plus filesystem/abstract listeners in `/proc/net/unix`.
- Serialize selection/startup through a same-owner, non-symlink per-user lock.
- Launch Xvfb with its explicit display and stripped display environment; never use displayfd autodiscovery or delete foreign socket/lock files.
- Require a live owned child and matching listener PID/UID through SO_PEERCRED before launching clients. Failure terminates only the spawned child.

Existing command guards also reject old low-display registries, so the tracked helper cannot adopt the former`:0` sessions. Ignored local prototypes remain historical evidence and must not be run again. Forced teardown may leave stale high-display paths, which are skipped rather than automatically removed.

All24 focused helper tests pass, including new occupied-display, inherited-display, abstract-listener, startup timeout/failure and listener ownership cases. Independent review found no remaining blocking issue. A real corrected-launcher check chose`:100`, rendered a hidden raylib window, closed its owned server, and verified the desktop X0/X0_/.X0-lock inode and modification timestamps were unchanged. Desktop XOpenDisplay still succeeded after teardown. Local evidence is `rebuild/build/private-display-regression.json` and its adjacent logs.

Earlier audits accurately recorded input directed to the private game, but their broader claim that the desktop was untouched was incorrect. The hunt checkpoints/contact evidence remain valid; display isolation is qualified by this later correction.
