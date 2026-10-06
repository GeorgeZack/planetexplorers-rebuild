# Passive low-health recovery: source lifecycle

Audit date: 2026-10-06. This document distinguishes source-derived rules from the native implementation checkpoint. The audit used local source, the original database and a read-only prefab decode. It did not launch the original or operate the desktop.

## Exact rule and provenance

`Assets/Resources/DataBase/localData.bytes`, `initprop` player row1, includes buff30200046 in `InitBuffList`. Its `skBuff` row has type65300, priority0, stack limit1, lifetime−1, interval1 and no begin/periodic/end effects. Its expression is:

```csharp
if (target.expFunc.InCoolingForGettingHurt(4)
    && target.raws[1] / target.sums[0] < 0.33f) {
    target.raws[1] += target.sums[0] * 0.01f;
}
```

Here raw1 is HP and summed0 is maximum HP. The expression uses a literal1% maximum-HP increment; it does not read the separate `HPRecover` attribute. There is no clamp to33%: HP164/500 becomes169, while165/500 does not qualify. General HP clamping is against maximum HP, not the recovery threshold ([PESkEntity.cs:121](../../Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs#L121)).

The database SHA-256 is `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376`. `PlayerProtoDb` reads the initial-buff array from that row ([PeEntityCreator.cs:749](../../Assets/PeWorld/PeEntityCreator.cs#L749)). The generated recovery metadata should retain the input hash and exact expressions instead of introducing balancing constants.

## Hurt clock and scheduler

[SkEntityHelper.cs:25](../../Assets/Scripts/SkillSystem/SkEntityHelper.cs#L25) initializes the latest-hurt timestamp to0. Both `GetHurt` and permitted `TryGetHurt` stamp `Time.time` ([SkExpFuncSet.cs:38](../../Assets/Scripts/SkillSystem/SkExpFuncSet.cs#L38), [line89](../../Assets/Scripts/SkillSystem/SkExpFuncSet.cs#L89)). These functions do not require a positive HP delta before stamping; for example, permitted projectile damage absorbed by its separate energy shield still reaches the timestamp assignment. Unsupported damage paths should not be inferred from HP differences alone.

`InCoolingForGettingHurt(4)` means `Time.time >= lastHurt + 4`, including equality ([line110](../../Assets/Scripts/SkillSystem/SkExpFuncSet.cs#L110)). Damage changes this timestamp without restarting the buff's interval phase.

The driver is **one global attribute pass every eight rendered frames**, not every display frame and not FixedUpdate. [PeSingletonRunner.cs:9](../../Assets/Scripts/Assist/PeSingletonRunner.cs#L9) calls the singleton manager from Unity `Update`; [SkEntityAttribsUpdater.cs:13](../../Assets/Scripts/SkillSystem/SkEntityAttribsUpdater.cs#L13) compares `Time.frameCount` against its previous pass plus8, processes registered entities in reverse order, then stores the current frame. Its initial previous-frame value is0. A newly created updater after frame8 can therefore run on its first call; the gate is not eight frames after each entity mount.

Within an entity, [SkAttribs.cs:29](../../Assets/Scripts/SkillSystem/SkAttribs.cs#L29) visits mounted buffs in reverse order. The pass executes buffs before recalculating dirty summed attributes ([line210](../../Assets/Scripts/SkillSystem/SkAttribs.cs#L210)).

[SkBuffs.cs:94](../../Assets/Scripts/SkillSystem/SkBuffs.cs#L94) records the mount's `Time.time`, with execution index0. `Exec` ([line159](../../Assets/Scripts/SkillSystem/SkBuffs.cs#L159)) has two separate branches:

1. Index0 evaluates immediately and increments the index.
2. If elapsed time is **strictly greater than** index×interval, evaluate once more and increment the index.

Consequently a delayed first execution can evaluate twice. Later passes process at most one overdue interval, without a catch-up loop. The index advances even when the expression rejects healing because HP is high or the hurt cooldown is active. A delay does not replace the mount time with the latest execution time.

## Fresh creation, archive load and remount

Fresh `CreatePlayer` calls `InitAttrs` with prototype attributes and initial buffs ([PeEntityCreator.cs:1676](../../Assets/PeWorld/PeEntityCreator.cs#L1676), [line1969](../../Assets/PeWorld/PeEntityCreator.cs#L1969)). `InitSkEntity` reaches `InitEntity`, which mounts the initial buffs immediately ([PESkEntity.cs:258](../../Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs#L258)).

`SkAliveEntity.Start` also schedules `CheckInitAttr` after **0.5 scaled seconds** ([SkAliveEntity.cs:278](../../Assets/Scripts/SkillSystem/SkAliveEntity.cs#L278)). If HP is positive, that check mounts every initial buff again ([PESkEntity.cs:263](../../Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs#L263)). Stack limit1 removes the existing same-ID instance before adding the new one ([SkBuffs.cs:110](../../Assets/Scripts/SkillSystem/SkBuffs.cs#L110)). The fresh player's second mount resets its phase and first-execution index.

Archive loading follows a different path: `ReadEntity` instantiates the saved prefab and calls `Import`, without going through fresh-player `InitAttrs` ([PeCreature.cs:212](../../Assets/PeWorld/PeCreature.cs#L212)). `PeEntity.Import` restores the entity prototype before deserializing its components ([PeEntity.cs:163](../../Assets/Scripts/PeEntity/PeEntity.cs#L163)). `SkAliveEntity.Deserialize` initializes the skill entity, restores attribute values, then assigns the prototype's initial-buff list ([SkAliveEntity.cs:320](../../Assets/Scripts/SkillSystem/SkAliveEntity.cs#L320)).

The actual source prefab `Assets/Scripts/PeEntity/Resources/EntityPlayer.prefab` was decoded read-only with the existing `rebuild/build/unity-tools/bin/python` UnityPy environment. Its skill component has `m_InitBuffList=[]`, `_lastestTimeOfGettingHurt=0.0` and `_lastestTimeOfHurtingSb=0.0`. Prefab SHA-256: `7119a434d3e1527cca656614ab347f87593913510a5302bef117a2e7834e4d3f`. Thus load initialization does not mount this buff from prefab data. The delayed0.5s check performs the first alive mount; its first evaluation occurs on a subsequent eligible attribute pass. This is source/prefab evidence, not measured original-load timing.

## Death, revive and serialization

Death removes the initial buffs ([PESkEntity.cs:448](../../Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs#L448)); the delayed initial check also dispatches death instead of mounting when HP is nonpositive. `Action_Revive` raises HP to at least1, restores oxygen/stamina and sends `State_Revive` ([Action_Live.cs:587](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs#L587)). The handler remounts initial buffs and clears the dead flag ([SkAliveEntity.cs:422](../../Assets/Scripts/SkillSystem/SkAliveEntity.cs#L422)). It does not reset the existing hurt timestamp.

`SkAliveEntity.Serialize` writes version and attribute serialization; its initial-buff-list writer is commented out ([SkAliveEntity.cs:300](../../Assets/Scripts/SkillSystem/SkAliveEntity.cs#L300)). [SkAttribs.cs:236](../../Assets/Scripts/SkillSystem/SkAttribs.cs#L236) serializes raw attribute floats only, and its deserializer explicitly excludes buffs and modifier variables. The hurt timestamp assignments occur only at declaration and damage notification. Neither timestamp nor recovery phase is persisted.

A newly loaded entity therefore starts with lastHurt0 against the existing global scaled-time clock. It does **not** acquire a fresh four-second post-load hurt cooldown. Its saved HP remains authoritative; the recreated buff may heal after mounting when the ordinary conditions permit. No additional PECS fields are required to reproduce this transient ownership model.

## Native implementation checkpoint and explicit boundaries

The subsequent [stamina/comfort increment](STAMINA_RECOVERY.md) shares this clock/lifecycle and advances saves to v16 solely for raw comfort. Statements below about unchanged v15 bytes describe the HP-only checkpoint.

Implemented in C: [pe_passive_recovery](../../rebuild/src/pe_passive_recovery.h) separates the external attribute clock from transient entity state. It implements the eight-frame gate, float HP rule, initial/periodic evaluations, fresh remount versus loaded first mount, and death unmount. The exporter [export_passive_recovery.py](../../rebuild/tools/export_passive_recovery.py) guards the active database/source paths and generates exact metadata with provenance. General revival remains unsupported; merely assigning positive HP after death does not implicitly remount a buff.

The sandbox calls its health adapter once per display frame before fixed-step movement/combat. Its process-local active-sandbox clock survives save loads and menu roundtrips. Each successful hunt transaction reports the change in committed `player_hits`; this timestamps admitted zero-damage hits as well as HP loss, without notifying on failed transactions. Every contact in one display frame uses the same validated timestamp. The notification is infallible for the already validated owner, and death unmounts immediately. The low-level standalone damage helpers remain independent of `PeGame`.

`PeGame.passive_recovery` is transient. Successful save loading sets its fresh-load marker only after validation; failed loads preserve the live state. The PECSv15 payload is unchanged. This increment is integrated into Adventure Sandbox; Legacy and the separate tutorial host retain their previous behavior. `pe_game_step` remains a fixed-step API and does not itself run the display-frame attribute scheduler. Headless hosts that exercise recovery must drive the explicit clock/frame API.

The following are explicit integration decisions, not automatically equivalent to Unity:

- Unity uses process-global scaled `Time.time`, not the native accelerated day/world clock. A native active-sandbox epoch surviving loads/menu roundtrips is a bounded epoch adapter; omitted menu/loading time must be documented.
- Source single-player pause sets `Time.timeScale` to `.0001`, not zero ([PeGameMgr.cs:363](../../Assets/PeLauncher/PeGameMgr.cs#L363)). Fully frozen native pause remains an intentional timing adapter. Display frames and scaled time are separate inputs.
- The runner's script execution order is0 in its `.meta`; this audit does not establish a total order against every damage producer. Native recovery runs before fixed-step contacts in a display frame. The delayed initialization callback runs before that frame's attribute pass. Both choices are explicit ordering adapters, not recovered cross-script ordering.
- Source clocks/expressions use float values. Native time accumulates in double and is converted to float before buff comparisons/arithmetic. This preserves the tested boundaries without claiming bit-identical Unity time accumulation.
- The host currently supplies its existing capped frame delta, at most.1s, and fully freezes scaled time while unfocused/paused. Very slow display frames therefore do not advance recovery by their full wall duration. Menu/loading/tutorial time is omitted from the native clock epoch. A first process load is distinct from reloading within an already running Sandbox.
- Existing Juice and Medpack clocks remain on the previous fixed-step path. This increment is not a port of all buff ordering, stamina, comfort, hunger or oxygen.
- Do not heal dead players, introduce catch-up loops, reset buff phase on damage, clamp to33%, or persist a new recovery cooldown merely for convenience.

Focused tests cover threshold/equality and overshoot, initial versus periodic execution, first delayed double evaluation, eight-frame scheduling, blocked-pulse phase advancement, same-frame committed damage, death without implicit revival, fresh0.5s remount versus load first mount, unchanged save bytes, failed-load atomicity, and clock/pause adapters. See [core recovery tests](../../rebuild/tests/test_passive_recovery.c) and [actual host regression](../../rebuild/tests/test_sandbox_health_host.c). The latter includes the production host helpers and links without running graphics; its committed-hit-count inputs do not replace the separate real capsule/damage/rollback tests.

Verification checkpoint:

- Optimized graphics executable build and full `test_core`: pass, zero failures.
- Actual sandbox health/equipment host tests, existing consumable/contact regression and sandbox UI tests: pass, no asset skips in this selected run.
- Focused clock and actual health-host ASan/UBSan runs: pass with leak detection. This is focused sanitizer coverage, not a new full-suite sanitizer run.
- All13 exporter tests, `check-content` and23 CLI tests: pass.
- Documentation link/line checks and structural index rebuilt for navigation.

Local logs are under ignored `rebuild/build/passive-recovery/`; the separately run host sanitizer log is `rebuild/build/health-host-sanitize.log`. No game was launched for this increment; native retreat/recovery presentation and ordinary four-hare completion remain unverified. Stamina cadence/comfort, capsule history and shield layer qualification remain separate next tasks.
