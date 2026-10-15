# Typed Adventure mission events9077–9084

October7,2026, US/Central. **Source-derived:** guarded mission, target, plot and dialogue-role contracts. **Implemented in C:** a disconnected event reducer with injected atomic side effects. No original game, graphics, desktop input or new archive capture was used. This does not enable any additional mission in the playable sandbox or change its save format. Native medicine delivery still occurs on the user-selected final4100 acknowledgment; this module neither owns nor changes that flow.

[The exporter](../../rebuild/tools/export_adventure_mission_events.py) generates [typed C metadata](../../rebuild/generated/pe_adventure_mission_events_source.h) and [complete evidence rows](../../rebuild/generated/pe_adventure_mission_events_source.json). [The reducer](../../rebuild/src/pe_adventure_mission_events.h) has no dependency on `PeGame`, rendering, actual inventory or NPC/geometry owners. It implements target-bearing missions9077/9078/9079/9080/9082/9084. Repeatable zero-target choice shells9081/9083 retain exported prerequisites, mutual exclusions, linked choices and dialogue, but have no invented one-shot runtime lifecycle.

## Recovered event distinctions

| Mission | Admission/source events | Completion/source events |
| --- | --- | --- |
|9077|Requires9127; opening4107–4110; target7507|No item reward and `NextQuest=0`; completing it makes9078 prerequisite-eligible without offering or accepting9078|
|9078|Requires9077; player entry4111 and opening4112; discovery4504|No next quest; completion makes9079 prerequisite-eligible|
|9079|Requires9078; player entry4113 and opening4114–4115; discovery4505|Acceptance queues **Get plot5** before the travel target completes; no completion plot/NextQuest|
|9080|Plot5 requests this mission; no opening; acceptance items657/662/660×1|Any qualified source use target5509/5510/5511 can complete it; no completion items; NextQuest9084 requests its opening|
|9081|Repeatable choice shell after9079; opening4117; linked choices9082/9083, excluded by either active/completed choice|Metadata-only lifecycle|
|9082|Qualified choice; acceptance597×1; current-possession target2534 requires1038×50|Manual turn-in removes50, grants478/490/696/465/474/473/481×1, advances the NPC chain once, then presents4121|
|9083|Repeatable refusal shell, excluded by9082|Metadata-only lifecycle|
|9084|Opening4122–4124; discovery4506|NextQuest9114 is emitted as an external request;9114 is outside this reducer|

Discovery4504/4506 use source role2's starting reference, and4505 uses role5, all with strict5m source descriptors. Assembly-use targets reference item1127 and three town descriptors with radius100, `AllowOld=1` and `ComMission=1`. The exporter retains these **references**, not invented world coordinates, NPC identities, colonies or B-key block substitutes. Dialogue roles are decoded from the source scenario tokens; no entity identity is inferred from a role name.

All catalog dependencies remain explicit:9080 accepts657/662/660, while9082 accepts597, removes1038 and rewards478/490/696/465/474/473/481. Catalog closure is maintained separately from this reducer. Exported definitions alone do not establish an executable quest transaction: the future real callback must qualify every item/component/formula dependency, inventory ownership, current possession and capacity before committing. The reducer does not claim a working gameplay grant. Its test ledger is a synthetic atomic-effect oracle, including an injected missing-catalog rejection to verify rollback even when a dependency is unavailable.

## Actual opening and completion call chains

Acceptance timing is recovered from the active UI path, not inferred solely from `TalkOP`:

1. `UINPCTalk.UpdateNpcTalkInfo(MissionID,type,...)` passes the resolved source talk list and mission type to `MatchTalkInfo`; type1 corresponds to `TakeMissionType_Get`.
2. `MatchTalkInfo` assigns `missionTrigger` and its type **only to the final talk row**. Earlier rows receive no mission trigger.
3. `OnClose` advances the displayed row, resolves the actual entity or Adventure role token, then dispatches that row's trigger through `MissionManager.SetGetTakeMission`.
4. The single-player path enters `ProcessSingleMode(Get)`, checks admission and random-mission binding, and calls `SetQuestVariable`, which reaches `PlayerMission.SetQuestVariable1`.
5. For these target-bearing missions, `SetQuestVariable1` adds the active mission flag, queues the Get plot, then handles acceptance items when global mission initialization has completed and `HasGetRewards` is false.

