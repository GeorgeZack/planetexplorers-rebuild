# Sandbox construction undo and redo

October6,2026. The C sandbox now has a five-action undo/redo journal for its supported half-meter cube/ramp cell edits. This is a source-derived implementation checked without launching either game. [The source audit](CONSTRUCTION_HISTORY_SOURCE.md) identifies original list, input, resource and lifecycle behavior; ordinary native cube acceptance is recorded below, followed by a bounded rotated-ramp replay session. Player contact with and traversal of a replayed ramp remain unobserved.

## Controls and scope

In B-key construction, **Ctrl+Z** undoes and **Ctrl+X** redoes; either Control key works, and Undo wins simultaneous requests. The toolbar also has labeled Undo/Redo buttons. These buttons are a native usability addition replacing unavailable Save/Delete placeholders, not recovered original toolbar bindings. A held chord does not repeat. Ctrl+Z does not also toggle walking, and Ctrl+X does not also remove another block. Modal panels, their closing frames, pause, lost focus and death keep priority; held mouse presses remain owned through release.

Only the existing `PeB45Cell` cube/ramp edit path participates. A brush batch is one entry, at most64 cells; the journal retains five entries, discards its oldest on a sixth successful edit and clears redo only after a successful new edit. Direct tool mining does not record an action. Terrain deformation, tree cutting, legacy fence/ladder objects, creations and tutorial lesson construction are outside this journal. This does not implement Single Player Build or bypass Adventure materials.

## Material and geometry transactions

Replay runs the current authoritative placement/removal checks, rather than restoring an old world or inventory image. It preserves source per-material rounding:

| Operation | Material change |
|---|---|
| Place or redo placement | debit ceiling(cell count /4) |
| Remove or undo placement | refund floor(cell count /4) |
| Undo removal | debit ceiling(cell count /4) |
| Redo removal | refund floor(cell count /4) |

Thus placing one cell costs one resource, undoing it returns zero, and redoing it costs another resource. This source behavior is deliberately retained. Refund capacity, current material possession, bounds, supported shape, static geometry, terrain, player overlap and existing native3m reach remain authoritative. Replaying removal additionally requires every expected XYZ/type/material to match the current cells. An intervening tool edit may therefore leave a valid history entry blocked; no occupied or changed cell is silently overwritten.

Failed replay leaves cells, inventory and history cursor unchanged, with a failure message. Failed new edits do not evict history or clear redo. These all-or-nothing rules and exact-content guards are safer native adapters: the original list manager moves its entry even if the action reports failure. Successful replay does not increment the native historical `built` statistic again. This is an explicit statistic policy, not source mission-event reconstruction.

The host applies at most one requested replay per display frame before camera/picking queries and immediately refreshes shared collision after a change. Rendering reads the updated cell array directly. A display frame with no fixed simulation tick therefore still uses the same geometry for its visuals, rays and movement sweep. Replay never steps game time, consumable/tool clocks or creature simulation.

## Owner and lifetime

[pe_build_history](../../rebuild/src/pe_build_history.h) owns the bounded journal in `PeGame`; `pe_game` retains the only material/geometry transaction owner. The array contains cell values, no pointers or copied inventory/world owners. Clone/move and exiting/reentering B mode or the graphics host preserve history. New game, successful load and successful world replacement clear it; failed loads/replacements preserve the live state. Source has no established history archive owner, so these load/world boundaries are documented native policy.

PECSv17 bytes are unchanged. Saving retains live history but serializes only the resulting cells/materials and existing state. Reload clears both undo and redo without reversing any saved edit. Save validation checks journal structure, including capacity/cursor, valid cells, unique positions within a batch and zero unused records. It does not require every historical edit to remain replayable in the current world.

## Verification and next manual check

Commands from the repository root:

```sh
make -C rebuild build-history-test sandbox-build-history-host-test
python3 rebuild/tools/export_build_history.py --check
```

Regression proof: [PLAYTEST](../PLAYTEST.md).

Remaining manual checks: successful replay while moving and player contact/climbing on a replayed ramp. Completed key/toolbar/Pause/B/load cases are recorded below.

## Ordinary native acceptance

