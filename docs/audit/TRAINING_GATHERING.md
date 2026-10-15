# Tutorial gathering, crafting and mining after combat

This audits `746 → 719 → 748 → 757 → 721 → 751 → 752`. The native training frontend now completes the real-contact combat handshake and all five herb proximity/timer actions through748, validated in a6000-frame graphical run with one safety axe. The tree/script/crafting bridge has headless coverage through751. The authored mining mound, real marching-cubes collision and staged shovel edits now also pass headless tests through752; the12000-frame graphical mining route subsequently passed through752 after five real shovel hit events and three actual removals, with9006 dirt. Entry into719 still requires an actual completion of kill objective1025 and acknowledgement of dialogue300230, as described in [training enemy](TRAINING_ENEMY.md); no fake death or bypass is added.

## Sources and reproducible fixture

The principal code is under `Assets/Scripts/Jaluca/TrainingField/`: `TrainingTaskManager.cs`, `TrainingRoomConfig.cs`, `HoloherbTask.cs`, `HoloherbGather.cs`, `HoloherbAppearance.cs`, `HolotreeTask.cs`, `HolotreeCutter.cs`, and `TerrainDigTask.cs`. Mission, item, recipe and dialogue data come from `Assets/Resources/DataBase/localData.bytes`.

| Input | SHA-256 |
| --- | --- |
| `Resources/TrainingManager_New.prefab` within that source directory | `7a20d53f94a4f2531bd75e705c3dea9d0523e601d43749250e62134c7eb3369e` |
| `TerrainDigTask.cs` | `084a9d48b3b76070b01db28ff87606a4b69ce8ebeecbd7c56d7643b1bad3b9d0` |
| Repository localData | `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376` |

[export_training_gathering.py](../../rebuild/tools/export_training_gathering.py) writes ignored `rebuild/build/reference-assets/training-gathering.json`. It exports nine source task/gather/cutter components, composed transforms, available colliders, original mission/target/plot/item/recipe rows, 23 English dialogue entries, and all 726 mound samples. This is an audit fixture, not a render-asset exporter or runtime mission interpreter.

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_training_gathering.py \
  --installed-game '/home/admin/.local/share/Steam/steamapps/common/Planet Explorers'