The native owner distinguishes offered state from accepted state and acknowledges source opening IDs in order. Only the last acknowledgment commits acceptance. It retains progress in its value snapshot but does not implement the actual conversation renderer, speaker binding, cancellation presentation or Unity talk scheduling. For an externally qualified no-opening offer, an explicit zero-valued acceptance command performs the transition; this is a native event API convention, not an invented visible dialogue page. Fresh commands require source initialization and external NPC/choice qualification; recorded state uses snapshot restore instead.

`PlayerMission.CompleteTarget` separately checks target completion and automatic mission reply. In the single-player branch, target suffixes≥500 bypass the ordinary all-target wait, and `ForceCompleteMission` reads use-target `m_comMission`. All three9080 targets have suffixes509–511 and `ComMission=1`. Thus a single **qualified assembly-use target** makes9080 completion-ready. Its separate `AutoReply=1` then dispatches `CompleteMission`; a generic item use or arbitrary construction does not qualify.9082 instead has `AutoReply=0`: current-possession readiness alone does not grant its rewards; an explicit qualified turn-in is required.

`CompleteMission` advances the NPC chain before mission-state update/removal/rewards, queues Complete plots afterward, and presents ending dialogue or requests `NextQuest`.9082's rewards therefore precede its4121 ending page; acknowledging that page grants nothing again. Completion plots use `Story_Info_Complete=3`, not the in-progress enum value2. The generated metadata and method guards pin those distinctions.

## Native owner, queues and transactional boundary

`PeAdventureMissionEvents` retains offered/active/completed masks, opening position, target readiness, the separate acceptance-grant latch, ending-pending state, queued NextQuest and plot5-pending state. Plot processing is an explicit command, not a side effect of merely accepting9079 or completing its travel target. Processing pending plot5 can accept9080 and grant its acceptance items while9079 remains active. Repeated plot notifications do not repeat grants; a restored pending plot with an already active/completed9080 is cleared without another batch.

`PlayerMission.m_GetRewards` is serialized independently in the original; it is not derivable from an empty bag or active quest alone. Native snapshot restore validates and copies its corresponding latch with no callback or item replay. A snapshot with active9080 but no required acceptance-grant latch is rejected. This is a bounded value-owner contract, not a PECS extension or Unity save importer. It does not guess which items were already spent.

Side effects are returned as typed, source-ordered batches: accept, Get plot, acceptance items; or chain advance, completion, required removal, completion items, Complete plot, ending/NextQuest. An injected callback must preflight **the whole batch**, including item/catalog/capacity/current-possession and NPC-chain requirements, then commit atomically or return false without side effects. Reducer state and output remain unchanged on rejection, so retry is safe. The returned batch is an observation of the completed transaction and must never be executed twice. Atomic batching is a deliberate native safety policy; the original active calls can mutate before a later failure.

Eligibility checks source prerequisite/mutex state only. The caller still owns actual NPC offers, choice-shell availability, current inventory readiness, qualified town/colony use and target geometry. This kernel cannot establish those facts by setting a Boolean. Source skill-tree rewards, general chain selection, repeated refusal conversations and externally requested9114 remain unported; the corresponding raw source fields remain available for later work.

## Verification

Sixteen guarded method/enum bodies pin admission, opening trigger dispatch, active acceptance/completion ordering, auto-reply, plot processing and separately recorded grant ownership. Source quest/target/plot/dialogue-role rows are also hashed and retained. Negative Python tests alter next-quest edges, grant quantities, Get versus Complete plot, required removal, target completion policy, dialogue roles, source grant guards, recorded latch output and final-page trigger selection; regeneration must reject those changes.

```sh
python3 rebuild/tools/export_adventure_mission_events.py --check
python3 rebuild/tests/test_adventure_mission_events_export.py
make -C rebuild adventure-mission-events-test
```

These are headless source-contract and synthetic transaction checks. No live mission, inventory grant, new quest UI, NPC behavior, assembly recognition or playable Town Defense is claimed. The existing gameplay quest boundary remains unchanged.
