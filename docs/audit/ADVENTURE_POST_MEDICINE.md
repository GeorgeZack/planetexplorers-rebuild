# Naoko's post-medicine Bug Hunt

This document separates the source quest contract, original9075 acceptance/destination captured on October5, and the bounded C travel implementation. Medicine9072 completion advances Naoko's Adventure chain32 from group1 to group2, which selects9074. The database and relevant serializers determine the following contract; actual random destinations and encounter positions require a fresh original checkpoint or an explicitly bounded native random-generation implementation.

The native travel module now uses the archived source destination(-349,19.5,233). Focused C acceptance/arrival/persistence checks pass with installed route geometry. A later physical native run verified acceptance, ordinary traversal and reload at the encounter-pending boundary. Six-hare combat remains unimplemented; source contract, native implementation and physical observation remain separate evidence.

## Conversation and progression

| Mission | Trigger / objective | Outcome |
| --- | --- | --- |
|9074|Requires completed9072; player4101 asks whether supplies will arrive, Naoko4102 describes dangerous animals.|Repeatable information exchange; offers9075/9076. No grant/reward.|
|9075|Choose4103, “Okay.” `TalkOP=0` accepts immediately. **Bug Hunt**, discovery4524.|Travel to an acceptance-generated point. Auto-completes, advances chain32 to group3, starts9126. No items.|
|9076|Choose4104, “Nope.”|Repeatable refusal, no items/progression; permits reopening the offer.|
|9126|Auto-started from9075; target1518.|Kill four prototype13 lepus hares. Source creates six mission agents. Auto-completes and starts9127. No items.|
|9127|Return discovery4525 to role3 Naoko.|Completion dialogue4105→4106, rewards592/593/594/595×1, then9077.|
|9077|Requires9127; source4107–4110 announces the attack.|Town Defense7507, source30-second time field, town4 position, count15 and `td_id=520`. Distinct encounter; not implied by finishing the hunt.|
|9078|Naoko chain32 group3, requires9077.|Return to role2/Eitai within5m;4111/4112. No items.|

The9074 row uses `GuanLianList=9075,9076`, `MutexLimit=1:9075,9076`, `PreLimit=1:9072`, `MaxNum=-1`. Refusal9076 also has `MaxNum=-1`; as with medicine's refusal, the source `SetMission` path does not persist a completion count for repeatable negative-limit exchanges.9075 is one-shot and cannot be abandoned (`MaxNum=1`, `bGiveUp=0`). The intro, travel, kill and return stages share a large `TempLimit` list. [MissionManager.IsTempLimit](../../Assets/Scripts/Mission/MissionScript/MissionManager.cs) suppresses an offer if any listed quest is currently active; this is an active-mission exclusion, not a completion-history requirement. The bounded native chain has none of the unrelated exclusion quests, but must still prevent duplicate hunt acceptance.

The source `RewardSP=2` on9075/9126/9127/9077 is the same conditional skill-tree behavior described in [the medicine audit](ADVENTURE_MEDICINE.md): the inspected completion branch actually mounts a value5 buff only when skill trees are enabled. No fixed currency or unconditional two-point reward should be invented.

## Discovery4524 is generated at acceptance

`AdQuest_EnterArea4524` stores `DistPos=1_0_130_15`, `TrackRadius=60`, `NotForDungeon=1`. `ReferToType.Player=1`. [PlayerMission.MissionOperationAdRand](../../Assets/Scripts/Mission/PlayerMission.cs) resolves the player position when the mission is initialized, selects `Random.insideUnitCircle.normalized *130`, adds it in XZ, truncates both coordinates toward zero, and queries source terrain height (town-aware when appropriate). The result is saved in `TypeSearchData.m_DistPos`; it is not a fixed130m offset from Annot's center or Naoko's initial spawn.

[MissionManager.UpdateSearchMission](../../Assets/Scripts/Mission/MissionScript/MissionManager.cs) uses strict XZ distance below15m for random Adventure discovery, rejects original-world playerY<0 for this target, and checks the current scene. TrackRadius60 is a map/tracking parameter, not the arrival threshold.9075 has `AutoReply=1`, no item reward and `NextQuest=9126`; its arrival must not be displayed as the completed four-kill hunt.

The installed route covers X[-640,-224), Z[64,480), Y[0,64). The archived Naoko position is approximately(-277,26.5,127), so an easterly or southerly130m destination can be outside recovered terrain. Silently clamping it, moving it to Annot, or rerolling until it fits would change the source mechanic. A source-pinned implementation first needs an actual accepted9075 original save, its4524 destination, and sufficient recovered geometry. A native random implementation must reject an unsupported candidate before accepting the quest, preserve state on failure and disclose its bounded-world restriction; it must not accept a target that cannot be reached.

A strict bound check is necessary but does not prove a walkable route. A complete playable slice also needs supported destination terrain, an ordinary traversal test, and room for the subsequent encounter: up to15m from the player at9126 start plus5m scatter, scaled capsule radius up to.78m, and ongoing creature movement. General world generation/streaming remains outside this bounded route.

