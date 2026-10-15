# Original-game playtest and C comparison

## October14 ordinary native repair cancel and replacement

[Acceptance](audit/CAMP_REPAIR.md#cancellation-and-same-category-replacement-session-02),
[evidence](reference/repair-cancel-20261014/checkpoints.json). Adventure,
1280×720, production binary `f5c07847…a8e`, earned `partial-reloaded` save
`f3e087ff…68bc`; session `repair-private-20261014-02` on `:120`, no original
launch.

- Cancel repair → Stop (Enter) with 15.5 s displayed: the job stopped and
  shovel instance 10 stayed in the machine at 3543. Wood stayed 193 (no
  refund).
- Replace selected with full Wood Axe instance 12 (3553): the axe went into
  the machine and the shovel took slot 427 at 3543. Repair was disabled for
  the full axe.
- Take item returned the axe to slot 425 and left the machine empty. Each
  instance was owned exactly once in every save.
- The Escape sent at `Sandbox ready` did not pause the game. Cancel still
  preceded completion.

## October14 ordinary native ramp replay

[Ramp acceptance](audit/CONSTRUCTION_HISTORY.md#rotated-ramp-replay-session-03),
[evidence](reference/build-native-20261014-ramp/checkpoints.json). Adventure,
1280×720, production binary `f5c07847…a8e`, earned five-cube checkpoint
`69bc9209…5c62`; session `build-private-20261014-03` on `:119`, no original
launch.

- A four-cell Wood ramp at 90 degrees was saved as `type=9` (shape 2,
  rotation 1), material 11 at x64–65, y58, z664–665. Wood went 190 → 189 and
  `built` went to 3.
- Ctrl+Z removed it (Wood 190) and Ctrl+X restored it (Wood 189). The saved
  cells were identical and `built` stayed 3.
- Ctrl+Z sent in one input batch with W, about 4.1 m from the ramp, showed
  *Cannot undo construction here.* Cells and Wood were unchanged; Undo/Redo
  states were consistent with the retained history.
- The contact and traversal attempts did not isolate the ramp's interior and
  establish nothing. Contact, climbing and a successful moving replay remain
  pending.

## October14 ordinary native construction history

[Native acceptance](audit/CONSTRUCTION_HISTORY.md#ordinary-native-acceptance),
[evidence](reference/build-native-20261014/checkpoints.json). Adventure,
1280×720, production binary, earned repair checkpoint; no original launch.

- One-cell placement/undo/redo: Wood 193 → 192 → 192 → 191.
- Four-cell batch and toolbar replay: final five cells, Wood 190, `built=2`.
- Save and B exit/reentry retain history; Pause blocks Ctrl+Z; a 1.5-second
  held chord undoes only one batch.
- A fresh private session removed the single cube, then actual Load restored
  the five-cell save. Paused re-save was byte-identical (`69bc9209…5c62`).
  Ctrl+Z and Ctrl+X then displayed empty-history messages; saved cells and
  materials remained unchanged.
- Claude reached its usage limit. Session 01 expired through its watchdog;
  Codex completed session 02 alone and explicitly stopped it. Both baselines
  stayed unchanged; owned process groups were absent.

Ramps, moving replay and blocked transactions remain outside this physical
acceptance. Full check: 282 jobs, 980 Python tests, 98 optional skips.
Sanitizer: 113 passed, zero failures/skips. Initial sandboxed LeakSanitizer
failure was resolved by the unrestricted full rerun.


## October14 ordinary native camp repair

Observed in the native production runtime with ordinary private-display input
([details](audit/CAMP_REPAIR.md#ordinary-native-acceptance),
[evidence](reference/repair-native-20261014/checkpoints.json)). No
original-game comparison. This answers the October 7 "manual visual acceptance
outstanding" note for the Wood Shovel path only.

- Session `rebuild/build/repair-private-20261014-01` (`:116`), from the earned
  trade save `21a80c12…1ac0`, unchanged afterwards. Binary `f5c07847…a8e`
  (runtime `35124c7`).
- Travel: walked from town toward the lot. The traced route's lot exit is a
  one-way drop, so a read-only height grid located an east ramp at z ≈ 399.5;
  the player walked up it to (−609, 401). No construction, excavation or
  relocation.
- Right-click opened the panel. Inserting the shovel (instance 10, 3543)
  quoted Wood: 1 and 60 seconds; Confirm paid 1 Wood (194 → 193).
- `partial`: running, 3,677,326 ticks, instance 10 at 3543 held by the machine.
  After a 10 s advance, Pause → Load and a paused re-save gave a byte-identical
  file (`f3e087ff…68bc`). After Resume the clock continued (34.9 s shown).
- Completion happened with the panel closed and the player away.
  `finished-in-machine`: instance 10 at 3553, not running, 0 ticks. Take
  returned it to slot 425. `completed` (`49ed477c…a96a`): machine empty,
  Wood 193, money 261.
- Teardown: explicit stop sent SIGTERM to the owned game (−15, supervisor exit
  241); no lease or process remained.

## October14 ordinary native selling and buyback

Observed in the native production runtime with ordinary private-display input
([details](audit/TOWN_SELL_BUYBACK.md#ordinary-native-acceptance),
[evidence](reference/trade-native-20261014/checkpoints.json)). No original-game
comparison.

- Session `rebuild/build/trade-private-20261014-01` (`:115`), from the earned
  food save `40e47365…e114`, which was unchanged afterwards. Binary
  `f5c07847…a8e` (runtime `e72816b`).
- One ordinary dig wore Wood Shovel instance 10 from 3553 to 3543 (saved
  `dug`). While it was equipped, Sell refused it ("Unequip this item before
  selling."). After the Knife was re-equipped it sold for 41, matching float32
  42 × (0.8 × 3543/3553 + 0.2) = 41.905 truncated.
- After close/reopen it was bought back for 41, and the vendor was credited 42
  (the source asymmetry). Saved state shows the same instance 10, slot 425 and
  durability 3543.
- Sold 10 Wood for 40 and bought back 4 for 16; the ×6 row survived
  close/reopen. Money 237 → 278 → 237 → 277 → 261; Xing Zhang's purse
  4480 → 4439 → 4481 → 4441 → 4457.
- Saved `traded`. A distinguishing sale of 1 Wood (265 / 4453, a separate ×1
  row) was undone by Pause → Load: the buyback list was empty and 261 / 4457
  restored. A paused re-save after a second load is byte-identical to `traded`
  (`21a80c12…1ac0`).
- Comfort HUD, measured on in-world frames: happy face. The fill's last teal
  column is 589 at saved comfort 303.53 (expected edge 589.85) and 587 at
  283.41 (587.28). This is a screenshot measurement at those frames' times; no
  revival flash was observed.
- Not checked: conversation digits versus quickslots, the small window, panel
  drag, a held close button, and other vendors.
- Teardown: explicit stop sent SIGTERM to the owned game (−15, supervisor exit
  241); no lease or process remained.
- Checks: full check 282 passed, 0 failed; 978 Python tests, 98 skipped (skips
  are not passes) (`rebuild/build/compiler-tmp/trade-check.log`). Inspector
  formatter tests: 4 pass. The sanitizer is the director's pre-push run.

## October14 Adventure HUD comfort fill, face and drop flash

Implemented in C and rendered in staged private-display fixtures only:
[comfort HUD](audit/COMFORT_HUD.md). No ordinary play or original-game
screenshot is claimed.

- `ui-shared-render-capture` keeps exact tutorial/Legacy sandbox HUD parity
  (93,170 / 23,320 / 5,936 pixels). With comfort staged at 0, the Adventure
  hunger capture still matches the tutorial outside the hunger track.
- Comfort 50/175/400 draws face3/face2/face1 with left-anchored fills of 7/23/52
  columns at 1280×720 (6.4/22.4/51.2 expected), and matches everywhere outside
  the face and track. A flash staged 0.2 s after a 25-point drop has alpha
  0.300 and differs only inside the track.
- At 320×240 the face is 3×3 pixels and face2 is identical to face3.
- Results (log paths are under `rebuild/build/compiler-tmp/`):
  - Full check: 282 passed, 0 failed; 977 Python tests, 98 skipped (skips are
    not passes; three are the UnityPy prefab tests, which pass under
    `check-sources`) (`comfort-check.log`).
  - `check-content` (`comfort-content.log`) and `check-sources`
    (`comfort-sources.log`) pass.
  - Render capture: `comfort-render.log`.
  - Pre-push sanitizer: 113 passed, 0 failed, 0 skipped, with leak checks enabled (`comfort-prepush-sanitize.log`; per-suite logs in `rebuild/build/test-logs/sanitize_*.log`).
  - `ui-shared-test`, `sandbox-ui-test` and `cli-test` pass (`comfort-ui-cli.log`).

## October14 ordinary native food loop

Observed in the native production runtime with ordinary private-display input
([details](audit/HUNGER.md#ordinary-native-acceptance-of-the-food-loop),
[evidence](reference/food-native-20261014/checkpoints.json)). No original-game
comparison.

- Baseline: earned completed hunt save, SHA256 `9a28960b…e0d2`, unchanged
  after both sessions. Binary `9ee96016…3d01` (runtime `6939ae0`).
- Session 01 (`rebuild/build/food-private-20261014-01`) was aborted by the
  watchdog before any purchase (`invalid input lease action`: the supervisor
  predated the new `wheel` action). Nothing is claimed from it.
- Session 02 (`rebuild/build/food-private-20261014-02`): bought three
  Compressed Biscuit I (391) and one Biscuit II (392) from Xing Zhang; money
  325→237, stock 35→32 and 20→19. Ate 391 from the bag; a 392 retry 0.40 s
  later used nothing. Dragged 391 to quickslot 4. A key-4 press with the
  inventory open was ignored by the panel gate (not a cooldown test); with it
  closed, key 4 ate one 391, and a retry 0.75 s later used nothing. Wall-clock
  gaps are not engine cooldown timings.
- Saves (Pause → Save): `purchase` hunger 292.713318; `eaten` hunger 325.773621,
  391 ×1, 392 ×1, quickslot 4 = 391. After 15 s of play, Pause → Load restored
  the saved clock; saving while paused gave a file byte-identical to `eaten`
  (SHA256 `40e47365…e114`). The food cooldown is transient and resets on load.
- The live hunger bar's fill moved 691 → 692 → 690 → 688 px, matching
  730 − 64 × hunger/500 (screenshot measurement).
- The quickslot eat showed no message, because the message line skips text
  identical to the previous message. Not fixed here.
- Teardown: explicit `stop` sent SIGTERM to the owned game (return −15,
  supervisor exit 241); not a clean application quit. No lease or process
  remained.
- Checks: listed in [HANDOFF](HANDOFF.md).

## October14 Adventure hunger HUD bar and starvation notice

Implemented in C and rendered in a staged fixture only: [the shared HUD
hunger bar and starvation notice](audit/HUNGER.md#hud-bar-and-starvation-notice).
No ordinary play or original-game screenshot is claimed.

- `ui-shared-render-capture` (private Xvfb `:110`) keeps exact tutorial/Legacy
  sandbox HUD parity at 1280×720, 640×480 and 320×240 (93,170 / 23,320 /
  5,936 pixels). An Adventure HUD at hunger 150/500 matches the tutorial
  outside the track (92,850 / 23,224 / 5,902 pixels) and fills the track's
  right 30% (`rebuild/build/ui-shared-captures/sandbox-hud-hunger-*.png`).
  The tutorial's empty track now draws rotated, as the prefab specifies.
  Inspected visually at 1280×720 and 640×480; at 320×240 the bar is present
  but too small to read.
- The starvation notice repeats on every start, a labelled adapter: the
  original's is one-shot per player lifetime. Ending shows nothing.
- Current results, after all code and test changes: full check 280 jobs, 966
  Python tests, 95 skipped (skips are not passes); sanitizer 112 suites with
  leak scanning; content and source regeneration pass; `ui-shared-test`,
  `sandbox-ui-test` and `cli-test` (the game executable's CLI only) pass. The
  exporter's `--help`, `--check` and `--localization` were checked directly
  and by `test_hunger_export.py`. Logs: `rebuild/build/compiler-tmp/hud-final-*.log`.

## October13 Halt lock and stop effect live in both hosts

By the user's decision to apply the lock as the original does,
[Halt](audit/HALT_STOP_EFFECT.md#live-in-both-hosts) now runs in the sandbox
and the tutorial. Implemented in C from source; the Move/Jump refusal was not
observed in the original, and the drawn stop is not compared with it. No
original-game launch occurred.

Headless: lock lengths are 14 and 30 steps at 60 Hz for walk and run stops,
and 53–56 for a sprint stop. Over the installed assets the real sandbox actor's largest
vertex displacement from the baked pose is 0.001 mm (walk stop), 22 cm (run
stop) and 80 cm (sprint stop). Mid-effect in a sprint stop the stepping foot
is 0.54 m along the motion and the chest 0.34 m forward and 0.23 m down, at
body yaws 0, 90 and 215 degrees.

Rendered: the tutorial 15,000-frame progression passes with the lock active
(5 stops, 65 locked steps, 14,853 solved poses, building lesson reached). The
three Adventure smokes pass. One private-display sandbox session on a newly
owned Xvfb display, with ordinary key input and the Adventure smoke fixture as
its checkpoint, sprinted sideways for 1.6 s and released: five captures over
the next 1.5 s show the run, a forward lunge with a deep crouch, then an
upright stand (`rebuild/build/halt-private-20261013a/stop-sheet.png`). That
session shows the effect draws a coherent pose; it did not measure the lock,
and its fixture checkpoint is not earned progress.

Follow-up the same day: the mask also refuses herb gathering in both hosts,
silently (source `DoAction(Gather)`); headless host tests cover it, and the
tutorial progression still passes with the same counts. Not observed in the
original and not exercised by a rendered session.

Also prepared, with no launch: the uneven-ground
[grounder capture](audit/FINALIK_GROUNDER.md#prepared-uneven-ground-capture).
Its probe compiles against the installed assemblies and its analyzer passes
eight offline tests on synthetic captures. With these follow-ups the regression passes 276
jobs/955 Python tests with 94 optional skips; the tutorial progression
smoke passes again.

**Observed in the original, October 13**: one supervised 120-second Adventure
session on sloped terrain, operator input only, 1,453 frames.
[`pe_grounder` agrees](audit/FINALIK_GROUNDER.md#observed-on-uneven-ground)
with the original's grounder to 0.2 mm and 0.23°, with the foot rotation
limit and spine bend fitted per frame because the game rewrites them while
playing and that capture recorded them only at attach. Rays hit only voxel
terrain. A first launch aborted before arming and recorded nothing. No host
calls the grounder; this is not a visual or rendered check.

Follow-up without a launch: the foot rotation curve and both setting writers
are [source-derived and implemented](audit/FINALIK_GROUNDER.md#settings-written-while-playing).
The curve's evaluation is checked against exact arithmetic, not the engine.

Step 3, implemented in C: [the grounder is connected](audit/FINALIK_GROUNDER.md#live-in-both-hosts)
to the drawn player in both hosts. Headless suites cover the shared host, the
solver path and the actor. Rendered smokes report foot requests up to 8.7 cm
on imported dune terrain (sandbox, 90 frames) and 3 mm on the tutorial floor.
**Not visually accepted**: no one has watched it on a slope, step or built
surface, and it is not compared with the original's drawn result.
**Owed by the user (October 14 request):** a manual look at the player
standing still on a slope, in both the sandbox and the tutorial — foot
placement, foot tilt and pelvis height while idle.
The 15,000-frame tutorial progression passed four times with the grounder
(14,853 solved poses and updates, foot requests up to 24.3 cm) and **failed
once**, with 14,614 poses drawn. That run's log was not kept, so the cause is
unknown; the grounder is presentation only and the passing runs match the
earlier counts exactly. October 14, after commit `c7c993a`: eight more runs,
logs kept under `rebuild/build/grounder-progression/`, all passed with
identical counts (14,853 poses and updates, 14,808 grounded, 0.243 m, 5 Halt
stops, 65 masked steps), 254–255 s each. The smoke uses a fixed step and its
desktop-input block is excluded, so the earlier failure is not reproduced and
remains unexplained; it may have overlapped another heavy job.

October 14, implemented in C: [trees and the ground contact](audit/FINALIK_GROUNDER.md#trees-and-the-ground-contact).
Trees catch the grounder's rays in both hosts and the ground angle comes from
each capsule's own support contact. Rendered smokes: sandbox 90 frames, 8.7 cm
on dune terrain and 1.2 cm on the Adventure fixture; tutorial 3 mm. The
15,000-frame progression passes with the same counts as before (14,853,
14,808 grounded, 0.243 m); the player cuts the lesson tree from beside it, so
no foot ray lands on it there. No tree was stood on in any run; this is
headless and smoke coverage only.

October 14, implemented in C: [hunger is connected in the Adventure sandbox](audit/HUNGER.md#connected-in-the-sandbox)
with PECSv23. The 90-frame character-panel smoke on the older Adventure
fixture shows Hunger 299.7 / 500 (the prototype's 300 after the first drain
pulse) beside Comfort 499.3. Full check 280 jobs, 963 Python tests, 95
skipped; sanitizer 112 suites; content regeneration and CLI pass. Eating,
starvation and the stamina penalty are covered headless only; nobody has
played with hunger, and no save from this version has been reloaded by hand.

Historical, grounder step 3 (October 13): full check 277 jobs, 957 Python
tests, 95 skipped; sanitizer 110 suites.

Historical, Halt step (October 13): full regression passes 275 jobs/947 Python tests with 94 optional skips;
skips are not passes. Sanitizer passes 109 suites with leak scanning, and the
installed-asset drive harness passes under ASan/UBSan. 26 CLI cases pass. No
exporter changed in this step. PECSv22 is unchanged.

## October13 always-on FinalIK solve connected to drawing

Implemented in C, not compared with the original: [the zero-offset solve](audit/PLAYER_IK_DRIVE.md)
now draws the player in the sandbox and tutorial hosts. No original-game
launch, desktop input or save occurred. A new staged group,
`player-ik-drivers` (64 parts in 23 groups), was installed; nothing was
replaced.

Headless, over the installed assets, the real sandbox actor drew every part of
every scene through the solve. The largest vertex displacement from the baked
pose was 5.6 mm idle, 7.4 mm walking, 2.6 mm running, 1.0 mm in Knife attacks
6–9, 18.8 mm in the running attack and 5.6 mm in tool actions; a dead player
stayed baked.

Rendered, unattended and without input: the sandbox 90-frame smoke reported 90
poses drawn through the solve and the three Adventure smokes passed. The
tutorial 90-, 1,500- and 15,000-frame smokes passed, the last reaching the
building lesson with 14,853 solved poses, 8 melee starts and 24 contacts. The
1,500-frame tutorial frame rendered with and without the staged groups
differs only in 36 scattered pixels along the player's arm, hip and leg; the
sandbox frames differ by at most one intensity step. Screenshots and logs are
in `rebuild/build/ik-drive-check-20261013/`. This establishes that the path
draws an intact character, not that the pose matches the original.

Found while checking: the tutorial host refused to start with the installed
22-clip sprint body. Its clock check now also accepts exactly RunSwordAttack
then SprintSwordAttack after the 20 shared clips.

Full regression passes 274 jobs/947 Python tests with 94 optional skips;
skips are not passes, and the installed-asset drive tests ran. Sanitizer
passes 108 suites with leak scanning, and the installed-asset drive harness
passes under ASan/UBSan. Content and source regeneration pass; 26 CLI cases
pass. PECSv22 is unchanged.

## October13 disconnected solver poses for the player clips

[The pose audit](audit/PLAYER_IK_POSE.md) records animated local poses of the
25 solver transforms, derived offline from the existing isolated-Animator bake
captures for the 31 staged female clips (991 samples) and staged as the new
`player-ik-pose` group. No game launch, desktop input or save occurred, and no
existing staged group was replaced.

`pe_fbbik` solves every staged sample. Baked limb segments match the prefab
rest lengths to a micrometre. Without offsets the solve is exact on 21 clips
and moves a bone by up to 1.9 cm on the other 10, which agrees with the
October 12 observation that the original's idle solve moved bones by up to
3.3 mm. The staged poses and the staged defence actor's skin palettes agree
to 2.4 micrometres over all samples for the 20 joints they share. Nothing is
drawn from these poses and no host calls the loader or `pe_ik_skin`.

Full regression passes 272 jobs/941 Python tests with 94 optional skips
(`ikpose-check.log`); skips are not passes, and the two real-pose tests ran
because the staged groups are installed on this machine. Sanitizer passes 107 suites
with leak scanning (`ikpose-check-sanitize.log`). 26 CLI cases pass. Content
and source regeneration were last run on the grounder tree above; no exporter
with a `--check` mode changed since. No graphics check was run; PECSv22 is
unchanged.

## October13 disconnected FinalIK grounder

[The grounder audit](audit/FINALIK_GROUNDER.md) records the installed
`GrounderFBBIK` update read from the saved complete REA inventory, a committed
77-method CIL excerpt bound to installed bytes, and the female settings
regenerated from `FemaleModel.prefab`. No assembly execution, graphics launch,
desktop input, save or recovered runtime asset replacement occurred.

The isolated C suite passes the female settings, flat ground, one-frame and
steady-state arithmetic over lower ground, a 0.2 m step, half weight, a drop
deeper than a step, ground above the root, 10° and 40° slopes, no ground, the
pelvis damper, first-update and repeated-time cases, disabled and zero weight,
and atomic invalid-input failure. A second double-precision transcription
agrees with C over 2,240 updates in 40 terrain scenarios to 4e-7 m and 5e-8
quaternion dot. This guards transcription only: rays are a caller callback and
the quaternion natives are conventional adapters. **Nothing was compared with
the original**; the October 12 capture was on flat ground, where the grounder
contributes under a millimetre. No gameplay gate was opened and no host calls
it.

Full regression passes 269 jobs/935 Python tests with 94 optional skips
(`grounder-check.log`); skips are not passes. Sanitizer passes 105 suites with
leak scanning (`grounder-check-sanitize.log`). Content regeneration passes
(`grounder-check-content.log`); source regeneration passes including all six
grounder-export tests under UnityPy (`grounder-check-sources.log`); ordinary
Python skips two of them. 26 CLI cases pass. No graphics check was run;
PECSv22 is unchanged.

## October12 observed FinalIK solver session

**Observed in the original.** Native Steam client, isolated bake profile,
launch-only shim, tutorial mode, female player, 1280x720, quality 0. The
operator stood, walked, ran and stopped sharply for 120 seconds; no input was
injected. `ReferenceFbbikProbe.cs` added two delegate subscribers to the
player's `FullBodyBipedIK` and removed them at the end; it wrote no solver
field, transform, input or save. Capture `female-fbbik-02.jsonl` holds 1,399
frames and ended at its time limit. A first launch recorded nothing because
the probe looked for the solver under the wrong object.

Replaying each frame's recorded inputs through `pe_fbbik`
([audit](audit/FINALIK_FBBIK.md)): no failures; bone position error max
0.29 mm, mean 0.003 mm; rotation max 0.18°; node position max 0.29 mm, while
the original's IK moved bones by up to 0.67 m. Live topology and settings
match the generated rig. Solver weight was 1 and every effector and bend
weight 0 on all frames, so pins, PullBody and bend goals were not observed.
Offset writers subscribe as `IKAnimEffectCtrl`, `InteractionSystem`,
`GrounderFBBIK`. The live solver root is the
`IK` object; the exporter now derives it and the rig has 25 transforms.

The same capture holds thirteen stops (eight near 4.7 m/s, five at 6.9–7.7).
[`pe_halt`](audit/HALT_STOP_EFFECT.md#observed-comparison), with foot, start
frame, speed and heading fitted per stop, reproduces the body, hand and
vertical shoulder offsets to 2.0 mm, including shoulder offsets up to 11.6 m.
The fitted foot matches the foot rule on observed foot positions in 13 of 13,
and fitted speed and heading match observed body motion within 0.47 m/s and
4.6°. The Halt mask was not observed. Frame rate was about 11 per second.

No gameplay gate was opened and no host calls the solver or the effect. Full regression
passes 266 jobs/926 Python tests with 92 optional skips
(`fbbik-observed-check.log`); skips are not passes. Sanitizer passes 104
suites (`fbbik-observed-check-sanitize.log`). Content and source regeneration
pass, including all six solver-export tests under UnityPy
(`fbbik-observed-check-content.log`, `fbbik-observed-check-sources.log`). 26
CLI cases pass. No rebuild graphics check was run; PECSv22 is unchanged.

## October12 disconnected FinalIK full-body solver

[The solver audit](audit/FINALIK_FBBIK.md) records the installed
`IKSolverFullBodyBiped` update read from the saved complete REA inventory, a
committed 229-method CIL excerpt bound to installed bytes, and the female rig
regenerated from `FemaleModel.prefab`. No assembly execution, graphics launch,
desktop input, save or recovered runtime asset replacement occurred.

The isolated C suite passes female bindings, zero-offset pose reproduction,
the zero-weight return, body/hand offsets with planted feet, position and
rotation pins, PullBody, the limb reach limit, the offset-only path, rigid
segment lengths, a `pe_hit_ik` head hit fed through the solver, and atomic
invalid-input failure. A second double-precision transcription agrees with C
on 84 random cases to 0.2 mm and 2e-6 quaternion dot. This guards
transcription only: both share conventional adapters for the engine's
quaternion natives, and no original pose was compared. GrounderFBBIK, the
pose held at Initiate and host skeleton mapping remain open; no gameplay gate
was opened.

Full regression passes 265 jobs/922 Python tests with 92 optional skips
(`fbbik-check.log`); skips are not passes. Sanitizer passes 104 suites/0 skips
with leak scanning (`fbbik-check-sanitize.log`). Content regeneration passes
(`fbbik-check-content.log`); source regeneration passes including all six
solver-export tests under UnityPy (`fbbik-check-sources.log`); ordinary Python
skips two of them. 26 CLI cases pass. Whitespace check passes and the
structural index is rebuilt. The owner is unconnected to both hosts, so no
graphics check was run and no visual acceptance is claimed. PECSv22 and
existing sprint/recovery behavior are unchanged.


## October12 disconnected AnimFollow drive and rig

[The drive audit](audit/ANIMFOLLOW_DRIVE.md) now records installed11-body/
10-joint/66-nonrigid correspondence, strength profiles and collider/material
metadata. The complete saved REA inventory and committed CIL were reused and
bound to installed bytes. No assembly execution, graphics launch, desktop
input, save or recovered runtime asset replacement occurred.

The isolated C suite passes source profile/identity checks, PD error history,
vector caps and unclamped totals,360-degree retention, shared damper, root/null
joint admission, signed cadence/wrap and atomic invalid/overflow cases. A
bounded decoded-CIL float32 operator replay agrees bit for bit with259 C force
requests/777 components. This qualifies the modelled arithmetic adapters,
not Mono/Unity native operators. Actual quaternion/joint evaluation, physical
rig consumption, material/friction behavior and reaction blending remain
unqualified; no gameplay gate was opened.

Focused source regeneration and all five exporter tests pass using UnityPy
(`animfollow-drive-focused-source.log`); ordinary Python skips its three
installed cases. Full regression passes263 jobs/916 Python tests with90 optional skips
(`animfollow-drive-check.log`); skips are not passes. Optimized build and26 CLI
cases pass. Sanitizer passes103 suites/0 skips with leak scanning enabled
(`animfollow-drive-sanitize.log`). Content/source regeneration and all five
installed drive tests pass (`animfollow-drive-content.log`). Local evidence
links and whitespace checks pass; the structural index is rebuilt. The C and generated
source owners are unconnected to both hosts; no new visual acceptance is
claimed. PECSv22 and existing sprint/recovery behavior are unchanged.


## October12 sprint Knife — source, C fixture and normal native session

[The sprint integration](audit/SANDBOX_SPRINT_KNIFE.md) installs only the
qualified401 leaf and distinct source-root curve. Body/Knife22 and defence31
replace complete old groups with preserved backups. No original client or
live desktop input was used. The separate reaction/get-up/shield gates stay
closed; PECSv22 is unchanged.

Actual exported-asset C fixtures pass open/wall collision, four-stage combos,
three handoffs,89 ticks/88 blade samples, shared post-move blade/16 defence
clocks, once-only contact advancement and exact combat/movement stamina.
Synthetic tests cover strict curve profile/malformed bytes, optional pose
admission, exporter selection and installation rollback. Normal production
private-input testing observes one sprint start and visible lunge, zero new
contacts/kills and unchanged checkpoint. Free third person, camera-facing
attacks ON, and Shift+s around one LMB pulse are recorded in
[the bounded result](reference/sprint-native-20261012.json). Existing4/4 hunt
kills belong to the loaded baseline. The sandbox returned main menu result2;
explicit supervisor stop then ended the process-15. Do not claim process
exit0, new progression, exact observed root timing/distance, user sprint
acceptance or original-game parity.

The user subsequently accepted the previous standing/walking one-click
recovery with “feels good!”; this is qualitative feedback, with exact
settings/timing unrecorded. The150ms natural recovery remains a native adapter.

Final sanitizer passes102 suites/0 skips (`sprint-final-sanitize.log`);
CLI passes26 cases (`sprint-cli.log`). Final full regression passes261 jobs/911 Python tests, with87 optional skips
(`sprint-final-check4.log`); skips are not passes. Content/source regeneration
passes (`sprint-final-content2.log`), including the sprint-only stage06. Earlier full checks caught old tool
and starter-sword tests comparing historical21/30 exports with the expanded
live body/defence. Their preserved baseline fixture paths now retain exact
clock, palette, prefix and input-hash assertions; no assertion was relaxed.
Local evidence links, screenshot hash and installed/staged hashes agree.


## October12 Knife single-click recovery — user feedback and C fixture

The user reported an abrupt/early-looking ending when standing or moving
forward, with one click then waiting. This is qualitative native feedback,
not exact timing or original-game observation. [Natural recovery](audit/KNIFE_RECOVERY.md)
now mixes the inactive tail into locomotion for a native150ms, without changing
mechanical combat/root clocks. Focused actor and exported-host consume checks
pass. A private optimized fixture passes25 Knife leaves/200 panels plus idle
and walking recovery sheets; both recovery sheets were inspected. The final
run exits0, checks readback tokens and unchanged staged inputs. No desktop
input, original launch, save or installed asset changed. User retest is pending.

The first full check could not compile the extracted input host after its
new actor-cancellation callback; its tracked extractor and mock were corrected.
The equipment-host mock also needed its action binding moved into the mocked
melee setter. Focused host tests and actor ASan/UBSan/leak scanning pass; the
full sanitizer registry passes101 suites with zero skips. The final full
check passes258 jobs/905 Python tests with87 ordinary-interpreter skips
(skips are not passes); CLI passes26 tests. Links, retained image hashes and
`git diff --check` pass, and the structural index is rebuilt. Logs: `knife-recovery-check-final3.log`,
`knife-recovery-sanitize.log`, `knife-recovery-cli.log` and
`knife-recovery-render-02.log`, all under `rebuild/build`.

## October12 native reaction animation preview — observed in fixture

The [private-display preview](audit/REACTION_PREVIEW.md) passes actual optional
reaction-group GPU loading, matching defence and25 clips/200 panels. All sheets
were reviewed, sprint/get-up in detail; props and sampled capsule wires follow
the body. Capture04 exits0 and verifies staged inputs unchanged. No live action,
save, installed asset or desktop input changed. The first capture contained
stale previous-frame screenshots; double rendering and a per-clip readback
token corrected it. Capture01 is unqualified.

`make -C rebuild check` passes258 jobs/905 Python tests with87
ordinary-interpreter skips (skips are not passes). `cli-test` passes26 tests.
Existing and outside-build output guards reject before display startup; local
links, retained image hashes and `git diff --check` pass. The structural index
was rebuilt. `make -C rebuild check-sanitize` passes all101 suites with
ASan/UBSan and per-suite leak scanning, zero skips. Logs are
`rebuild/build/reaction-preview-check.log`, `reaction-preview-sanitize.log`,
`reaction-preview-cli.log` and `reaction-render-04.log`. Registry sanitizer
coverage does not include the optimized GPU fixture or establish driver leaks.

## October 12 sprint root animation stage — source-derived only

[Sprint clip401 extraction](audit/SPRINT_ROOT_CHANNELS.md) passes the unchanged
bounded native root proof and its own starting-basis arithmetic replay. The
separate56-knot source group regenerates exactly; five synthetic exporter tests
pass. Source EndAction displacement is approximately2.70m forward, not an
observed sprint distance. The group is uninstalled and has no runtime reader.
No game launch, desktop input, save or installed-asset change occurred.
`make -C rebuild check` passes258 jobs and905 Python tests with87
ordinary-interpreter skips (skips are not passes). Logs are
`rebuild/build/sprint-source-check.log` and `sprint-source-content.log`.
`make -C rebuild check-content check-sources` also passes, including the new
sprint stage and the installed reaction/AnimFollow source checks. Local link
targets and `git diff --check` pass; the structural index was rebuilt. No C
code, allocation or ownership changed, so no new sanitizer or graphics
acceptance is claimed.

## October 12 host reaction group — implemented in C, headless only

`pe_sandbox_actor` now validates the staged reaction set as an
[optional second group](audit/REACTION_PRESENTATION.md#host-reaction-group),
and the sandbox host requires its matching 25-clip defence actor when the
group is present. Nothing selects or draws a reaction clip, the group is not
installed, and the live game loads what it did before. No game was launched,
no desktop input was sent and no save or installed asset changed.

Mocked and real-asset actor tests pass, the latter with the installed assets
plus the staged group mapped to its host folder names. The sandbox host's
start-up path with the group present was not run in a window, and no
graphics smoke was run for this change.

`make -C rebuild check` passes 257 jobs and 900 Python tests with 87
ordinary-interpreter skips (skips are not passes). `check-sanitize` passes
all 101 suites, the real-asset actor test also passes under ASan/UBSan, and
`cli-test` passes.

## October 12 reaction pose bake — observed original capture, staged headless

Observed in the original: one supervised isolated-sampler session produced
`female-reaction-01.jsonl` with the [prepared preset](audit/REACTION_PRESENTATION.md#bake-result-and-staged-group).
Mode: tutorial with a female player. Settings: isolated bake profile,
1280×720 windowed, `-force-gfx-direct`, no Zink override, Steam client
running. Inputs: the user entered the tutorial; the tool sent none, and the
arm file was created once the player stood in the scene. Outcome: 25 clips,
1,052 poses, end `complete`, 120-frame post-cleanup marker, then signal 9.
How the window was closed was not observed, so teardown is not validated.

Strict analysis validates every sample with all 42 bones moving in every
clip. NormalIdle repeats capture 06 to 4e-7 by bone path. The four mirrored
pairs are mirror images within 3.5 mm at the hands, so the engine applied
clip mirroring. BlockRepulsed is nearly static (about 1.5 cm hand travel).

Implemented as staged data: `rebuild/build/starter-pair-reaction-01` holds
the 25-clip body, sword, shield and defence group. It is standalone, not
installed, and not loadable by the current host. Twenty-one stage/attachment
tests pass with UnityPy and no skips; the ten stage tests pass under
ASan/UBSan. No native graphics, desktop input to the rebuild, save or
installed-asset change occurred. Poses are isolated leaves: no directional
blend, layer mix, ragdoll blend or root motion. Live reaction, get-up,
sprint-attack, shield, pounce and defense gates stay closed.
[Compact evidence](reference/reaction-pose-result.json).

## October 12 installed AnimFollow — compiled/serialized evidence, headless only

[AnimFollow lifecycle](audit/ANIMFOLLOW_LIFECYCLE.md) is implemented in the
disconnected `pe_animfollow`, bound to shipped Assembly-CSharp CIL and the
installed female's complete serialized component payloads. It supplies active
admission/body policy, controller flags, exact state/signal names and scalar
requests. PD error, velocity, Animator state, water/ray queries, quaternion
alignment, physics/joints/friction and scene cadence remain caller inputs or
requests. This establishes no rig solve, pose or physical/visual parity.

The view's ragdoll state reads mActive, distinct from the wrapper callback
flag. Source control reads the preceding drive error, waits for entry-transition
exit before orientation, and uses actual error/idle admission for completion.
The actual setter's RagdollLock tag is bound by the exporter, distinct from
collision-ignore tag IgnoreMe. No game or target assembly was executed, no
desktop input was sent and no player saves or recovered runtime asset groups
were changed. Live reaction/shield/pounce/defense gates stay closed.

Focused core and all six UnityPy exporter tests pass without skips. The final
runtime build passes. Final `make -C rebuild check` passes 257 jobs, 895 Python
tests and 87 ordinary-interpreter skips (skips are not passes).
`check-sanitize` passes all 101 suites with ASan/UBSan and leak detection.
`check-content` and `check-sources` pass; the latter compares regenerated
AnimFollow outputs and runs all six exporter cases with UnityPy and zero skips.
Documentation link and whitespace checks pass.

The initial core fixture expected the third-call alignment capture on its
fourth call and was corrected before full verification. Initial complete
MonoBehaviour reads exposed the stripped header limitation; a pinned metadata
layout now consumes every byte of both installed components. Final review
also corrected an assumed IgnoreMe body-policy label to the actual
RagdollLock setter literal and added its exported getter/test. Both full check
and sanitizer were rerun successfully after that correction; use the final
logs, not the preceding runs, as this checkpoint's baseline. Full leak-scanned
runs used unsandboxed supervision because of the already established ptrace
restriction. [The handoff](HANDOFF.md) records logs and next work.


## October 12 Wentfly, Knocked and GetUp — source-derived, headless only

[Ragdoll action lifecycles](audit/RAGDOLL_ACTIONS.md) are implemented in the
disconnected `pe_ragdoll_action`: ordered mask/view/motor/freeze requests,
exact source relations, partial source-fault reporting, readiness and deferred
force helpers. The player prefab overrides Knocked's default delay to 2s.
Its expiry requests immediate GetUp while keeping the action registered until
GetUp's relation cancels it. GetUp waits for actual view state rather than an
invented clip-length timer. No host connects this foundation.

No game was launched, no desktop input was sent, and no player saves or
recovered runtime asset groups were changed. No ragdoll physics, get-up pose,
live reaction, invulnerability or difficulty equivalence is established.
Installed AnimFollow base behavior, FinalIK solving and the prepared reaction
bake remain separate prerequisites.

Focused core and all six UnityPy exporter tests pass with no skips. The final
runtime build passes. Full `make -C rebuild check` passes 255 jobs, 889 Python
tests and 84 ordinary-interpreter skips (skips are not passes).
`check-sanitize` passes all 100 suites with ASan/UBSan and leak detection.
`check-content` and `check-sources` pass, the latter comparing the new outputs
and running all six new exporter tests with UnityPy and no skips. Documentation
links and whitespace checks pass.

Full check and sanitizer ran outside sandbox supervision because the preceding
round established LeakSanitizer's ptrace restriction. A preliminary focused
fixture failed: it began with no view, then erroneously supplied a present view
when asserting missing-view cleanup. Supplying the absent-view update fixes
the fixture; that failed assertion is not counted as a pass. Local logs and
next work are listed in [the handoff](HANDOFF.md).


## October 12 hit IK — source-derived, headless only

[Female hit IK](audit/HIT_IK.md) is implemented in the disconnected `pe_hit_ik`
from checked-in C# and prefab data: force/random-draw profile, exact transform
routing, concurrent/restarted part effects and additive FinalIK effector
requests, including pinned root displacement and duplicate links. No game
was launched, no desktop input was sent and no player saves or recovered asset
groups were changed. No visible reaction, stun/knockback, physical behavior or
original-game observation is established. The FinalIK solve remains missing.

Focused core and all six UnityPy exporter tests pass without skips. The
runtime build passes. Full `make -C rebuild check` passes 253 jobs, 883 Python
tests and 81 ordinary-interpreter skips (skips are not passes).
`check-sanitize` passes all 99 suites with ASan/UBSan and leak detection.
`check-content` and `check-sources` pass; the latter compares the regenerated
hit-IK outputs and runs its six exporter tests with UnityPy and zero skips.
Documentation link and whitespace checks pass.

The first sandboxed sanitizer invocation failed at LeakSanitizer's ptrace
restriction. The sandboxed full check also failed five existing leak-scanned
subprocess suites (directional player, running player, baked shovel, baked
pickaxe and training UI input). Both complete checks were rerun successfully
outside sandbox supervision; the failed attempts are not passes. Local logs
and next work are listed in [the handoff](HANDOFF.md).


## October 12 Halt stop effect — source-derived, headless only

[Halt](audit/HALT_STOP_EFFECT.md) is recovered from checked-in C#, the female
prefab curves and the action table into the disconnected `pe_halt`. Neither
host calls it; nothing was observed in the original or rebuilt game. It yields
IK effector offsets, not a pose: the FinalIK solver is not recovered. The
original's Halt mask blocks Move and Jump for 0.2–0.9 s after a stop; the live
sandbox is unchanged and does not.

Results: `make -C rebuild check` passes 251 jobs (877 Python tests, 77
ordinary-interpreter skips, which are not passes); `check-sanitize` passes all
98 suites; `check-content` and `check-sources` pass, the latter running the
six Halt exporter cases with UnityPy and no skips.

## October 12 sprint attack timing — source-derived, headless only

Skill 20110013 and clip 401 are bound by the reaction exporter; `pe_combat`
gains the sprint event clock and `pe_sandbox_melee` a fail-closed readiness
flag. No host sets it, so live play is unchanged: a press above 7 m/s is still
rejected. Nothing was observed in the original or the rebuilt game for this
increment. `make -C rebuild check` passes 249 jobs (871 Python tests, 74
ordinary-interpreter skips, which are not passes); `check-sanitize` passes all
97 suites; `check-content` and `check-sources` pass, the latter running the
sixteen reaction-contract cases with UnityPy and no skips.

## October 12 sword lunge motor — implemented, headless and private-display checks

[The lunge motor](audit/SWORD_LUNGE_MOTOR.md) changes the live Adventure
sandbox: blade root motion now reaches the player in the original controller's
order (previous-frame delta, 10 m/s hold, one-tick latch, coast before the
first request, cancelled tail) instead of in the tick that produced it.

Implemented in C and checked headless: in the constructed flat fixture the real
staged RunSwordAttack lunge travels 3.9405 m by the earlier direct path and
3.8666 m in source order, never exceeds 10 m/s per tick and owns exactly one
tick after the action ends.

Actual native observation, private display `:100`, production build, unchanged
checkpoint `strafe-physical-start.pesave`: the knife was drawn and four blade
attacks started with no motion error. None was a running start, because the
private input tool cannot hold a movement key while clicking. The session was
stopped by the tool, not by a menu quit.

Not observed: a running lunge in the live host after this change, and any
trajectory comparison with the original game.

Results: `make -C rebuild check` passes 249 jobs (868 Python tests, 73
ordinary-interpreter skips, which are not passes); `check-sanitize` passes all
97 ASan/UBSan/leak suites. PECSv22 is unchanged.

## October 12 reaction animation contract — static and headless qualification

[The animation contract](audit/REACTION_PRESENTATION.md) reads the installed
player controller, its state behaviours and clip identities for Whacked and
Repulsed. `pe_whacked` implements the Whacked lifecycle and its nominal BeHit
exit clock in an isolated C owner. Neither gameplay host calls it.

Source-derived, not observed in play: Whacked ends on the BeHit state's exit
event `HitAnimEnd`, nominally 0.933 s after the trigger; the hit clip is chosen
by `MoveStyle`; Repulsed plays a separate stagger clip set selected by local
velocity. No reaction pose is baked. Unity's transition frame timing, blend
weights and layer mixing are unqualified.

A `hit-reaction` bake preset is prepared (later the same day extended to 25 clips, 1,052 samples at 30 Hz, with the get-up and sprint-attack clips). Its
clip plan validates against the installed controller and the sampler compiles;
the original game was not launched.

Results: `make -C rebuild check` passes 248 jobs (868 Python tests, 73
ordinary-interpreter skips, which are not passes). `check-sanitize` passes all
96 ASan/UBSan/leak suites. `check-content`, `check-sources` and `cli-test`
pass; `check-sources` runs the fourteen reaction-contract cases with UnityPy
and no skips.

This increment also corrects the October 12 HumanPhyCtrl page: its statement
that some `ApplyMoveRequest` callers were absent from the checked-in C# was a
search error on ISO-8859 source files. The callers are in the C#.

No game, desktop input, normal save or installed asset modification was needed.
PECSv22 and live damage remain unchanged.

## October 12 HumanPhyCtrl consumption — static CIL and headless qualification

[HumanPhyCtrl qualification](audit/HUMAN_PHY_CTRL.md) reads the compiled-only
controller from the installed Assembly-CSharp CIL and binds it to the female
model's serialized values, Rigidbody, capsule and the project's 0.02 s fixed
step. `pe_human_phy` implements the request latch, ground/water/drag state and
per-FixedUpdate force requests in an isolated C owner. Neither gameplay host
calls it. It integrates and collides nothing.

Source-derived, not observed in play: requests add and are held to 10 m/s; a
request is consumed one frame after it is made; only two FixedUpdate calls run
per Update; a grounded body's whole velocity is replaced by the request. The
PhysX velocity-snapshot assumption, contact response and Mono float width are
unqualified.

Results: `make -C rebuild check` passes 246 jobs (852 Python tests, 67
ordinary-interpreter skips, which are not passes). `check-sanitize` passes all
95 ASan/UBSan/leak suites. `check-content` and `check-sources` pass; the latter
runs the sixteen HumanPhyCtrl cases with UnityPy and no skips and compares the
committed CIL excerpt with the installed assemblies. The decoder's instruction
count equals REA's for all 51,847 method bodies in the three assemblies.

No game, desktop input, normal save, pose capture or installed asset
modification was needed. PECSv22 and live damage remain unchanged.

## October 10–11 admitted Repulsed — source and headless qualification

[Repulsed qualification](audit/REPULSED_ACTION.md) follows the live female
BeatParam binding, five authored curves, row 6 action relations and the original
clock/cleanup order. `pe_repulsed` implements those admitted-action requests in
an isolated C owner. Neither gameplay host calls it; HumanPhyCtrl consumption,
reaction poses, shielding and live stun/knockback remain separate gates.

Static REA inspection of the installed UnityEngine assembly derives the
initial zero-direction angle as 90 degrees. The first admitted reaction cannot
qualify a shield block from that direction. The focused IL record is checked
against the pinned assembly; this is static engine evidence, without managed
execution or a new original-game observation.

Full regression passes 243 jobs, with 834 Python tests and 61 optional skips;
skips are not passes. All 17 source/prefab exporter cases pass with UnityPy and
no skips. An independent double Hermite oracle checks 840 force-profile values
and 404 movement phases within explicit tolerances, without claiming bit-exact
engine equivalence. All 94 ASan/UBSan/leak jobs pass with no failures or skips.
Production build, content/source regeneration and all 26 CLI cases pass.
No graphics, desktop input, normal saves or installed asset
changes were needed; PECSv22 is unchanged.

## October9 building materials and NPC label visibility

The user's three08:34–08:35 native screenshots exposed fabric/roof depth holes and NPC names through the hotel wall. [Source recovery and implementation](audit/BUILDING_MATERIALS.md) qualify original cutout/two-sided building materials and add a native camera-to-label obstruction rule for Westina, Eitai and residents. Source bundles, both installed OBJ/MTL/PNG groups and their archived manifests retain their exact hashes; only new material sidecars are installed with staging/backups. No world save or gameplay state changes.

Private native render checks pass: legacy depth holes reproduce, corrected cutouts preserve backing geometry, reverse-side cloth renders, mixed material slots sharing a texture retain distinct cutoffs, opaque output ignores source opacity, and caller culling enable/face survives draws. Inspected legacy/corrected views cover dwelling, store, hotel and store underside in both crops. The label fixture hides the blocked anchor while preserving the clear label. Shared player/NPC/tutorial material GPU tests and portraits still pass. Selected [screenshots and hashes](reference/building-materials/result.json) are retained; the wall report includes desktop chrome and is recorded by hash rather than copied.

Production build and full regression pass240 jobs (815 Python tests,54 ordinary-interpreter optional skips). The new source-backed exporter tests pass3 cases with UnityPy and0 skips; content/source regeneration checks and26 CLI checks pass. The final ASan/UBSan/leak run passes all93 jobs with0 failures/skips, including the new headless material parser. Optional Python skips are not passes; a broader UnityPy discovery was terminated before completion and is not claimed as successful. These are controlled native fixtures and automated checks, not ordinary gameplay acceptance or a new original-game visual comparison. No original client, desktop input or normal player save was used.

## October9 checkout relocation — headless validation

[Portable paths](audit/PORTABLE_PATHS.md) replaces compiled checkout roots with executable-relative native assets and runtime fixture discovery. Future exporter source labels are relative; absent historical absolute paths resolve only with exact subtree and SHA256 agreement. Existing manifests and captures were not rewritten. The moved starter-pair checks pass17 tests, including the actual rig/attachment clocks and retained group hashes; actual pickaxe regeneration no longer skips its moved inputs.

Relocation fixtures move a compiled C path probe and a real Python environment to new directories containing spaces, then run from unrelated directories. Console launch and Bash/Zsh activation follow the new interpreter directory. Full regression passes238 jobs (812 Python tests,53 ordinary-interpreter skips); separate UnityPy discovery passes915 tests with0 skips. All92 sanitizer/leak suites,26 CLI checks and content/source regeneration checks pass. The rebuilt game contains no old-checkout strings. No original game, real graphics window, desktop input or normal player save was used; main-menu routing validation uses existing headless stubs.

## October7 in-game Settings access

Both hosts now expose Esc → Settings through the same `pe_ui_settings` input/view used by main menu. A successful toggle updates the live host preference and the existing configuration file. Back/Esc returns to Pause; opening and closing frames retain modal/pointer ownership and cannot also resume, save/load or attack. No world save payload changed.

Optimized production build, shared/menu settings tests (also ASan/UBSan with leak detection), sandbox UI routing, shared UI tests, three tutorial input checks (including extracted actual Escape dispatch under ASan/UBSan), and26 CLI checks pass. The tutorial harness needed its extraction boundary updated for the new Settings ownership guard; that fixture failure was corrected and the tests pass. Paired actual UI capture passes at1280×720,640×480 and320×240; Pause and Settings were inspected in both modes at normal/small sizes. The settings renderer remains shared and read-only. Captures/logs are under `rebuild/build/ui-shared-captures` and `rebuild/build/ingame-settings-*.log`.

These are automated input/render checks, not a new ordinary gameplay or original-Steam acceptance session. No user desktop input or user configuration was changed by testing. The installed executable SHA256 is `93f487d21abe215b9ae332ea42245370eb56e276d1e18ee9852529e0436496b8`.

## October7 camera-facing attack preference

Source-derived: the original default uses requested movement, falling back to body forward; `AttackWhithMouseDir=true` instead projects the cursor ray onto a player-height plane. No new Steam session or claim about the user's original saved setting is made. [Direction and preference contract](audit/MELEE_TARGET_FACING.md#optional-camera-facing-attacks).

Implemented in C: main-menu Settings exposes a persistent default-OFF camera-facing attack preference, scoped to free third person. Both hosts read the same preference/helper; Adventure retains target assistance and frozen stages, and tutorial retains its stationary combo adapter. No PECS payload or Sword/Shield activation changed.

Automated checks pass: optimized build; preferences load/save/error rollback; actual-menu keyboard/mouse ownership, five-entry navigation and save failure/retry; pure direction/mode/invalid-input tests; real-asset sandbox host idle/moving/default/camera starts, frozen stages and latest pressed continuation; focused ASan/UBSan with leak detection for preferences, menu, direction and real-asset sandbox facing. Shared UI, sandbox UI, training asset clocks, consumable host regressions and26 CLI checks pass. This is focused verification, not a new complete runtime/asset suite.

The actual menu renderer passes scripted OFF→ON persistence at1280×720 and800×450 on the qualified private display. [Normal OFF](reference/attack-settings/settings-off.png) and [small ON](reference/attack-settings/settings-on-small.png) were inspected; labels stay inside the panel. Paired tutorial/sandbox HUD/history pixel captures pass at1280×720,640×480 and320×240; normal/small HUDs were inspected. These are staged render checks, not physical attack acceptance. Settings tests write isolated files under build, never the user's preference or world save. Original Steam and user desktop input were not used.

The actual tutorial host also passed isolated scripted runs with both values: OFF ran15,000 frames to mission756 with nine NPCs,19 construction additions/one removal/score50; ON ran3,600 frames through combat to mission719. Both exited0 and recorded eight melee starts,24 contacts and hareHP0. Logs are local under `rebuild/build/preferences-training`. This checks regression and preference loading in the lesson host, not an ordinary manually earned combat run or visual original parity. The final installed executable SHA256 is `0e2810885a3dcf4524e74f45f3103c285816e80db6e442df35745b6c8a34c1d0`.

Session: 2026-10-03. Reference: installed Linux Planet Explorers V1.1.4, Unity 5.2.4f1, launched with `rebuild/tools/launch_reference.py` into the isolated reference profile. No Unity editor or plugins were needed. The original source/assets and normal saves remain separate from the rebuild.

## Evening sandbox reference pass

The [sandbox alignment audit](audit/SANDBOX_ALIGNMENT.md) records a separate isolated native session in Adventure Normal and Single Player Build. Adventure passed the observed supply→learn Wood Shovel→craft→equip→excavate→place dirt→manual save/load sequence. Counts included wood200→190 and dirt0→6→5; reload preserved the recipe, equipment, counts and visible placed block. Build began with large stockpiles and learned recipes, kept its clock at13:00, and did not debit the tested dirt placement. These are original-client observations, not new C parity results.

Adventure slot1 and Build slot2 are preserved under `/home/admin/.local/share/planet-explorers-sandbox-reference/PlanetExplorers/GameSave`. Only Adventure was reloaded. The reference process was then closed through Quit Game. No C gameplay implementation changed or C test suite reran for this documentation-only pass. See the linked audit for screenshots, full generation settings, exact limits and ordered implementation gates.

## Observed original behavior

| Scenario | Input and observed result | Evidence |
| --- | --- | --- |
| Tutorial entry | Tutorial opens Andy’s simulation deck and the Camera mission. Default camera is third person. | [Original tutorial](reference/native-tutorial.png) |
| Control camera | F5 changes to Control Mode. Its tip says pointer movement turns the camera and Tab reveals the cursor. | Captured during the live session. |
| First-person camera | Another F5 changes to First Person and the game reports Camera completed. | [Camera completion](reference/camera-completed.png) |
| Basic movement | WASD movement, a grounded Space jump, grounded Shift+W sprint, and middle-mouse autorun complete the Move lesson. Sprint attempted during a jump did not satisfy the grounded sprint objective. | [Move completion](reference/movement-completed.png) |
| Interface explanation | Andy explains quickbar, menu, minimap and mission log. Advancing its dialogue starts the location-based Move mission. | [Location mission](reference/location-mission.png) |
| Location movement | Navigating to the marked area completes Move and starts Interaction, asking for five Herbal Juice and right-click storage access. | [Location completion](reference/location-completed.png) |
| Storage transfer | E opens the emergency kit, showing a five-juice stack. Right-clicking that stack moves all five into inventory, closes/removes the kit and completes Interaction; Andy asks the player to return. | [Storage opening](reference/storage-open.png), [transfer and completion](reference/storage-transfer.png) |
| Return to Andy | Reaching Andy marks Move [Com]; right-clicking Andy and selecting “I have the items from the storage box” completes it and opens medicine dialogue. | [Reply menu](reference/andy-reply-menu.png), [completion](reference/andy-return-completed.png) |
| Inventory grant | After Andy's explanation, opening inventory shows ten Herbal Juices and an additional five-item grant notification. The five carried from storage were retained. | [Inventory and grant](reference/inventory-extra-grant.png) |
| Quickbar assignment | Dragging Herbal Juice from inventory to a valid quickbar slot completes Items and opens “Good job!” dialogue. The stack remains ten. | [Assigned quickbar](reference/quickbar-assigned.png) |
| Consumption | After the use prompt, pressing the assigned number 2 consumes one juice, displays a cooldown, completes Items, and opens “Confirmed.” The stack is nine. | [Consumption](reference/juice-consumed.png) |

Mission IDs 739, 747, 718, 740, 741, 742, 743, 744 and 749 come from the [source/tutorial audit](audit/TUTORIAL.md); screenshots establish the displayed behavior and order, not numeric IDs. Movement speeds and stamina constants were recovered from source/serialized models, **not measured from these screenshots**. Storage, inventory grant, quickbar assignment and consumption are now observed; knife equipment and the combat prompt are also observed below; successful combat, gathering, crafting, mining and building lessons are not yet confirmed by this session. Transferring the complete five-item stack does not resolve whether the legacy script also accepts a partial stack.

## Corrections incorporated in C

- Walk/run/sprint speeds 2/5/8 m/s, sprint entry at 15 stamina and consumption 5/s, jump cost 10. Z toggles walk and middle mouse toggles autorun; backward input cancels autorun. Jump trajectory and collision response remain provisional.
- Terrain picking and height queries intersect the rendered marching-cubes triangles. Exact source fixture cases and directional full-mesh comparisons cover previously missed surfaces.
- Mining follows source centered sample-box targeting, threshold retries, axial-neighbor support and strict distance from player feet plus one meter. Imported crop origins participate in nearest-even rounding. The visible digging marker uses the same selected sample.
- The playable `--training-room` mode connects the tutorial state machine to camera changes and successful controller actions through 739 → 747 → 718 → 740 → 741 → 742 → 743 → 744 → 749, then grants/equips knife 33 and enters 746. Source English dialogue, explicit acknowledgements, spatial objectives, one-time storage/grant operations, inventory-open edges and actual quickslot assignment are implemented. The independent validated snapshot is not yet connected to session persistence.
- Installed room/tool meshes, material assignments and diffuse textures can be recovered reproducibly. The C preview renders the room’s 55,618 triangles at the source placement. [C room preview](reference/rebuilt-room-deck.png). Lighting, emission, holographic effects, NPCs and gameplay collision are not reproduced by this preview.

## Validation and comparison limits

Integrated C tests and ASan/UBSan with leak checks pass. Content generation, launcher isolation, asset-coordinate/export checks and CLI validation pass. The generated-world and original-terrain smoke fixtures both completed 90 frames, with 9,765/9,644 terrain triangles and five region uploads each. The fixture gathers, crafts and equips a tool, mines terrain, places cube/ramp cells, starts another crafting job and round-trips through a save.

The room preview also completed 90 frames. A raylib 6 OBJ material-table insertion hang was reproduced and corrected through guarded, collision-free material symbols; [asset recovery details](audit/ASSET_RECOVERY.md) document the dependency workaround. A successful preview establishes recovered geometry/material loading, not complete visual parity.

The separate playable training mode uses all three recovered room colliders (56,446 triangles) through a static BVH and swept capsule controller. Its 90-frame graphics smoke reached mission 718 with camera mask 3 and movement mask 15. A separate 2,381-frame interactive session completed the same lessons through actual F5, W, Shift+W, Space and middle-mouse inputs, then canceled autorun with S and exited through Escape/Q. [Interactive C training room](reference/rebuilt-training.png). Tests cover floor support, wall sliding, a 25 cm step, ceiling contact, source speeds/stamina, and the source three-second jump-action timeout during a long fall. [Collision details](audit/SCENE_COLLISION.md).

The largest remaining gaps are original character/camera/animation behavior, later mission/dialogue integration, full inventory/use/quickbar behavior, and original resource-prop placement. Training has Herbal Juice, one knife and one quickslot, plus HP and source timed healing/cooldown, and the herb lesson ledger. The new optional stationary combat integration adds knife damage and death dispatch; its 3600-frame graphical check has now passed through 719. Mission persistence remains absent. The remaining HUD and generic inventory/item-use system are incomplete. Its fallback capsule avatar, jump trajectory, immediate acceleration, camera smoothing and lighting are substitutes; static room collision does not replace the sandbox’s provisional terrain collider. The current bounded world and procedural placeholder trees are not the original full world.

## Automation notes

Reference automation targeted only the original client window. The C input check used a separate temporary driver that verified the training window title and rebuilt executable path before focusing it. Screenshots capture its window bounds after focus settles. Stable X11 keycodes through XTest work with this old Unity player; dynamically remapped Wayland keyboard injection caused F5 to be interpreted incorrectly. Mouse look uses small relative XTest motions; clicks use Wayland virtual-pointer events with a small preparatory movement so repeated clicks register. The 150 ms button hold satisfies the source’s strict 250 ms right-click interaction limit. Every held key is released after the action. Later navigation is driven by screenshots rather than assumed completion after a key press.

The proven actions are now available through [reference_input.py](../rebuild/tools/reference_input.py), with eleven headless targeting/key/coordinate tests, including bounded drag endpoints. The successful original quickbar action was `drag 806 256 420 670` in the 1280×720 client; these coordinates belong to that captured layout, not every window size. Its pointer protocol is bundled with the original license; the helper builds locally on first click. See the [usage instructions](../rebuild/README.md#automated-reference-input).

The live storage comparison also confirmed that closing inventory can leave a separate tutorial tip open. Camera look remains unavailable until that tip/dialogue sequence is dismissed; an issued look command is not evidence that the camera rotated. The driver itself executes bounded inputs, while screenshot inspection verifies the resulting game state.

## Extended C interaction playthrough

The 1500-frame graphics smoke physically walked the recovered room through storage and the return target, then exercised the core inventory/quickbar APIs. It ended at mission **745**, camera mask **3**, movement mask **15**, with **nine** Herbal Juices. The optional original emergency-kit model loaded at its authored location; it shares the exact source mesh/material recovered from the installed emergencykit3 bundle. Its separate box collider is not yet active.

A separate real keyboard/mouse session completed the same sequence without setting player position or mission state. It acknowledged dialogue, walked to the kit, opened it with E, right-clicked the stack, returned to Andy, acknowledged the reply, and opened inventory. [Storage opening](reference/rebuilt-storage-open.png), [transfer](reference/rebuilt-storage-transfer.png), [ten-item inventory](reference/rebuilt-inventory-ten.png).

Dragging outside the quickslot left mission 744, ten items, and an empty slot unchanged. Dragging onto slot 1 advanced to 749 without moving the inventory stack. After the two use lines, pressing 1 consumed one item and reached 745 with nine remaining. [Rejected drop](reference/rebuilt-rejected-drop.png), [valid assignment](reference/rebuilt-quickslot.png), [completed sequence](reference/rebuilt-items-complete.png). The final session log records **12,753 frames**, camera mask 3, movement mask 15, kit-transferred and Andy-reached flags, and final position `(28.769,1.647,13.026)`. Exit through Escape/Q completed normally.

An earlier interactive attempt was terminated with SIGTERM by its hosting command session after storage transfer; no coredump was recorded. The repeated complete input check used a detached process and normal UI exit. No game state was patched to advance the verification. Screenshots retained here contain the tested game window; a transient screenshot taken during window/workspace settling is excluded.

Integrated core tests, ASan/UBSan/leak checks, thirteen CLI cases and reference-input tests pass. Optional installed-asset tests passed with UnityPy; the default environment skips those two cases. Review also fixed speaker labels, immediate camera-mode pitch clamping, early smoke-window closure returning success, and focus loss being handled after drag-release events. The completed input session exercises normal interaction; synthetic focus-loss/drop regression remains untested. The 24-byte PETT v1 snapshot still supports only its earlier mission subset and rejects later states rather than losing extended inventory/dialogue data.

## Equipment, medicine and recovered actor extension

In the original client, continuing past consumption grants Knife ×1. The Equipment tab displays it; right-click equips it and completes the equipment objective. The next prompts explain arming/attacking, and the kill objective shows Lepus Hare 0/1. One nearby hare and a knife swing are visible. No hit, damage number or death was verified. [Original knife inventory](reference/knife-inventory.png), [equipped knife](reference/knife-equipped.png), [hare encounter](reference/hare-encounter.png).

The C implementation now grants the unique knife at 745, retains ownership through equip/unequip, advances only on a valid equip, and acknowledges the source combat-entry dialogue. The repeated 1500-frame graphics sequence passed at **746**, with **nine juices, owned/equipped knife, 500/500 HP**, camera mask 3 and movement mask 15. Final position was `(29.403,1.794,12.014)`. [C equipment sequence](reference/rebuilt-knife-sequence.png). This fixture uses movement through room collision plus interaction APIs; a separate pointer check is recorded below.

Medicine now has the source immediate heal, ten-second cooldown, replacement healing buff, HP clamp and no-resurrection behavior. Core tests cover strict one-second tick boundaries, the last tick before expiry at fixed step 601, replacement at cooldown step 600, zero HP and ownership/state guards. These timings are recovered source rules, not measured from a stopwatch in the original client. Integrated ASan/UBSan/leak and fifteen CLI tests pass.

The original hare mesh, diffuse and 48-bone idle/run animation render in C. The 90-frame actor smoke played both clips and exported a successful screenshot; visual inspection shows coherent limbs, texture and deformation. [Recovered run pose](reference/rebuilt-hare-run.png). The optional training actor idles at the authored encounter position, settled onto room collision, at fixed scale 1. That establishes asset recovery and rendering, not AI, attacks, damage or death. [Recovery details](audit/ACTOR_RECOVERY.md).

A second full C keyboard/mouse playthrough continued through the new equipment UI. After the same storage/Andy/juice chain, five equipment dialogue acknowledgements unlocked inventory; selecting Equipment displayed the owned knife, right-click equipped it, and the two combat-entry lines opened mission746. Right-click unequipped it, then another right-click re-equipped the same knife, with no repeated grant or mission transition. [Knife inventory](reference/rebuilt-knife-inventory.png), [equipped](reference/rebuilt-knife-equipped.png), [unequipped](reference/rebuilt-knife-unequipped.png), [combat boundary](reference/rebuilt-combat-boundary.png). Normal Escape/Q exit logged38,308frames, mission746, camera/movement masks 3/15, juice9, knife owned/equipped1/1, HP500/500, position`(31.260,1.830,13.132)`. Navigation included jumps onto the return platform; no position or mission state was patched. This process predates the leisure-patrol integration and validates equipment controls, not new AI.

The integrated actor viewer subsequently passed a630-frame check through all eight exported clips (idle/run/five melee/walk) with the recovered hitbox overlay active. Visual inspection shows the defence and forearm volumes following the deformed model; disabled source records are omitted. [Animated hitboxes](reference/rebuilt-hare-hitboxes.png). Each clip receives about1.31seconds, reaching every recovered attack window; this does not play the complete1.5-second attack2 tail. The new leisure-patrol training smoke also passed1500frames at 746, with nine juices and an equipped knife: hare mode1, position`(3.293,1.632,17.898)`, one patrol decision. Headless source geometry/timing tests and sanitizers pass. Player contacts and damage remained unconnected at that stage; the new integration is described below.

## Evaluated player recording and recovered outfit

A separate isolated native session reached the knife lesson using ordinary input and captured **566** evaluated frames over **45.60962 seconds**, with a clean stop-file end. All four female stationary attacks appeared in both camera views. Stable clip clocks establish TP rate=Animator.speed and FP rate=.65×Animator.speed; C tests now preserve that difference. The visible model root is authoritative for placement because the component/entity root remained stale. [Full calibration](audit/PLAYER_ANIMATION.md).

The recovered tutorial outfit selects female items95/131/192. Reconstructing the source appearance slots and per-submesh palette duplication reproduces the captured combined renderer's complete **67-entry bone-name sequence**. The C preview plays nine body/material parts, with two additional original knife material parts attached by the captured weapon-root matrices. Both nine-part and eleven-part graphics checks passed; the eleven-part screenshot shows the knife gripped during the sampled attack. [Recorded clothed knife pose](reference/rebuilt-player-knife-recording.png). Original shaders, morphs and general animation evaluation remain outside this result.

The first recovered-character integration rendered its recorded idle pose. A **1500-frame** graphics smoke completed mission746 with camera mask3, movement mask15, nine juices, one owned/equipped knife, HP500/500, and the patrolling hare. [Recovered player in training](reference/rebuilt-training-player.png). That earlier integration used a fixed standing pose during movement; the later locomotion integration below replaces it when baked assets are supplied. A matching captured stowed-knife recording now replaces the waist proxy when present; a second 1500-frame smoke verified its attachment. Both recorded body and weapon use the same root/yaw and appear only under their applicable visibility/equipment conditions.

The complete headless suite, 18 CLI tests and ASan/UBSan/leak checks passed after the scene retention fix and calibrated combat clock change. Subsequent visual-only training integration was compiled and checked by the 1500-frame smoke.

The PEAC2 recording format fixes the shortened final sample interval while retaining PEAC1's uniform-grid timing. All 48 native knife-capsule poses now agree with independent interpolation of captured blade matrices within **3.21e-7 m**. Fresh graphics checks passed for the eleven-part TP recording (100 frames), nine-part FP recording (40 frames), and the training room (1500 frames) using the version2 body/stowed assets.

The optional recorded-geometry melee fixture also passes with PEAC2. At explicit test target position `(0,0,1)`, yaw180°, normal scale/difficulty, it reports a Head1 contact at recording .25s, authored clip .106723234s. Source damage `19²/(29+19)×2` reduces HP212 to196.958328. Eight closed-window checks, two separated-target misses and one duplicate-hit check pass. This is a controlled native integration test with reconstructed event activation, not an observed original-game hit or a playable combat completion. [Reproduce the fixture](../rebuild/tests/REFERENCE_MELEE.md).


## Baked locomotion in training

The original sampler recovered 345 poses across ten female clips. A separate 62-pose idle capture reproduced every matrix of all 42 skin bones exactly, while both sessions exposed a native cleanup crash after writing complete output. This validates the retained pose data, not the sampler's teardown. [Investigation and limits](audit/ANIMATOR_BAKE.md).

Training now selects six clips from measured horizontal displacement and accepted jumps. A 90-frame graphics check passed at mission718 with camera mask3, movement mask15, and animation mask47 (idle/walk/run/jump/land). The original outfit renders coherently; omitted blends, directional/weapon locomotion, sprint clip and root motion remain explicit approximations. The optional two-part stowed knife shares the body's selected clip and time using recovered mountWaist matrices.

The extended **1500-frame** graphics check also passed at 746 with nine juices, the conserved equipped knife and HP500/500. Its animation mask 63 exercised **all six** locomotion states, including fall. The source-mounted knife remained attached in the rendered idle pose. [Current animated character in training](reference/rebuilt-training-locomotion.png).

## Stationary knife combat integration — native graphical check passed

The current C frontend connects the four baked female stationary third-person knife stages to `PeCombat`, the source `mountMain` blade capsule and the animated hare defence capsules. It shares body/blade authored time, charges stamina, gates contacts with recovered clip events, deduplicates targets per stage and updates actual target HP. A positive-to-zero transition emits the prototype 13 death receipt once. Source completion line 300230 must be acknowledged before719 presents 4072/300231; no herb progress or resource grant is generated by that transition.

The separate stowed/drawn exports preserve both original knife materials and exact ten-clip body timing. Six exporter tests pass, including native capsule poses and geometry-fingerprint rejection; the actual drawn asset passed345 capsule-pose checks under ASan/UBSan. Headless stationary-host tests cover combo stages, stamina, misses, permissions, deduplication, death, cancellation and atomic invalid input. These are not evidence of an original-game hit.

The **3600-frame graphical fixture passed with exit 0**, reaching mission 719 after the death and both gathering-entry lines were acknowledged. It recorded camera/movement masks 3/15, locomotion mask 63 (all six states), **eight attack starts and 24 actual capsule contacts**, and hare HP **0/212**. Player HP remained 500/500, with nine juices and one conserved owned/equipped knife. Pursuit used real controller movement through the room; neither player position nor target HP was assigned to force completion. [Completed C combat sequence](reference/rebuilt-training-combat-complete.png). The run log is `/tmp/pe-training-melee-3600.log`.

The full core suite, 18 CLI cases and ASan/UBSan with leak checks also passed (`/tmp/pe-melee-validation.log`). A second 3600-frame run repeated the same 24 contacts. The [first-contact framebuffer](reference/rebuilt-training-melee-contact.png) shows coherent blade grip and hare placement. The updated capture waits two display updates and shows target HP 204/212, with coherent body/blade placement. Reproduce the sequence with the command and optional asset-installation instructions in the [rebuild README](../rebuild/README.md#stationary-training-combat-assets-and-check).

Known limits: armed movement still uses normal locomotion; drawing/stowing switches attachments immediately without its source animation; aiming follows the camera provisionally; damage eligibility uses a fixed tutorial-pair whitelist rather than the full reputation/faction pipeline. The hare remains a leisure actor with no hostile attack simulation or ragdoll. Contacts are instantaneous capsules, without swept history; moving, airborne and first-person attacks, root motion, full blends and procedural IK are unsupported. The original client’s successful hits/kills remain unobserved. The herb ledger connects the frontend from 719 to 748; its 6000-frame graphical check passed, and the subsequent9000-frame check verifies tree cutting and crafting through751.


## Herb lesson 719 → 748 — graphical check passed

The C controller now starts the source herb ledger only after combat completion dialogue. After 4072/300231, E selects the nearest live eligible herb at its recovered world position. Successful input starts the 1.1-second depletion timer and locks requested movement/jump until depletion. Five depletions grant one safety axe 1527 atomically and enter 748 with 300233. Herbs produce no inventory resource. No knife put-away restriction is imposed. Repeated actions, dead/modal starts and out-of-range inputs cannot duplicate progress; the headless regressions and ASan/UBSan/leak checks pass.

**The 6000-frame graphical fixture passed with exit 0**, walking to all five actual positions after completing combat. It reported mission 748, five depleted herbs, axe count 1 and ledger mission 748. Nine juices, the owned/equipped knife and player HP 500/500 were preserved. Combat recorded eight attack starts, 24 actual contacts and hare HP 0/212; locomotion mask 63 exercised all six states. [Completed herb lesson](reference/rebuilt-training-herbs-complete.png). The full core suite, 18 CLI cases and ASan/UBSan/leak checks also passed (`/tmp/pe-gathering-validation.log`). The earlier 3600-frame fixture still reaches 719 without gathering input. Commands and optional recovered-asset installation are in the [rebuild README](../rebuild/README.md#training-herbs-and-next-lesson-boundary).

The C nearest-herb input differs from source independent herb listeners, which can start more than one in-range herb on the same key. Rendering shows recovered diffuse meshes directly, without the original hologram effects, appearance fade or player gather animation. No original-game herb interaction has been observed in this session, so this implementation is source-derived rather than a demonstrated original-runtime parity result. The subsequent9000-frame run verifies tree cutting and crafting through751; mining remains under implementation.


The next headless controller bridge, 748→ 757→ 721→ 751, passes strict compilation and ASan/UBSan/leak tests. Coverage includes shared-slot knife/axe ownership, missing held/ray/readiness gates, modal/dead starts, released-cut completion, seven-hit wood conservation, blocked grant retries, script learning, premature replicator opening, exact 180-tick crafting and the second shovel grant. The subsequent9000-frame graphical run also passed through751 with11wood, zero scripts, two shovels, recipe1 learned and the axe equipped; knife ownership remained intact.


## Tree cutting and crafting — graphical check passed

The 9,000-frame run exited successfully at mission 751 after actual room movement, seven cuts, script use and 180 fixed crafting ticks. Final state: 11 wood, zero scripts, two shovels, recipe 1 learned, axe equipped, knife owned but unequipped, nine juices and player HP 500/500. Five herbs were depleted; combat again used eight starts and 24 posed contacts to reduce hare HP from 212 to zero. The complete core and Python suites, 18 CLI cases, and ASan/UBSan/leak checks passed before this run (`/tmp/pe-crafting-validation.log`).

[Tree cutting](reference/rebuilt-training-tree-cutting.png), [crafting in progress](reference/rebuilt-training-crafting.png), [completed sequence](reference/rebuilt-training-crafting-complete.png). Body and axe share the original female Felling1/2/3 baked clocks, recovered in 75 poses from original run 08. The original stayed alive for 120 updates after sampling; ordinary window close returned −9, consistent with its verified self-kill handler. This observes isolated clip poses, not an original cutting outcome or complete controller blending.

The tree has its authored solid capsule, blocks walking and jumping, and disappears after depletion. Source scripted hit timers remain distinct from the Felling2 clip event. Draw/stow, readiness and the final exit transition remain simplified. The tree capture was subsequently refreshed by the mining run, verifying the foliage alpha cutoff and wrapped-text corrections.

## Mining lesson — 12,000-frame graphical check passed

The unattended run exited successfully at mission 752 with five swings, three actual material-1 removals and 9,006 dirt. Hits occurred at frames 3857, 3937, 4017, 4097 and 4177. Local samples were (17,6,20), (17,6,21) twice and (17,6,22) twice, corresponding to room coordinates (9,2,12–14). The first, third and fifth hits removed samples; intervening damage awarded no resource. Final terrain had zero triangles after the source mound cleanup. Shovel durability of 3528 and 3553 proves independent instances and five wear charges of 5.

The earlier sequence remained intact: eight knife starts and 24 contacts, hare HP 0/212, five herbs, seven tree hits, one consumed script, learned recipe 1, 11 wood, two shovels, nine juices and player HP 500/500. All six locomotion states ran. [Actual excavation](reference/rebuilt-training-mining.png), [completed run](reference/rebuilt-training-mining-complete.png). The foliage cutoff and wrapped-text corrections are visible in the refreshed [tree capture](reference/rebuilt-training-tree-cutting.png). Full core and Python checks, 18 CLI tests, ASan/UBSan/leaks, and content/marching-cubes/mound regeneration checks pass (`/tmp/pe-mining-validation.log`).

This is a source-derived C gameplay result, not an original-game dig observation. Run 09 recovered 91 isolated body poses and matching shovel attachments; the original remained alive for 120 updates and closed ordinarily with the installed self-kill exit −9. Source state transitions use discrete stages, and terrain uses temporary diffuse colors. A later bounded overlap-recovery helper is sanitizer-tested for manual crafting positions; this graphics run began outside the mound and did not exercise recovery.


## Construction lesson — 15,000-frame graphical check passed

The complete unattended sequence exited successfully at mission 756, after solving mission 753 through real construction. It made 19 single-cell additions and one single-cell removal. Each addition cost one dirt and the one-cell removal refunded zero, leaving exactly 8,987 dirt from the earlier 9,006. The source score reached 50: all 50 nearest-hit queries avoided the receiver colliders. The test then verified zero remaining construction cells and zero terrain triangles after source task cleanup. [Construction in progress](reference/rebuilt-training-building.png), [completed run](reference/rebuilt-training-building-complete.png), [final event log](reference/rebuilt-training-building-result.txt).

The preceding lessons remained intact: eight knife starts and 24 contacts reduced hare HP to zero; five herbs, seven tree hits, script learning and timed crafting yielded 11 wood and two shovels. Five shovel events removed three real samples. Equipped-shovel durability was 3528, the second shovel retained 3553, and the player finished with nine juices and HP 500/500. All six locomotion states ran. The source chain proceeds from 752 build-mode entry to 753 receiver blocking and then directly to 756; this C run stops before creation functionality.

The interactive frontend uses B for Build Mode, left click for a size-one dirt cube, Alt + left click for removal, and T for rotation. Placement targets source-style voxel boxes and the 32 authored terrain anchors, while movement and beam occlusion use actual shape geometry. Pending clicks survive render frames without a physics tick; dialogue acknowledgement clicks cannot also place blocks. Combined overlap recovery covers newly appearing terrain and receiver slabs. Those input and recovery cases have focused review and headless geometry regressions; the unattended run exercises the same editing/query APIs rather than physical mouse clicks.

Recovered receiver meshes, marker textures and transforms are loaded. Marker rendering reproduces source additive blending, half-intensity tint, two-sided drawing, disabled depth writes and the original absolute-distance endpoint behavior. The receiver procedural/hologram shader, textured line width/glow, appearance fades and particle lifecycle remain incomplete. Terrain and dirt blocks use temporary diffuse colors. No original-client construction outcome was observed in this session.

The full core and Python suites, 18 CLI cases, ASan/UBSan/leaks, content regeneration and terrain-source checks pass (`/tmp/pe-building-validation-02.log`). Two additional cross-fixture tests compare the compiled C rays and collider vertices against recovered source/installed assets and pass. The initial graphics attempt rejected noncanonical generated OBJ/MTL headers before frame 0; the exporter and native validation checks were corrected before this successful run.


## Nine tutorial NPCs

The original tutorial supplied nine completed per-Avatar idle captures: three female clips with 62 poses and six male clips with 70 poses,606 total. Each used its own original skeleton and effective humanoid clip. [Original tutorial after capture](reference/original-training-npcs.png). All files closed successfully; the original then received an ordinary window close. The configured 120-update post-cleanup watchdog was not observed before closing, so no watchdog success is claimed; see [capture details](audit/NPC_IDLE_SAMPLING.md).

The C frontend now shows Adisa, Moric, Vera, Allen, Anjum, Viola, Andy, Percy and Malus, using 12 original material parts, source x/z and orientation, and bounded capsule settlement against the recovered room. Andy uses the scripted 275-degree yaw. Nearby names are hidden by room walls, idle clocks pause with simulation, and the NPCs remain nonblocking as in the source layer matrix. The actual Andy model replaces his temporary capsule, retaining the existing lesson interaction.

`make -C rebuild training-npcs-smoke` passed 1750 frames with exit 0: all 9 characters loaded, 29.167 seconds of idle simulation, mission 746, kit transferred, Andy reached, 9 juices, one owned/equipped knife, full HP and all six player locomotion states exercised. The last 250 frames inspect NPCs through a test-only camera; player position and mission state are not teleported or patched. Each of the nine screenshots was inspected for recognizable appearance and intact posed geometry. Malus's lower body is occluded by the foreground railing in his gallery camera. [Andy](reference/rebuilt-training-npc-9041.png), [Adisa](reference/rebuilt-training-npc-9004.png), [Viola](reference/rebuilt-training-npc-9031.png), [run log](reference/rebuilt-training-npcs-result.txt).

Core, Python exporter/analyzer/sampler tests, 18 CLI tests and ASan/UBSan/leak checks pass. Native asset validation also covers every sampled pose. This adds appearances and idle presentation; optional side conversations, NPC behavior trees, facial morphs and Percy's weapon attachment remain unimplemented.


## Original-source UI integration — manual checks and complete progression

The current training UI replaces the earlier flat panels with recovered atlas frames, bitmap-font metrics, item icons and portraits. Inventory/storage category grids, independent C character information and its rotatable recovered-player preview, the source mission tracker/log, tutorial Replicator and supported menu actions are integrated. The static tutorial minimap and nine NPC markers follow recovered source inputs. Thirteen frame/health/dialogue/window effects use the original masks/noise with translated specular and holographic scanline shaders; no phase-identical Unity framebuffer claim is made.

The initial HUD/dialogue/minimap effects were inspected in `/tmp/pe-ui-effects.png`. That is a rendering check of the first seven roles, not evidence that all thirteen roles or every window/input combination has passed graphics validation. The latest complete test suite and positive UI core harness pass. Manual checks passed at1280×720 for welcome/Tips, I/C/Q/Y windows, the source blue cursor, inventory category clicks and close button, mission close and menu→Replicator. A954×1048 portrait window also passed character display and drag-to-rotate preview checks. The replacement UI’s15,000-frame rerun reached mission756 with all state checks satisfied: eight attack starts,24 contacts, five herbs,11 wood, two shovels with durability3528/3553,8,987 dirt,19 blocks added/one removed, receiver score50, HP500 and nine juices. The final tracked15,000-frame run after visual polish also passed with exit0 and the same expected state. The earlier 15,000-frame construction result and 1,750-frame NPC result above predate this UI replacement and must not be treated as its regression results.

The core now opens C independently of I, without granting items or completing inventory mission743. Right-click character-slot removal returns the same knife/axe/shovel, preserving ownership and per-instance durability; equipment still enters from the bag. Source startup lines300215 and300258 are acknowledged in order before camera objectives can count. Six generated-text tests pass against the original 19 missions and 24 targets; full core and targeted strict ASan/UBSan/leak checks pass for character state and startup ordering. These are headless results, not physical mouse/keyboard coverage of the replacement UI.

Interactive controls are Space/Enter/dialogue click to acknowledge, I inventory, C character, Q mission, Y Replicator, category clicks and equipment right-click, actual juice drag/drop into assignable slot1, and1/right-click that shortcut to use it. Character-preview dragging rotates the model. The menu exposes Inventory, Character, Replicator, Mission and Pause. Source-shaped inactive controls do not imply implemented general armor, full bag operations, additional shortcut pages, arbitrary crafting or complete settings/history.

Runtime font recovery corrected authored UILabel scales through the original `UISwitchFont`/`MakePixelPerfect` path: headings37→32, tabs/dialogue20→18 and overhead names30→26. Font metrics therefore follow native bitmap sizes rather than serialized label scale alone. The C dialogue policy blocks movement and F5 while any line is active; source input code does not establish that blanket rule. The first new smoke attempt exposed a harness dependency on changing camera during dialogue: its frame60 request was rejected, leaving first-person mode and preventing stationary third-person melee. The harness now retries third-person after dialogue clears; the core modal gate is unchanged. The subsequent tracked run reached the full progression result above and exited0.

Screenshots: [welcome](reference/rebuilt-ui-welcome.png), [Tips](reference/rebuilt-ui-tips.png), [inventory](reference/rebuilt-ui-inventory.png), [character](reference/rebuilt-ui-character.png), [mission](reference/rebuilt-ui-mission.png), [menu](reference/rebuilt-ui-menu.png), [Replicator](reference/rebuilt-ui-replicator.png), [portrait character view](reference/rebuilt-ui-portrait.png). These checks cover the named interactions and captured layouts, not every unsupported/general UI operation.

The [final tracked run log](reference/rebuilt-ui-result.txt) records the command, date, exit0 and PASS. Additional captures show [storage](reference/rebuilt-ui-storage.png), [quickbar assignment](reference/rebuilt-ui-quickbar.png), [crafting](reference/rebuilt-ui-crafting.png) and [completed progression](reference/rebuilt-ui-complete.png). Other storage-category buttons and general window dragging remain unsupported; rotating the character preview does not implement draggable windows. These results establish the tested C interface and progression, not full original UI parity.

## October 5 — sandbox UI and application routes

The C application was exercised with the bounded `rebuild_input.py` driver, selecting only its exact executable and current PID. Main Menu → New Sandbox → Main Menu → Tutorial → Main Menu → Continue Sandbox completed in one process. The retained sandbox still had mission9032, its supplied inventory and the learned Wood Axe formula after the tutorial visit. The tutorial had its independent fresh mission739. Both hosts released their graphics resources before the next window opened; closing the application exited0. [Route log](reference/sandbox/rebuild-menu-routes.log).

Actual keyboard/pointer inputs moved the player from32,32 to32,29, completed the provisional Arrival/supply interaction with E, opened Q/I/C/Y, used a recipe book with right-click, selected a Replicator row and used F9. Inspecting that real save confirmed recipe13 learned, one completed craft, wood190, two axes, one shovel, no pending craft, HP500 and ten juices. The [mission view](reference/sandbox/rebuild-supplies-mission.png), [supplied inventory](reference/sandbox/rebuild-supplied-inventory.png), and [learned recipe](reference/sandbox/rebuild-learned-recipe.png) show those states. The crate exercises source-backed grants; it is not evidence of recovered settlement/NPC generation. No new original-game/Steam comparison was performed in this session; the October3 original Adventure/Build fixtures remain the reference.

The [character view](reference/sandbox/rebuild-character.png) was inspected at1280×720 after resizing through1280×800. A prior resize while the cursor was captured produced an offset image and changed camera pitch. The host now releases capture on resize, ignores recapture mouse deltas for two frames, and starts at the menu's1280×720 size; the repeated resize check placed the HUD/minimap/panels correctly. This establishes the repeated local check, not a diagnosis of every compositor/driver combination. Screenshot export also flushes the final draw batch before reading pixels so the complete UI is included.

Core and source/exporter tests,22 headless CLI cases, sandbox UI routing and actor-adapter tests pass. The full core ASan/UBSan/leak run passes. Separate UI/actor sanitizer checks cover stable equipment/shortcut identity, modal input ownership, source clip clocks and render-failure handling. Adventure saves preserve learned formulas, one-time grants, health/medicine clocks and shortcuts; v1–3 rebuild saves migrate as Legacy. Valid airborne saves are not settled to ground when opening the frontend. Legacy retains its earlier controller/validator; Adventure uses the actual terrain/construction capsule geometry.

## October 5 — timed sandbox tools, trees and construction

The final bounded physical-input tool run used `python3 rebuild/tools/playtest_sandbox_actions.py`. Its helper creates fresh Adventure saves through starter/supply/equipment APIs, then deliberately positions only the player/camera for clear, repeatable views. Digging uses open ground at45,45; felling approaches isolated existing tree7. World density, tree positions/health/yields, inventory and pristine3553 durability are not patched to manufacture outcomes. The host uses its new6m camera default. Earlier captures at the dense32,32 spawn verified edits but obscured the actor; those are not the visual acceptance images below. No new Steam/original-client run occurred.

Each case opened the actual C application, held the physical left mouse button for eight seconds, released it, saved with F9 and inspected the reloaded save. Digging dispatched two authored events, consumed10 durability, removed one resource sample and yielded two dirt. Its later target became invalid, so held input did not continually spend wear on an invalid indicator. Felling dispatched six events, consumed30 durability, reduced only tree7 from255HP/eight remaining wood to zero, and increased wood200→208 once. Tool instance10/shovel12 and instance12/woodaxe26 were preserved; ten juices, unrelated trees and other resources were unchanged. Both applications exited0. These input durations are wall-clock test controls, not an inference that Unity dispatches at identical absolute times during Animator blends. [Structured results](reference/sandbox/rebuild-tools-result.json), [input log](reference/sandbox/rebuild-sandbox-actions-physical.log), [dig state](reference/sandbox/rebuild-tools-dig-state.txt), [fell state](reference/sandbox/rebuild-tools-fell-state.txt).

The recovered body/tool poses were inspected in the [shovel action](reference/sandbox/rebuild-tools-dig-action.png) and [wood-axe action](reference/sandbox/rebuild-tools-fell-action.png). The source mesh tree disappears after depletion in the [felling result](reference/sandbox/rebuild-tools-fell-after.png). Shovel target-wire geometry is the same sample query used by its action host. Tool entry/exit blending, instant draw/stow, fixed avatar, held-only axe presentation and generic pickaxe fallback remain documented limitations; visible recovered poses plus headless exact event tests do not establish a complete original animation graph.

Construction was separately played with actual B/number keys/pointer input at1280×720. Selecting unaffordable dirt showed a red candidate; selecting wood showed the [green preview](reference/sandbox/rebuild-build-ui-preview.png). Left-click placed four actual cells and changed wood200→199. Clicking the material toolbar did not add cells or swing a tool. Alt+left-click on the placed brush's visible face removed those four cells and restored wood200. The first removal attempt had aimed at the original terrain point below the raised/snapped brush and correctly removed nothing; the corrected visible-face input passed. Equipped shovel/instance/durability and juice count were unchanged throughout. [Placed cells](reference/sandbox/rebuild-build-ui-placed.png), [removed cells](reference/sandbox/rebuild-build-ui-removed.png), [input/state log](reference/sandbox/rebuild-build-ui-physical.log). The candidate is computed through the shared pure core target and inventory/geometry validator, then revalidated at click time. It does not preview furniture or removal brushes.

The same process returned through sandbox pause→main menu→tutorial→main menu→Continue Sandbox. The new tutorial session had mission739 and all nine recovered NPCs; the retained sandbox still had200 wood, no constructed cells and its unchanged equipped shovel. [Continued sandbox](reference/sandbox/rebuild-build-ui-continued.png). This exercises unloading/reloading the new actor/world GPU assets across graphics contexts. The main menu also passed its bounded screenshot run: [menu capture](reference/sandbox/rebuild-smoke-main-menu-final.png).

The6m camera default is exercised by those successful graphical runs. The separate physical wheel-input check could not obtain game-window focus on two attempts; compositor queries then reported no active window and both displays in DPMS-off state with a lock blocker. The driver sent no wheel input after the failed focus check and closed only its own C window. Wheel-notched zoom and its GUI suppression therefore remain **unverified by physical input**; no lock/desktop settings were changed to force the test. Source limits and current implementation assumptions are in the [camera audit](audit/SANDBOX_CAMERA.md).

Optimized core tests and full ASan/UBSan/leak checks passed after the final construction-preview extraction, including the complete material/shape/rotation/brush matrix, actual placement/candidate equality and read-only failure contracts. UI routing tests passed normally and under sanitizers for build palettes independent of quickslots, inherited free-cursor behavior, toolbar gaps/disabled buttons, modal closing frames, furniture→voxel switching and scaled layouts. Actor and world-renderer mock-GPU/ownership tests passed, plus actual recovered-asset CPU pose validation. The complete Python/source suite and23 CLI cases passed. System Python skips three UnityPy-dependent world-export tests; all five separately passed using `/tmp/pe-asset-tools/bin/python`, and `export_sandbox_world_assets.py --check` verified the generated model/collider bundle. Skipped cases are not counted as coverage.

Earlier in this October5 integration, the full15,000-frame tutorial regression also reached mission756 with PASS: nine NPCs, eight knife starts/24 contacts, five herbs,11 wood, two shovels at3528/3553,8,987 dirt,19 cells added/one removed and receiver score50. [Regression log](reference/sandbox/rebuild-training-regression.log), [capture](reference/sandbox/rebuild-training-regression.png). That full run predates the new sandbox-only action/world/toolbar integration; the later manual route above rechecks tutorial loading and return, not the entire lesson chain again. The Adventure fixture's90-frame character/crafting/airborne runs also passed before this action slice; a valid airborne save continued falling rather than being forced onto the ground.
## October 5 — sandbox tree collision, save compatibility and zoom

The unlocked desktop allowed the pending wheel test and the new integration checks to finish. This checkpoint adds Adventure tree movement collision, PECSv5 validation/v4 overlap migration, and the independently checked Unity5.2 scalar camera damping. Earlier entries describe earlier builds.

**Original observed:** the Steam-installed native V1.1.4 client loaded manual Adventure slot1 from a newly copied isolated profile, `/home/admin/.local/share/planet-explorers-tree-reference-20261005`. In third-person locked-camera mode, forward movement into a nearby tree stopped at its base; a further two-second W hold retained displayed coordinatesP603,295. [At trunk](reference/sandbox/original-tree-contact.png), [continued hold](reference/sandbox/original-tree-held.png). This is qualitative movement obstruction, not a measured capsule radius, hidden tree identity, original felling result or full physics comparison. The game was closed through Quit Game and confirmation. All14 archives in the preserved baseline profile still match their October3 hashes. [Session/provenance](reference/sandbox/tree-reference-session.json).

**C physical inputs passed:** `python3 rebuild/tools/playtest_sandbox_trees.py --output-dir rebuild/build/sandbox-tree-playtest-final`, at1280×720, using new isolated fixtures and actual keyboard/mouse events. The harness rejects stale save results and removes old success reports before each run.

- Live tree7 contact stopped forward X near41.383. Another.2s W hold advanced only.001m instead of the unobstructed1m; the capsule remained outside the combined1.3m trunk/player radius. [Contact](reference/sandbox/rebuild-tree-tree-contact.png), [blocked forward motion](reference/sandbox/rebuild-tree-tree-blocked.png).
- Eight seconds of held left mouse produced six authored axe strikes,30 wear and one eight-wood award. Tree7 became depleted; other trees and tool-instance identity were unchanged. A subsequent one-second W hold moved X to36.418, across the former trunk. [Depleted](reference/sandbox/rebuild-tree-tree-depleted.png), [crossed](reference/sandbox/rebuild-tree-tree-passed.png), [saved states](reference/sandbox/rebuild-tree-tree-states.txt).
- A controlled v4 save inside tree7 displayed “moved0.55m” on load. Explicit F9 created v5; reloading and saving it retained the position and tree state. The original v4 SHA256 was unchanged. [Visible migration notice](reference/sandbox/rebuild-tree-migration-notice.png). The migration algorithm preserves saved velocity/clocks; normal gameplay settling after startup is separate.
- Three upward wheel notches changed desired camera distance6→3.6m and the camera settled to3.6m. Three further notches inside the character panel left it unchanged. [Default](reference/sandbox/rebuild-tree-zoom-default.png), [closer](reference/sandbox/rebuild-tree-zoom-closer.png), [after panel](reference/sandbox/rebuild-tree-zoom-after-panel.png). Final host telemetry reported desired3.600m/distance3.600m.

A first, longer forward-hold assertion expected exact immobility and failed. Investigation reproduced its trajectory headlessly: the terrain normal introduces a small sideways component before trunk contact, and the round trunk permits sliding around it. Minimum separation remained1.300622m with no overlaps; on a flat-plane control the player stayed at the contact point. The regression therefore checks suppressed inward motion and nonpenetration over a short hold, then actual depletion/crossing. No forced pinning or collision workaround was added.

[Machine-readable result](reference/sandbox/rebuild-tree-result.json), [physical run log](reference/sandbox/rebuild-tree-physical-final.log), [artifact hashes](reference/sandbox/tree-rebuild-evidence.json). Core/content and all23CLI tests passed, as did the full core ASan/UBSan/leak run and focused tree/camera sanitizer suites. The UnityPy source-header/PESC check and all six world-export tests passed separately. Three90-frame Adventure graphics smokes (character, crafting and airborne save) exited0. These checks establish this native subset; procedural tree placement/yields, full CameraForge, terrain presentation, settlement populations and pickaxe Mine actions remain incomplete.
## October 5 — Wood Pickaxe19 Mine integration

The next sandbox equipment slice is implemented and physically tested. The designated operator launched the installed original with the isolated animation sampler, entered the female tutorial and captured Mine1–3. The closed recording contains91poses, with22/42/27frames at30Hz. All42body bones mapped and moved; maximum hierarchy reconstruction error was approximately1.05e−6. The original logged its120-frame post-capture watchdog, responded to Escape, then exited after Quit Game and Yes. Its exit sidecar reports−9, consistent with the shipped self-kill handler. This captured authored poses and event metadata; it did not measure an original pickaxe gameplay outcome. [Recovery report](reference/sandbox/mine-capture-report.json), [responsive original](reference/sandbox/rebuild-mine-original-after-capture.png), [session and source hashes](reference/sandbox/mine-session.json).

Exact pickaxe geometry (216vertices/188triangles), Main/Back attachments, and the nine-part Mine body now share the authoritative C action clock. The tool uses its source entry speed1.5, stamina4, thresholds50/20 and distinct female/male ChangeTarget/DigTerrain events. Male loop duration remains separately represented rather than inheriting the nearly equal shovel duration. Missing action groups cannot fall through to generic mining for the same input. Source blends, IK, MinePutOn/MinePutOff and moving/air/first-person actions remain outside this bounded female third-person host.

The fresh1280×720 physical harness ran shovel, pickaxe and axe fixtures with eight-second mouse holds. Shovel again produced two strikes/10wear/two dirt; axe produced six strikes/30wear/eight wood. Pickaxe produced **one Mine strike, five wear and two dirt**, retaining tool instance11 and unrelated resources/trees. Its snapped terrain target disappeared after that sample was removed, so continued held input produced no additional edits. The saved state changed durability3553→3548 and dirt0→2. [Pickaxe action](reference/sandbox/rebuild-mine-mine-action.png), [edited terrain](reference/sandbox/rebuild-mine-mine-after.png), [saved state](reference/sandbox/rebuild-mine-mine-state.txt), [all three results](reference/sandbox/rebuild-mine-result.json).

A separate closer-camera check inspected [the stowed pickaxe](reference/sandbox/rebuild-mine-close-stowed-close.png) and [raised Mine pose](reference/sandbox/rebuild-mine-close-mine-raised-close.png). Opening Character shortly after starting, before the first strike, cancelled the action despite continued held mouse input: zero strikes, zero resource changes and unchanged durability. [Panel cancellation](reference/sandbox/rebuild-mine-close-cancelled-by-character.png), [result](reference/sandbox/rebuild-mine-close-result.json). Review also corrected the cyan mining indicator remaining visible on a broken tool. The final rebuilt executable repeated the physical pickaxe check successfully after that draw-condition fix.

Validation passed: full optimized core and ASan/UBSan/leak suites, the complete Make test suite and content-regeneration checks, all23CLI cases,13sampler tests,7pickaxe-exporter tests, and actual every-frame native skinning for all four installed groups. Actor tests reject all14partial pickaxe groups and mismatched source clocks. Action tests cover female/male first hits at fixed ticks68/74, male second hit170, exact loop wrap, target locking, capacity conservation, empty-attempt wear, equipment replacement and cancellation. Reproduce desktop checks with `python3 rebuild/tools/playtest_sandbox_actions.py`; `--action mine` selects only the pickaxe fixture. The [source audit](audit/SANDBOX_PICKAXE.md) and [action contract](audit/SANDBOX_ACTIONS.md) retain the remaining differences from Unity.

## October 5 Adventure supplier interaction

The C sandbox now replaces its supply beacon with recovered archived Eitai, a source NPC choice window and dialogue4089→4090–4094. The [supplier result](reference/sandbox/rebuild-supplier-result.json) records isolated fresh fixture and output saves. Only player pose/camera are selected by the fixture; it starts with mission9031, arrival0, supplies0 and the source starter inventory.

Physical input verified that proximity completes Arrival without supplies; E opens the menu; clicking its offer and cancelling on4092 leaves tools/books absent. Tab/free-cursor right mouse then opened the same NPC; five background clicks acknowledged4090–4094. Final state held one shovel, pickaxe, axe and shovel script, zero learned formulas, and unchanged200 wood. The full inventory remained identical after reopening the postclaim menu and F9/F10. Runtime counters reported zero tool swings and resource changes. All owned test windows exited normally. [Choice window](reference/sandbox/rebuild-supplier-menu.png), [final page](reference/sandbox/rebuild-supplier-page-4094.png), [accepted objective](reference/sandbox/rebuild-supplier-accepted.png), [reloaded close-only menu](reference/sandbox/rebuild-supplier-reloaded-menu.png).

The NPC is visible, turns toward the interacting player and uses a front-facing portrait generated from the same saved appearance. The full final paragraph fits the recovered dialogue panel at1280×720. This is C desktop verification against source/archive evidence and previous original observations, not a new side-by-side Steam run or a claim of identical portrait lighting.

Reproduce from the repository root, with recovered supplier assets and an unlocked desktop:

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" make -C rebuild all build/adventure_supplier_fixture
python3 rebuild/tools/playtest_adventure_supplier.py
```

The script creates isolated outputs under `rebuild/build/supplier-playtest`, releases bounded inputs, and closes only its owned C process. Headless tests separately cover strict proximity/range boundaries, tree/terrain placement changes, pre-Arrival close-only selection, modal input, final-page capacity failure and replay/idempotency. See the [implementation checkpoint](audit/SANDBOX_ALIGNMENT.md#october-5-supplier-interaction-checkpoint) for remaining settlement limits.

After supplier integration, physical tool regressions also passed: shovel2 swings/2 resource changes, pickaxe1/1, and axe6/6. [Regression result](reference/sandbox/rebuild-supplier-tools-result.json). The final integrated core/UI tests pass after the pre-Arrival close-only adjustment.

## October 5 recovered terrain textures

The native sandbox now renders the original diffuse terrain atlases with source triplanar/triangle blending. [Grass before digging](reference/sandbox/rebuild-terrain-dig-before.png), [digging](reference/sandbox/rebuild-terrain-dig-action.png), [grass/soil/stone cutaway after reload](reference/sandbox/rebuild-terrain-layers-reloaded.png), and [original Dragon Desert import](reference/sandbox/rebuild-terrain-imported.png) were inspected in the C frontend. The cutaway is a controlled fixture crossing the four render-region boundaries; its trees were cleared for visibility. Ordinary shovel/pickaxe checks separately verified actual transactions:2/2 and1/1 swings/resource changes. [Result manifest](reference/sandbox/rebuild-terrain-result.json).

New palette and mocked-GPU tests, focused ASan/UBSan/leak checks, actual asset tests/regeneration, ordinary core/UI/Python checks and23CLI checks passed. Some unrelated optional asset tests skipped under system Python; terrain asset checks passed without skips in the asset environment. The C runtime remains independent of Unity. The [terrain material audit](audit/SANDBOX_TERRAIN_MATERIALS.md) documents exact recovered behavior and remaining lighting/world-generation differences.


## October 5 Adventure welcome

Westina's recovered archived appearance and nine source dialogue pages now precede Arrival in the C sandbox. The fresh Adventure120-frame smoke auto-opened1949 with mission0. The controlled1280×720 physical harness verified automatic opening, consumed B/quickslot/F9 input, cancellation on page3, F9/F10 resume, all remaining replies, and final4088 acknowledgment. Before that acknowledgment, mission/Arrival/supplies stayed0. Afterwards page9 and mission9031 persisted; reopening Westina showed only “That's all.” Inventory matched the original starter inventory throughout, with zero tool swings/resource changes. [Opening](reference/sandbox/rebuild-welcome-page-1949.png), [resumed reply](reference/sandbox/rebuild-welcome-resumed-4083.png), [full final paragraph](reference/sandbox/rebuild-welcome-page-4088.png), [completed interaction](reference/sandbox/rebuild-welcome-completed-menu.png), [saved states](reference/sandbox/rebuild-welcome-result.json).

A separate post-welcome supplier regression passed proximity Arrival, cancellation without supplies, E/right-mouse selection, five final dialogue acknowledgments, exactly one supply package, close-only replay and F9/F10 inventory stability. [Supplier regression](reference/sandbox/rebuild-welcome-supplier-result.json). An initial regression attempt stopped because a concurrent build replaced the running executable, causing the PID-scoped driver's identity check to reject capture; the final run used the completed executable and exited0. The two successful harnesses closed their owned windows normally. These are bounded encounter fixtures, not an observed walk through a reconstructed settlement.

Validation: full core/UI tests and ASan/UBSan/leak suites pass; all23CLI cases and generated content checks pass. Five dialogue exporter, four welcome appearance and seven supplier Python checks pass. The general Make test target passes with its existing unrelated optional-asset skips. Actual C skinning evaluates558 Westina part/frame combinations (9parts×62samples), totaling7,991 vertices. Two exports were deterministic and original input hashes remained unchanged. [Evidence hashes](reference/sandbox/welcome-evidence.json).

Source/archive identity, dialogue and existing original screenshots were used; this was not a fresh Steam comparison. Source town placement, colony-site plot markers, NPC gestures, player portrait and original lighting remain unported. The [welcome audit](audit/ADVENTURE_WELCOME.md) describes native page persistence and old-save compatibility. Reproduce with `python3 rebuild/tools/playtest_adventure_welcome.py` after building the executable and `build/adventure_supplier_fixture`; do not rebuild during desktop input.


## October 5 UI spacing and movable panels

Quest tracking now clears the entire208-reference-pixel minimap frame in both tutorial and sandbox. The shared tracker starts at224; the title's existing−7 offset leaves9 reference pixels below the map. [Camera at1000×700](reference/ui-windows/tutorial-tracker-1000.png) and [Arrival at1280×720](reference/ui-windows/sandbox-tracker-1280.png) were inspected.

Inventory, Character, Storage, Replicator and Mission use shared title-bar dragging. All child hit targets translate with their panel, decorative panel bounds clamp on resize, close buttons are excluded from drag initiation, and drag/release frames consume gameplay input. Panel order is shared between drawing and pointer selection, with Inventory above Character; covered equipment controls cannot receive inventory clicks. Positions remain session-local, with no save-format change.

Physical tutorial and sandbox checks at1280×720 and1000×700 verified moved Character/Inventory roots, the moved Inventory close button, reopening at the retained position, resize clamping and Mission dragging. [Tutorial panels](reference/ui-windows/tutorial-panels-dragged.png), [sandbox moved close](reference/ui-windows/sandbox-moved-close.png), [sandbox resized panels](reference/ui-windows/sandbox-panels-resized.png), [moved Mission panel](reference/ui-windows/tutorial-mission-dragged.png). The input harness primes the newly released Xwayland cursor with a harmless modal-gap click before the first drag. All owned windows closed normally; sandbox recorded zero tool swings/resource changes.

Core geometry tests cover all five panel groups, reference sizes down to640×360, translated hit targets, close/item exclusion, cancellation and off-screen clamping. Full optimized core and sandbox UI routing tests pass; the sandbox routing suite also passes ASan/UBSan. No full tutorial progression rerun was needed for this presentation/input change. Run `python3 rebuild/tools/playtest_ui_windows.py` after building `all build/adventure_supplier_fixture`. [Artifact hashes](reference/ui-windows/evidence.json).


## October 5 recovered Adventure settlement

The original native V1.1.4 client loaded archived Adventure slot1 from a copied profile. A bounded observation probe captured the entire64×32×128 starttown crop from30 loaded LOD0 chunks stable across two frames; all30 source archive hashes remained unchanged. The C host was then driven through its main menu and tested with recovered buildings, terrain and NPC anchors. Physical checks covered welcome acknowledgment, movement beyond localZ64, hotel wall blocking, F9/F10, the downhill Eitai encounter, reflected-screen NPC picking and final supplies acknowledgment. The original and final C session exited through their UI. A preliminary C test process was terminated after saving during the renderer correction.

The visual test caught mirrored source geometry/signs; sandbox projection, screen queries and horizontal input were corrected and verified with readable HOTEL text, positive-X right strafe and successful off-center NPC selection. Original/C screenshots are different views, not a pixel-parity assertion. [Full integration and limitations](audit/ADVENTURE_SETTLEMENT.md), [captures/results](reference/settlement/result.json). Optimized core, full ASan/UBSan/leak checks, UI routing and23CLI tests passed. Extra population, machine/shop interaction, vegetation, wider terrain and the trip to Annot remain unsupported.


## October 5 townspeople, shops and the route to Annot

The original native V1.1.4 client was loaded through the isolated CPU terrain/AA0 launcher, using a copy of Adventure slot1 under `planet-explorers-route-reference`. Ordinary keyboard movement traversed the northern and southern terrain strips, then entered Annot around the western container buildings. The read-only probe captured338 loaded LOD0 chunks, each stable for two consecutive frames, assembling416×64×416 samples at source(-640,0,64). All30 original archive hashes remained unchanged. Maximum occupied terrain elevation was29, with no empty columns or top clipping. Captures are accumulated observations, not a simultaneous world snapshot.

The original displayed “Mission Completed: The Delayed Supplies” on entering Annot. A separate manual slot3 saved successfully at source(-287.3446045,21.5192337,145.3632965), preserving the initial checkpoints. Naoko Ueda's menu named Haleiwa, then showed the source reply about the illness and the two medicine-mission choices. Neither choice was accepted; the original was quit through its UI (launcher exit143). [Arrival](reference/annot-original-arrival.png), [Naoko menu](reference/annot-original-naoko-menu.png), [reply](reference/annot-original-naoko-reply.png), [next choices](reference/annot-original-medicine-choices.png). These observations establish arrival and the dialogue boundary; the C implementation currently ends after the reply without accepting9072.

The native headless route fixture followed49 waypoints using ordinary collision movement and jumps, with no teleporting or terrain edits. It traveled1273.7m over15527 simulation ticks, checked for capsule penetration each tick, preserved gameplay state through a midroute save/reload, and completed Annot. [Original sampled path](reference/annot-route-original-walk.json) and [native route proof](reference/annot-route-rebuild-headless.json) distinguish periodic probe samples, later rounded HUD detours and the exact archived endpoint. The probe stopped once coverage completed, so its final telemetry alone does not establish arrival.

Physical C checks used the exact-PID input driver. Cherish's shop displayed the eight supported offers; an attempted two-sword purchase failed because the fixture had only one free inventory slot, preserving money10500 and stock3. Buying one cost158, leaving10342 and stock2. The fixture's extra10000 currency is test-only. F9 was blocked while modal, the shop dragged and clamped at1000×700, and F9/F10 plus reopening retained currency, inventory and stock. The shop session exited0 through the main menu. [Shop](reference/town-rebuild-shop.png), [resized window](reference/town-rebuild-shop-dragged.png), [persisted purchase](reference/town-rebuild-shop-persisted.png).

The graphical route check continued the normally walked prearrival checkpoint beside Annot's western containers. WASD and one jump up the terrain bank reached source(-279.434967,22.496981,144.897232), completed Annot and removed its tracker. No teleport or terrain edit was used. That first process later ended with SIGTERM143 after saving, with no core dump; the cause was not established. Reloading the actual arrival save allowed walking to Naoko, selecting the Haleiwa question, reading4096, closing at the explicit medicine-quest boundary and verifying F9/F10 persistence with unchanged500 currency and supply inventory. The final dialogue session exited0 through the UI. [Arrival](reference/annot-rebuild-arrival.png), [menu](reference/annot-rebuild-naoko-menu.png), [reply](reference/annot-rebuild-naoko-reply.png), [checks and artifact hashes](reference/town-route-graphical.json).

All25 archived actors load and settle against the captured route. One already-supported saved NPC pose sits on a terrain edge whose capsule contact normal is below the generic player walkable threshold. A source-only static-pose validation retains her exact clear XYZ only with nearby real upward contact and a walkable center floor; player movement thresholds are unchanged. Optimized and ASan/UBSan/leak regressions cover unsupported air, overlap, synthetic crop-bottom support and malformed poses. Full core and sanitizer suites pass with the installed route; UI,23CLI, content and exporter checks pass. A stale desktop crash notification was traced to an earlier corrected loader test, not the current binaries. The Make test run's Python checks were completed in batches after a SIGTERM interrupted the aggregate run.

This milestone has25 recovered residents, twelve buildings, three starting-town shops with17 supported offers, and the bounded walk/arrival/Naoko introduction. Annot shop catalogs, medicine mission9072, NPC schedules/combat/recruitment, source vegetation/water/lighting and general world streaming remain outside it. The native inventory still has32 slots. Start New Sandbox to use the expanded route; existing saves preserve their prior world bounds.


## October 5 Annot medicine and expanded shops

The original native V1.1.4 client loaded manual slot3 from a fresh copy of the route profile at `planet-explorers-medicine-reference`. Ordinary movement reached Naoko. The player declined the medicine request, reopened the same conversation, then accepted. The original showed **Herb Collecting** and **Gather 5 units of Herbal Drink 0/5**; the newly granted pink script disappeared when used from inventory. Manual slot4 saved successfully, preserving slot3. All seven baseline checkpoint files retained their SHA256 hashes. The original was closed through its Quit Game confirmation; the launcher returned137, so this is not recorded as a clean process exit. [Original choices](reference/medicine-original-choices.png), [accepted mission](reference/medicine-original-accepted.png). The read-only saved-state comparison confirms active9072, its acceptance grant recorded once, newly learned formulas337/388/390, and script877 absent from both player inventory and the global item registry. [Archive evidence](reference/medicine-original-save.json) records hashes and exact serializer offsets. Original vendor purchases, timed medicine crafting and final delivery were not exercised in this run.

The native transaction fixture loaded the preceding normal-money v8 graphical checkpoint without altering it. It learned existing scripts to free inventory slots, accepted9072, learned877, and bought formula337 ingredients through Xing's ordinary shop API: Sunflower239×5, Tulip235×10, Lotus Seed241×10, Grassland Plant238×5 and Euphorbia233×5. Currency went500→395. Five ordinary timed batches finished after301 fixed ticks, producing917×5. Player position remained source(-279.456207,22.5010395,129.81488), and world hash remained6852b4422bda2475. Inventory, recipes, stock, clock and pose matched after save/reload. This is a headless transaction/crafting check; it does not claim physical travel to Xing. [Reproducible trace](reference/medicine-rebuild-headless.json), [fixture](../rebuild/tests/adventure_medicine_fixture.c).

Physical native checks matched the source medicine choice layout at the bottom dialogue panel, including refusal and acceptance. They exposed a held-click leak when closing a modal: the remaining press could start a world action and pointer recapture could change the camera. The host now retains pointer ownership through button release and defers capture; headless checks cover multiple held frames, mixed buttons, release and a fresh world click. [Native choices](reference/medicine-rebuild-choices.png).

Full optimized tests pass with the installed route and25/25 archived actors, as do the full ASan/UBSan/leak suite, generated-content checks and23 CLI tests. The first broad run failed on writes because this machine's `/tmp` user quota was exhausted, even with apparent free filesystem space. Final core checks ran in child-only mount namespaces binding repository-local temporary directories to `/tmp`; no host mount or unrelated files changed. One earlier full sanitizer process ended143 before completion; the subsequent isolated run exited0 with zero failures. Focused UI and medicine/save sanitizer checks also pass.

PECSv9 migration tests retain the exact v8 prefix and original17 stock entries, default medicine/new267 entries for older saves, cover active/completed medicine and reject malformed state/count/stock without altering the destination. Eight vendors now expose284 source offers. Catalog availability does not establish equipment, consumable, machine or deployable behavior; quest9074 and the conditional skill-tree reward remain unimplemented.

After the input correction, the same150ms physical acceptance click preserved exact saved yaw0.646779895/pitch−0.0839999989, currency500 and one script877, without triggering a world tool. The separate headless-ready checkpoint was loaded through the game's Load button. Final4100 was acknowledged by clicking its arrow; the saved result had medicineState2, no917, exactly925×10/554×1, currency395, unchanged camera and terrain. F10 retained completion in the Mission panel. [Accepted](reference/medicine-rebuild-accepted.png), [completed](reference/medicine-rebuild-completed.png), [reloaded Mission panel](reference/medicine-rebuild-completed-log.png), [save inspection](reference/medicine-rebuild-graphical.json).

Ordinary movement then reached Tammana and Sherley. Tammana's42-item catalog scrolled to rows35–42 and clamped there; unavailable funds prevented an armor purchase. Sherley's31-item catalog sold one Torch10 for70, leaving325 currency and stock2 from3. F9/F10 and reopening the shop retained those values. [Armor scrolling](reference/medicine-rebuild-armor-scroll.png), [reloaded purchase](reference/medicine-rebuild-shop-reloaded.png). These two physical vendors plus source/unit checks establish the bounded expanded catalog; not every vendor or item was physically exercised. Some original gender suffix glyphs still fall back in the recovered font. The low-contrast shop footer and unsupported separator glyphs were corrected after these captures.

Both native graphical sessions exited0 through Return to main menu and Quit. The original acceptance/learning checkpoint remains available separately for future original-client crafting and delivery comparison.

## October 5 — Torch and post-medicine automated continuation

No new physical original-client or native graphical playtest was performed for this increment. The preceding Sherley purchase proves the saved inventory instance exists; it does not verify the new held/placed Torch controls, lighting, Get/Turn or reload visuals. An attempted native display initialization failed at X11 `:0`; it did not reach a graphical test.

Source-derived implementation now connects inventory equip, left-click draw, R stow, P placement, left-click commit and P/right-click cancel. E/right-click on a placed Torch opens modal Get/Turn. [The deployment audit](audit/SANDBOX_DEPLOYABLES.md) records exact source ray/range/trigger rules, instance transfer, Charcoal recovery and rendering limits; [the equipment audit](audit/SANDBOX_PURCHASED_ITEMS.md) records recovered mounts and the immediate-transition/Normal-locomotion approximation. Native Get is immediate; the source pickup panel's strict greater-than2-second timer remains unported. These are implementation and source claims, not observed original equivalence.

The full optimized core suite passes with zero failures. `make test check-content cli-test` completed successfully with `ASAN_OPTIONS=detect_leaks=0`, including23 CLI cases. Fourteen optional reference-fixture tests were skipped; those skips are not passes. The first attempt stopped in the pre-existing baked-shovel check because LeakSanitizer could not inspect processes under the current tracing restrictions. The full core AddressSanitizer/UndefinedBehaviorSanitizer suite then passed with zero failures using `make sanitize ASAN_OPTIONS=detect_leaks=0`. The default sanitizer target still enables leak detection. Focused optimized checks cover source-priced purchase, preview/place, exact worn identity, turn, query-only collision and full-inventory Get retry. [V10 save regressions](../rebuild/tests/test_v10_save.c) place two Torches through ordinary APIs, roundtrip wear/turn/ownership, load a literal v9 prefix with default new state, reject18 checksum-preserving malformed payloads without changing an owned destination world, and ensure Get after reload grants Charcoal only once. Dialogue checks cover9074 intro, refusal/retry and unavailable acceptance; attachment and terrain GPU-mock tests cover matching asset clocks, exact equipped identity/emitter transforms and nearest-eight light selection. Six hunt Python tests and five Torch exporter/manifest tests pass; generated inputs were checked. Focused ASan/UBSan checks pass with leak detection disabled. LeakSanitizer fails under the sandbox's tracing restrictions, so this continuation adds no leak-check pass. Shader compilation/link checking is separate from pixel correctness.

A read-only archive audit inspected all ten Adventure checkpoints in `planet-explorers-hunt-reference` and `planet-explorers-medicine-reference`, hashing every input before/after. No archive changed. Neither profile contains accepted9075 or completed9072; latest slot4/auto0 retain active9072. [Checkpoint inventory](reference/hunt-capture-inventory.json) preserves exact IDs/hashes. Therefore native hunt acceptance remains unavailable, and positive active-hunt save/arrival test branches explicitly report absent coverage. No random destination, kill or mission completion was fabricated. [Post-medicine contract](audit/ADVENTURE_POST_MEDICINE.md) describes the capture needed to enable9075 and the separate six-hare9126 integration still required.

Pending physical checks are to operate the already purchased Torch through draw/stow, visible terrain light, placement, walking around its trigger-only geometry, Turn/Get and save/reload; then finish original medicine, accept9075 and capture its real4524 destination in a fresh slot. Existing reference checkpoints must remain intact.


## October 5 — live medicine delivery, hunt capture and Torch operation

This later live continuation supersedes the previous unavailable-display and missing-hunt-reference claims. Earlier records remain chronological evidence of their own runs. The original native client used the isolated `planet-explorers-hunt-reference` Adventure profile, preserving existing manual checkpoints; native Torch testing used the saved, normally purchased instance from `build/medicine-final-graphical.pesave`.

**Observed in the original:** The player bought the five-batch formula337 ingredients from Xing through the game UI for105 currency (500→395), crafted five Herbal Drinks917, returned to Naoko and selected4099 to deliver them. [Selected recipe](reference/hunt-original-recipe.png) and [crafted objective](reference/hunt-original-crafted.png) record the UI. The [delivery frame](reference/hunt-original-delivery.png) shows “Mission Completed: Herb Collecting” while response4100 is still open. Source re-audit confirms completion, consumption and rewards occur on4099 selection, before4100 opens; this corrects the previous source-timing interpretation. The user explicitly chose to retain the native final4100-ack transaction as an intentional difference; [the medicine audit](audit/ADVENTURE_MEDICINE.md#october-5-original-delivery-timing-and-retained-native-policy) records the mismatch and exact call order.

Naoko's next conversation was then accepted, producing9075 Bug Hunt. The original saved a new manual slot5 (`save4`) before arrival, preserving slot4 and earlier checkpoints. [Accepted hunt](reference/hunt-original-accepted.png) and [successful save](reference/hunt-original-saved.png) accompany the [read-only hunt archive](reference/hunt-original-save.json): active9075 agrees between mission records, completed9072 is recorded, and discovery4524 is **source(-349,19.5,233)**. [Package inspection](reference/hunt-original-inventory.json) verifies395 currency, zero917,925×10 and554×1, learned337/388/390 and no running craft. Every input file retained its hash during inspection. This checkpoint establishes original acceptance and the saved destination, not original arrival, six visible agents, kills or native physical traversal.

**Implemented and checked in C:** The real destination now enables supported-route acceptance. It fits the installed416×64×416 route, including the15m arrival disk and21m encounter margin. Export regeneration and all six hunt Python tests pass. Focused optimized strict C hunt/v10 checks pass with the actual terrain/collision assets: supported acceptance, strict15m XZ arrival, rejection below sourceY0, transition only to encounter-pending9126, unchanged inventory/currency/RNG and active/pending quest persistence. These headless tests do not establish the entire approach route's physical traversability. Six-hare combat, four-kill progress and9127 return remain unimplemented.

**Physically operated in C:** Native PID312184 loaded the medicine-completed save containing Sherley's normally purchased Torch10. Inventory Equipment/right-click equipped it; left-click drew it; P and an aimed left-click placed it; E opened the placed-item menu; Turn was clicked. F9/F10 retained the placed Torch and its interaction. [Reloaded Torch](reference/torch-native-reloaded.png) records the result. Get displayed the [pickup progress panel](reference/torch-native-get-progress.png), then removed the Torch and added Charcoal1001×1. [Charcoal result](reference/torch-native-charcoal.png) records the UI; after another F9/F10, debugger inspection verified deployed_count0, equipped instance0 and exactly one Charcoal. This establishes the tested native control/transfer/persistence sequence, not source-versus-native lighting or turn-animation equivalence. The process was intentionally killed through GDB after inspection; it did not exit cleanly through the UI.

The held Torch appeared low and forward because the current adapter uses Normal poses with the verified source mount; the missing Sword pose/transition remains a presentation gap. The physical placement press also exposed a held-input leak: after transfer removed the equipped instance, the remaining held press produced an unarmed mining message. The sandbox host was updated to retain press ownership through release, but this record precedes the fix's physical retest. Initial mouse focus/input problems were resolved with compositor Wayland clicks; the temporary native XTest override was removed. Input-driver recovery is separate from a gameplay parity claim.

At this point remaining checks included travel to the archived destination and the placement-release retest, subsequently updated below. Six-hare attack/reload observation and source Torch posture/light comparisons remain separate work. The user selected retaining native final4100 acknowledgment, so medicine timing is an intentional difference. The [post-medicine plan](audit/ADVENTURE_POST_MEDICINE.md#next-implementation-plan--six-hare9126-encounter) separates movement, actual combat, death credit, persistence and later return/defense work.


### Later original arrival and final verification

The original Adventure player then physically reached the archived hunt area through ordinary travel. The [arrival frame](reference/hunt-original-arrival.png), at HUD sourceP:-336,227, shows completion of the travel **Bug Hunt** and the new **Bug Hunt: Kill4 lepus hares0/4** objective. This is the transition9075→9126, not completion of the four-kill encounter. A [fresh manual slot6 save](reference/hunt-original-encounter-saved.png) (`save5`) preserved the pre-arrival slot5 (`save4`); the original was paused while its checkpoint was inspected. It subsequently received Quit Game and its Yes confirmation through the UI; PID281204 was verified absent and launcher session77003 returned143. This is not recorded as a zero-exit process test.

[Read-only encounter evidence](reference/hunt-original-encounter.json) confirms completed9075=1, active9126 with `STEP=0` and `MONSTER0=13_0`, and target1518. Exact saved player position is source(-336.993835,20.714342,227.883118),13.051m XZ from the archived4524 point. This is the position at saving, not the exact threshold-crossing location or timestamp. Save5 input hashes remained unchanged during inspection, and all save4 hashes still match the prior capture. The decoded random-mission target contains no saved center or agent IDs; the decoded creature record contains no monster prototype13. Other archive sections were not exhaustively decoded. No spawn positions, six-agent visual count, combat or creature reload behavior are inferred from those absences.

Final optimized core, content regeneration and23 CLI checks pass. The optional recovered-asset group was rerun with its actual dependencies:35 tests passed, zero skips (`build/final-parity-optional-assets.log`), resolving the aggregate run's optional skips without treating those earlier skips as passes. A direct full AddressSanitizer/UndefinedBehaviorSanitizer run with `ASAN_OPTIONS=detect_leaks=1` completed with zero core failures and exit0 after172.193 seconds (`build/final-parity-sanitize-direct.log` and `.json`); no timeout occurred. This successful direct run supersedes the earlier environment-limited leak coverage, while retaining the earlier failures as historical results. Automated verification does not establish native physical hunt traversal, six-hare combat or exact original rendering equivalence.


### Native placement release retest and retained medicine timing

Final-build native PID362100 repeated Torch placement with the left button held for a full second. The [retest capture](reference/torch-native-release-fixed.png) retains the Torch-placed message instead of the earlier unarmed-mining warning. This physically verifies the new placement press ownership through release for the tested sequence; earlier leak captures remain historical evidence. The same session physically selected4103 to accept9075 and displayed the actual pinned destination tracker. This record establishes native acceptance only; traversal was still underway at this checkpoint.

The user chose **Keep current final-response timing** for medicine. Native completion therefore remains on final4100 acknowledgment, intentionally later than the original4099 selection. Preserve the existing atomic item exchange and capacity retry. The source discrepancy is documented in the [medicine audit](audit/ADVENTURE_MEDICINE.md#october-5-original-delivery-timing-and-retained-native-policy), rather than treated as an outstanding change request.


### Native physical hunt arrival and reload

The final native build physically selected4103 at Naoko; the [accepted quest frame](reference/hunt-native-accepted2.png) showed the archived destination tracker. Ordinary WASD travel reached source(-343.558502,19.320673,222.071869),12.207946m XZ from4524, and changed hunt_state1 to2. The [arrival frame](reference/hunt-native-arrival.png) reports the explicit encounter-pending boundary. F9 saved, then the explicit Pause→Load game→Resume flow and Q reopened the [reloaded Mission panel](reference/hunt-native-reloaded.png) with that state intact. [Read-only PECSv10 inspection](reference/hunt-native-travel.json) records checksums, accepted/arrived player positions, identical source destination and deployed_count1 retained in both checkpoints. This supersedes the earlier pending native traversal check. It establishes the traveled native approach and persistence, without claiming six spawned hares, combat, four kills or the return mission. Native PID362100 then used Return to main menu→Quit; session62142 exited0. This final native run ended cleanly through its UI, separately from the earlier GDB-terminated Torch inspection and original launcher exit143. All test graphics sessions were closed.


## October 5 — original hare reloads and native stationary knife

**Observed original.** The launch-only [encounter observer](audit/ADVENTURE_HUNT_ENCOUNTER.md) ran twice in the isolated hunt profile, loading preserved manual slot6 (`save5`) each time. The closed captures contain600 and338 frames. Both identify all six mission-owned prototype13 actors alive at full HP, with `MONSTER0=13_0`; every actor's anchor and scale differs between reloads. [Bounded comparison](reference/hunt-encounter-reloads.json) preserves capture hashes, cohort data and unchanged hashes for all seven files in each of the earlier save4/save5 checkpoints. The first cohort's actual agent anchors and stats are [pinned separately](reference/hunt-encounter-cohort.json); moving entity poses are not substituted for spawn anchors. No average/random center is inferred.

Nearby hares approached and attacked without a player attack; player HP fell to zero in both runs. Captured HP spans5.903976→0 and109.451553→0, respectively. The [death/pause screenshot](reference/hunt-original-reload-hostile.png) retains the0/4 objective. This establishes hostile incoming damage in the observed situation, not exact perception radius, attack selection, per-hit timing or damage formula. No original hare was killed, so nonzero-kill reload behavior and9127 completion remain unobserved. Each client was quit through its confirmation; both launcher sessions returned137 after their probes had already ended cleanly. These are not zero-exit original checks. No manual checkpoint was overwritten.

**Implemented and physically checked in C.** The sandbox now uses [its own stationary Knife33 host](audit/SANDBOX_MELEE.md). At1280×720 on the installed route, load `rebuild/build/torch-release-check.pesave`, equip the existing knife from inventory, draw with one press, swing with the next, issue timed fresh clicks, open/close inventory, stow with R and save to a distinct `knife-host-playtest.pesave`. [Swing](reference/knife-native-swing.png), [chained pose](reference/knife-native-chain.png) and [stowed pose](reference/knife-native-stowed.png) were captured. Exit diagnostics recorded five swing stages and zero resource changes. The host continued to display the explicit encounter-unavailable objective; no targets or kills were simulated.

A second launch used the final build after correcting moving/airborne presses on display frames with no simulation tick. It loaded the separately saved equipped knife, rejected a press while W was held, then accepted a fresh stationary press. Opening inventory canceled the swing; holding left while the panel was open did not attack. After closing it, a one-second hold drew the knife without repeating, and a new press started the second swing; R stowed it. [Retest capture](reference/knife-native-cancel-retested.png) and the local `knife-host-retest.log` accompany an exact two-stage total and zero resource changes. Both native sessions returned through the main menu and Quit with exit0. Precise per-stage stamina debit, six-target atomicity and all four stages are automated-test evidence; still images alone do not measure these values. This is not a physical combat-contact or four-kill hunt check.

**Headless preparation.** General Adventure hare movement now uses explicit positions/current-center patrol and the shared terrain/building/tree snapshot. All six observed anchors support their actual scales on the installed route; a blocker at the sixth anchor prevents a staged pack from committing. `pe_hare_damage` passes authored forearm-versus-player-defence contact and HP/death tests. Neither test enables passive targets or grants quest progress. Native hostility/perception, guide timing and the encounter/persistence host remain next work.


Validation for this checkpoint: `make -C rebuild test check-content cli-test` passed, as did the full `make -C rebuild sanitize` with `ASAN_OPTIONS=detect_leaks=1`. The ordinary Python run skipped14 optional asset checks; rerunning the six affected suites with the local UnityPy environment passed all35 tests with zero skips. The new six-case encounter exporter test and its `--check` also pass. Graphics evidence is the two physical native sessions above, not a full tutorial or hostile-encounter rerun. Local logs are `rebuild/build/hunt-knife-final-checks.log`, `hunt-knife-sanitize.log` and `hunt-knife-optional-assets.log`.

## October 5 — bounded hunt host, incoming damage and original return checkpoint

This checkpoint supersedes earlier native zero-target/unavailable9126 implementation statements. The bounded host now prepares six pinned actors, selects a source-derived player threat, connects exact posed outgoing/incoming contact, and saves kill progress in PECSv11. Full original AI/timing, pounce, critical hits, corpse loot and9127 reporting/rewards remain outside the native scope; see the [combat audit](audit/ADVENTURE_HUNT_COMBAT.md).

**Original combat observation.** The first detailed probe launch crashed after18 records and is invalid capture evidence. The second launch, PID557717, loaded preserved manual slot6; the player had died before arming. The operator armed the probe and clicked the ordinary revive-item OK once. The630-frame capture closed with `stop-file` and passes `--require-melee`. [Bounded evidence](reference/hunt-combat-observed.json) preserves live source head sensors, three hares selecting player20000, HP0→1→166.5 followed by119.938583→96.6578751→0, single-guide melee30100004/30100047 and pounce30100053. All captured mission counters stayed `13_0`. No player attack input or manual save occurred during the capture. Attack2/two-guide timing and exact causal attribution of individual HP changes remain unobserved. The [observer audit](audit/ADVENTURE_HUNT_COMBAT_OBSERVER.md) separates the successful retry from the first crash's diagnostic-only records.

After capture closure, Cancel on the next death dialogue produced a loading transition back to town. The UI showed [2/4 kills](reference/hunt-original-town-two-kills.png), then a [return-to-town objective](reference/hunt-original-return-objective.png), without player attack input. No cause or attacking entity is inferred. A [fresh manual slot7 save](reference/hunt-original-return-saved.png) became `save6`. [Read-only archive evidence](reference/hunt-original-return.json) confirms9126 completed once,9127 active STEP0, actual return discovery4525 at(-266,25,136), reward definitions592–595×1, ending replies4105/4106 and subsequent9077. Rewards were not delivered in this observation. All seven files in the older save4/save5 checkpoints remained unchanged, and all three manual archives were checked again after exit. This is a completed-kill checkpoint, not partial nonzero-kill reload coverage. The original received System→Quit Game→Yes; launcher exit137 occurred after clean capture closure and is not described as a zero-exit original check.

**Native physical incoming damage.** Native PID589934 loaded the earlier saved equipped-knife fixture at the hunt area. The [world frame](reference/hunt-native-cohort-world.png) shows the bounded encounter rendering. Hares inflicted incoming contact damage and the player reached [death](reference/hunt-native-incoming-death.png). Pause/load restored a separately written PECSv11 checkpoint; the [loaded frame](reference/hunt-native-v11-reloaded.png) records resumption. This physically exercises prepared rendering, incoming contact/HP loss, death handling and checkpoint load. It does not establish outgoing blade contact or four kills.

The native log records12742 gameplay frames, no resource changes and final counters after the load of **0 knife swing stages,0 outgoing contacts,2 incoming contacts,0/4 kills**. Those counters are transient for the reloaded encounter, not a cumulative count across the entire session. The sandbox returned result2 to the main menu; UI Quit then ended the application with exit0. Local evidence is `rebuild/build/hunt-host-playtest.log`. A second outgoing-combat physical run remains pending at this checkpoint.

**Automated verification.** Full AddressSanitizer/UndefinedBehaviorSanitizer core checks pass with `ASAN_OPTIONS=detect_leaks=1` and zero failures in `rebuild/build/hunt-host-sanitize.log`; the exercised sanitizer binary was rebuilt at23:47:31. The preceding full optimized/content/23-CLI run passed in `hunt-host-final-checks.log`. The fresh final optimized build/test rerun also passed in `hunt-host-final-optimized.log`, with launcher session47436 confirmed exit0; content and23 CLI checks passed in the preceding final-checks log. Optional UnityPy/recovered-asset suites pass51 tests with zero skips in `hunt-host-optional-assets.log`. The separate nine defence exporter/native-loader tests also pass, including19-clip repeatable recovery and sanitizer posing of602×16 capsules. Original ten-clip and installed19-clip PEAC/PEHB/JSON outputs regenerate byte-identically. These checks do not establish full original/native combat parity.

### Later native outgoing contact

Native PID600765 ran a second physical hunt check from the earlier `knife-host-retest` fixture. An initial attempt ended in death before a proper attack sequence. The operator copied that same unchanged older fixture to the distinct test path `rebuild/build/hunt-host-second.pesave`, then used Pause→Load to restore its ordinary500HP checkpoint. This was a fixture reload, not an HP cheat or save-content edit; recovered assets were not changed.

After resuming, a leftward look adjustment and separate draw/fresh-press inputs used0.1-second presses separated by0.2-second releases. The [combat frame](reference/hunt-native-outgoing-contact.png) records nearby hare attack presentation and player HP130. The authoritative exit diagnostics in `rebuild/build/hunt-host-second.log` record15028 gameplay frames, **1 knife swing stage,1 real outgoing contact,8 incoming contacts,0/4 kills**, and zero tool/resource changes. The screenshot alone does not identify the exact outgoing collision or prove a kill; the runtime contact log establishes that one outgoing contact occurred.

The operator paused, returned to the main menu and clicked Quit. The process was verified gone, but its launcher was not tracked through a wait/reap result, so no exact exit-code claim is made for this second session. The first native incoming/death/load session's recorded exit0 remains separate evidence. This extends physical coverage to an outgoing Knife33 contact without claiming native four-kill completion, partial kill-count reload, loot or9127 reporting/rewards.


## October 6 — sandbox camera modes and hunt return

**Implemented and physically checked in C.** Sandbox starts in third person with a free pointer. F5 cycles free third person → captured third person → first person → free third person. Ordinary WASD movement works with the pointer visible. Short RMB on Naoko opens [his menu](reference/sandbox-free-camera-npc.png); holding RMB and issuing relative look rotates the [third-person camera](reference/sandbox-free-camera-orbit.png), and release restores the pointer. [First person](reference/sandbox-camera-firstperson.png) hides the player model. The final check used native PID1255029 at1280×720, the normal bounded input driver, and a .4-second held RMB followed by XTest look240,0. The initial implementation captured on every press and failed the short NPC-click check; capture now waits for3px accumulated motion or a.25s hold, and the repeated physical short-click check passes. These thresholds are native input adapters, not measured original thresholds. See [camera policy](audit/SANDBOX_CAMERA.md).

**Original return observation.** Native original PID1242702 loaded preserved manual slot7/save6 in the isolated hunt profile. Ordinary movement reached displayed P:-265,134. The [first ending reply](reference/hunt-return-original-arrived.png) appeared automatically with Mission Completed: Bug Hunt and a Jet Pack Script×1 toast, before acknowledgment. Two successive acknowledgments showed [Naoko's ending reply](reference/hunt-return-original-4106.png), then [Sajiva's incoming-monsters opening](reference/hunt-return-original-next.png). Source rows identify these as4105,4106 and9077's opening. Both bounded probes ended before arrival, so they establish neither exact reward counts nor frame ordering; their [negative reports](reference/hunt-return-negative-observations.json) remain separately labeled. No manual checkpoint was overwritten or new post-return archive saved. The original closed through Quit Game confirmation. See the [return audit](audit/ADVENTURE_HUNT_RETURN.md).

**Native return UI/save segment, with staged prerequisites.** The isolated `hunt-return-ui-fixture.pesave` explicitly changed an earlier checkpoint to state3/kills4 while preserving its player position, inventory, equipment and HP. It contained none of scripts592–595. This is a UI/persistence fixture, **not evidence of physically completing four kills**. Native PID1255029 then walked through ordinary WASD input from source(-276.806,22.501,127.720) into the strict return radius. Automatic rewards and4105 appeared near source(-270.273,22.491,133.285). Escape, F9 and F10 resumed [4105](reference/hunt-return-native-4105-reloaded.png); after acknowledgment, another cancel/save/load resumed [4106](reference/hunt-return-native-4106-reloaded.png). Final acknowledgment reached [completed reporting](reference/hunt-return-native-complete.png); a further save/load retained completion. Normal menu Quit ended the C process with exit0.

[Read-only native-loader inspection](reference/hunt-return-native-inspection.json) verifies states4/5/6, pending talks4105/4106/0, and exactly one each of592/593/594/595 at every checkpoint, with unchanged money325 and known-recipe count20. Save hashes were unchanged by inspection. Script grants happen on discovery; advancing/reloading does not grant again. The scripts are learnable recipe items; this increment does not implement their jetpack/glider/parachute/rope-gun movement. PECSv12 uses the unchanged v11 payload and widens the existing hunt-state domain. Town Defense9077, source role4 movement, full original camera behavior and actual four-kill/partial-combat reload verification remain outstanding.

**Verification.** `make -C rebuild` and `make -C rebuild test check-content cli-test` pass, including23 CLI cases. Full ASan/UBSan with leak detection passes. The ordinary Python run retains19 optional skips in unchanged asset suites; those skips are not passes and their earlier separate validation remains historical evidence. All32 changed content/dialogue/hunt exporter tests and all three regeneration checks pass under the installed UnityPy environment. Strengthened return/v12 tests, added after the full run started, separately pass optimized strict warnings and ASan/UBSan with `detect_leaks=1`; their isolated awarded-reload child uses `_exit`, so exit-time leak scanning applies to the parent. Tests include strict axial/diagonal5m boundaries, capacity failure/retry, actual transaction→save/reload at both pages and completion, and no duplicate grants. Local logs are `camera-return-final-build.log`, `camera-return-checks.log`, `camera-return-sanitize.log` and `return-v12-final-sanitize.log` under `rebuild/build`. The graphical evidence is the focused physical session above, not a new full tutorial playthrough.


## October 6 — user-operated original combat and graphics crash

The user controlled the original through the ordinary isolated launcher (PID1292077), without an injected observation component or assistant input. A focus-gated regional video recorder captured the game at30fps,1280×700, without audio. Its CPU encoder was used because the installed NVENC/FFmpeg API versions did not match. Capture is visible UI evidence, not internal skill/inventory telemetry. The recorder has stopped. Local raw files, logs and frame review remain under `rebuild/build/user-playtest-20261006`; the main segment012 is155.7seconds, SHA256 `c62078e7e8a7436d29f9f9cd9f2baa8c812ca633b2fe8c3617647eb62b667c62`, and fully decodes without errors. Two earlier focus-transition files have no usable frames; they are not accepted footage.

Relative to segment012, sampled frames show Bug Hunt0/4 through55s,3/4 at65s, Return to town at70–72s, and the player's ending reply at74–78s. The recording shows Jet Pack, Parachute and Glider script notifications alongside the [defense preparation panel](reference/user-defense-prep.png), followed by [Rope Gun](reference/user-defense-rope-reward.png). This strengthens visible reward evidence but does not audit inventory contents or identify hidden numeric mission IDs. Town Defense shows0/15 with two waves remaining during preparation, then1/15 at125s,3/15 at135s and [7/15](reference/user-defense-seven.png) near143.4s. The player and NPCs are both active; no per-kill attribution is inferred. The final image becomes nearly unchanged from144.967s through closure. Defense completion and partial-progress reload were not recorded.

The user reported a crash. The [bounded crash report](reference/user-playtest-crash-20261006.json) confirms SIGABRT at08:10:31CDT after Mono reported a native SIGSEGV. Faulting worker1292091 executed `mov (%rsi),%edi` with RSI=0xc in a `/memfd:/.glXXXXXX` mapping, followed by NVIDIA OpenGL580.178.04 frames. The earlier PID540553 crash has the same instruction/register value and matching NVIDIA caller offsets. The main thread was waiting in native synchronization. No OOM kill appears in the inspected journal interval. This implicates the graphics path; exact driver-versus-game responsibility and the recorder's contribution remain unresolved. The recurrence without the in-game probe shows that probe is not required for this failure. Raw diagnostic logs remain local; the extracted core copy was deleted.

Manual slots5/6/7 still match all21 previously recorded archive hashes. No new slot8 was found. Therefore those reference checkpoints are intact; unsaved session progress is not established as recoverable. The saved video remains usable despite the game crash. A process-local diagnostic retry with `__GL_WRITE_TEXT_SECTION=0` has been prepared but not launched or validated; it is not a fixed-crash claim. See [runtime diagnosis](REFERENCE_RUNTIME.md#october6-user-playtest-crash).


## October 6 — defense reference recovery and rendering diagnostic

Original PID1319444 used the process-local `__GL_WRITE_TEXT_SECTION=0` diagnostic and crashed again during the defense briefing. The third core repeats the same generated-GL instruction, RSI0xc and first five NVIDIA caller offsets; the process-local flag did not prevent the failure. The [comparison report](reference/reference-render-diagnostics-20261006.json) preserves the bounded findings. A separate launch (PID1335454) used `-force-gfx-direct` with the ordinary isolated native profile, without a recorder or injected observer. It loaded manual7, reached the hunt return, acknowledged4105/4106 and the four defense opening replies, then saved [new manual8](reference/town-defense-original-saved.png) during preparation. The [archive](reference/town-defense-original-active.json) confirms9077/7507, center(-284,27,134), protected Naoko20018 and zero progress. Earlier checkpoints were preserved.

No player attacks were issued after defense acceptance. The operator observed both waves, counter values0/3/5/6/13 and [mission completion](reference/town-defense-original-complete.png). This verifies progress without player attacks, without attributing individual kills to a particular NPC. New manual9's [archive](reference/town-defense-original-complete.json) lists9077 completed once and no active/random mission;9078/9079 were not automatically accepted. Reloading manual8 physically returned to [active defense](reference/town-defense-original-reloaded.png),0/15 with two waves, and subsequently the first wave began. The first post-load capture was delayed, so it does not establish the exact reset/preserved preparation seconds or partial-kill reload behavior.

The operator used Quit Game and confirmed; the launcher returned143. No new core exists for this run, but this is not an exit0 claim. The single successful defense run does not prove a general graphics fix. `launch_reference.py --single-threaded-rendering` now exposes the diagnostic as an opt-in switch;13 launcher tests include the original game's valueless-argument ordering requirement. See [runtime instructions](REFERENCE_RUNTIME.md#october6-single-threaded-rendering-diagnostic) and [defense scope](audit/ADVENTURE_TOWN_DEFENSE.md). Full native defense remains unavailable: source wolf/rhino animation, collision, cohort placement, NPC participation and the route boundary still require implementation.


## October 6 — Med Pack925, PECSv13 and Tab/F5 verification

**Implemented and physically checked in C.** Medpack925 consumes one item, heals350 capped to max HP, and uses its own ten-second cooldown; Herbal Juice retains its independent cooldown and timed buff. PECSv13 appends one finite0..10 float to the literal v12 payload; older versions infer zero. Invalid/truncated cooldown data must not mutate live state. Source full-HP use still consumes a pack; that policy is covered by focused tests. See [consumables](audit/SANDBOX_CONSUMABLES.md).

The physical fixture explicitly staged HP50 and quickslots2/3 for existing925/916 stacks of ten in an earlier completed-return checkpoint. This does not establish physical injury, medicine rewards or four native hunt kills. Final PID1371835 used normal keyboard quickslots3,2,2, then paused: the repeat Med Pack press during cooldown did not consume a second pack. [Both cooldowns are visible](reference/medpack-native-both.png). Pause Save→Load→Save while paused produced byte-identical v13 checkpoints. [Read-only loader inspection and hashes](reference/medpack-native-inspection.json) confirm HP490/500, nine of each consumable, medpack cooldown8.15002823, juice cooldown7.21666667 and an active juice buff at elapsed2.78333333. These values come from the save loader, not image inference. Healing subsequently reached the visible full bar as the juice continued.

In that same session, F5 selected captured third person, Tab released the pointer, and a fresh F5 entered [first person](reference/camera-tab-f5.png). Opening inventory, pressing F5 and closing inventory [retained first person](reference/camera-modal-stayed-first.png); the modal did not leak the cycle. Another F5 returned to free third person, confirmed by exit diagnostics mode0. The operator used Pause→Main menu→Quit; the supervised process returned0. The initial earlier native attempt1359899 ended with SIGTERM/status143 and no core; only the completed second session establishes the final checks. No full tutorial rerun or four-kill combat completion is claimed.

**Defense and targeting foundations.** The source defense exporter and disconnected clock now cover30-second preparation,10-second inter-wave cooldown,20-second second preparation and deferred Ready behavior. The separate sword-target acquisition helper preserves source ordered candidates, exact115°/3m/1.5m geometry and the original order-sensitive replacement behavior. Both pass strict optimized and ASan/UBSan checks. Neither is wired to live gameplay: full defense assets/placement and Unity candidate ordering/independent attack facing remain outstanding. [Next combat audit](audit/SANDBOX_COMBAT_NEXT.md) explains the required running-attack pose/root-motion recovery.


**Final validation for this increment.** `make -C rebuild` and the uninterrupted final `make -C rebuild test check-content cli-test` pass (23 CLI cases), including the new integrated targeting test. Full ASan/UBSan with leak detection passed for the medpack/camera/v13 core; the subsequently added, host-disconnected targeting helper passes its own strict optimized and ASan/UBSan/leak run. The defense clock likewise passes its focused sanitizer. The ordinary Python test run has19 optional skips, which are not passes; the four skipped perception recovery cases were separately run with the installed UnityPy interpreter (all6 tests pass). All16 content tests also pass there. The earlier full run received SIGTERM after core success; its completed remainder passed separately, and the uninterrupted final rerun supersedes that interruption. Logs remain under `rebuild/build`: `final-verification.log`, `current-final-build.log`, `current-sanitize.log`, `current-unity-checks.log` and `current-content-unity.log`.


## October 6 — directional player assets and native strafe check

**Recovered source poses.** The isolated original sampler recovered20 female body clips, including six Normal side/back leaves and four authored turn leaves. The closed621-pose capture passed its120-update cleanup-survival check; the installed original later exited with signal9 after ordinary Quit confirmation, not exit0. The [directional asset audit](audit/DIRECTIONAL_PLAYER.md) records hashes, staging/installation and its separate native-parser/capsule validation. Retaining turn leaves does not reproduce the original movement blend graph or body auto-rotation.

**Native physical check.** Native PID1657905 used the staged hunt-return UI fixture, with its preexisting Knife33 equipped, at1280×720 in free third person. The operator drew the knife, held A/D/S for running, toggled Z to walk, then held A/D/S again. The [right-running frame](reference/strafe-native-run-right.png) clearly shows the player and drawn attachment. Left-walk and both backward captures remain local because nearby scenery partly or almost completely obscures the player; they are not accepted as clear pose images.

The runtime reports locomotion mask4037: NormalIdle, NormalRF and all six new NormalWL/WR/WB/RL/RR/RB selections. This mask and the operator's input sequence establish that every new directional path was exercised; a still does not prove precise clock synchronization or source blend parity. [Bounded result and hashes](reference/strafe-native-result.json) preserve the evidence. Exit diagnostics record20021 sandbox frames, camera mode0, zero tool swings/resource changes, zero knife swing stages and zero outgoing/incoming contacts. The displayed4/4 kills and completed quest came from the staged prerequisite fixture; **this run produced no combat kills**. Pause→Main menu→Quit ended the application with exit0.

**Verification.** Full optimized tests, content regeneration checks and all23 CLI cases pass in `rebuild/build/strafe-verification.log`. Focused locomotion and hunt-controller tests also pass optimized and ASan/UBSan with leak detection; controller readiness now rejects any missing directional defence pose before combat starts. The recovered asset tests validate all29 defence clips separately. The full15000-frame training regression passes with exit0: tutorial mission756 (gathering ledger752), all nine NPCs,19 construction additions/one removal/score50, and eight melee starts/24 contacts leaving hareHP0. It exercises the new installed assets through the existing tutorial progression and verifies retained action behavior; locomotion mask63 covers the original six clips and does **not** exercise tutorial strafing. Evidence remains in `rebuild/build/strafe-training.log` and `smoke-training-building.png`. Full `make -C rebuild sanitize` also passes with exit0, ASan/UBSan and `detect_leaks=1`, with zero core failures in `rebuild/build/strafe-sanitize.log`. These checks do not establish original/native full blend parity, moving Knife attacks, ordinary four-kill completion or partial kill-count reload.


### Captured tutorial directional check

A separate ordinary-input tutorial run (native PID1689786,1280×720) acknowledged the two starting replies, pressed F5 once for captured third person, held D/A/S for1.5seconds each, toggled Z, then repeated D/A/S walking. [D running](reference/strafe-tutorial-d-run.png) and [A walking](reference/strafe-tutorial-a-walk.png) show unobscured representative poses. Input labels are deliberately separate from authored left/right names: tutorial retains its existing unreflected presentation, as derived in the [coordinate audit](audit/PLAYER_STRAFING.md#tutorial-coordinate-review).

The [bounded tutorial result](reference/strafe-tutorial-result.json) records4314 frames at mission739 and locomotion mask4033: idle plus all six new side/back clips. The player owned/equipped no knife; mining swings, melee starts and contacts all remained0. This physically exercises the six new selections in the tutorial as well as the earlier sandbox check. It does not establish source turn/blend/handedness parity or action-facing behavior while attacking. Pause→Main menu returned from tutorial; clicking Quit in the main menu then reaped the application with exit0. Local log: `rebuild/build/strafe-training-manual.log`. This focused physical check is separate from the full15000-frame scripted progression and its recorded exit0.


## October 6 — free-mode facing and smooth movement transitions

**Implemented and physically checked in C.** Native PID1759253 ran the corrected build at1280×720. In sandbox free third person, A/D/S running and Z-walking turned the body toward travel; stopping retained the new heading. The [stopped S-facing capture](reference/movement-free-stopped-facing.png) records one endpoint. Captured third person was also exercised with A/D/S running and walking. The sandbox reports11465 frames, final camera mode1 and locomotion mask4037. It used the earlier staged hunt-return fixture with an existing Knife33: the displayed4/4 was preexisting, not earned. One stationary Knife stage ran, with zero incoming/outgoing contacts, tool swings or resource changes.

The same process then entered the tutorial, acknowledged its replies and pressed F5 for captured third person. D/A reversals and Z-walking with A/D/S/W produced3461 frames at mission739, camera mask1 and locomotion mask4039. The player owned/equipped no knife; melee starts/contacts and mining swings remained0. The [tutorial transition grid](reference/movement-tutorial-transition-grid.png) contains twelve chronological frames sampled at30Hz from1.6–approximately2.0seconds of its recording. Both this grid and the local sandbox grid at2.0–approximately2.4seconds show intermediate poses during direction changes. They support the native smoothing observation, **not parity with Unity's original blend graph, curves or timing**.

The accepted CPU recordings contain328 frames/10.933332seconds for sandbox and234 frames/7.800005seconds for tutorial, both1280×720 at30Hz; both recorders returned0. An initial GPU recording failed with status54 and is excluded. Automated pointer warping affected the camera framing, so these images do not establish camera-quality acceptance. Returning through the main menu and quitting reaped the native application with exit0. The [bounded result](reference/movement-facing-blend-result.json) records inputs, hashes, logs and limitations; local raw videos remain ignored under `rebuild/build`.

Full optimized build/tests passed with exit0 in `movement-verification.log`; its19 optional Python skips are not passes. Full ASan/UBSan with `detect_leaks=1` passed with exit0 in `movement-sanitize.log`. The new full15000-frame tutorial regression also passes with exit0 in `movement-training-smoke.log`: mission756, all nine NPCs, locomotion mask3647,19 construction additions/one removal/score50, and eight melee starts/24 contacts leaving hareHP0. This verifies retained scripted progression with the current facing/blend code; it is separate from the manual movement recordings and the earlier directional-build regression. Ordinary four-kill completion, partial combat reload, moving attacks, full source turn/blend parity and exhaustive equipment/action-facing checks remain unverified.


## Hands-on checklist — current movement and running-Knife build

Use the **C rebuild**, starting from its main menu; [launch instructions](../rebuild/README.md#build-and-run) and [sandbox controls](../rebuild/README.md#sandbox-controls) are in the README. This checklist targets the newest free-mode facing and locomotion-smoothing correction. The earlier directional runs above predate it; the focused physical checks above record the corrected build, while this checklist remains useful for broader user testing. Grounded female third-person running Knife33 attacks are now available in the sandbox with the installed qualified assets; the tutorial retains stationary attacks. The later running-attack checkpoint below records their separate validation.

1. **Sandbox free-mode turning.** Find clear ground with an unobscured player. In free third person, hold A, D and S separately for about two seconds. The body should turn smoothly toward travel independently of camera orbit, then keep its heading when you stop. Sustained sideways steps are not expected in this mode. Press Z and repeat walking, then restore running. Try W→A, W→S and quick A↔D reversals; watch for abrupt pose changes, repeated foot-cycle restarts or a body that always faces forward.
2. **Captured-mode strafing and camera controls.** Press F5 once for captured third person. Hold A/D/S running and Z-walking: the body should face the camera direction while making side/back steps. Change direction rapidly and release to idle to check smooth transitions. Cycle through first person (hidden body) and back to free third person. Free mode permits WASD with a visible pointer; holding/dragging RMB looks around and release restores the pointer. Try Tab then F5; the camera should still cycle.
3. **Collision and footing.** Walk/strafe into a solid wall or tree, then move diagonally along it. The body should stop or slide along the surface without passing through it; steps should settle toward idle when actual movement stops. Try an ordinary slope and one Space jump on clear ground. Report clipping, hovering, snapping or animation that keeps running against a stationary blocker. NPC bodies are intentionally not player blockers.
4. **Owned equipment and action facing.** Equip a Knife, shovel, pickaxe, axe or Torch already in your inventory. Draw once, then repeat turns in free mode and A/D/S in captured mode, at walk and run speeds; stow and repeat. Watch for detached tools, motion that disagrees with the body, or Torch flames/lights leaving their mount. Axes currently have held presentation only, so an invisible stowed axe is a known limit. If you normally use a supported stationary action, check that its body and tool stay aligned until it finishes. Exact action poses do not use the locomotion crossfade. For the sandbox running case, equip/draw Knife33 on clear, level ground in third person, release the draw click, hold W long enough to reach normal running speed without Shift, then make a fresh attack click while continuing W. Check the lunge, held blade and return to movement; repeat after stopping to compare stationary behavior. Admission uses actual grounded speed above4.5 and at most7m/s, so sprinting does not qualify for the supported branch. Walking or sufficiently slow blocked movement instead selects the ordinary branch now enabled by the later walking checkpoint. This check does not apply to tutorial moving attacks.
5. **Tutorial comparison.** Return through Pause→Main Menu and enter the tutorial. Acknowledge the starting dialogue. Repeat the free-mode turns, stops and quick reversals, then F5 into captured third person for A/D/S and Z-walking. Both hosts should turn into travel in free mode and permit side/back steps in captured mode. They retain different presentation conventions, so record the pressed key rather than guessing an authored clip name. Tutorial combat remains stationary; do not expect the sandbox running lunge there.
6. **UI ownership and reload.** Open I or C, move the pointer over the panel and click/drag it. That input must not also swing a tool or turn the world camera. Close the panel, release the button, then make a fresh world press. If testing sandbox save/load, use your usual save deliberately; after loading, check that no previous turn, gait or action resumes unexpectedly. Finish through Pause→Main Menu→Quit.

For a problem, report **sandbox/tutorial, camera mode, walk/run, equipped item and drawn/stowed state, exact keys, and expected versus actual behavior**. A short clip showing the player and nearby ground is especially useful; include whether scenery obscured the pose. No save or quest modification is required for the movement checks.


## October 6 — user-operated original running-attack demonstration

The user controlled original native PID1819072, launched with the isolated hunt profile, CPU terrain, MSAA disabled and process-local single-threaded rendering at1280×720. Instructions requested manual slot5, an equipped/drawn knife, three separated stationary swings, W-running attacks without Shift, then additional clicks for a chain. No injected observer or input logger was present: these are requested inputs, not measured branch admission or confirmation of which save was selected.

Two foreground-only, silent30Hz recordings closed with recorder exit0:12.333s and70.835s. The second shows travel away from the settlement, separated swings on open ground, travel alternating with attacks, and a longer slash sequence with turns. The [separated-swing grid](reference/running-knife-user-separated.png) samples16 frames at3Hz after a24s seek; the [chain grid](reference/running-knife-user-chain.png) uses the same extraction after58s. Both are cropped chronological contact sheets, read left-to-right then top-to-bottom. The original System menu is visible at the recording end. Recording has stopped; the original remains open and user-owned. All63 previously hashed files across nine preserved manual checkpoints are unchanged.

The [bounded record](reference/running-knife-user-result.json) retains settings, video hashes, extraction parameters and limitations. This provides a visual reference for attack poses/recovery and travel-to-attack transitions. It does not measure source skill IDs, attack speed, held inputs, Animator timing, root displacement or collision response, and shows no enemy contact/damage/kill evidence. Following-camera screen motion cannot qualify a metric movement curve. The denser isolated root-motion capture and independent live telemetry comparison remain the running-attack implementation gates; no C behavior changed in this observation.


## October 6 — qualified running Knife motion, live branch and native checks

These are separate evidence streams. [Root06](reference/running-knife-root06.json) qualifies an isolated female408 clip curve under unchanged error/density tolerances. [Live03](reference/running-knife-live03.json) confirms actual Knife33/20110014, runtime thresholds4.5/7 and positive-weight RunSwordAttack in the original; its247-frame llvmpipe capture runs near2fps and does not establish precise Unity motor/event timing. The original remained responsive through SYSTEM→Quit→YES and exited−9, the recorded self-kill pattern rather than exit0. The source game used process-only Mesa software-rendering variables after two failed NVIDIA launches, not a system graphics configuration change. All63 preserved manual-checkpoint files remain hash-identical.

The matching21-clip body/held/stowed Knife group,30-clip defence and qualified PERM1 motion were installed with prior folders preserved. [The implementation audit](audit/SANDBOX_RUNNING_KNIFE.md) records the shared once-per-tick interval, swept movement and resulting pose/contact ordering, plus explicit stationary continuation and motor/blend limitations. Optimized core tests and full ASan/UBSan with leak detection pass;44 focused Python tests pass with0skips. This does not retroactively resolve older optional-suite skips.

The full `make test` result is composed: its core and prerequisites passed, then an outdated equal-count Torch asset fixture failed. After the fixture correction, all four Torch tests and44 remaining Python recipe commands passed;19 optional UnityPy skips remain skips. This is not an uninterrupted exit0 claim. Current content checks and all23 CLI cases pass. The first new tutorial smoke exited1 before gameplay at a strict21/20 body/axe clip-count check. A bounded loader correction now allows only trailing body `RunSwordAttack`, preserving every shared clock and exact action-pair checks; focused optimized/sanitizer tests and seven installed pair checks pass. The corrected graphical result is separate below.

Native PID1944794 used free third person and the existing `strafe-physical-start.pesave`, whose hunt count was already staged4/4. Ordinary W movement plus fresh attack clicks produced three running starts/three swing stages over11,256 frames. The operator inspected a51.666s silent CPU video and motion sheet; [the lunge still](reference/running-knife-native-lunge.png) provides bounded body/held-Knife presentation evidence. There were zero outgoing/incoming contacts, zero tool swings and zero resource changes. The displayed4/4 is inherited fixture state and establishes no kills. Return to main menu followed by Quit exits0.

Native PID1958917 then used the existing `hunt-host-second.pesave`, a500HP/0kills test checkpoint, without HP/stat edits during this run. Over5,367 frames it recorded one running start plus one continuation, six aggregate outgoing and eight incoming contacts, player death and0kills. The29.066s recording includes idle time in which the player was already receiving damage before attack input. These aggregate counts must not be assigned solely to stage408. No tool/resource changes occurred; returning to main menu then Quit exits0.

[The bounded native result](reference/running-knife-native-result.json) retains exact inputs, fixture paths, video/log hashes and both exit codes. The tested sandbox executable hash is `19098b252cb991706821ecad5d1f415e8f18c0d69dd80690058fef78f03fc918`; these sessions preceded the tutorial-only loader correction and subsequent rebuild. Current native fixture hashes were recorded, but no pre-run native hash baseline exists for an independent unchanged-byte claim. The operator issued no saves; separate output paths were supplied and neither output save file was created. No desktop actions were performed by the evidence-review agent.

This verifies ordinary native running admission/presentation and real contact in a running-plus-continuation session. It does not establish precise source trajectory, full Unity controller/motor/blend/hit response, isolated408 contact totals, four real hare kills/cleanup or partial-progress reload. Obstruction/cancellation/stationary-regression and action-cleared save/load checks retain their own physical gates. The corrected15,000-frame tutorial regression now passes with exit0 at mission756: all nine NPCs with250s clock, locomotion mask3647, eight melee starts/24 contacts/hareHP0, five mining swings/three removals, and19 construction additions/one removal/score50. It resolves the startup mismatch described above while retaining stationary tutorial combat. Local evidence is `rebuild/build/running-next/training-smoke.log`, `training.exit` and `training-complete.png`. This final binary contains the tutorial-only compatibility correction after the sandbox physical sessions; it adds no further sandbox runtime change.


## October 6 — numbered conversation responses

**Implemented and physically checked in C at1280×720.** The tutorial's [Andy welcome](reference/numbered-tutorial-welcome.png) shows `1. Continue`. The operator pressed9, which left the page unchanged, then1 to advance to Camera Tips and1 again to close it. That tutorial process exited0. Existing source dialogue and lesson transactions were retained.

In the sandbox, [Naoko's only response](reference/numbered-sandbox-naoko.png) was `1. That's all.` Pressing2 left the menu open and the medpack count at ten;1 closed it. [Tammana's response menu](reference/numbered-sandbox-vendor.png) showed `1. Shop` and `2. That's all.` Pressing1 opened the actual shop. The operator later visibly reopened Tammana's menu and pressed2: it closed with the then-current nine medpacks unchanged. These checks establish visible numbering, selection and an absent response in ordinary NPC interaction.

Escape from the shop closes the entire interaction to the world, rather than returning to the NPC response menu. A following2 used the ordinary medpack quickslot. A later quick interaction attempt did not open a menu;2 again used the world quickslot. Those events are not modal input leaks. The successful response2 closing check used the visibly reopened menu. Local `vendor-reopened.png` and `vendor-response-two.png` retain that before/after sequence under `rebuild/build/hunt-qol-20261006`.

Optimized sandbox UI and isolated training suites pass; both also pass ASan/UBSan with leak detection. The actual tutorial input-block harness passes its two sanitizer tests, including top-row/keypad selection and closing-frame protection against quickslot/build actions. Return states3–6 select source9127 for both mission title and description; the obsolete rewards-unavailable text is removed and its mapping passes the subsequent optimized UI test. Keypad input, disabled medicine delivery, final4100 acknowledgment and pointer-release behavior were not newly physically tested in this session. See the [numbered-response audit](audit/NUMBERED_CONVERSATIONS.md) for those headless checks and transaction boundaries.

The sandbox was handed to the user for manual four-hare testing. This conversation check establishes no new combat kills, return completion, partial-progress reload or sandbox exit result.


## October 6 — manual hunt and consumable interruption report

The user operated the native build from the earned hunt-acceptance checkpoint in an isolated save, with Knife33 and the existing medicine rewards assigned to quickslots. A bounded [event-log snapshot](reference/hunt-manual-consumable-feedback.json) records two attempts ending in death at0/4: the first has23 aggregate outgoing and7 incoming contacts; the second has39 outgoing and12 incoming. These are contact counts across targets, not credited deaths or isolated running-attack hits. The recorded combat clock excludes modal suspension and is not wall time. No session-exit result or four-kill completion is claimed.

The user reported that medpack/juice use briefly stopped both the player and combat NPCs. Inspection found that the native quickslot path marks item use as whole-world UI consumption, which resets movement/autorun and skips hunt simulation; this is a native input-routing bug. The corrective input separation and its verification belong to the [consumable audit](audit/SANDBOX_CONSUMABLES.md). Difficulty comparison remains open: quest prerequisites, available equipment and the original starter clothes' defense need separate source evidence before adjusting balance.


## October 6 — consumable movement correction

After the user returned desktop control, the previous native session exited0 through the main menu. Its saved position was preserved separately before launching the corrected build from the same earned checkpoint. Both normal user saves and the original game remained untouched.

The corrected native session used a foreground-only, silent recording under `rebuild/build/hunt-qol-20261006/fixed-video`. Top-row2/3 consumed Med Pack/Herbal Juice during heldW movement. A separate middle-button autorun check consumed a Med Pack without another movement key: [the recorded use frame](reference/consumable-autorun-fixed.png) and subsequent frames retain the running pose, drawn knife and advancing position (approximatelyP:-312,175→-314,177→-316,179). Captured third-person camera orientation stayed steady. This establishes the visible autorun/stow correction; exact per-frame movement continuity is covered by the production-input regression separately.

Ordinary travel then reached the real hare encounter. Juice use/cooldown retry and three incoming contacts were recorded; the operator paused atHP155.712646 and exited0. This short graphical check does not isolate a hit on the exact consumable input frame. The actual host-input/core regression covers that requirement: the healing frame moves0.083328m and accepts a real posed hare contact, taking HP120→470 from healing and then427.925629 after damage. No enemy stats, player defense or clothing state were altered. Four-kill completion remains unverified physically.


## October 6 — starter outfit defense and wear

**Implemented and physically checked in C.** Native PID2142398 loaded the earned v13 save, creating the source starter outfit through the documented compatibility migration. The [Character panel](reference/starter-outfit-character.png) showed the coat, pants and shoes icons with Defense12. Ordinary travel reached the real hunt encounter. Eight incoming contacts reduced HP500→215.395752; no new player attacks or consumables were used before this checkpoint, and the hunt remained0/4.

Read-only inspection of the confirmed immutable v14 save records the following worn instances:

| Item | Instance | Remaining durability |
| --- | ---: | ---: |
| Issued Overalls95 | 31 | 2127.539794921875 |
| Issued Pants131 | 32 | 2127.539794921875 |
| Issued Shoes192 | 33 | 833.5396118164062 |

The operator used Pause→Load→Save. The worn checkpoint and subsequent save are byte-identical, SHA256 `5d4ddf20af03086e0b5f8566e6a8d1fba2507ec96c453303902a08b0319d7d30`; the reloaded Character panel showed HP215 and Defense12. The [bounded physical report](reference/starter-outfit-physical.json) preserves the evidence. An earlier live-file inspection detected changes during its read and is not used for these confirmed worn values or unchanged-file claims.

The final optimized frontend build and full `make test` exit0;21 optional UnityPy-dependent skips are not passes. Full core ASan/UBSan with leak detection, content regeneration and all23 CLI cases pass. The real-contact regression separately verifies formula-derived protection, first-hit breakage changing the next hit, calculated overkill wear and transactional rejection. See the [outfit audit](audit/STARTER_OUTFIT.md) for the source rules, PECSv14 migration and exact test scope.

After reloading the worn checkpoint at approximatelyHP215, the operator made a bounded stationary-Knife attempt, consumed one Herbal Juice, and pressed an empty quickslot4; no Med Pack was consumed. This follow-on attempt recorded six outgoing and five incoming contacts, then death with0kills. It did not start at full HP and is not a controlled survival or original/native difficulty comparison. The operator restored the prior checkpoint through Pause→Load, then returned to the main menu and quit with exit0. The confirmed worn save remained unchanged.

The physical checkpoint verifies presentation, incoming-hit wear and persistence without repair or duplicate identities. The follow-on attempt adds ordinary outgoing contact with worn clothing, not four-kill completion. Neither verifies physical clothing breakage, matched original/native damage timing or survival balance.


## October 6 — sword capture and shield foundations

**Observed in the original, isolated sampler:** the ten-leaf female Sword capture completed304 poses and120 cleanup-survival frames. The original UI remained responsive through Quit confirmation; launcher status137 records the source process's SIGKILL exit, not a clean exit0. [The capture report](reference/starter-sword-bake-20261006.json) preserves hashes and validation limits. Pure clip samples establish the sampled transforms, not live controller transitions, root movement or masked shield composition.

**Implemented and tested headlessly in C:** shared melee clocks distinguish Wood Sword1 rate1.0 from Knife33 rate1.37. Shield ownership retains item34 and durability in one bag instance, independently of the primary weapon, with a four-byte PECSv15 selection reference. The incoming-combat adapter stages source shield defense9, eligible block protection.22 and calculated-hit wear with outfit/HP changes, including breakage and failure rollback. Focused optimized and ASan/UBSan/leak checks pass; [shield ownership](audit/SHIELD_OWNERSHIP.md) records their scope. Normal sword/shield use remains disabled pending host and presentation integration.

**Recovered assets, not installed:** the coherent stage contains31 body/sword/shield clips and40 defence clips, preserving existing clip payloads. Native PEAC/scene bounds now allow64 clips; focused loader, every-frame capsule and exporter checks pass, with77 focused Python checks and zero skips. At that earlier stage, source backward SwordWB441/SwordRB708 and composed shield-block assets were missing; the later backward02 capture below supersedes the backward-pose gap. See [pose qualification](audit/STARTER_SWORD_POSE_GROUP.md). No native graphics or playable sword/shield claim follows from staging these assets.

Two subsequent read-only original observer launches crashed before recording poses. The second used single-threaded rendering and still reproduced the generated OpenGL fault; [the crash report](reference/shield-observer-crash-20261006.json) distinguishes the evidence from an unproven driver/game diagnosis. A process-only Mesa/llvmpipe launch recorded187 frames, but none qualified shield blocking: the player remained in first person, with SheildBlock false. [The software report](reference/shield-observer-software-20261006.json) records this negative result and unchanged14 manual-save files. The game remained running when handed to the user for a manual equipment/control check; no exit result or successful block capture is claimed.

The user subsequently reported software rendering was too slow for interaction. A fresh Zink GPU launch reached the menu with a confirmed hardware renderer. The user reported it playable when asked about loading `audit`; [the diagnostic record](reference/shield-zink-workaround-20261006.json) preserves process-only settings. No measured FPS, completed shield capture or long-term crash fix follows from that report.

Final foundation verification completed with exit0 for optimized build, full tests, content checks and all23 CLI cases;29 optional Python/UnityPy skips remain explicit skips. The full core ASan/UBSan run also exited0 with leak detection enabled. Logs and exact commands are retained under `rebuild/build/starter-pair-next/{checks-done.json,final.log,sanitize-final.log}`. The later separate backward-preset preparation has34 focused checks without skips and an installed-Unity managed compile; those followup tool edits postdate the full optimized run.


The original manual Zink capture closed at3600 frames over74.367614 seconds. [Detailed bounded evidence](reference/shield-observer-zink-20261006.json) validates451 steady Sword-block frames, including useful idle/walk/run mixtures, drawing, attacks and later block restoration. No fully pure gait-plus-block sample meets the unchanged.9999 diagnostic threshold; the source itself retains authored style-point and speed blends. This is positive composed-pose evidence, not an exported looping block asset or a reason to discard the mixed samples. All14 protected save files remain unchanged. After the user returned control, SYSTEM→Quit Game→YES exited137, matching the known source self-kill pattern.

The first backward-sampler launch crashed before arming under Zink; [the core audit](reference/sword-backward-zink-crash-20261006.json) records the distinct NVIDIA/Gallium near-null fault. A fresh retry adding only process-local `ZINK_DEBUG=flushsync,nobgc` completed48 samples for SwordWB441/SwordRB708 and120 cleanup-survival updates in the female tutorial. Ordinary Andy acknowledgement advanced Camera tips, followed by SYSTEM→Quit Game→YES and raw exit−9/launcher137. These observations establish that capture and continued responsiveness, not guaranteed graphics stability or a zero exit. Capture validation/staging is recorded in [the pose-group audit](audit/STARTER_SWORD_POSE_GROUP.md).

The backward followup passed strict validation for all48 samples/42 mapped bones. The new coherent stage has33 matching body/equipment clips and42 defence clips, preserving earlier payloads, transforms, textures and capsules. Seven unified tests (including every-frame native defence queries under ASan/UBSan/leak detection) and ten backward attachment tests pass without skips. [The compact result](reference/sword-backward-pose-result.json) records hashes and tolerances; [the responsive tutorial menu](reference/sword-backward-complete-menu.png) supplies bounded lifecycle evidence. These assets remain staged and normal native Sword/Shield activation remains disabled.


## October 6 — offline Sword/shield actor integration checkpoint

**Implemented and checked offline in C; normal use remains disabled.** The backward02 capture is complete:48 validated samples now form the staged33 body/equipment and42 defence group. `pe_sandbox_actor` has item-specific Sword1/Knife33 melee interfaces, grounded Sword backward/shared-turn selection and exact-instance passive Shield34 binding. Body, weapon, shield and defence query use the same read-only pose names, weights and authored clocks. The existing21-clip Knife assets remain compatible; the new stage is not installed. See [the current pose/actor audit](audit/STARTER_SWORD_POSE_GROUP.md#current-c-actor-and-host-integration-groundwork).

Mocked actor tests and a real-asset CPU harness pass optimized and under ASan/UBSan with leak detection. The latter loads installed21, staged33 and a read-only mixed overlay, performs actual skin/capsule calculations, and mocks only the graphical boundary. Real source-blade contact tests separately verify sword wear, per-target depletion, no miss/deduplication wear, overkill and transactional rollback; `rebuild/build/sword-wear/result.json` records all four build/run exits0. These results do not establish native graphical attachment/block presentation or four-kill completion.

The full `integration-full.log` test run exited0 with29 optional skips **before the latest host/UI fixes**. The reviewed latest frontend/UI/tutorial-clock/consumable followup exits0 in `integration-reviewed.log`; UI/tutorial-clock sanitizer checks are recorded in `integration-ui-sanitize.log`, and the current focused game ASan/UBSan check passed with silent `game-focused.log`. `sandbox-equipment-host-test` also exits0: actual static host helpers pass strict optimized and ASan/UBSan/leak checks with zero warnings/skips, recorded in `rebuild/build/equipment-host/verification.json`. Those helper tests mock presentation and asset callbacks; they do not add graphical or recovered-geometry evidence. This is bounded followup coverage, not an uninterrupted latest-tree full-suite claim.

The normal host leaves `starter_pair_available=false`. Masked shield blocking, the source air-family selector, slope/first-person admission and full Unity controller behavior remain incomplete. No new graphics were launched for this checkpoint, no staged assets were installed, and no ordinary playable Sword/Shield flow is claimed.

### Subsequent native Character check

A separate90-frame native smoke loaded an explicitly staged v15 fixture: ordinary API starter supplies, selected Wood Sword1 with durability0, selected intact Wood Shield34, and intact starter clothing. [The Character screen](reference/starter-pair-unavailable-character.png) shows Attack10, Defense12, Wood Sword durability0/2369 and dimmed equipment icons. This verifies that exhausted Sword loses its bonus and an unavailable shield does not inflate displayed defense. The build exited0; there were no tool swings or hunt contacts. The fixture is deliberately arranged UI evidence, not ordinary equipment acquisition, working block presentation or a hunt playtest. Its isolated save paths and protected earned-hunt hash are recorded under `rebuild/build/starter-pair-next/character-ui-*`. Installed assets remain unchanged.

### Subsequent original masked-composition replay

The isolated original female tutorial completed a three-sample clone replay of the user's recorded Sword idle/run/walk plus shield layer. The callback-free controller bundle loaded as one exported controller with608 references to452 distinct event-free clips. Returned current states, independent normalized clocks, layer weights, clip weights and parameters match all three source observations exactly. Human-bone model-space positions still differ by maxima0.04724m,0.05769m and0.00912m respectively. The clone deliberately disables built-in foot IK and carries no live gameplay/IK components; the residual differences have not yet been isolated to a single cause. This establishes a working bounded composition mechanism, not full pose parity, seamless loops or normal native blocking.

The [bounded runtime record](reference/shield-composition-smoke-20261006.json) preserves three earlier rejected attempts and their diagnoses: incorrect bundle enumeration, an initially unlocalized nonfinite metadata field, then effective state duration+Infinity with playback frozen. Final capture serializes that frozen duration explicitly while retaining strict actual-clock, clip and matrix checks. All four sessions survived120 cleanup frames and ordinary Andy acknowledgement; [the final SYSTEM menu](reference/shield-composition-responsive.png) followed Camera tips. Root quit each through SYSTEM→Quit Game→YES; launcher137/raw−9 matches the previously observed original self-quit pattern, not exit0. No ordinary saves or production runtime assets were modified. See [the shield-layer audit](audit/SHIELD_LAYER_CAPTURE.md) for source hashes, limits and the next qualification step.

## October6 passive recovery — source and headless verification only

The [recovery increment](audit/PASSIVE_RECOVERY.md) adds source buff30200046 to Adventure Sandbox: conditional1% maximum-HP pulses below33%, after at least4 seconds without a committed hit. Its source lifecycle was recovered from code, database and a read-only player-prefab decode. Neither original nor native graphics was launched for this increment.

The optimized executable build and full core suite pass with zero failures. Actual health/equipment host helpers, the existing consumable/contact regression and sandbox UI checks pass; selected asset-dependent checks did not skip. New clock and host tests pass ASan/UBSan with leak detection. All13 exporter tests, content regeneration and23 CLI checks pass. The host test verifies unchanged literal v15 saves, successful-load transient reset, failed-load atomicity and a loaded dead player remaining dead. Its damage-count inputs are staged committed notifications; existing capsule/dedupe/rollback tests provide the separate damage coverage.

This establishes source/headless behavior, not an observed retreat/recovery or completed ordinary hunt. Native clock epoch, capped delta, pause and same-frame ordering adapters are documented in the audit. The four-hare ordinary-combat completion and partial-kill physical reload checks remain outstanding.


## October6 stamina and comfort — source and headless verification only

The [stamina increment](audit/STAMINA_RECOVERY.md) implements the source stamina, comfort and derived-delay buffs in Adventure Sandbox. Recovery uses the shared eight-display-frame attribute gate, separate pulse indices and the source old-delay/new-raw-comfort ordering. Action tails refresh consumption time even when stamina is already zero. The Character panel now displays comfort; no native/original graphics session was run to inspect it.

The final optimized executable build and full core suite pass with zero failures and no asset skips. The initial core run found one tool test expecting the removed continuous regeneration; the corrected test drives the real attribute scheduler and verifies fixed ticks cannot heal stamina. Nine focused current/historical save suites pass, including literal v15-to-v16 prefix preservation, old-save500 default, transient clock reset and invalid-state atomicity. The actual health/stamina/equipment host, existing consumable/contact, sandbox UI, content,25 exporter tests and23 CLI tests pass. Focused clock, new-save and actual stamina-host ASan/UBSan runs pass with leak detection; this is not a new full-core sanitizer run.

PECSv16 adds four bytes of raw comfort, preserving existing inventory, equipment, world and quest state. General hunger, food, bed/rest and revival behavior remain outside this increment. Ordinary native four-hare completion, partial-kill physical reload and the recovery presentation checks remain outstanding.


## October6 — offline melee capsule-history correction

Source-derived and implemented in C; no original/native graphics session in this increment. The source-first audit's third correction now applies previous/current capsule sampling to sandbox blades and all four authored hare forearm parts. Player/hare entity centers are recovered from the correct model subtrees; sibling ragdolls and outer OutColliders are excluded. Damage values, creature counts and save versionv16 remain unchanged. [Lifecycle, numerical qualification and adapters](audit/MELEE_CAPSULE_HISTORY.md) describe exactly what is reproduced.

Validation at this checkpoint:

- Full optimized core: PASS,0 failures (`rebuild/build/capsule-history/core-final.log`). Recovered running408 contact and arranged four-kill/reload lifecycle fixtures use outgoing history; this remains arranged headless evidence, not ordinary gameplay completion.
- Optimized executable and actual equipment host build/check pass. Sandbox inventory/UI and consumable contact host checks pass (`host-integration.log`); the equipment helper verifies that the real frontend opts into the exported player center.
- Numerical helper and incoming hare damage/hunt suites pass focused ASan/UBSan with leak detection. Outgoing resolver, sandbox melee and recovered running-pipeline suites also pass focused ASan/UBSan/leak checks, built with `-Werror` (`outgoing-sanitize-build.log`, `outgoing-sanitize.log`). No new full-core sanitizer run is claimed.
- Existing generated content checks and23 CLI cases pass. New numerical exporter10 cases, player-center11 cases and hare-center7 cases pass with no skips in their qualified runs; the latter two use `rebuild/build/unity-tools/bin/python`. Ordinary Python player tests explicitly skip the real UnityPy-dependent case; such skips are not passes. All three new regeneration checks pass.
- Standalone installed-Mono/original-managed comparison:4,103 cases,2,714 hits, all hit/miss outcomes agree; maximum component error2.3840332019631205e-7. [Manifest](reference/melee-capsule-history-numerics.json) records the hashes/results. Selected18 cases are committed; the full generated oracle/corpus remains ignored locally.

Tests cover intermediate-only contact, previous-only one-sample behavior, source radial/closest-point rules, shared multi-target snapshots, active updates despite misses/dedupe/no skill guide, same/new-instance activation, inactive retention, per-part mapping, invalid rollback, wear and death cleanup. Native60Hz sampling/activation pinning and bounded target eligibility remain explicit adapters; source broadphase/effected-trigger callback ordering is not recovered. This work does not establish visual fidelity or easier/harder combat. Four ordinary native kills, physical partial-kill reload, retreat/recovery and shield block-pose qualification remain open. The user retains desktop control.


## October6 — offline attack playback and shield admission

Source-derived and implemented in C, with no original/native graphics session. [Attack playback](audit/ATTACK_PLAYBACK.md) uses the exact player0.65s curve, independent trigger notifications, source timestamp persistence across action/equipment changes, and a pre-contact snapshot across same-display catch-up. PECSv16 is unchanged. The [shield action owner](audit/SHIELD_ACTION.md) is disconnected from ordinary input and rendering; it does not remove the composed-pose gate.

The focused strict optimized and ASan/UBSan suites pass with leak detection and zero asset skips. They exercise playback, source56-mask shield admission, geometry/damage separation, synthetic multi-target permission/dedupe/rollback, and real recovered running408 assets. In the arranged fixed-hare comparison, contact occurs at tick14; response/miss actions end at ticks61/52 with the same authored endpoint1.1680725812911987 and stamina95. Root-motion deltas, body vertices and blade poses change consistently with the response interval while action wall time advances by1/60. Detached same-display catch-up copies retain the pre-contact response. This is controlled integration evidence, not earned ordinary combat. Logs: `rebuild/build/melee-playback-sanitize.log` and `playback-support-sanitize.log`; adjacent `.command` and `.exit` files preserve invocation/result.

The full optimized core suite passes with0 failures and no asset skips (`rebuild/build/attack-playback/core-final.log`). The optimized executable, actual frontend equipment helper, content regeneration and23 CLI tests pass (`rebuild/build/attack-playback/integration.log`). Eighteen offline exporter tests and regeneration checks pass with UnityPy, zero skips (`source-check.log`); a separate installed ResourceManager check confirms the exact modern component payload (`installed.json`). The SHA-pinned original native arithmetic replay matches all4,109 C inputs bit for bit, maximum absolute error0; [the preserved report](reference/unity5-curve-arithmetic.json) hashes the final metadata. Only bounded arithmetic ran, with no engine initialization or desktop input.

Review found and fixed invalid/backwards begin/queue timestamp admission before stamina mutation, and premature response resampling within one display frame. Existing geometry/history, HP, item wear and save owners remain authoritative. Physical response presentation, ordinary four-kill cleanup, partial-progress physical reload and shield composed-pose acceptance remain open. The user retains desktop control.


## October6 — offline humanoid arithmetic and existing-pose comparison

No new game/graphics session or desktop input. [The extraction proof](audit/HUMANOID_ROTATION_PROOF.md) qualifies272,217 muscle/limit/sign arithmetic samples against copied, bounded native instructions with zero bitwise differences. The experimental helper remains outside runtime CORE. Source binary inspection separately confirms the52+20+20 muscle loop correction and its two enable flags.

Three existing closed unlayered captures supply697 poses across22 clips, with27,880 comparisons of40 non-root local bases. The final comparator checks capture/launch/source/Avatar/helper hashes and keeps inputs unchanged. After loop correction,28 non-limb bones have maximum local-basis error2.47121e−5 across the entire set;12 limbs still differ up to1.35407013. [The compact manifest](reference/humanoid-local-rotation-comparison.json) records every bone and per-clip residual. These are matrix-component differences, not physical distance or complete pose parity. Root/world/goal solving and composed shield poses remain unqualified.

The focused Makefile target passes its C native-golden/validation tests and35 Python cases with zero skips. The final C helper suite passes ASan/UBSan with leak detection. No native runtime behavior, save format or installed animation group changed; full gameplay/graphics reruns would not validate this missing limb computation. Four ordinary native kills and partial-progress physical reload remain open. Desktop coordination was requested separately; no answer or elapsed waiting time has been treated as permission.

## October6 — source-derived Revival Shot integration

[Revival Shot937](audit/SANDBOX_REVIVAL.md) now has a dedicated native death-modal transaction, recovery remount and transient1.5s contact grace. The implementation uses repository C#/database contracts; no original or native graphics session was launched. Upright/no-ragdoll presentation, next-display get-up completion, atomic preflight and clamped v16 saved Comfort are explicit adapters. Full original get-up animation and town-return respawn are not implemented.

The full optimized Makefile suite passes, including core0 failures; its broad Python run has47 optional skipped cases across14 suites, which are not passes. The new12 revival exporter cases have0 skips. Content regeneration and23 CLI tests pass. The final frontend and actual revival/health/stamina host helpers were explicitly rebuilt after review; the existing consumable/contact test still verifies movement and a real incoming capsule contact while healing, with a single attack-clock advance. Logs are under `rebuild/build/revival-validation/{optimized,host-final,content-cli}.log`.

Focused revival core and actual host tests pass ASan/UBSan with leak detection, zero skips (`rebuild/build/revival-tests/sanitize.log`, `rebuild/build/revival-host/sanitize.log`). Save tests use an explicitly staged two-kill state to verify persistence and one-shot consumption; this does not establish ordinary partial-kill reload. Recovered player/hare capsule tests show that disabled defence suppresses HP/wear/guide-dedupe changes while actor, skill and history clocks advance; later same-guide contact succeeds after reenable. This is arranged headless evidence, not a combat playthrough.

The final full core ASan/UBSan run also passes with0 failures and leak detection enabled (`make -C rebuild sanitize ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `rebuild/build/revival-validation/sanitize.log`). No asset-dependent test reported a skip in that run.

Review corrected death's global simulation freeze and complementary melee/hunt dispatch during the revival modal. A health test's fresh-entity fixture now resets the new transient clock when it resets the other recovery owners. The new panel, death→revive→combat flow, four ordinary native kills and partial-progress physical reload still need a desktop session; the pending coordination question remains unanswered. No renderer or physical parity is inferred from these checks.

## October6 — offline sword branch and movement policy

[The sword policy increment](audit/SWORD_MOTION_POLICY.md) connects one source branch selector to display input and core admission and shares the existing running horizontal request math. A concrete diagonal boundary now selects ordinary instead of incorrectly starting the running skill; rejection leaves stamina/action state unchanged. Ordinary slow-moving attacks remain unavailable pending their own root extraction. No game, graphics or desktop input occurred.

The focused optimized and ASan/UBSan/leak integration suites pass with0 C failures and no asset skips. They include the existing recovered running408 playback/contact, outgoing interval, combo and wall/tree collision tests. The executable build, dependency-light content checks and23 CLI cases pass. The separate UnityPy source check passes all15 exporter cases without skips; the system interpreter skips four optional prefab cases. The pinned installed Mono oracle matches4,101 magnitude float-bit/branch results exactly, and a separate read-only installed component check verifies the4.5/7 binding. Logs: `rebuild/build/sword-motion-check`; numerical provenance: [report](reference/sword-variant-numerics.json). This is focused validation, not a new full-core or native playthrough result.

Four ordinary native kills, actual partial-progress reload and graphical revival acceptance remain outstanding. Source evidence and arranged contact tests do not close them.

The accompanying [root-channel investigation](audit/PLAYER_ROOT_CHANNELS.md) reuses the closed running06 capture. Source RootT translation scaled by FemaleAvatar777 agrees at822/1773 in-range timestamps with maximum errors3.547/10.563μm; exact-end interpolation errors are separately65.320/19.499μm. The seven synthetic decoder/comparison tests pass. Static byte-range evidence identifies root channels, importer flags and human-scale use, but the final delta/basis chain remains incomplete. The [pinned report](reference/player-root-channel-verification.json) explicitly leaves ordinary runtime qualification false; no recovered motion asset was installed.


## October6 — offline ordinary roots and walking Knife integration

The [walking-attack increment](audit/SANDBOX_WALKING_KNIFE.md) enables grounded female third-person ordinary attacks while moving through speed4.5, with source-derived Attack6–9 root displacement. Running408 keeps its qualified captured curve; later running combo stages now use the corresponding ordinary roots. One outgoing interval owns movement, collision, body/blade and incoming defence. PECSv16 and damage/HP/wear values are unchanged; tutorial moving attacks and ordinary Sword/Shield presentation remain gated.

**Source-derived evidence:** the pinned native RootT-to-deltaPosition chain and twelve isolated arithmetic cases qualify bounded unblended extraction. The report retains `ordinary_clips_observed_sequentially=false`: no original gameplay was run to obtain these curves. Four exported dense XZ tracks form2392-byte PESR1 with SHA256`f0d8b2171c75b1098123e482115d5d1a576a309fc494807076e3d4330661f3fb`. The final fresh export and `--check` pass; the identical group is installed at `rebuild/build/reference-assets/player-sword-motion`. Existing poses and captured running motion were preserved.

**C verification:** final `make all sword-walking-test sword-policy-test hunt-consume-test` passes. Walking tests use actual recovered body/blade/hare and root assets, covering forward/back/side admission, boundaries, root-produced speed, outgoing combo/end intervals, single movement ownership, obstruction and arranged post-sweep blade contact. Reader, walking integration and actual host dispatch pass optimized and ASan/UBSan with leak detection and no asset skips. Host checks validate48 body/Knife/defence clock comparisons; a deliberately missing motion path returns77 rather than passing. Ten proof/decoder and four exporter tests pass. Final `check-content`, all23 CLI cases and the registered `sandbox-sword-motion-host-test` pass. Logs are under `rebuild/build/sword-root-check`, `sword-walking` and `sword-motion-host-check`.

**Limits:** these are focused checks, not a new full-core/full-sanitizer or graphics run. Arranged contact is not an ordinary hunt kill. No original/native graphics session or desktop input occurred. Walk→attack/combo/wall presentation, the preceding Revival Shot UI, four earned native hare kills and physical partial-progress reload remain pending. Full source Animator blending/target-facing, bit-identical arithmetic and PhysX behavior are not established by this increment.

## October6 — source-derived vendor wallets and v17 save extension

[Vendor wallets](audit/TOWN_WALLETS.md) now preserve the eight realized starting purses and source observed noon-refill latch, and normal purchases atomically credit the NPC alongside player/stock debit and inventory grant. The exporter reads the existing digital-Adventure archives without changing them and guards the active source clock/money/serialization/purchase paths. PECSv17 preserves the literal v16 prefix plus100 bytes for eight identified balance/latch records. Pre-v17 Adventure receives archived defaults, explicitly without reconstructing historical purchases/refills. Selling/buyback remain unimplemented.

Final focused optimized wallet/save checks and all15 exporter tests pass with zero skips; generated output `--check` passes. Final strict ASan/UBSan with leak detection covers both wallet/save and the actual sandbox display helper, with0 failures and no graphics/asset skips. Cases include source divide/floor time evaluation, noon/morning boundaries, skipped mornings, no maximum cap, paused/focused/dead-player behavior, fixed-step isolation, atomic purchase/refill overflow, malformed v17 payloads and old-save migration. The large-seconds source-expression regression is a diagnostic within the native finite-double domain, not an observed original-game noon defect or claim that Unity's integer timer can represent that value.

Final optimized `make all test town-wallet-test check-content cli-test` exits0, including the complete core suite, actual wallet host adapter and all23 CLI cases. The ordinary Python interpreter reports51 optional skipped tests across the broader suites; they are not passes. All15 new wallet exporter tests run without skips.

The broad core ASan/UBSan run with leak detection also exits0. It was built before the final `fmod`→source divide/floor/multiply correction; the final optimized suite and final focused wallet/save/host sanitizers cover that correction. This is a composed verification result, not a claim that the broad sanitizer binary contains the final clock expression. An initial parallel build/check run stopped at stale generated wallet output while the exporter was being finalized; the frozen exporter and complete optimized rerun pass.

Evidence is under `rebuild/build/town-wallet-core/verification.json`, `town-wallet-export/verification.json` and `town-wallet-check`. No original/native gameplay, desktop input or new physical purchase/refill check occurred. Ordinary four-hare completion and actual partial-progress reload, plus walking/revival visual acceptance, remain outstanding; arranged test kills do not satisfy those checks.


## October6 — ordinary hunt acceptance preparation

The [focused acceptance guide](audit/HUNT_ACCEPTANCE.md) now supplies a byte-identical isolated copy of the preserved earned0/4 checkpoint and a tracked read-only native save inspector. Current-loader inspection confirms500HP, Defence12 through the documented old-save migration,8 Medpacks,10 Juices,10 Revival Shots and the intended quickslots. Source/baseline/working SHA256 remains`2b7f763da6bd453da8fb8102a914983e1913c625211803f5151648120ee0ca6f`; no fields were edited or resaved. Build, JSON parsing, expected state and missing-save/usage failure checks passed. The stale walking/running restrictions in the user-facing controls were corrected to match the already implemented behavior.

No graphics, desktop input, ordinary kill or partial-progress reload occurred. The pending desktop coordination question remains unanswered; the user retains control. The guide distinguishes a user-run session from agent operation after a handoff. This preparation does not replace the remaining physical acceptance gate or extend the goal with unrelated economy work.


## October 6 — source-derived vendor selling and buyback (no graphics)

The [trade implementation](audit/TOWN_SELL_BUYBACK.md) adds atomic vendor-funded sales, source component-selected prices, exact worn-instance repurchase and Buy/Sell/Buyback tabs. [The source audit](audit/TOWN_SELL_BUYBACK_SOURCE.md) records the Adventure currency field, Repair condition binding, zero-value consumables, source buyback purse asymmetry and session-only evidence. No original/native executable was launched for this increment, no desktop input was sent and no user save was changed.

Executed from the repository root with `TMPDIR=$PWD/rebuild/build/compiler-tmp`:

- Optimized `make -s -C rebuild -j2 all town-trade-test`: exit0; focused transaction/lifecycle C checks report0 failures, exporter11 tests pass with0 skips.
- Final `make -s -C rebuild -j2 test check-content cli-test`: exit0; complete optimized core0 failures, source/generated checks pass and23 CLI tests pass. Broader Python tests retain51 optional skips; these are not passes.
- Full `make -s -C rebuild sanitize` with default `ASAN_OPTIONS=detect_leaks=1`: exit0, complete core0 failures, including the new transaction and save/session tests.
- Focused UI ASan/UBSan with `ASAN_OPTIONS=detect_leaks=1`, a separate `BUILD=build/town-trade-check/sanitized` and instrumented CFLAGS: exit0, real shop input/core transaction test passes with no graphics/assets/skips.

The tests cover worn/exhausted durable identity, source rounding and vendor-credit asymmetry, partial stack sale/repurchase, zero-value transfers, equipped/shield refusal, unsupported LifeLimit prices, both purse limits, full bag/list rollback and cross-owner aliases. The lifecycle suite checks clone/move/world replacement, save-with-live-buyback, failed-load retention, successful-load clearing and identical v17 serialized bytes after round trip. The actual UI router checks both currencies through transactions, modal digits/closing-frame held-release ownership, close/reopen/vendor isolation, row compaction/scrolling, dragging and800×600 geometry. These are headless checks, not rendered layout acceptance.

During development, the first build exposed an include preceding the existing POSIX feature macro; its order was corrected before final compilation. One UI test initially assumed a zero-money player could not buy back Juice916; the source price is zero, so the final test asserts success and uses a worn Knife for the insufficient-funds case. The completed optimized and focused sanitizer runs use that corrected test. Local logs are ignored under `rebuild/build/town-trade-check`.

Graphical acceptance and ordinary four-hare completion/partial-progress reload remain pending. The trade audit contains a short disposable-checkpoint checklist for the user's next manual session. Buyback clears on load/new game, while money and the completed sale remain saved; the UI explicitly states this boundary.


## October 6 — source-derived construction history (no graphics)

The [construction history implementation](audit/CONSTRUCTION_HISTORY.md) adds a five-action transient journal for supported sandbox cube/ramp batches, source Ctrl+Z/Ctrl+X and labeled native toolbar buttons. [The source audit](audit/CONSTRUCTION_HISTORY_SOURCE.md) qualifies history capacity, material rounding, inverse modes, shortcut priority, mining exclusion and source failure/lifetime limitations. No original/native game window was launched, no desktop input was sent and no user save was changed.

Executed from the root with `TMPDIR=$PWD/rebuild/build/compiler-tmp`:

- Focused optimized build, history/lifecycle, actual-host/input, existing sandbox UI and content checks: exit0; construction C0 failures, source exporter14 tests with0 skips, headless host collision/input and existing UI pass.
- Full optimized `make -s -C rebuild -j2 all test check-content cli-test`: exit0; complete core0 failures, source/generated checks and23 CLI checks pass. Broader Python tests retain51 optional skips, which are not passes.
- After the renderer-only hover-summary correction, final `make -s -C rebuild all cli-test`: exit0, final executable rebuilt and23 CLI checks pass; no core/input code changed after their tests.
- Full `make -s -C rebuild sanitize` with default `ASAN_OPTIONS=detect_leaks=1`: exit0, complete core0 failures including new history and lifecycle tests.
- Focused actual-host ASan/UBSan with `ASAN_OPTIONS=detect_leaks=1` and isolated `BUILD=build/construction-history-check/sanitized`: exit0, no graphics or optional asset skips.

The core tests cover mixed-material ceiling debit/floor refund, zero refund with a full bag, four-cell and64-cell batches, ramps, five-entry eviction, successful-new-edit redo invalidation, insufficient materials/refund capacity, changed/missing/occupied cells, player/terrain/legacy-block/reach constraints, malformed journals and unjournaled mining. The save suite checks independent clone/move, active undo and redo preserved by saving, failed-load retention, successful-load clearing, failed/successful world replacement and unchanged v17 wire bytes after round trip.

The actual host/input test verifies immediate ray and movement-sweep refresh after undo/redo with no fixed simulation step, unchanged game/tool clocks and native `built` statistic, simultaneous-key priority, Ctrl-released-first chord ownership through release, modal/closing-frame and pause/focus/B-mode guards, and toolbar pointer ownership through held/release frames. The final renderer chooses either the hover hint or material summary so they cannot overprint; this static review does not establish graphical acceptance.

Local logs are ignored under `rebuild/build/construction-history-check`. The construction audit provides a short later manual check. It remains pending alongside ordinary four-hare completion/partial-progress reload; these headless fixtures do not establish those physical outcomes.


## October 6 — fixed vegetation examples (source and headless checks)

The user requested example foliage, trees and herbs, deferring random placement until terrain generation. [The implementation](audit/SANDBOX_VEGETATION.md) adds two existing cuttable tree instances, six primary-yield herbs and two decorative plants near new-sandbox spawn; existing Adventure saves require explicit `--vegetation-examples`. [The source audit](audit/SANDBOX_VEGETATION_SOURCE.md) records actual wild prototype identities, source gathering bounds/action masks and offline mesh/material recovery. PECSv18 preserves the literalv17 payload and appends648 bytes for bounded plant placement/depletion. No original/native window was opened, no desktop input was injected, and no ordinary user checkpoint was modified.

Completed focused checks:

- Direct offline extraction recovered all four wild plant meshes and their original diffuse texture. Source/generated metadata and independent qualified asset hashes pass;13 focused Python tests pass with0 skips. The first exporter output failed the shared OBJ/MTL provenance-preamble validator; correcting and regenerating the exporter output resolved it without weakening the loader.
- Focused vegetation core/save tests pass with0 failures: deterministic supported placement, idempotence, capacity rollback, primary yields, no RNG use, renderer-bound selection/range, obstruction, clone/move/world replacement and v18 round trips. Save validation checks malformed flags/counts/prototypes/coordinates/depletion, duplicates, unused records, truncation/trailing bytes, literalv17 migration and unchanged state/file on rejection.
- Final actual-host optimized and strict ASan/UBSan/leak checks pass. They exercise nearest-target/modal priority, successful and full-bag harvest ownership through pointer release, source melee/tool busy admission, ongoing movement and production hunt-loop admission. They do not claim physical keyboard/mouse or animation acceptance.
- Final frontend CPU checks pass optimized and ASan/UBSan with leak detection, all four installed OBJ/MTL/PNG groups and0 skips. Tests check exact triangles, shared picking/render bounds, source cutoff, partial/malformed groups and oversized/truncated PNG rejection. The warning from the intentional truncated-image fixture is expected. No GPU context is created. The earlier temporary absent-group skip during asset regeneration is superseded by these final actual-asset passes.

A separate headless route fixture starts at local(34,27.487,284), world origin(-640,0,64), on the installed416×64×416 route. It places the sample patch and verifies save/reload at ignored `rebuild/build/vegetation-check/examples.pesave`. A subsequent isolated movement probe clones that checkpoint for each target and walks through `pe_game_step_collision`, with no teleport or terrain edits. All six herbs are reachable in75,90,125,76,90 and172 fixed ticks respectively; each harvest produces its exact primary yield and survives a separate save/reload. Both trees are reachable in99 and159 ticks. The probe explicitly grants/equips Wood Axe26 and supplies the selected tree indices to the authoritative tool clock: six contacts fell each tree and add8 Wood. This establishes collision-controlled approach and core cutting, not graphical ray picking, Safety Axe presentation or source loot parity. The base example checkpoint is untouched.

Final complete regression, with `TMPDIR=$PWD/rebuild/build/compiler-tmp`:

- `make -s -C rebuild -j2 all test check-content cli-test`: exit0, complete optimized core0 failures, source/generated checks and26 launch-option tests pass. The broader Python suites retain51 optional skips; the vegetation source/asset tests have0 skips. Log: `verified-full.log`.
- `make -s -C rebuild sanitize`: exit0, complete core0 failures with ASan/UBSan and default `ASAN_OPTIONS=detect_leaks=1`. Log: `verified-sanitize.log`.
- Final frontend rebuild/26 CLI checks also pass after the source cutout shader correction; `make -q -C rebuild all` confirms the executable is current. No graphics acceptance is inferred.

Earlier passes exposed stale version17 assertions and the legacy-fixture writer's version ceiling; those test fixtures were updated to strip the v18 suffix before exercising historical payloads. Historical gameplay/save validation was retained, and the final complete runs above supersede the failed/interrupted preliminary logs.

Local evidence is ignored under `rebuild/build/vegetation-check` (fixture, route access probe, full regression and sanitizer logs) and `rebuild/build/vegetation-ui-check` (actual-host and final visual CPU checks). Graphical acceptance remains pending: leaf transparency/lighting, grounding, herb/NPC selection, Safety Axe chopping, ordinary gather input while moving and visible depletion after reload. Random spawning/regrowth, source Gather animation/timing, optional/fallback loot and full foliage shader behavior remain outside the supported claim.

## October 6 — inventory categories and source harvesting

The user requested both inventory improvements and harvesting fidelity without waiting for manual attention. [Inventory](audit/SANDBOX_INVENTORY.md) now has four independent420-cell Adventure categories,42 visible cells per page, stack splitting, source exchange, explicit native Shift+drop merging and source consolidation/sorting. Legacy/tutorial retain32 cells. PECSv19 retains the older wire layout and appends26888 bytes of authoritative inventory; its compact32-cell prefix is checked, not a second item owner or a general downgrade mechanism. Historical saves migrate valid Adventure objects in stable category order without changing instance IDs, wear, equipment/quickslots or the allocation counter.

[Harvesting](audit/SANDBOX_HARVESTING.md) qualifies the displayed tree as prototype48 and implements its source hardness and primary/optional production, plus herb fallback and optional seed drops. Tree and herb transactions stage the saved native RNG with inventory, so a capacity failure cannot silently lose loot or reroll the result. The catalog now contains323 items and95 formulas. Placement remains fixed; the native xorshift stream does not reproduce Unity's exact random sequence. Immediate herb gathering, seed planting, regrowth and procedural distribution remain separate presentation/gameplay gaps.

Focused verification completed:

- Inventory core/save/harvest suite:0 failures. Covers independent category saturation, source sorting/consolidation, split/swap/merge, exact durable identities and wear, full-category rejection, old-save migration, occupied slots beyond the historical32 and last-page positions, malformed current payload/prefix rejection and unchanged state/file on failure.
- Actual sandbox UI and shop routing: optimized checks pass, including category paging and move/split/merge/sort controls, high-index equipment/Resource1259, modal pointer ownership and controls contained at640×480,800×600 and1024×600. The functional inventory UI sanitizer check also passes; later narrow-window and disabled-Mission-tab additions are checked in the final optimized run. No rendered or physical UI acceptance is inferred.
- Harvest kernel, vegetation and actual axe action clocks: optimized and ASan/UBSan/leak checks pass. Tests include primary/fallback/seed paths, scaled math, independent category saturation, exact final-contact rollback, Safety Axe19-contact and Wood Axe12-contact felling and source tool wear/stamina. Actual vegetation-host checks pass with no asset skips.
- Actual installed/staged actor asset test:1 test,0 skips, passing normally and with ASan/UBSan/leak detection, using categorized Adventure sword/shield cells420/421. The mesh/attachment tests do not establish on-screen animation acceptance.

An isolated CPU-only route probe loaded the unchanged prior PECSv18 `rebuild/build/vegetation-check/examples.pesave` (SHA256 `5b4da58b544642ccad4c2e652eb3f984a57f4e248a2ac6cb681b824153291933`). Migration yielded categorized1680 storage on the installed416×64×416 route with all original vegetation. The probe destroys each cloned owner before reuse and walks through `pe_game_step_collision`, without teleports or terrain edits. All six herbs were reached in75,90,125,76,90 and172 ticks; complete production, RNG, depletion and inventory survived isolated save/reload checks. The two trees were reached in99/159 ticks and passed source-capsule2m interaction rays. With an explicitly granted Wood Axe26, each took12 authoritative contacts, granted16 Wood and20 combined Rubber/Resin, and debited60 durability. Full inventory/RNG and equipped identity matched the expected transaction after reload. The original fixture hash remained unchanged. Evidence is ignored under `rebuild/build/inventory-harvest-route`.

The supplied crash report and systemd records were reviewed. The23:36 test_core SIGSEGV followed a stale capacity fixture: supplies were unexpectedly accepted, then a nonfatal assertion was followed by a NULL dialogue dereference. The fixture now fills the appropriate category and dialogue/text assertions are null-safe. The23:30 inventory UI SIGABRT was an overly strict test assertion rejecting simultaneous full-modal and action ownership; permitting both retains the stronger modal and no-leak assertions. No OOM entries accompanied these failures. Two narrow native/Python test compile lists also needed the extracted inventory owner. Corrected tests pass; these preliminary failures are not counted as successful runs. The user's original crash handoff remains untouched locally.

No original-game or native graphical window was launched, and no desktop input was sent. One later visual session can check the new bag controls/readability alongside herb selection, depletion and axe presentation. Core capacity, loot and save contracts do not depend on another original-game session.

The final optimized build and aggregate checks passed as a composed run: all native prerequisites and full core0 failures, content regeneration and26 CLI tests in `rebuild/build/inventory-harvest-verified-final.log`, followed by the corrected actor-asset test and every remaining Python recipe in `rebuild/build/inventory-harvest-python-remainder.log` (exit0). Together the ordinary Python suites retain51 optional skips; these are not passes. New inventory/harvest source checks and actual vegetation/actor asset checks have no skips. The earlier logs ending at linker/assertion failures are superseded only for the checks rerun successfully.

The final full core ASan/UBSan run also passed with0 failures and `ASAN_OPTIONS=detect_leaks=1` (exit0), logged at `rebuild/build/inventory-harvest-sanitize.log`; its warning-free compile log is `inventory-harvest-sanitize-build.log`. The Makefile sanitizer compile command was run separately while the optimized prerequisite builds completed, and the resulting binary ran after the optimized core finished, avoiding overlapping core fixture writes. Existing explicit child-process leak-scan limits remain as documented by their fixtures. No new coredump was recorded after the corrections. Changed-document links and whitespace checks pass; the local structural index was rebuilt.

## October 7 — camera recovery and offline combat foundations

[Camera environment](audit/CAMERA_ENVIRONMENT.md) now runs the source66-ray activity evaluator and source two-second obstruction recovery in Adventure. Only final displayed geometry feeds its latch; aim queries are read-only. The existing native eye/rise/capsule framing and crop boundaries remain explicit adapters, with no claim of full near-plane/YawPitchFinding or visual camera parity. Successful loads reset transient camera/recovery; PECSv19 is unchanged.

[Hare pounce preparation](audit/HARE_POUNCE_PREPARATION.md) adds a disconnected source lifecycle/request owner and an isolated ninth generic animation clip. The existing eight-clip actor payload, skeleton and mesh remain unchanged; the production hunt does not select pounce. [Defense spawning](audit/TOWN_DEFENSE_SPAWN.md) adds source-ordered wave planning, deferred versus created members, stable beacon ownership and one-time death credit. Plans retain unsupported out-of-crop anchors; no live9077 acceptance, actor creation or save integration is enabled.

Executed checks:

- Optimized frontend build, original scalar/control camera tests, new environment tests, actual sandbox camera/collision-host tests and defense planner/member tests pass. Log: `rebuild/build/offline-next-build.log`.
- The actual camera host passes ASan/UBSan with leak detection (`camera-environment-host-sanitize.log`). Its real collision fixture distinguishes trees from walls, checks repeat aim calls cannot change the clock, and verifies final feedback, hold/wheel behavior, pause/first-person/overview gating, backend rollback and Legacy dispatch. The pure environment suite separately passes optimized and ASan/UBSan/leak checks.
- Pounce lifecycle and actual staged PEAC/parser/31-pose/four-forearm-capsule tests pass optimized and ASan/UBSan/leak checks. Source and staged asset validation pass; the new8-test source/asset suite has0 skips. Explicit `hare-pounce-assets-test` also passes. Recovered binary assets remain ignored locally.
- Defense planner/member tests pass optimized and ASan/UBSan/leak checks; eight source exporter tests have0 skips. Actual source clock requests drive two plans independently of completion. Tests preserve an eastward80m request outside the crop and complete15 credits with surviving/pending members retained.
- Four camera exporter tests, eight pounce exporter tests and eight defense exporter tests pass; existing generic actor/hitbox Python regressions pass with three optional skips, which are not asset passes. All content checks and26 CLI checks pass. Logs: `offline-next-source-tests.log`, `offline-next-content-cli.log`.

The actual-route camera CPU benchmark evaluated99,000 rays against387,048 collision triangles at five saved/archived anchors. Mean sampling cost was .378–.429ms per frame; observed maximum .541ms, with setup measured separately. Results were finite/repeatable and game/world/snapshot state unchanged. This single CPU run is not a rendered-FPS comparison; evidence is ignored under `rebuild/build/camera-environment-route`.

Two initial isolated assertion failures were incorrect test expectations: activity2 produces a slightly-above1.8 floating target, and a50m southward defense sample was not outside the crop. Fixtures were corrected against source arithmetic; final runs above supersede those failures. Subsequent development tests limit their own stored core dumps while retaining failures/exit codes. This does not prevent system crash notifications; the later inventory-discard records demonstrate that distinction. No original game, graphical rebuild, recording or desktop input was used.

The [Gather audit](audit/GATHER_ACTION_GATE.md) also checked335 recovered actor files and the installed controller. No player Gather poses are recovered: the action depends on exits from three overlapping humanoid states, not the tutorial's fixed timer. No guessed delay or replacement pose was added; immediate transactional harvesting remains its declared adapter.


## October 7 — inventory discard and follow-up mission contracts

[Inventory discard](audit/INVENTORY_DISCARD.md) now follows Delete → select → confirm, destroying a whole cell with protected-item and exact-owner checks. Split first for partial disposal. The modal revalidates item/count/identity/wear, consumes save/load even on its opening frames, and retains pointer ownership through close/release. No new save fields were introduced.

[Typed mission events](audit/ADVENTURE_MISSION_EVENTS.md) are a disconnected source-derived9077–9084 reducer with ordered dialogue acceptance, plot queues, separate grant latches and injected atomic effects. Synthetic callbacks verify rollback and one-time transactions; no additional playable quest, actual NPC chain or save integration is implied. [Catalog closure](audit/ADVENTURE_FOLLOWUP_CATALOG.md) adds nine books and thirteen recipes, for341 items/108 formulas. Tests use actual book-learning and timed-crafting APIs; colony and vehicle outputs remain unsupported for use/deployment.

The final aggregate in `rebuild/build/offline-next-final.log` exited0: optimized frontend, full optimized core (0 failures), inventory UI, general sandbox UI, shop trade UI, discard and mission-event tests, all content regeneration checks and26 CLI tests. This is a full core run plus selected relevant suites, not a new complete `make test` run. The general inventory/save/harvest cases are included in that core run.

Focused discard, mission-event and actual follow-up learning/crafting tests also pass ASan/UBSan with leak detection enabled. Source tests pass with zero skips: four inventory, seven mission-event, nineteen content and eleven trade exporter tests. New item icons and overlays already exist in the recovered atlas. Rendered confirmation layout and visual camera feel remain untested; no original game, graphical rebuild or desktop input was used.


## October 7 — offline camp repair and camera geometry qualification

[Camp repair](audit/CAMP_REPAIR.md) now has guarded source/installed/archive metadata for existing machine20006 and63 ordinary material profiles. Its disconnected owner transfers an exact bag instance, quotes/debits materials atomically, runs a60-second per-display truncated clock, cancels without refund and restores durability once. Its snapshots are value contracts, not a PECS extension; no live interaction or repair UI is enabled.

`make -C rebuild camp-repair-test camp-repair-source-check` passes. The focused native owner also passes ASan/UBSan with leak detection, and all ten UnityPy-backed source tests pass with zero skips. All63 quotes and current transaction cases are covered; no current profile supplies cross-category or multiple-material coverage. Original partial-job reload and animated machine presentation remain unobserved. The source check requires the qualified local UnityPy/reference inputs and fails rather than calling an unavailable decoder a pass.

[Camera geometry qualification](audit/CAMERA_GEOMETRY_GATE.md) decodes the active.8m near plane, auxiliary near-camera pass and exact ring/sphere/rise math. A reproducible CPU-only probe passes strict optimized and ASan/UBSan/leak checks, confirming native initial-overlap and pointer-ray-origin behavior. Nearby Unity5.3 primary documentation is identified as version-limited evidence. No geometry/projection runtime change is inferred from this audit. No original game, graphical window or desktop input was used for either increment.


## October 7 — review of the three supplied crash reports

[The incident review](audit/TEST_CRASH_REVIEW_20261007.md) reconciles the camera, defense-planner and inventory-discard reports with their four development-time assertion aborts. Their corrected tests were already in pushed checkpoints. A fresh optimized focused run of all three passes with exit0; no runtime patch or player-save operation was needed. An isolated camera probe reproduces the original activity2 versus literal1.8f target mismatch and identifies the first differing velocity bit. Historical executable/core mismatches remain explicit; current probe values are not historical core locals. No new full-suite or sanitizer coverage is claimed. The user's reports and original system cores remain unchanged.

## October 7 — persistent camp repair and native interaction

The existing camp Repair Machine20006 now connects its [source-derived service](audit/CAMP_REPAIR.md) to the Adventure host. `PeGame.repair` owns one exact inserted bag instance and its job; PECSv20 retains the literal v19 payload and appends32 bytes. Older versions infer an empty machine. The movable native panel lists supported unequipped items, confirms material spending/cancellation, shows durability and remaining time, and returns the same instance. Cancellation has no material refund. Closing the panel preserves the item/job; ordinary movement and hunt updates remain enabled after closing. Pause, focus loss and leaving the host suspend the display clock; death does not. No offline repair time is inferred.

The actual-host integration fixture stages a supported player pose near the machine on the installed416×64×416 route, ray-picks it through real terrain/static collision, and exercises the actual UI router. It verifies selection→insertion→confirmation, single material debit, exact-instance quickslot cleanup, closed-panel movement/hunt admission, fixed-tick independence, pause/focus suspension, mid-job save/load, range/death panel cancellation, held/release pointer ownership, completion and exact-instance collection. These are automated C results, not earned travel, rendered acceptance or an original partial-job observation. The spatial fixture separately verifies both installed crops, including blocked and visible approaches; see [interaction qualification](audit/CAMP_REPAIR_INTERACTION.md).

The focused UI fixture covers stale selection, missing materials at confirmation, cancellation/restart costs, completion during a pending cancel confirmation, full-category return retry and same-category replacement. It tests moved/resized panel/button bounds at1280×720,640×480 and320×240. The saved-state fixture covers literal v19 migration, malformed/aliased suffix rollback, clone/move ownership, death, full-bag persistence and refusal to replace a world containing a reserved item. Listing uses a shared advisory filter with authoritative validation at commit; a canonical-empty clock avoids scanning the whole inventory every display.

The first aggregate run exposed test-order contamination: the new asset-backed repair fixture populated the parent's immutable settlement cache, so a later synthetic missing-geometry fixture could not configure its own root. The repair fixture now runs in a child process with normal exit for leak checks, matching other installed-asset fixtures. No runtime cache reset or weaker missing-asset check was added. Final validation results follow below. The desktop and normal player saves remain untouched. No new original-game launch or graphical UI acceptance is claimed; manual visual acceptance remains outstanding.

The frontend build, full optimized `make test`, content regeneration checks and26 CLI cases pass. The aggregate reports zero C core failures and751 Python cases with51 optional skips; those skips are not validation. The dedicated UnityPy camp-repair source check passes all11 cases with zero skips. Focused repair spatial, UI, persistence and actual-host integration each pass ASan/UBSan with leak detection. Local evidence is in `rebuild/build/repair-full-check-final.log`, `repair-content-check.log`, `repair-host-sanitize.log` and the focused repair test directories; build evidence is intentionally uncommitted.

The full final core executable also passes ASan/UBSan with `ASAN_OPTIONS=detect_leaks=1`, exit0 and zero failures (`rebuild/build/repair-sanitize-run.log`). The repaired fixture isolation passes in both optimized and sanitized aggregates. Documentation links/whitespace were checked and the local structural index refreshed. Visual/manual repair acceptance remains outstanding.

## October 7 — bandages, shop quantity entry and item details

This batch follows the persistent-repair checkpoint and is intended for consolidated native review. [Restoration Bandage924](audit/SANDBOX_BANDAGE.md) restores200 HP and shares925's existing saved ten-second group9 cooldown. It uses the existing stock/script/formula acquisition paths without starter grants or a save-version change. Herbal Juice916 remains independent. A public-API fixture buys bandages, buys/learns their script, buys ingredients, completes timed crafting and exercises use, cooldown boundaries and round-trip persistence in both medicine directions. Its money and initial HP are staged; it does not establish ordinary travel or gameplay balance.

The extended [hunt-consume host fixture](../rebuild/tests/test_hunt_consume.c) now runs both925 and924 through the real UI router and extracted production host movement/action ordering, with recovered collision and posed hare contact. Each medicine allows movement and the scheduled incoming hit on its use frame; a cross-medicine cooldown retry also leaves movement and enemy clocks active without consuming stock. Both cases move.083328m and receive42.074371 damage after healing. The fixture isolates one actor from a staged six-actor encounter; it is not earned combat, survival acceptance or a visual animation check.

[Shop quantity/details](audit/SHOP_QUANTITY_AND_ITEM_DETAILS.md) adds typed Buy/Sell/Buyback quantities with separate acceptance and trade actions, exact row-state binding and cancel/focus-loss restoration. Read-only hover text shows source-scaled condition, source first-formula learned status and current price/material information. Raw wear and inventory ownership are unchanged; fresh stock uses a non-owning full-condition display copy. Focused tests cover actual transactions, overflow/empty input, stale wear/stock, disappeared final rows, held/release ownership and bounds at1280×720,640×480 and320×240. Independent review found and corrected a disappearing-final-row retargeting bug before the final passing run. An earlier test-development assertion came from an incorrect stock fixture assumption; the corrected new fixture reports ordinary exit1 failures rather than aborting.

The frontend build, full optimized core, content regeneration,26 CLI cases, and focused sandbox UI/inventory/shop/repair/discard/hunt-consume tests pass. Source guards pass23 content, four shop-detail and five camera-export cases with zero skips. Bandage, quantity/details and pure camera tests individually pass ASan/UBSan with leak detection. Local logs are under `rebuild/build/review-batch-*` and the focused test directories. These are scoped current checks, not a new run of every optional Python/Unity asset test.

The [camera geometry evaluator](audit/CAMERA_GEOMETRY.md) also passes analytical query/control-flow and rollback checks. It remains disconnected from the live host; callback tests do not qualify native overlap semantics, near-camera rendering or pointer projection. No new original-game session or user-desktop input is involved in this batch. [Review steps](../rebuild/README.md#review-batch-shop-details-bandages-and-repair) cover the visible features together with the preceding repair service.

An isolated Xvfb draw-only fixture links the real UI renderer, recovered atlas/fonts/effects and core, stages five shop/inventory/repair states, and captures15 images at1280×720,640×480 and320×240. Inspection caught false condition text on recipe scripts: their positive prototype defaults did not imply a Durability component. Generated `pe_item_has_durability` now follows the actual source class membership; new regressions exclude fake condition on877/916/924/925. Final captures show corrected labels, worn versus fresh condition, repair material quotes and a brighter quantity instruction. Panel/tooltip bounds fit;320×240 text remains too small for usability acceptance. The fixture does not draw world/actors or test live interaction. Its private Xvfb child exited cleanly without changing system configuration or touching the user's display. Captures/logs remain ignored at `rebuild/build/ui-details-captures`; the audit records the optional target and dependency provenance.

After that presentation correction, the frontend/content/26 CLI checks and focused quantity/details optimized plus ASan/UBSan/leak checks pass again. The actual hunt-consume integration also passes ASan/UBSan with leak detection for both925 and924 (`rebuild/build/review-batch-consume-sanitize.log`). Ordinary native review remains pending; these automated/rendered checks do not substitute for it.

The full core gameplay executable completes ASan/UBSan with `ASAN_OPTIONS=detect_leaks=1`, exit0 and zero failures (`rebuild/build/review-batch-core-sanitize.log`). That run began before the final additive presentation-only durability helper; the final helper and UI are covered by the fresh content, frontend, focused optimized/sanitizer and private-render checks above. No gameplay/save layout changed during that run. Documentation links/whitespace pass and the local structural index is refreshed for this checkpoint.

## October 7 — ordinary hunt, partial reload and earned return

The [ordinary acceptance audit](audit/HUNT_ORDINARY_ACCEPTANCE.md) closes the native four-hare gate using normal production-game input on private Xvfb displays. A first earned kill survives Pause Save→Load→Save with byte-identical checkpoints and identical full-inventory inspections. Continuing from that checkpoint earns the remaining three kills, removes all mission hares, and changes the objective to return. Ordinary travel then grants exactly one each of592–595 before4105 acknowledgment. Number1 advances the ending pages; a pending4106 save resumes correctly, and final acknowledgment preserves exactly one set of rewards. A subsequent normal launch displays completed Bug Hunt.

These are rendered C observations supported by input journals, actual contact logs, screenshots and hashed saves; no gameplay fields were edited and no smoke mode was used. Failed attempts, deaths and ordinary reloads remain in the evidence. The user's desktop and original Steam game were untouched. Combat balance, animation/camera quality and9077 remain separate gates. The local video is incomplete and does not cover the successful kill sequence.

The supporting inspector now covers all1680 categorized slots and reports pose/motion; two formatter regressions pass. The new guarded private-input helper passes19 pure/subprocess tests and independent review, including parent-death cleanup and input-watchdog races. A final-helper isolated launch/input/capture/stop check also passes. Session shutdown used explicit process termination after saving, not clean in-game Quit. No gameplay code or save wire format changed, so prior gameplay-suite results retain their recorded scope; this increment runs the focused tooling checks.

## October 7 — test-display collision and recovery

The user reported X11 `:0` launch failure on their Wayland desktop. Local checks confirmed that old Xvfb displayfd autodiscovery had replaced Xwayland's filesystem X0 socket, despite its live desktop lock; X0 refused connections while X0_ still completed an X11 handshake. This corrects the earlier complete-isolation claim: test input targeted the owned server, but startup damaged the desktop connection path. [The incident audit](audit/PRIVATE_DISPLAY_COLLISION.md) records recovery without restarting Hyprland/Xwayland, the shared explicit high-display fix and its24 passing tests. A hidden raylib probe now initializes the NVIDIA OpenGL renderer on desktop Xwayland and exits0. A separate corrected-launcher probe on`:100` passes without changing the desktop sockets/lock or breaking XOpenDisplay. No gameplay code, save state, system package or desktop configuration changed.


## October 7: shared tutorial/sandbox UI checkpoint

Implemented in C: the two hosts now call one renderer for the normal HUD, minimap chrome and bounded quest tracker, with shared font/wrapping, inventory/dialogue frames and menu/health/mission shortcuts. Gameplay data and specialized panel contents remain mode-owned. [Ownership and durable rules](audit/SHARED_UI.md) include the paired manual checklist; root AGENTS and PROJECT_MAP point to those owners.

Automated evidence: production build passes; shared HUD draw-call checks, seven structural guards, sandbox routing checks, three production-extracted tutorial input checks, six text checks and four UI-asset checks pass with zero skips in these focused suites. Shared/input routing sanitizer coverage is recorded in the audit. The actual two UI draw entrypoints rendered on private display `:101` at 1280×720, 640×480 and 320×240 with identical staged HUD values. All 122,426 compared HUD pixels match (93,170 + 23,320 + 5,936); fifteen captures cover paired HUD/inventory and bounded long titles. See [tutorial](reference/shared-ui/tutorial-hud-1280x720.png), [sandbox](reference/shared-ui/sandbox-hud-1280x720.png) and [tracker](reference/shared-ui/shared-long-tracker-640x480.png). Visual inspection confirms the title/objective clearance. The smallest fixture is a bounds check, not a claim of comfortable readability.

The supervisor used the corrected high-display selection. Desktop socket/lock identities and mtimes were unchanged, and the existing Xwayland connection remained available afterward. No desktop input, original game or user save was touched. These are staged rendering and automated routing results, not a completed ordinary manual playtest. User review of both modes at equal sizes, dragging/closing panels and held-button release is next. Camera-only geometry/projection integration remains the next gameplay implementation target.


## October7: conversation history and camera alignment

[Conversation history](audit/CONVERSATION_HISTORY.md) now shares one model/view between modes, with latest20 content-distinct messages, selected-response capture, scrolling/dragging, Clear and PECSv21 sandbox persistence. Tutorial history is transient. [Camera alignment](audit/CAMERA_ALIGNMENT.md) connects source GeometryClamp to a camera-only backend and gives rendering/picking one immutable display pose. Its source anchor, collider-cooking and close-rendering differences remain explicit native adapters.

Production build, full optimized core/aggregate suite, content checks and26 CLI tests pass. Aggregate Python tests retain51 optional dependency/asset skips; new focused tests and installed-UnityPy content/query checks have zero skips. Focused ASan/UBSan with leak detection passes for history plus17 historical-save suites, history rendering/input, camera host/projection/query. Existing collision regressions also pass optimized checks, as recorded in the audits. This is not a new full-core sanitizer claim.

Paired real UI fixtures on private`:102` produce identical HUD and top/bottom history pixels at1280×720,640×480 and320×240 (27 captures). An ordinary production session on private`:103`, using an unchanged copied pending-return save, records Naoko's visible final reply, saves one exact history row, clears it and restores it through F10 reload. [Restored log](reference/conversation-camera/restored-after-clear.png). All three camera modes render, and ordinary movement into reach followed by a short RMB click on Tammana's off-center body opens [her menu](reference/conversation-camera/vendor-open.png). Source code and CPU matrices separately establish projection/reach behavior; these captures do not prove exhaustive wall/terrain/tool fidelity.

The sandbox returns to Main Menu with result2; the owned supervisor then intentionally terminates that menu process. Original checkpoint/baseline hashes and desktop socket/lock state remain unchanged, with desktop XOpenDisplay available afterward. No Steam session or desktop input was used. User review should now focus on small-window UI/history handling and camera orbit/wall/cursor behavior in all F5 modes; original auxiliary-camera composition and animated anchors remain unported.

## October7: user review of shared UI, history and camera checkpoint

After checkpoint `f70c129d`, the user reported “looks good” in response to the shared UI, conversation history and camera review batch. Record this as overall user visual acceptance of that batch; the response did not identify individual camera obstruction cases, window sizes or history operations, so it is not an exhaustive per-scenario test record.

## October7: Adventure attack direction, target acquisition and reaction extraction

[The new facing increment](audit/MELEE_TARGET_FACING.md) connects source default movement/body-forward input, qualified hare query geometry and ordered posed-defence selection to the existing Knife33 host. Body, blade, root movement and incoming defence share the resulting heading; camera yaw remains independent. Stable cohort enumeration, native capsule broadphase, heading-only aiming and fixed-tick/combo scheduling remain explicit adapters. [Hit-reaction decisions](audit/HIT_REACTION.md) are separately extracted/tested and disconnected. Shield presentation/activation and numeric damage balance are unchanged.

Production build and full optimized core pass (`rebuild/build/combat-alignment/core-final.log`:0 failures). Focused actual equipment/motion host, sword policy, actor, consumable/AI and blade-facing integration checks pass, with real installed assets where required. A compile failure in the extracted consumable fixture was fixed by including the newly introduced production facing state; its final run confirms uninterrupted movement, incoming contacts and exactly one active-knife clock advance during healing. A prior combined build/core command was terminated with143 and had the pre-fix relative asset-path test; the separately rebuilt current full core run exits0. Neither failed attempt is a pass or a new runtime crash report.

Focused ASan/UBSan runs with `detect_leaks=1:halt_on_error=1` pass for the actual blade-facing host, actor snapshot/attachment dispatch, pure facing arithmetic, real hare target bridge and reaction decisions. This is not a new full-core sanitizer claim. Source/export mutation suites pass24 attack/facing,13 hunt-targeting and19 reaction tests with zero skips under UnityPy. CLI checks pass26 tests. Content regeneration results are recorded separately below.

A normal production session on private`:104` used a byte-for-byte copy of the preserved earned one-kill checkpoint (`222bd1ad…0057`); no staged geometry, reduced stats, smoke gameplay, original-game launch or desktop input was used. [The manifest](reference/melee-facing/check.json), [input journal](reference/melee-facing/inputs.jsonl), [runtime trace](reference/melee-facing/native.log) and [free-facing view](reference/melee-facing/free-facing.png) preserve the evidence. Initial no-target swings retained body heading while camera yaw stayed−1.801262rad; subsequent movement changed attack heading independently. Frame5785 acquired living hare2146483653 and set body yaw−86.298637degrees without changing camera yaw. Continued swings produced actual contacts; the final encounter counters report28 outgoing/10 incoming, and the overall host reports32 swing stages and one moving ordinary start. Counters after reload are not aggregate contact totals across the whole session. No running start or newly earned kill is claimed.

The player died during the check. Ordinary revive/heal/load input was used, and an unintended click on the still-open Pause panel saved only the isolated progress copy. Early loading/menu-owned input is not evidence of successful attacks. This session verifies native selection/facing and contact integration, not balanced difficulty, every combo direction, original overlap order or final animation quality. The host returned to the main menu after6647 frames with result2, then the private supervisor deliberately stopped it with SIGTERM; no clean process-exit0 claim is made. Source/baseline hashes and desktop socket inode/mtime identities remained unchanged; a desktop X connection still succeeded afterward.

The current `make -C rebuild check-content` also exits0 with no skips, including the new targeting/reaction guards. Build outputs and native saves remain ignored; only bounded logs/screenshots and provenance are committed.

## October7: source-derived hare loot and PECSv22

[Hare loot](audit/HUNT_LOOT.md) now uses the active Unity automatic-drop path rather than the commented-out corpse component. The C owner rolls source loot11 on actual contact deaths and preserves independent world drops through fourth-kill cleanup, return, saves and reloads. Pickup requires a living player within strict4m and transfers each complete stack independently; full categories retain blocked stacks. Game-time expiry, unique world-item ownership and literal v21 migration are covered. Native RNG, fixed death-root pickup/marker placement, immediate collection presentation and display scheduling are explicit adapters; this is not original visual or difficulty acceptance.

Source qualification ran entirely offline. The loot exporter/prefab suite passes11 tests with zero skips under UnityPy. Content closure adds Soft Fur1228, bringing the bounded catalog to342 items without adding recipes;24 content,11 trade-price and11 repair-export checks pass. All seven source icons already existed in the installed UI atlas. The independent source/content check passes; no original assets, Steam archives or user saves changed.

Focused ASan/UBSan with leak detection passes for the loot runtime, save corruption/migration/lifecycle cases and actual marker host path. Marker checks use the real reflected projection and collision snapshots, with only drawing/display dimensions recorded: strict10m visibility, grouping, viewport rejection, tree/box occlusion and readonly game/UI/camera state pass. The real shared icon renderer also ran on an owned private high display at [1280×720](reference/hunt-loot/loot-1280x720.png) and [640×480](reference/hunt-loot/loot-640x480.png); both captures were inspected. These are deliberately staged clusters over a grid, not an ordinary earned hunt. No Steam session or desktop input was used.

The first aggregate core attempt exposed two test-fixture issues: a stale341-item assertion and the new save test initializing the process-immutable settlement asset cache before an older custom-geometry test. The count/closure expectation was updated, and the route save test now runs in a child process when linked into core. Its standalone sanitizer main runs directly for teardown/leak coverage. Historical v1–v21 tests retain their intended envelopes while subtracting the new suffix. The existing world-depth corruption fixture now targets its actual intended depth field rather than an unrelated later suffix. Neither initial failure was a runtime crash or a pass.

The optimized real-contact wrapper test passes using installed Knife33 poses: three actual deaths produce three independent rolls, including a batch begun at three quest credits that completes the hunt. A separate staged Sword1 geometric fixture produces six simultaneous deaths/six rolls; this does not enable Sword/Shield gameplay. Simultaneous incoming damage/outfit/shield wear and outgoing sword damage/wear/death roll back together on loot failure, then commit on retry. Capacity, RNG and identity failures preserve game/combat/melee/output state; replaying a completed encounter creates nothing. The initial Knife fixture incorrectly expected the wider Sword overlap and was corrected to compare actual fatal Knife contacts with exact drop groups; no production contact or damage behavior changed.

The final production build and full optimized core pass with zero failures (`hunt-loot-build-final.log`, `hunt-loot-core-final.log`). Shared UI/sandbox UI, actual equipment-host, save-inspector and26 CLI checks pass. The consumable host extractor was updated to require the new contact wrapper; real-asset checks pass for both925/924 movement and incoming AI contacts, cooldown retries, exactly one active blade-clock update and the modal negative control. The fourth-kill return message now retains priority over an immediate pickup notice. This is a composed focused verification plus full optimized core, not a new full aggregate Python-suite or full-core sanitizer claim. The installed binary SHA256 is `b9cf8f82287d1cb3d8e7ee13d98c4eedb56eda77871c042859bdfd88a6fec5c6`; restart older running processes to use it.

The final real-contact wrapper ASan/UBSan run also exits0 with `detect_leaks=1:halt_on_error=1` (`hunt-loot-sanitizer-combat-final.log`), covering the installed Knife and staged Sword cases described above. Runtime/save standalone sanitizer mains were rebuilt and rerun after fixture isolation, both passing with leak detection. Together with the marker sanitizer, these qualify the new owners and contact transaction; they do not establish full-game sanitizer or new ordinary-hunt visual acceptance.

## October7: requested debug controls and hairline transparency

Implemented in C: Adventure's existing in-game Settings now exposes a default-OFF, session-only debug toggle. The three explicit commands top up five Herbal Drinks, grant/heal a bounded Knife/medicine kit, or skip prerequisites and settle at the ordinary hare encounter. Hunt travel retains partial kill credit, refuses completed hunts, exits construction and selects free third person. It keeps Pause/Settings open and never writes a save. The medicine button leaves the normal Naoko turn-in and user-selected final4100 timing intact. Sword/Shield remain gated. See [debug contract](audit/DEBUG_TOOLS.md).

Debug transactions passed optimized tests and focused ASan/UBSan with leak detection: four save/reload cases, identity/wear/cooldown preservation, full-bag/counter/broken-Knife/blocked-world rollback, reserved repair, pending loot and build-history ownership. The actual host dispatcher tests passed focus/Pause/settings gating, single-use requests, asset preflight rejection and construction/camera reset across all three modes. This is staged testing, not earned progression or a fresh physical hunt playthrough.

Settings tests and focused ASan/UBSan with leak detection passed. Private captures show the real shared Settings renderer at1280×720,800×450 and640×480, including debug OFF/ON. Long notices use shared text wrapping and remain complete above Back at small sizes. Tutorial/main-menu settings retain the ordinary controls. Evidence: `rebuild/build/debug-settings-wrapped.log`, `debug-settings-sanitize.log`, `settings-captures/` and `debug-host-controls-agent.log`.

The user's screenshot reported transparent hairline holes. Source inspection identified cutout hair writing depth under the legacy all-alpha renderer, plus opaque eye textures whose alpha is not transparency. Source-qualified material sidecars and the shared actor renderer now apply the active shader cutoff before depth writes and restore source double-sided culling. Both hosts receive the correction; no actor meshes, animation clips or PNGs were changed. [Actor material audit](audit/ACTOR_MATERIALS.md) records source identities, installation backups and limitations.

Six exporter unit tests (including renderer ancestry and shared-material cache isolation) and focused manifest/parser ASan/UBSan with leak detection passed. A private GPU fixture reproduces the former depth holes, verifies opaque/cutout thresholds and culling-state restoration, and captures actual NPC20009, player and tutorial9004 portraits at three angles with legacy/corrected policies. Inspected before/after images remove the hairline/background holes and blue-through eyes. These are controlled renderer checks, not a complete original-lighting or every-character visual acceptance claim. The legacy CPU blend-boundary regression also passes after extending its API mocks.

Final integration checks: optimized executable build and full core regression pass (`debug-material-integration.log`, `debug-material-core.log`), shared HUD/sandbox UI tests pass, and all26 CLI checks pass. Paired real tutorial/sandbox UI captures match HUD/history pixels at1280×720,640×480 and320×240; normal/small HUD images were inspected. Evidence: `debug-material-finalchecks.log`, `ui-shared-captures/`. Focused sanitizer results above are not a claim of a newly run full aggregate sanitizer suite. No original game was launched and no desktop input was taken for this increment.

Final source hierarchy qualification installed55 sidecars with backups in `rebuild/build/actor-materials-20261007-v3/`. Every sidecar is byte-identical to the GPU-tested v2 policy; only source evidence became stricter. Enabled shader replacement must be on an ancestor of the exact recorded renderer, with matching material reference. Six final exporter tests pass.
