# Sandbox construction undo and redo

October6,2026. The C sandbox now has a five-action undo/redo journal for its supported half-meter cube/ramp cell edits. This is a source-derived implementation checked without launching either game. [The source audit](CONSTRUCTION_HISTORY_SOURCE.md) identifies original list, input, resource and lifecycle behavior; ordinary native cube acceptance is recorded below. Ramp and movement acceptance remain pending.

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

Focused checks exercise source rounding, mixed materials and ramps,64-cell transactions, sixth-entry eviction, redo invalidation, resource/refund/geometry failures, exact conflicts, mining exclusion, malformed journals, clone/move and save/world lifecycle. The real host/input regression checks chord/pointer ownership and immediate ray/sweep refresh with no fixed tick. Optimized full core/build, content and23 CLI checks pass, as do full core and focused actual-host ASan/UBSan with leak detection. The14 source-exporter tests have no skips; the broader Python suite retains51 optional skips, which are not passes. [PLAYTEST](../PLAYTEST.md) records commands and the separate renderer-only rebuild.

When the user next tests the build, place a small cube/ramp batch, undo/redo via keys and toolbar, repeat while moving, and verify a menu blocks the shortcut. Check that leaving/reentering B keeps history and loading clears it. Use an isolated checkpoint and account for source rounding when comparing material counts. No fresh original-game run is needed to recover these rules, and no graphical acceptance or ordinary hunt completion is claimed here.


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

Verification: full unrestricted `make -C rebuild check` passed 282 jobs,
980 Python tests with 98 optional skips; `check-sanitize` passed 113 suites
with leak detection, zero failures/skips. Logs are
`rebuild/build/compiler-tmp/build-acceptance-check-unrestricted.log` and
`build-acceptance-sanitize.log`. The initial sandboxed check failed five
Python jobs because LeakSanitizer cannot run under ptrace; it was rerun
outside the sandbox without disabling leak detection. All twelve evidence
crops were checked for exact source pixels.

Still unobserved in this acceptance: ramps, replay while moving, blocked
geometry/resource/refund transactions, sixth-entry eviction, failure to load,
small-window input and physical collision after replay. Headless checks cover
several of these separately. No original visual parity or difficulty claim.
