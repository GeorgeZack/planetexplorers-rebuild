# Stamina recovery and comfort

Source audit: 2026-10-06. This is a bounded port of three player initial buffs, not the complete hunger, oxygen, sleep or survival system. Source rules below were established by local code/database inspection without a new original-game launch. Implementation verification remains a separate checkpoint.

## Source metadata

The original `Assets/Resources/DataBase/localData.bytes` has SHA-256 `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376`. Player `initprop` row1 supplies stamina100/100, recovery speed40, reduction multiplier1 and raw recovery delay0; comfort500/500, recovery speed−.1388 and reduction multiplier1.

The selected rows are exported by [export_stamina_recovery.py](../../rebuild/tools/export_stamina_recovery.py), with generated provenance in [pe_stamina_recovery_source.json](../../rebuild/generated/pe_stamina_recovery_source.json) and C constants in [pe_stamina_recovery_source.h](../../rebuild/generated/pe_stamina_recovery_source.h). Modify the exporter and regenerate rather than editing those outputs.

| Buff | Interval | Exact expression |
| --- | --- | --- |
|30200052 stamina|1s|`if(target.expFunc.InCoolingForConsumingStamina(target.sums[6])){target.raws[3]+=target.sums[5]*target.sums[4];}`|
|30200049 comfort|5s|`target.raws[8]+=target.sums[10]*target.sums[9]*5`|
|30200175 delay|1s|`target.buffPostAdd=(1-target.raws[8]/target.sums[7])*2`|

