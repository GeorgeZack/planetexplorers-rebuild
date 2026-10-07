# Hare pounce: offline extraction and isolated lifecycle

October 7,2026, US/Central. This completes the bounded extraction/state-owner recommendation in [the combat follow-up audit](SOURCE_AUDIT_COMBAT_FOLLOWUP.md). The production hunt still selects its existing five ordinary attacks. This work does not enable pounce, increase aggression, integrate root movement or certify a new visible combat behavior. No Unity/game launch, graphics context or desktop input was used.

## Source contract

[export_hare_pounce.py](../../rebuild/tools/export_hare_pounce.py) guards the active, non-`ATTACK_OLD` `BTPounce` class, exact XML attributes, source controller/prefab, Linux epsilon and ordinary Animator trigger implementation. Skill30100053 is linked to external trigger10250 and its `.967` main/cooldown durations, zero preparation effect, and `False` loop condition. [Generated evidence](../../rebuild/generated/pe_hare_pounce_source.json) retains source hashes and full database rows. The skill's preparation effect does **not** select the animation; `BTPounce.SetBool("attack5",true)` does. The older five melee skills retain their existing nonzero-effect validation.

| Contract | Source value |
| --- | --- |
|Behavior weight/cooldown|`.5`; `Time.time-lastCD > 5`, skill runnable separately|
|Ready geometry|`SqrDistanceLogic` in inclusive`[4,25]`, source scope±15degrees, unobstructed|
|Movement interval|Inclusive`[.267,.6]` after skill start|
|First tick after`.6`|Clear attack bool/directions, request ordinary Run pursuit, return Success immediately|
|Nominal stop time|`.967`, used only by the already-stopped tail; not a compulsory dash duration|
|Skill|30100053, main`.967`, trigger10250, independent of movement action completion|
|Animator state|`Base Layer.Attack.attack5`, hash336979320, direct speed1, nonloop|
|Clip|`attack5_pouncefront`, path1506232535475238862;31 dense frames/30Hz; duration0.9666671752929688|
|Attack window|Both forearms, inclusive normalized`[0.574999988079071,0.8019999861717224]`|
|Separate effects|850 at normalized`.3199999928474426`;891 at`.6600000262260437`|

`SqrDistanceLogic` is the source bounds-gap query, not center-to-center distance. The isolated owner accepts its actual result and the actual `PEUtil.IsScopeAngle` result from a future host; it does not replace the latter's cross-product/degenerate tolerance with a guessed angle test. `CanAttack` field/water compatibility, weighted registration order and selection remain separate admission contracts. Begin accepts an already selected object and repeats ReadyAttack geometry, as the active source Init does; it does not independently reroll probability or repeat the cooldown gate.

## Isolated state owner

[pe_hare_pounce](../../rebuild/src/pe_hare_pounce.h) stores only transient action state and emits requests. Its API has no direct access to player HP, enemy positions, inventory, Animator, skill executor or saved game state. Inputs use absolute scaled float timestamps; nonfinite or backward time fails atomically.

- **Begin** remembers gravity, requests gravity0, clears desired movement effect and stops movement. It does not start a skill.
- **Unaligned Tick** requests facing the full target delta. It fails only after strictly more than5seconds. Alignment is checked first, so an aligned tick can start even after that deadline.
- **First aligned Tick** latches facing, stamps cooldown/start time, requests`attack5=true` and StartSkill30100053, then returns without movement.
- **Before`.267`** requests StopMove. During the inclusive movement interval, speed is computed once as`(horizontal distance+1)/(.6-.267)`. It then emits current forward movement and the supplied current target XZ-facing direction. Target movement never recomputes the latched speed.
- **After`.6`** emits StopMove,`attack5=false`, zero movement/facing directions and Run pursuit in that source order, then returns Success. The host must still call Reset after terminal status.
- **Reset** requests speed0, false attack bool, zero directions and saved gravity. It retains the cooldown. It has no StopSkill command: the source call is commented out. Target loss similarly returns Failure and leaves cleanup to Reset.