October 14, 2026. Observed in the production C runtime, Adventure, 1280×720,
through guarded private-display input, with no cheats, fixture grants, save
editing or original-game launch. [Checkpoint manifest and lossless
crops](../reference/build-native-20261014/checkpoints.json) retain saved cells,
material counts, hashes, journals and teardown evidence. Source-derived rules
remain distinguished from these native observations.

Claude operated session `rebuild/build/build-private-20261014-01` on `:117`
from the earned repair checkpoint `49ed477c…a96a` (Wood 193, no cells).
B mode and the Wood cube material were used:

- One cell was placed, undone with Ctrl+Z, then redone with Ctrl+X.
  Wood went 193 → 192 → 192 → 191, consistent with ceiling debit and floor
  refund. The saved `one-cell` inspection has one cube and `built=1`.
- History survived Save and B exit/reentry. A four-cell cube brush then
  committed one action on its initial press; the subsequent drag was not a
  swept-brush placement. Toolbar Undo removed that batch and refunded one
  Wood; Redo restored it and debited one. Final Wood was 190.
- Ctrl+Z under Pause left both visible constructions intact after Resume.
  Holding Ctrl+Z for 1.5 seconds removed only the four-cell batch once; the
  one-cell construction remained. Ctrl+X restored the batch.
- The earned `constructed.pesave` has five cells, material 11 / Wood 279,
  and `built=2`: replay did not inflate the saved statistic. Its SHA256 is
  `69bc92098f46fab765ee4cd257fcd5f65164cb3f905c96796d17ba8371805c62`.

Claude hit its usage limit before completing the load check. Its retry was
canceled; session 01 had already reached its lifetime deadline and the
watchdog killed the owned game (−9). The baseline remained unchanged. No
inputs were sent to that expired session.

Codex became the sole operator of fresh session
`rebuild/build/build-private-20261014-02` on `:118`, starting from the earned
five-cell save. Alt+left-click removed the separate single cube, producing a
live history entry and a visibly different world. Pause → Load restored it;
a paused Save produced `reloaded.pesave`, byte-identical to the baseline.
After Resume and B exit/reentry, Ctrl+Z displayed **Nothing to undo.** and
Ctrl+X displayed **Nothing to redo.** Both buttons were disabled. A further
save retained all five exact cells, Wood 190 and `built=2`. These UI operations
establish history clearing; the read-only inspector uses the current loader
and cannot report live history.

Session 02 ended by explicit guarded stop (owned game SIGTERM, −15).
Both baselines are unchanged and all six owned game/server/supervisor PIDs
were absent afterwards. Session 01's watchdog exit is not a clean game quit.
The runtime binary was unchanged (`f5c07847…a8e`). Only the read-only inspector
changed: schema 6 reports saved cell encoding, mapped item and `built`, with
signed-coordinate, rotation and legacy-empty formatter coverage.

Still unobserved in this acceptance: ramps, replay while moving, blocked
geometry/resource/refund transactions, sixth-entry eviction, failure to load,
small-window input and physical collision after replay. Headless checks cover
several of these separately. No original visual parity or difficulty claim.
The ramp session below addresses part of this list.

### Rotated ramp replay (session 03)

October 14, 2026. Claude was the sole operator of fresh session
`rebuild/build/build-private-20261014-03` on `:119`. It started from the
earned five-cube checkpoint `69bc9209…5c62`: Wood 190, `built=2`, player at
(−608.58, 28.55, 395.32). The production binary was unchanged
(`f5c07847…a8e`). Input was ordinary only: no cheats, fixtures, save edits,
original launch or helper changes. [Manifest, inspections and lossless
crops](../reference/build-native-20261014-ramp/checkpoints.json) record every
saved position quoted here.

Coordinates use source = local + (−640, 0, 64). A cell (x, y, z) covers
local [x/2, x/2 + 0.5) on each axis. The player capsule radius is 0.30 m
(`PE_PLAYER_RADIUS`).

**Observed in C, ramp replay.** In B mode the operator pressed V for Ramp,
kept the four-cell brush and pressed T once ("90 deg"). A left click on open
grass placed a horizontal 2×2 Wood ramp batch. The message read *Construction
placed (one resource per four cells, rounded up)* and Wood went 190 → 189.
The paused save `ramp-placed` (`c2629f19…9228`, `built=3`) adds four cells:
x 64–65, y 58, z 664–665, each `type=9` (shape 2, rotation 1) and material
11 / Wood 279. In source coordinates that is x −608…−607, z 396…397 and
y 29.0…29.5.