## Kill1518 encounter

`AdQuest_KillMonster1518` has `MonsterID=13_4`, `TargetPos=1_0_15_5`, `PathIDtemp=0_13_6`, no town destruction, no receive quest. [AdRandomMission.LoadAdTypeMonster](../../Assets/Scripts/Mission/AdRandomMission.cs) interprets these as accepted prototype13 with kill count4, player-relative center distance15 and scatter radius5, and six individual prototype13 spawns (type0; type1 would set the group mask).

[SceneEntityMissionPointMonsterKill](../../Assets/Scripts/Scene/SceneEntityMissionPointMonsterKill.cs) resolves the player's position when the kill mission starts, chooses a new random heading for the15m encounter center, scatters the six agents with `AiUtil.GetRandomPosition(center,0,5)`, and leaves Y to the scene position agent. Each gets the mission monster-kill beacon and `canRide=false`. These positions are not the earlier4524 destination and are not tutorial spawn261.

The death listener compares the dying entity's prototype to the target prototype list and increments the active mission's `MONSTER0` variable. It does not restrict credit to the six spawned instance IDs or to a radius around the encounter. This row does not enable `m_mustByPlayer`; do not introduce an unsupported player-last-hit-only condition. Each actual alive-to-dead event must be delivered once in the native host. Four deaths complete1518; source `Stop` removes all remaining agents owned by that mission and unsubscribes its death listener. A reload must preserve mission progress and cannot grant duplicate deaths from an already-dead saved actor.

## Return and continuation boundary

`AdQuest_EnterArea4525` stores `DistPos=3_3_0_5`: role3 NPC anchor, no random offset, strict5m discovery with source `NotForDungeon=1`. Its position is initialized from the role-bound NPC, using the same integerXZ/terrain-height path. It is not a generic “press E near any NPC” completion.