Linux `PEMath.Epsilon` is`2^-23`, rather than `System.Single.Epsilon`. Source Reset only emits its restoration when the stored rotation start time is greater than that epsilon. The isolated owner preserves the original exactly-time0/tiny-time edge, where restoration is skipped. Production activation must deliberately account for that lifecycle boundary; this patch does not silently change source behavior or hide it with a boolean workaround.

The source's normally unreachable stopped tail is also retained: repeated Tick after the earlier Success can return Running until elapsed is strictly greater than`.967`. A correct behavior-tree host resets on Success rather than treating this tail as the primary schedule.

The helper for the normalized forearm window is independent of movement elapsed and skill guide eligibility. A future hit requires all of the source Animator window, continuing skill execution, spatial contact, permission and deduplication gates; movement success does not automatically close the skill or prove a hit.

## Staged generic animation

[export_hare_pounce_assets.py](../../rebuild/tools/export_hare_pounce_assets.py) stages the ninth clip in a fresh output directory. It uses the existing generic Transform/class4 translation/quaternion/scale decoder and palette path through an optional argument to [export_reference_actor.py](../../rebuild/tools/export_reference_actor.py). The latter's ordinary eight-clip export remains its default. The pounce tool refuses an existing destination or any destination inside the installed original game. It does not install the result over production assets.

The qualified installed bundle SHA256 is`94409ad1bf9570048a52c42671986907e2a5ca131f55f73737e5b7d7cff76c35`. The generated actor has48 joints and1488 finite joint palettes for the31 pounce frames. The original actor header's mesh/index/joint fields and all835072 bytes after its header are unchanged; only the clip count and appended ninth clip differ. Thus previous clip indices, geometry, inverse binds and the existing skeleton fingerprint remain stable.

The staged directory is ignored at`rebuild/build/reference-assets/hare-pounce-staged-20261007`. Its prefix proof records:

- Original PEAC SHA256:`0961b92864b38d8f905a34287b0da53203a0ecf643d91b26f92fe69d0bc0eeaa`.
- Staged PEAC SHA256:`5e53a46c7f8d51952df7690c956a98707ccbb441a76b848904fdd3bc92f94496`.

The existing hare PEHB binds successfully to the staged actor. Each forearm has two capsule records, yielding four posed attack capsules in the qualified window. The old manifest's separate non-skin `Bip01` body trigger remains unexported; the inspected `attack5` behavior names the two forearms, so this work does not invent a body capsule or silently substitute it for the authored list.

The manifest retains controller behaviors and effects as evidence. Effects, transitions, skill scheduling, branch arbitration, collision motion and full engine palette interpolation remain outside this staged extraction. Source zero average speed is not used to infer a stationary pounce: C# supplies the requested movement.

## Checks and remaining integration gates

```sh
python3 rebuild/tools/export_hare_pounce.py --check
python3 -m unittest discover -s rebuild/tests -p test_hare_pounce_export.py -v
# Extraction requires an absent destination and the existing isolated UnityPy environment.
rebuild/build/unity-tools/bin/python rebuild/tools/export_hare_pounce_assets.py --output NEW_DIRECTORY
# Checking already staged assets requires only ordinary Python.
python3 rebuild/tools/export_hare_pounce_assets.py --check
```

Executed: focused optimized lifecycle tests passed; the same tests passed ASan/UBSan with leak detection. The actual C PEAC loader, all31 posed samples, unchanged old-clip sample/skeleton checks and all four compatible attack capsules passed at the exact window boundaries and adjacent floats. That asset/parser suite also passed ASan/UBSan with leak detection. All8 source/export Python tests passed with0skips here, including altered source lifecycle, XML, skill, clip, controller window, corrupt palette data/hash and changed prefix rejection. Staged asset tests fail when explicitly invoked without their assets; the ordinary Python suite marks its optional local-asset checks skipped when absent. A skip is not asset acceptance.

The original630-frame observation already established that this branch is used. Another broad hunt is unnecessary for these source contracts. Production activation remains gated on source branch selection, separate skill lifetime, real collision/motor request application and shared Animator/contact ownership, followed by a targeted native visual/integration check. Ordinary hunt/shield acceptance is still separate; this isolated preparation does not change encounter difficulty or complete that physical work.