After Resume, Ctrl+Z showed *Construction undone.*, removed the rendered ramp
and refunded floor(4/4) = 1 Wood (189 → 190). Redo became enabled. Ctrl+X
showed *Construction redone.*, rendered the ramp again and debited 1 Wood
(→ 189). The paused save `ramp-redone` (`1506a675…132e`) has the same nine
cells in the same order, with identical packed type, rotation and material,
and still `built=3`. Replay did not increment the statistic. The two files
differ in bytes only through non-cell state; the inspector does not show live
history.

Both renders show the same sawtooth: each of the two cell rows is its own
0.5 m ramp with a vertical face. This matches the per-cell ramp geometry.
These stills establish orientation and presence. They do not establish a
contact frame.

**Observed in C, replay attempted while moving, rejected.** The helper presses
every key of a chord before one XTest flush. Ctrl+Z and W were therefore sent
in one input batch, and W was held for 0.6 s. The flush is not instrumented
against display frames. This is simultaneous input, not a replay in the
middle of a walk; the helper was deliberately not extended. The chord started
at `face-present` (−609.08, 28.73, 392.16). The game showed *Cannot undo
construction here.* while the player kept walking to (−612.01, 28.71,
392.80). Undo stayed enabled and Redo stayed disabled, consistent with the
retained one-entry history. Wood stayed 189, and `final` (`66909106…82ab`)
kept the nine cells unchanged. The live history cursor was not directly
observed.

The message does not name a cause. Source-derived inference: the native
removal check requires the cell within 3 m of the eye (player y + 1.5). From
the starting save, the nearest ramp point was about 4.1 m away. Reach is the
only failing check consistent with the saved positions, but the cause was not
observed.

**Contact and traversal attempts: not established.** Two attempts were made
without first verifying a clean approach line. Both are recorded as
negative results:

- `contact-before` (−608.30, 28.52, 396.38) placed the capsule 3 mm on the −x side
  of the ramp's x = −608 face and clear of the cubes. Holding S for 1.0 s ended
  at `contact-after-s` (−605.55, 28.50, 395.26), on the +x side of the ramp footprint
  at the same ground height. No intermediate frame or position was captured.
  A straight path would cross the footprint's z 396…396.5 row, but the save does not
  show whether the player climbed, slid along or skirted the ramp.
- `face-start` (−608.42, 28.52, 396.70) was beside the ramp, 0.12 m on its −x
  side, not in front of its vertical face. It was also 3 mm on the −z side of the
  baseline cube batch at x −609…−608, z 397…398. Holding A for 1.0 s moved
  4.5 m toward −z to (−609.08, 28.73, 392.16), away from both structures. This
  tests no face contact.
- Earlier short W/S moves near (−608, 397) were small. Both the ramp and the
  baseline cubes adjoin that spot, so they cannot be attributed to the ramp.

The ramp base is at y 29.0, while the nearby ground under the player was at
about 28.5. Ramp placement raises a batch to the highest terrain sample in
its footprint, so the low edge may stand up to 0.5 m above the surrounding
ground. Under the native controller (step 0.35 m, walkable normal y ≥ 0.65;
an adapter, not Unity physics), the 45° surface is walkable. The 0.5 m faces
exceed the step height, but the low-edge offset may also exceed it. Neither
climb nor block was observed. A future check needs a ramp whose base is level
with the ground, approached along a verified line. A location clear of the
cubes, such as ramp center x ≈ −607.5, is preferred. It should also take a
save or position reading while the player is on the ramp. Comparing the same
approach with the ramp present and undone would separate replay geometry
from the cubes.

The session ended with an explicit guarded stop (owned game SIGTERM, −15).
The supervisor, Xvfb and game PIDs were absent afterwards. The baseline was
unchanged. Four lossless 1000×720 crops (`ramp-placed`, `ramp-undo`,
`ramp-redo`, `moving-undo`) were checked pixel-exactly against their source
captures. No runtime code changed.