All three have unlimited lifetime and stack limit1. Stamina and delay share type65509/priority0, but both remain mounted: [SkBuffs.cs:110](../../Assets/Scripts/SkillSystem/SkBuffs.cs#L110) replaces matching IDs at their stack limit and handles higher/lower priorities; it does not remove an unrelated equal-priority buff merely because its type matches.

The stamina condition is **strictly** `now > lastConsumption + max(summedDelay,.1)`, unlike passive HP recovery's inclusive four-second comparison ([SkExpFuncSet.cs:114](../../Assets/Scripts/SkillSystem/SkExpFuncSet.cs#L114)). A qualifying pulse adds40 with the starter attributes. Comfort's pulse subtracts.694, including its initial execution. These are discrete pulses, not continuous per-second rates.

## Shared clock and two-phase attribute ordering

Use the [passive recovery lifecycle](PASSIVE_RECOVERY.md): Unity's global attribute updater runs every eight rendered frames; each buff executes initially and then checks strict `elapsed > index*interval`. A delayed first execution can evaluate twice, while later passes perform at most one overdue periodic evaluation. Rejected stamina recovery still advances its pulse index. Each buff has its own index even when it shares the mount time.

The player initial-buff list places comfort first, stamina later and delay last. [SkAttribs.cs:29](../../Assets/Scripts/SkillSystem/SkAttribs.cs#L29) executes the list in reverse. [SkAttribs.cs:210](../../Assets/Scripts/SkillSystem/SkAttribs.cs#L210) then recalculates dirty attributes in ascending numeric order. For the supported three-buff subset:

1. Delay30200175 executes first. It marks attribute6 dirty; it does not immediately alter summed delay.
2. Stamina30200052 evaluates using the **previous summed delay6**.
3. Comfort30200049 changes raw comfort8 if its own pulse is due.
4. Dirty stamina3 recalculates/clamps, then dirty delay6 evaluates from the **new raw comfort8**.
5. Dirty comfort8 recalculates and clamps to0..ComfortMax.

Therefore new comfort affects the newly computed delay in this pass, but cannot change the stamina pulse that already ran. The next stamina evaluation sees that delay. A comfort write does not itself mark delay6 dirty: delay normally updates on30200175's own pulse.

The temporary classification is literal: an expression containing `buff` enters the temporary-operation list ([SkAttribsModifier.cs:112](../../Assets/Scripts/SkillSystem/SkAttribsModifier.cs#L112), [line224](../../Assets/Scripts/SkillSystem/SkAttribsModifier.cs#L224)). Scheduled execution requests recalculation of its indexed attribute ([line247](../../Assets/Scripts/SkillSystem/SkAttribsModifier.cs#L247)). Recalculation resets the multiplier/additions and applies matching temporary operations ([line264](../../Assets/Scripts/SkillSystem/SkAttribsModifier.cs#L264)). Source repeats the operation according to the buff's execution count; this particular operation is assignment, so repeated identical evaluations have the same result. Collapsing those repetitions is safe only within this supported expression subset, not a general modifier rule.

General clamping occurs through [PESkEntity.cs:121](../../Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs#L121) after summed-attribute notifications. Near comfort0, a negative raw comfort value can be observed by delay6 **before** comfort8 clamps. This can produce a temporary delay above2; do not clamp that intermediate delay to2. Stamina similarly clamps against its maximum after raw additions, including a delayed double pulse.

## Consumption notifications and supported action tails

[SkEntityHelper.cs:27](../../Assets/Scripts/SkillSystem/SkEntityHelper.cs#L27) initializes last consumption to0. An actual summed stamina decrease updates it to `Time.time` through [PESkEntity.cs:484](../../Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs#L484). Increasing/clamping stamina during recovery does not count as consumption.

Several actions also refresh the timestamp while running, even when they do not debit stamina on that frame:

| Action | Source notification |
| --- | --- |
|Sword, including supported Knife|`UpdateAnimSpeed` refreshes during valid action updates; it precedes the completion test. Cost is charged separately per selected attack mode. [Action_Sword.cs:141](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L141), [line290](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L290), [line518](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs#L518).|
|Digging/mining|Refreshes while active and through the supported ending tail, except a frame returning early on completion. [Action_DigTerrain.cs:110](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_DigTerrain.cs#L110).|
|Axe|Refresh occurs before the ending/completion test. [Action_Fell.cs:121](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Fell.cs#L121).|
|Jump|Initial debit, then refresh during action updates when configured cost exceeds epsilon, including the landing evaluation. [Action_HumanMove.cs:846](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_HumanMove.cs#L846), [line889](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_HumanMove.cs#L889).|
|Sprint|Continuous successful debit invokes the common decrease notification; there is no separate unconditional refresh. [Action_HumanMove.cs:339](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_HumanMove.cs#L339).|

Only stamping a visibly decreased stamina value would incorrectly allow recovery during sword/tool/jump tails. Preserve the authoritative action clock and stamp activity from that owner's actual lifetime. Do not infer an active action merely from a held input or a failed action attempt.

One distinct source sprint edge remains: when proposed stamina falls to epsilon or below, the action ends before committing that proposed zero. The existing native depleted-sprint behavior is a separate movement difference; this recovery increment should not silently claim to resolve it.

## Initialization, save/load and death

Fresh creation mounts the buffs immediately; `CheckInitAttr` after.5 scaled seconds remounts alive initial buffs. If an eligible attribute pass occurs between those mounts, comfort receives two initial−.694 pulses. Archive load instead begins with the source prefab's empty initial-buff array; deserialization restores prototype IDs and the delayed check performs the first mount. See [PASSIVE_RECOVERY.md](PASSIVE_RECOVERY.md) for the exact prefab hash, read-only decode and lifecycle call chain.

Source serializes raw stamina, raw comfort8 and raw/base delay6, but not the temporary modifier, summed derived delay, consumption timestamp or buff phases. Deserialization starts summed values equal to raw values ([SkAttribs.cs:236](../../Assets/Scripts/SkillSystem/SkAttribs.cs#L236)). A loaded low-comfort player therefore initially has base delay0: the first mounted stamina pulse can use the.1 minimum before the derived comfort delay is rebuilt later in that pass. Do not eagerly derive delay during load or save a new consumption cooldown.

Death removes initial buffs; the native owner drops the temporary delay modifier on the next attribute pass. Revive restores maximum stamina and remounts the buffs, retaining existing comfort and last-consumption timestamp ([Action_Live.cs:587](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs#L587), [SkAliveEntity.cs:422](../../Assets/Scripts/SkillSystem/SkAliveEntity.cs#L422)). Buff removal marks its temporary attributes dirty; the bounded native host does not yet claim the complete original revive/action system.

## Native implementation contract and verification checkpoint

[pe_stamina_recovery](../../rebuild/src/pe_stamina_recovery.h) owns transient consumption time, summed delay and the three pulse indices. It consumes the shared display-frame clock and before/after lifecycle snapshots from [pe_passive_recovery](../../rebuild/src/pe_passive_recovery.h); it does not create a second clock. Game-owned stamina and comfort remain the authoritative vitals. Action owners retain their debit logic and notify successful consumption/active cost-owning action frames.

PECS **v16** retains the literal v15 payload and appends exactly **4 bytes: raw comfort as a float**. Values must be finite and within0..500. Versions1–15 infer500 as an explicit compatibility default. The existing saved `stamina_recover_delay` retains its raw/base meaning; derived delay and timestamps remain transient. Failed-load validation must not modify live state. See [pe_save.c](../../rebuild/src/pe_save.c) and [pe_game.h](../../rebuild/src/pe_game.h).

Adventure fixed-step simulation must **never** fall back to continuous stamina regeneration, including standalone/headless calls and the pre-mount/load interval. The display-frame recovery owner is required for recovery; fixed ticks may debit and notify activity. Legacy/tutorial behavior is a separate host contract. Do not add continuous regeneration merely because the display owner has not initialized yet.

Implemented in Adventure Sandbox: the existing health-frame adapter stages HP, stamina, comfort and both transient owners atomically. The Character panel displays current comfort. `pe_game_stamina_activity` receives committed generic-tool costs, timed tool activity and the real game-step action lock (including melee/jump ending frames). Invalid frame inputs preserve the display clock and all vitals. Legacy/tutorial retain their previous recovery path.

Focused verification has passed: the pure clock/order tests, actual sandbox stamina host, nine historical/current save suites, updated health host, both exporters (25 tests), content checks,23 CLI tests, sandbox UI, consumable/contact and equipment-host checks. Clock, stamina-host and new-save tests also pass ASan/UBSan with leak detection. These are focused sanitizer runs, not a fresh full-core sanitizer run. No selected asset-dependent check skipped.

The first full optimized core run exposed one old tool guard expecting continuous recovery. Its corrected regression now verifies zero recovery across fixed ticks and a full40 pulse through the actual display scheduler. The final optimized executable build and full core rerun pass with zero failures and no asset skips. Logs are under ignored `rebuild/build/stamina-recovery/`; focused save logs are `rebuild/build/stamina-save-*` and focused host logs are `rebuild/build/stamina-host-audit/`. No original/native graphics was launched; no physical stamina, comfort-display or ordinary hunt acceptance is claimed.

Required focused checks: strict cooldown equality and source pulse boundaries; old-delay/new-comfort ordering; dirty-only delay updates; negative-comfort intermediate delay; initial/delayed-double/remount pulses; independent pulse counters; stamina maximum clamp; low-comfort loaded first pulse; active/ending action timestamps; failed actions that do not stamp; death removal; v16 literal prefix and malformed/truncated/NaN saves; old-save500 migration; and Adventure fixed ticks with no recovery owner.

The passive-recovery audit's scaled-time epoch, frozen native pause, float precision and cross-script ordering caveats remain applicable. Comfort depletion alone does not implement source rest, beds, food, environmental effects or the complete survival dependency graph; those remain explicit omissions.