python3 rebuild/tests/test_training_gathering_export.py
```

UnityPy 1.25.4 is used in the isolated asset environment. Omit `--installed-game` for source-only export. `--output` selects a separate JSON destination. Data is fully built before replacing the output file. The mound reader accepts only the bounded literal/loop/SafeWrite grammar in the source method and rejects unknown statements, duplicate writes and oversized loops; it does not execute C#.

Installed resource lookup resolves `mainData`'s ResourceManager instead of reading every object in the large `resources.assets`. The installed `trainingmanager_new` root is GameObject **120048**. Its custom MonoBehaviour fields require the source custom-field schemas combined with the player's actual base header; strict full-byte decoding succeeded for the inspected components. Installed `database/localdata` TextAsset **9273** is **7,776,256 bytes and byte-identical** to the repository database. The 23 selected English lines also match installed `i18n.db`. All nine inspected component positions and the herb/tree counters match the source. Component IDs differ between builds, as recorded below. These checks establish selected data equality; the installed managed implementations were not decompiled and their runtime behavior is not proven identical.

## Mission contract and inventory accounting

All six missions below have zero `Get_DeleteItem`, completion rewards and `CoRemoveItem`. Grants are mission-acceptance grants, using the existing once-per-mission grant mechanism, not rewards to repeat every time a window opens.

| Mission | Explicit objectives | Acceptance grant | Opening / plot dialogue | Completion successor |
| --- | --- | --- | --- | --- |
| 719 | Placeholder area 4066; actual herb-depletion callback | None | 4072,300231 / plot432→300232 | 748 |
| 748 | Placeholder equip/tool target4122 and loot2093: wood279×1 | Safety axe1527×1 | 300233 / plot433→300234 | 757 |
| 757 | Use-item5023: script406×1, `AllowOld=0` | Wood-shovel script406×1 | 300235,300236 / plot434→300237,300238 | 721 |
| 721 | Loot2089: wood shovel12×1 | Wood279×20 | 300239 / plot435→300240; replicator first-open→300241 | 751 |
| 751 | Placeholder equip/tool target4123 and loot2032: dirt268×5 | **Another wood shovel12×1** | 4073,300242 / plot436→300243; dig-tool equip→4078 | 752 |
| 752 | Placeholder4105; actual build-mode entry callback | Dirt268×9000 | 4074,300244 / plot437→300245 | 300246,4075,4076, then753 |

Area targets 4066, 4122 and 4123 point to `(10000,0,0)`, radius1; these are event placeholders, not destinations. `HolotreeCutter` also directly completes 748 when its own HP reaches zero, after granting the wood. Treating every placeholder as a physical objective would stall these lessons. Conversely, merely equipping an axe or opening the replicator is not enough to fulfill the resource/item conditions.

With no unrelated gathering, spending or failed insertion, the intended item path is: tree produces one wood; acceptance of721 adds20, giving21; recipe1 consumes10, leaving11 and producing one shovel; acceptance of751 adds another shovel, giving **two**. Do not silently delete that second grant to simplify the sequence. The existing loot objective is possession-based; a crafting-only trigger would be narrower than the database condition. Mission721 also retains `Npc=9041`, `AfterLimit=2:737` and player-talk3089; the fixture preserves these fields rather than inventing additional eligibility semantics.

## 719: five simulated herbs

`TrainingRoomLoader` instantiates `TrainingManager_New` without replacing its authored transform. The root is `(12,1.5,12)`, scale1, yaw **−90°** (quaternion approximately `(0,.707106829,0,−.707106829)`). A simple sum of child positions gives the wrong placements.

`HoloherbTask` source component11405434 / installed340633 has `herbCount=5`, and five gather/appearance references. These are simulated scene props, with no item prototype or monster-spawn ID. The gather transforms in the same order as its serialized array are:

| Herb | Source gather component | Installed component | Composed Unity world position |
| --- | ---: | ---: | --- |
| offset1 | 11438508 | 348652 | `(23.277050,1.882331,5.659585)` |
| offset2 | 11405274 | 340465 | `(15.256583,1.882331,10.406357)` |
| offset3 | 11434286 | 347933 | `(9.546638,1.882331,16.267000)` |
| offset4 | 11409872 | 343799 | `(8.878967,1.882331,4.358108)` |
| offset5 | 11412708 | 344318 | `(21.384402,1.882331,16.221410)` |

The source gather script requires a GatherHerb input click, `!isDead`, `!isGathering`, and **3D squared distance <7** (distance strictly below √7≈2.646m). It stops Move, Sprint and Rotate, enables the Gather animation/mask, and advances its own clock. `HerbHitTime=1.1s` depletes the prop and clears the animation/mask; `HerbGatherTime=1.966s` ends the local gathering state. Movement velocity gating is commented out. The script itself has no aim-ray, facing or line-of-sight test even though the instructional text mentions a bag cursor. A replication should distinguish the explicit proximity condition from general world-plant targeting rather than inventing a stricter test without runtime evidence.

Depletion sets `isDead`, requests appearance fade, destroys the referenced `col` **GameObject**, and increments the task's count. Exactly the fifth depletion invokes mission completion. **There is no inventory item grant in this hologram gather path.** Adding generic herbs or Herbal Juice would change the lesson's accounting. Appearance fadeTime is .3s; the task closes the herb parent after fadeTime+.2s. Appearance objects billboard toward the camera and use a hologram render texture; their `(10,5,1)` local scales describe that visual plane, not a resource interaction radius.

Implementation guardrails: each herb should deplete once; the completed/hidden group should not accept more interactions; timer progression must not skip the hit if a large delta crosses both1.1 and1.966s. The last point is a robustness repair to the source's conditional timing, not an assertion of original behavior under a long frame.

## 748: seven-hit hologram tree

The tree task is source11473516 / installed354599. Its root is `(10.920831,1.582331,11.979339)`, retaining the parent's −90° yaw. `HolotreeTask.MaxHP=7` and cutter source11496244 / installed358447 **also has MaxHP7**. The cutter's billboard transform is `(11.413041,5.117926,11.748326)`, local scale20; do not use that visual plane origin as the trunk interaction target.

The root's CapsuleCollider13687620 has local center `(-.1,3,-.2)`, Y axis, radius1, height7, nontrigger, initially disabled. In world space its center is approximately `(11.120831,4.582331,11.879339)`; sphere-center endpoints lie at y2.082331 and7.082331. Mission initialization enables this collider and sets the task HP to7. The cutter initializes its separate HP in Start; source reinitialization does not clearly reset that second counter, so repeat-lesson support needs an explicit policy.

The cutter starts only with a nonnull `Action_Fell.m_Axe`, the EquipmentHold mask and held Cut input. It casts **all hits along the player's forward direction, from player position, for2m**, and accepts the collider on `transform.parent.parent` (the HoloTree root). This is not a camera ray, and `RaycastAll` does not enforce nearest-obstacle occlusion. It stops movement/sprint/rotation and waits for the animator's layer1 `DoNothing` condition before beginning its timed stages.

Timing constants are .6s entry, 1.1s repeating cut stage with a hit at .6s into that stage, then .533s exit. Each hit decrements **one** cutter HP and one task HP. The seventh cutter hit adds **wood279×1**, directly calls the CUT mission completion dispatcher and disables the root collider. The safety axe's general database `CutDamage=28` does not change this scripted seven-hit counter. The source does not check the return value of package `Add` before completing; a native atomic inventory-capacity policy should be documented as a correction, not concealed as exact callback parity.

The held input can be released; it clears the Fell animation boolean, and the state machine transitions out at a cut-loop boundary. These stages should not become seven instant key clicks or a generic terrain-mining swing timer. Axe particles, sound, stamina/durability effects and hologram rendering require their own supported paths; this cutter code does not itself subtract axe durability or stamina.

## 757 and721: learn the script, then replicate

Prototype406 is a Formula item, `learn=1`, stack size1. `ItemData.cs` turns `learn` into `replicatorFormulaIds`; `UseItemCmpt.LearnReplicatorFormula` invokes `Replicator.AddFormula(1)`. A successful newly learned formula causes one script item to be destroyed; an already-known formula returns false with the known-script tip and is not consumed by that branch. The successful-use event is distinct from simply opening inventory or selecting the icon. Preserve the inventory and formula-set relationship when implementing target5023.

`synthesis_skill` recipe1 produces wood shovel12×1, uses wood279×10, takes **3s**, and specifies `workspace=0`, `complex=1`, `formulaIds=1`. Those are content values; they do not justify an extra workbench requirement. The general replicator checks material counts and removes ingredients before running the work; completion inserts the product. A future implementation should reserve inventory capacity and define cancellation behavior rather than duplicating output or losing materials on a failed insertion.

Mission721 subscribes to `UICompoundWndControl.OnShow`; opening the replicator displays dialogue300241 and unsubscribes that prompt listener. It does not itself create a shovel or complete the objective. Completion hides the replicator. `Y` is the source toggle; the generated-world C sandbox's crafting shortcut and initial learned recipes are separate behavior.

## 751: absolute voxel mound and dirt objective

`TerrainDigTask.InitScene` immediately runs `CreatTerrian()` and subscribes to the equipment change event. A newly added equipment item whose editor type descends from5 queues4078 and unsubscribes. This accepts a digging-tool category, not only exact item12. The task's scene Transform is `(12,1.5,12)`, but **every SafeWrite coordinate in CreatTerrian is absolute voxel space**; applying that transform or its yaw would misplace the mound.

All samples have raw material1. The exact726 sparse writes are in the fixture, with columns `[x,y,z,density,material]`:

| Layer | Source samples | Density |
| --- | ---: | --- |
| y0, x−2..15, z2..21 | 360 | 255 |
| y1, x−1..14, z3..20 | 288 | 210 |
| y2, x4..8,z8..15 plus irregular perimeter | 59 total | 190 center; perimeter110/130/150/170 |
| y3, x5..6,z10..12 plus irregular perimeter | 19 total | 190 center; perimeter110/150/170 |

These writes overlay existing terrain; they do not first clear a rectangular volume. `DestroyTerrian()` clears the corresponding authored samples on mission exit; it does not restore their previous contents. The C lesson now stores these726 samples in an otherwise empty bounded volume with origin(−8,−4,−8), preserving negative source x coordinates and raw material1. Its marching-cubes triangles provide both the rendered mesh and a copied BVH collision snapshot; movement selects the earliest room, active tree or terrain contact. Edits refresh the snapshot before the following physics step. The original room floor remains a separate collider. See [authored training terrain](TRAINING_TERRAIN.md) for coordinate, snapshot and cleanup contracts.

Resource row1 has durability .21167, production `268,100;;`, fixed_num0 and self_num2. The general `DigTerrainManager.GetResouce` counts removed samples by material, calculates `(bonus+self_num)×sample_count` when fixed_num is zero, rounds any fractional remainder stochastically, and draws from the production probabilities. With zero bonus, a removed material1 sample therefore yields **two dirt268**, not one. Target2032 requires possession of at least five dirt; three such removals could produce six, but tool footprint and timing decide how many samples each dig removes. Do not equate the five-item target with exactly five swings or five voxels.

The source density damage path clears a sample at density≤127 and adds it to the removal list; air density0 is an early no-op. Reach/snapping must retain the audited dig-indicator behavior in [terrain parity](TERRAIN_PARITY.md). `PEDigTool` declares stamina cost5, but serialized tool values, buff-derived digging attributes, animation events and the skill invoked by the equipped prefab must be preserved before claiming exact swing timing or damage. The fixture retains complete shovel/item/resource rows to support that next step.

Completing751 clears the authored mound; entering752 grants9000 dirt and asks for build mode. The next753 objective is the receiver-ray occlusion puzzle, not a generic placed-block counter; its rendering and collision are beyond this gathering audit.

## Dialogue and validation fixtures

The fixture follows `Scenario.talk_id → talk_content localization ID → Translation.English`, retaining speaker IDs and exact text. Selected instructional sequence:

| Dialogue | Instruction |
| --- | --- |
| 300231 /300232 | Gather the simulated plants; approach, right-click or use E when the bag icon appears |
| 300233 /300234 | Safety-axe lesson; equip/draw, approach and hold left mouse |
| 300235 /300236 | Introduce replication and give the wooden-shovel script |
| 300237 /300238 | Open inventory I; right-click the script in Items to install it |
| 300239 /300240 /300241 | Need materials; open replicator Y; choose Wooden Shovel and click the hammer |
| 300242 /300243 /4078 | Mining introduction; equip shovel; draw, aim the dig box and hold left mouse |
| 300244 /300245 | Building introduction and B to enter build mode |

These are descriptions of the preserved lines, not substitute dialogue. Mission rows and plots preserve their full ordering, including short acknowledgements4072–4076. No text getter or training C file was modified.

The new standard-library test suite passes two tests covering all726 source coordinates, representative interior/boundary densities, exact per-layer counts, and rejection of unknown statements, duplicates and oversized loops. Source export completed, and the installed comparison recovered the matching positions/counters, identical database and equal selected English lines without launching graphics. Further meaningful runtime fixtures should exercise strict herb reach, one-time depletion, the seventh tree hit and one-wood grant, script consumption only after learning, timed recipe material conservation, the second shovel grant, material1 dirt yield, and mound removal on exit. Each mission gate must stay closed until its actual event or item condition occurs; this audit adds no fake kill or resource progress.


## Implemented headless lesson module

[pe_training_gathering.h](../../rebuild/src/pe_training_gathering.h) and [pe_training_gathering.c](../../rebuild/src/pe_training_gathering.c) implement a separate `PeTrainingGathering` state. The reusable module itself owns no renderer or combat dispatcher. `PeTraining` now embeds it as the sole resource ledger and supplies explicit mission, input and dialogue events; it does not duplicate those item counts in a second inventory. The host must explicitly enter719 after the real combat objective and dialogue finish. Subsequent `enter` calls accept only the documented successor after the current objective is complete. A duplicate or out-of-order entry fails without mutation; completion alone does not accept another mission. `next` reports the eligible successor, including753 after the build-mode event, but753 itself is outside this module.

The state owns the bounded **lesson inventory ledger** for axe1527, script406, wood279, shovel12 and dirt268, together with formula1 knowledge. It is still separate from the sandbox’s generic stack/slot inventory. The training host queries this ledger directly and keeps axe equipment/held state separately; copying its grants into another inventory would duplicate them. Counts are bounded to1,000,000 per item for overflow protection. Source mission grants apply atomically once at entry, and external acquisitions require an explicit host-confirmed call. Mission-grant and external-acquisition capacity failures leave state unchanged; a capacity-blocked tree hit consumes only that loop’s hit opportunity, without spending HP or granting wood, so release can still exit normally. The ledger does not spend ammunition, durability or stamina.

The implemented contracts are:

- `begin_herb(index,playerPosition)` uses the five recovered world positions and strict squared distance below7. Each herb has its own1.1s hit and1.966s activity clock. Exactly one depletion occurs per herb, with no inventory yield; the fifth marks719 complete. The movement-lock query clears each herb's lock at depletion, matching the source's mask release before the local activity timer ends.
- `step(dt,input)` accepts finite time in[0,60] seconds. For the tree, the host must report real held Cut input, an actual axe, EquipmentHold, a player-forward2m ray containing the correct tree collider, and animator layer1 DoNothing readiness. These are **inputs to the module, not physics or Animator implementations**. The pending start is latched as in the source. Its readiness frame starts at time0; entry requires strictly more than.6s, repeating cut cycles last1.1s with one hit at.6s, and exit lasts.533s. Releasing Cut latches the source Fell=false state through ENTRY/CUT and finishes the current loop; pressing again before exit does not undo cancellation. While still waiting for DoNothing, the source !cutting branch can re-arm after a valid held-input/ray check. Losing the axe at a hit aborts. Seven accepted hits produce exactly one wood and complete748. A full numeric wood count blocks damage/reward on the seventh hit while allowing its loop timer to continue. Held input retries at a later loop; released input still reaches exit and unlocks movement. This is an explicit conservation correction to the source's unchecked Add.
- `learn_script()` succeeds only during757 with a script and previously unknown formula1. It learns, consumes one406 and completes the use objective atomically. An already-known formula rejects use and retains the script, matching the source behavior; repeat-lesson recovery is not invented.
- `start_craft()` requires721, learned formula1, ten wood, no existing craft and room for one shovel. It immediately debits ten wood and reserves the output count. Three seconds later it adds one shovel and updates the possession objective. External acquisitions cannot consume the reserved numeric capacity. If an external shovel completes the objective while crafting, successor entry waits for the outstanding craft, preventing silent loss of paid materials. No cancellation policy is implemented. Accepting751 then grants the source's second shovel.
- `removed_voxel(before,after,x,y,z)` reads **immutable PeWorld snapshots**. It requires source-material mode1, matching seed/origin/source, a changed revision, in-bounds local coordinates, a nonair material1 sample before the edit and air afterward. Each accepted coordinate produces two dirt and is recorded against replay. Three zero-bonus removals can reach six dirt; once the five-item possession target is complete, further lesson receipts are rejected. Wrong-material, unchanged, partial-density-only, out-of-bounds, wrong-world and repeated-coordinate receipts cannot award dirt. The host remains responsible for supplying authentic committed snapshots and valid tool permissions: this module cannot cryptographically prove a caller's data. It never edits the world, grants on a click, or treats a density reduction as a removal.
- Accepting752 grants9000 dirt once. Only an explicit actual build-mode-entry event completes that lesson; placing arbitrary blocks is not substituted for later753's receiver puzzle.

Timer stepping processes all crossed event boundaries. This intentionally avoids the original scripts' dropped hits under long frames and their frame-dependent `if/else` transition delays; it reproduces authored event times and normal action sequencing, not every timing artifact of the Unity Update implementation. Herbal fade/render state, tree particles/sound, animation playback, genuine forward-ray queries and the room/voxel collision composition remain host responsibilities. The standalone ledger does not create or clear the mound. The implemented training host uses `pe_training_terrain` for the actual751 entry/exit edits and `pe_training_dig` for authored swing events, density damage and once-only removal receipts; the ledger remains the only owner of the dirt count. Pose capture and combat activation gaps remain unchanged.

## Native module verification

[test_training_gathering.c](../../rebuild/tests/test_training_gathering.c) covers719→752 source thresholds, action/reward retry, held-input cancellation, exact instance/grant conservation and real dirt removal. Expected wood1→21→11, shovel0→1→2, dirt0→2→4→6→9006. This headless entry does not manufacture the preceding746 kill.

[PLAYTEST](../PLAYTEST.md#tutorial-native-graphical-fixtures) owns actual herb/tree/craft/mining progression. [Tree assets](TRAINING_TREE_ASSETS.md), [mining](TRAINING_MINING.md) and [terrain](TRAINING_TERRAIN.md) own the complete source clock/geometry contracts. The earlier herb-only/frontend-cutting-pending checkpoint is obsolete.

## Authored mound and digging integration status

The integrated core suite passes with the exact726-sample mound, negative-coordinate origin conversion, source marching-cubes ray/BVH agreement, copied collision lifetime, malformed-mesh rejection and an empty collision snapshot after cleanup. `pe_training_dig` stages each actual material1 byte-density reduction and its removal receipt before committing the world and resource ledger. Its tests reach six gathered dirt and the one-time9000 grant, retain the two separate shovel durability values, and reject invalid/replayed progress. [Mining action audit](TRAINING_MINING.md) records authored events and controller approximations; [terrain implementation](TRAINING_TERRAIN.md) records generated data and geometry tests.

The frontend now rebuilds the rendered mesh and collision from the same revision before movement and places the dig indicator using the same crop origin as edits. The12000-frame graphical mining route passed through752 with five actual shovel hit events, three real sample removals and9006 dirt. These lesson features do not establish original world streaming, full shaders or archive compatibility. The later753 puzzle is separately implemented in [training building](TRAINING_BUILDING.md).