9127 has `AutoReply=1`, `TalkED=4105,4106`, `RewardItem=592_1,593_1,594_1,595_1`, `NextQuest=9077`, and `AdvPlot=3_4`. These differ from the current native medicine final-ACK adapter. The later original medicine observation and source re-audit show its completion occurs before4100 is displayed; see the [timing correction](ADVENTURE_MEDICINE.md#october-5-original-delivery-timing-and-retained-native-policy). Before porting this return step, verify how automatic completion, the talk queue and plot3_4 interleave in the original. Do not reuse the native medicine final-ACK transaction blindly.9077's defense waves and the later9078 return remain explicit separate work.

## Existing C reuse and practical implementation order

[Travel](ADVENTURE_ROUTE.md), [bounded combat](ADVENTURE_HUNT_COMBAT.md) and [return](ADVENTURE_HUNT_RETURN.md) are implemented. They own current transactions and adapters; [HANDOFF](../HANDOFF.md) alone sets next work.

## Reference capture contract

The accepted target and later encounter/return captures are archived below and in their feature audits. Inspect these before planning another original session; the former missing-target capture request is closed.

## Prepared native travel implementation

[audit_reference_hunt.py](../../rebuild/tools/audit_reference_hunt.py) decodes the complete bounded AdRM record layout, checks active quest IDs against the top-level record and hashes all input files before/after. Its `--require-hunt` option fails if9075 has no saved4524 target. [export_adventure_hunt.py](../../rebuild/tools/export_adventure_hunt.py) checks the source database contract and optionally consumes that observed evidence. Missing evidence produces an explicit unavailable flag; its zero initializer can never be accepted as a destination.

[pe_adventure_hunt](../../rebuild/src/pe_adventure_hunt.h) owns states0 unaccepted,1 travel9075 and2 encounter9126 pending. Acceptance requires completed medicine, the source route profile, full arrival-disk bounds, and supported authoritative collision at the reference destination. It commits only the state and source-space destination; inventory, currency and RNG are unchanged. Discovery uses the strict15m XZ rule and never grants kills or labels the entire Bug Hunt complete. State validation preserves pending progression across explicit world imports, although arrival requires the supported source route. The caller owns the actual Naoko encounter and modal dialogue.

This is an explicitly pinned reference quest instance, consistent with the current pinned town/population/terrain. It does not replay Unity's global random stream or shift the observed destination when the player accepts from a slightly different position. A generic130m random target generator remains separate work.

Six focused Python tests currently pass: deterministic export, unavailable-without-reference policy, changed-source rejection, complete/truncated/trailing-byte/nonfinite discovery archive decoding, capture cross-record agreement, and duplicate active mission rejection. The hunt C module and fixture pass strict compilation. The earlier absent-reference branch did not establish positive arrival coverage. The fresh capture and positive checks below supersede that limitation.

## October 5 earlier continuation checkpoint — missing reference

The early zero-reference acceptance gate is obsolete. The later captured4524 target and supported-route checks below remain authoritative; unsupported targets/worlds still fail closed.

## October 5 later checkpoint — captured reference and positive native checks

The original client completed medicine and accepted9075, then saved fresh manual slot5 (`save4`) in `/home/admin/.local/share/planet-explorers-hunt-reference`. [The new archive evidence](../reference/hunt-original-save.json) records active9075, completed9072 and the saved4524 discovery at **source(-349,19.5,233)**. The top-level and subworld active quest records agree; every save file was hashed before/after and remained unchanged. [The package inventory](../reference/hunt-original-inventory.json) independently confirms395 currency, no917,925×10,554×1, known337/388/390 and no running replication. The preceding ten-checkpoint inventory remains historical evidence and must not be used to claim that a hunt reference is still missing.

The actual target, its15m arrival disk and21m encounter margin lie inside the installed416×64×416 route. `export_adventure_hunt.py --capture docs/reference/hunt-original-save.json` now generates `PE_HUNT_REFERENCE_AVAILABLE=1`; its `--check` mode and all six Python tests pass. The export pins this observed quest instance without rerolling, clamping or shifting it relative to future acceptance positions.

Focused optimized C tests compiled with strict warnings including `-Werror` pass with zero failures. They load the installed route, accept the real target using the authoritative collision settle/support check, reject the exact15m threshold and negative sourceY, advance just inside the threshold to encounter-pending9126, and preserve inventory/currency/RNG. The v10 tests now exercise active/pending hunt persistence alongside deployment migration/malformed-save checks. These are positive headless checks, not physical native traversal or proof of the entire arrival disk's traversability. The encounter, four-kill progress and return 9127 were implemented later and accepted natively ([hunt acceptance](HUNT_ORDINARY_ACCEPTANCE.md)).

Original delivery exposed a separate ordering mismatch: the original completes9072 and applies rewards when4099 is selected, before displaying4100. The [medicine audit](ADVENTURE_MEDICINE.md#october-5-original-delivery-timing-and-retained-native-policy) records precise source calls. The user subsequently chose to retain native final4100 acknowledgment as an intentional timing difference; preserve that policy.

## Next implementation plan — six-hare9126 encounter

The six-agent encounter and ordinary native completion/partial reload now have [combat](ADVENTURE_HUNT_COMBAT.md) and [acceptance](HUNT_ORDINARY_ACCEPTANCE.md) owners. [Movement](HARE_MOVEMENT.md), [perception](HARE_PERCEPTION.md), [skill clocks](HARE_SKILL.md) and [damage](HARE_DAMAGE.md) own the recovered rules. The earlier proposed host/delivery sequence is retired.

## Later October 5 original arrival checkpoint

The original player physically reached4524 and observed travel9075 completion followed by9126's four-lepus-hare objective at0/4. [Arrival](../reference/hunt-original-arrival.png) and [fresh manual slot6 save](../reference/hunt-original-encounter-saved.png) accompany [read-only save5 evidence](../reference/hunt-original-encounter.json). The archive records completed9075=1, active9126, `STEP=0`, `MONSTER0=13_0` and target1518. Saved player source position(-336.993835,20.714342,227.883118) is13.051m XZ from the pinned destination; it is not a reconstructed exact arrival frame. All inspected save5 hashes remained unchanged, and prior save4 hashes still match the accepted-hunt capture.

The decoded AdRM target1518 is kind0 and carries no center/vector/agent IDs. No monster prototype13 appears in the decoded `ArchiveKeyCreature` list. Do not invent the missing positions or conclude that live actors were absent: the archive inspection is bounded. Source [`SceneEntityPosAgent`](../../Assets/Scripts/Scene/SceneEntityPosAgent.cs) implements `ISceneObjAgent`, while [`SceneMan`](../../Assets/Scripts/Scene/SceneMan.cs) exports only `ISceneSerializableObjAgent` objects. This supports the transient-agent limitation noted in the implementation plan; it does not establish actual reload behavior. In the original, six-agent visual counting, authored attacks and mid-encounter reload remain unobserved.

The live continuation's final automated verification includes full optimized core/content/23CLI passes,35 optional asset tests with zero skips, and a direct full ASan/UBSan/leak run exiting0 after172.193 seconds. See [PLAYTEST](../PLAYTEST.md) for logs and the distinction between these native checks and original observations.


## Later October 5 native physical travel checkpoint

The native player selected4103, then used ordinary WASD travel to source(-343.558502,19.320673,222.071869),12.207946m XZ from the captured target. The host changed from travel state1 to encounter-pending state2. After F9, the explicit Pause→Load game→Resume flow and Q Mission panel retained that boundary. [Accepted quest](../reference/hunt-native-accepted2.png), [arrival](../reference/hunt-native-arrival.png), [reloaded Mission panel](../reference/hunt-native-reloaded.png) and [checksum-verified PECSv10 inspection](../reference/hunt-native-travel.json) distinguish acceptance/arrival state and preserve the actual target. One deployed Torch remained in both inspected checkpoints. This establishes the tested native route and reload; it does not imply six-hare spawning, attacks, kills, return9127 or equivalence of every possible approach route.
