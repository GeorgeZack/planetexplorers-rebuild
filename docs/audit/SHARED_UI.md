# Shared tutorial and sandbox UI

The tutorial and sandbox now use the same owners for normal HUD presentation, minimap chrome, mission tracking, common inventory/dialogue frames, and pointer/chrome input. Host adapters supply the data. This implements the user's requested consistency between modes; it does not merge their gameplay progression or establish complete Unity UI parity.

## Durable ownership rules

| Owner | Responsibility | Host responsibility |
| --- | --- | --- |
| [pe_ui_draw](../../rebuild/src/pe_ui_draw.h) | Font choice, text wrapping/clipping, sprites/effects, item overlays/counts, buttons, panel/inventory/dialogue frames and pagination | Choose supported content and truthful labels; supply presentation values |
| [pe_ui_hud](../../rebuild/src/pe_ui_hud.h) | Normal HUD, nine quickslot presentations, health/stamina bars, hand/build chrome, minimap border/decorations, bounded mission title/objectives | Supply HP, stamina, item IDs/counts/cooldowns, mission text and completion flags |
| [pe_training_ui_layout](../../rebuild/src/pe_training_ui_layout.h) | Shared anchored geometry, half-open hit bounds, window offsets/clamping, stacking and title dragging | Supply visible windows and current viewport; keep offsets transient |
| [pe_ui_input](../../rebuild/src/pe_ui_input.h) | Health→Character and minimap-icon→Mission shortcuts, menu click/dismissal, action-only/full-modal pointer ownership through release | Apply requested panel changes and route mode-specific transactions |
| [Tutorial renderer](../../rebuild/src/pe_training_ui.c) / [sandbox renderer](../../rebuild/src/pe_sandbox_ui.c) | Adapt each mode's state to shared presentation snapshots | Keep lesson state and Adventure inventory/quests in their existing gameplay owners |

Apply a common visual correction in its shared owner and verify both hosts. Do not copy HUD frames, minimap decorations or text-wrapping algorithms into a host renderer to obtain a one-mode fix. An adapter may format a count, select a source mission, or decide whether information exists; it must not reconstruct the common drawing implementation. Shared renderers receive borrowed frame-local snapshots, never mutable gameplay owners or transaction callbacks.

Drawing and picking use the same layout fields. The minimap mission icon is `layout.mission_button` for both drawing and input. Move and resize panels through `pe_training_ui_windows_layout`, which translates their child controls and clamps their roots. Preserve the common back-to-front ordering when drawing and picking overlapping windows. HUD and dialogue remain anchored; movable panels use transient session offsets.

Tracker titles wrap inside bounded shared geometry, followed by objectives after the measured title height. Preserve the existing title clearance below the minimap, including narrow, tall and scaled viewports. Do not restore a fixed sandbox-only title box/gap or an unwrapped tutorial-only heading.

Input and simulation ownership are separate. A real modal consumes world input. A quickslot consumable or action-only HUD press suppresses new world actions without freezing movement, camera state or NPC simulation. Once a modal owns any mouse press, all held frames and the final release frame remain owned; capture and held-tool actions require a subsequent world press. A real modal may escalate an action-only press; it must never be downgraded while held. The sandbox's existing public pointer functions delegate to the shared gate. The tutorial uses that same gate before movement, digging and capture admission.

Menu dismissal owns its click, including a click over another control. A newly opened panel cannot process its opening click as an inventory/equipment action. Preserve these rules for direct hotkeys, HUD shortcuts, overlapping panels, focus loss and window dragging.

## Allowed differences

- Tutorial lesson inventories, grants, storage transfers, quickslot progression, crafting reservations and completion flags remain tutorial-owned. Adventure capacities, exact item instances, shops, repairs and quests remain sandbox-owned. Shared appearance grants no items or abilities.
- The tutorial uses its recovered minimap texture/crop and player presentation. The sandbox uses its current world texture, positions and supported markers. Sharing the border does not imply an identical map projection. Coordinates and clock labels appear only when an adapter supplies meaningful values.
- Mission text, objective counts, completion flags, known recipes, item counts and cooldown values come from the active mode. The renderer does not advance clocks or infer progress.
- Construction has a separate supported toolbar; it is not the normal quickslot HUD. Source-specific storage, Character/Replicator contents, town dialogue, repair panels and tutorial prompts remain specialized where their functionality differs. This increment is not a claim that every specialized window is identical.

When adding a new common element, extend the shared snapshot/layout/input API first, then connect both adapters and add paired checks. If one mode has no supported data or action, use an explicit unavailable/absent state; never invent tutorial grants or sandbox mission behavior to fill a visual slot.

## Verification at this checkpoint

Implemented in C and checked without graphics:

- [Structural ownership guards](../../rebuild/tests/test_ui_unification.py): seven tests pass with zero skips. They follow calls from each real host draw entry to the shared owners, reject duplicated common HUD/minimap sprite literals and local wrapping loops, check the shared mission-button geometry, and verify both input paths reach the common gate/chrome implementation. These are bounded source guards, not a C compiler or proof of every possible future duplication.
- [Shared input tests](../../rebuild/tests/test_ui_input.c): optimized and ASan/UBSan with leak detection pass. They cover viewport-scaled shortcuts, popup dismissal, full-modal escalation, held buttons and release-frame ownership.
- [Sandbox routing tests](../../rebuild/tests/test_sandbox_ui.c): optimized and ASan/UBSan with leak detection pass, including new shortcut/dismissal scenarios at 1280×720, 640×360 and 954×1048 and the existing consumable/modal transaction checks.
- [Production-extracted tutorial input tests](../../rebuild/tests/test_training_ui_input.py): all three tests pass with zero skips; their C fixtures use ASan/UBSan. They exercise actual tutorial routing/lesson transactions and the actual host block that prevents a held modal press from starting digging or camera capture.
- [Shared HUD draw-call tests](../../rebuild/tests/test_ui_hud.c) exercise the actual renderer and wrapper with synthetic font metrics and recorded drawing calls at four resolutions. Their optimized and ASan/UBSan/leak checks pass. These mocked GPU calls do not constitute pixel or visual acceptance.

The [paired real-renderer fixture](../../rebuild/tests/render_shared_ui.c) now passes on the qualified private Xvfb display `:101`, with recovered assets and llvmpipe. It calls both actual host UI draw entrypoints with equal staged HP/stamina, item counts, cooldown and hand identity. Exact HUD comparisons report **93,170 / 23,320 / 5,936 pixels checked, zero differences**, respectively at 1280×720, 640×480 and 320×240. Fifteen captures include paired HUD/inventory views and long-title tracking at those sizes. This verifies staged rendering; it does not play either host, advance a quest or test ordinary window interaction. The map contents/coordinates intentionally differ. The 320px fixture establishes bounds, not comfortable text readability.

Preserved evidence: [tutorial HUD](../reference/shared-ui/tutorial-hud-1280x720.png), [sandbox HUD](../reference/shared-ui/sandbox-hud-1280x720.png), [wrapped tracker](../reference/shared-ui/shared-long-tracker-640x480.png). These are C render fixtures with recovered Pathea Games UI assets, not original-game screenshots. Local inventory captures use the same directory under `rebuild/build/ui-shared-captures` and show shared frame placement. The long heading remains below the minimap and objective text follows the measured heading height. Desktop `X0`/`X0_` socket and `.X0-lock` inode/mtime values remained unchanged; `XOpenDisplay(:0)` succeeded after teardown. No desktop input, saves or original-client session was used.

Run `make -C rebuild ui-shared-render-capture` to reproduce; never run its graphical binary on the desktop display. The supervisor selects and validates an owned high display. Ordinary manual acceptance remains pending the checklist below.

The structural check can run directly:

```sh
python rebuild/tests/test_ui_unification.py
python rebuild/tests/test_training_ui_input.py
```

Use the current Makefile targets for shared drawing, sandbox routing and aggregate tests. Asset-dependent skips must remain visible and must not be described as passes.

## Manual acceptance checklist

Perform the same checks in tutorial and sandbox, using their own ordinary state; no lesson grants or save edits are needed merely to inspect chrome.

1. Compare the normal HUD, hand/build frame, nine labels, health/stamina bars and minimap decorations at 1280×720, a small window and a tall viewport. Map content may differ; borders, fonts and control placement should agree.
2. Inspect a short mission title and a wrapping title. The first objective follows the title without overlap, and the title stays below the minimap after resizing.
3. Click the health bar and minimap mission icon. Confirm the intended window opens and the opening click does not equip/use an underlying item. Close it, open the menu, then click outside over another control: only the menu should dismiss.
4. Move Inventory and Character by their headers, overlap them, then resize. Confirm close buttons/slots move with their frames, remain reachable, and only the topmost visible window receives input.
5. Hold a mouse button while closing a modal, move onto the world and continue holding. No digging, chopping or camera capture may begin until all buttons are released and a fresh world press occurs. Repeat with RMB, with a second button joining the hold, and after focus loss.
6. In sandbox, use a supported healing quickslot while moving and with active NPCs; repeat during its cooldown. Movement and NPC simulation must continue. In tutorial, retain the lesson's existing quickslot/consumption progression.

Record actual mode, viewport, input sequence, state and outcome in [PLAYTEST](../PLAYTEST.md). A shared source path or matching static screenshot alone cannot establish held-input behavior, original timing or complete Unity UI parity.
