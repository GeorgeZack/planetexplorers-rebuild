# Original hunt return observer

The return probe observes the preserved original manual slot7 (`save6`) to compare9127 completion with the C implementation. The [archived return checkpoint](../reference/hunt-original-return.json) records active9127, target4525 at source(-266,25,136), reward definitions592–595×1 each, ending replies4105/4106 and follow-up9077. Those definitions alone do not establish when inventory changes relative to arrival or displayed replies. Physical arrival and ending replies were subsequently observed, but both bounded field captures ended before that arrival; see the evidence below.

## Read-only contract

[`ReferenceHuntReturnProbe.cs`](../../rebuild/tools/ReferenceHuntReturnProbe.cs) runs bounded end-of-frame samples on Unity's main thread. It reads existing player XYZ, mission9127/9077 active flags and completed counts, reward-item counts in player and mission packages separately, and the current NPC talk object's activity, index, page IDs, mission triggers and current indexed ID. It does not call gameplay input, spawning, RNG, mission completion, inventory modification or save APIs. It does not inspect Animator, renderer, textures, skills or GPU fields.

`MainPlayerCmpt.gMainPlayer`, `MissionManager.Instance` and `GameUI.Instance` return existing objects in the inspected source. `PlayerMission.HasMission` reads mission information; completed counts come directly from `m_MissionState`. The observer deliberately avoids `PlayerPackageCmpt.package`, whose getter creates a missing package. Instead, it reads the existing private `mPackage`, each package's `mSlotListArray`, and each slot list's `mSlots`. It counts existing `ItemObject.protoId`/`stackCount` values without prototype-database lookup. Unavailable or partially deserialized package storage remains null, not an invented zero inventory. The talk index is the existing `UINPCTalk.m_CurTalkIdx`; queued pages are distinct from the talk GameObject being active.

The four reflected private fields were verified against installed `Assembly-CSharp.dll` metadata. The observer compiled offline against the installed original assemblies. Eight analyzer tests pass, covering reward-before-final-page ordering, missing inventory, previously completed quests, invalid closure, invalid states, clock direction, hidden queued pages and separate mission inventory. These checks validate observation parsing independently of the two later live captures.

## Bounded workflow

Root remains the sole desktop operator. The wrapper defaults to offline compilation. `--launch` explicitly starts a separate isolated original process; it never attaches to an existing PID. Preserve manual slots5–7 and baseline archive hashes. Load manual7, arm before approaching the archived return target, hold each visible reply briefly, then stop after closing the ending conversation. Capture at most3600 frames or120 real seconds; preparation should occur before arming.

```sh
python3 rebuild/tools/reference_hunt_return.py
python3 rebuild/tools/reference_hunt_return.py \
  --user-data /home/admin/.local/share/planet-explorers-hunt-reference \
  --output /home/admin/.local/share/planet-explorers-hunt-reference/pose-captures/hunt-return-01.jsonl \
  --frames 3600 --seconds 120 --wait-seconds 1800 --launch
```

The launcher prints fresh `.arm` and `.stop` paths. Creating those files only controls observation; the operator supplies all movement and dialogue inputs. Raw capture output must be fresh and inside the isolated profile. Compile-only execution does not change profile preferences; explicit launch follows the existing [reference runtime](../REFERENCE_RUNTIME.md) isolated engine/profile behavior.

```sh
python3 rebuild/tools/analyze_reference_hunt_return.py \
  /home/admin/.local/share/planet-explorers-hunt-reference/pose-captures/hunt-return-01.jsonl \
  --output rebuild/build/hunt-return-01.report.json --require-return
python3 -m unittest discover -s rebuild/tests -p test_reference_hunt_return.py
```

The analyzer requires clean closure and validates bounded fields/clocks. It records the frames immediately before and after mission/inventory/talk changes, the first visible4105/4106, and XZ distance squared to the archived target. `--require-return` additionally requires active-to-completed9127 and gains of at least one each592–595 in the player package. It does not require talk frames so a partial but valid capture can report exactly what was sampled. Do not interpret a passed requirement as proof of final acknowledgement timing. Events within one sample interval cannot be ordered; observed inventory deltas need operator context to support reward attribution. All raw/source hashes are retained in the compact report, which refuses to overwrite an existing output.

## October6 captures and later physical arrival

`hunt-return-01.jsonl` closed cleanly at its120-second time limit after2528 frames. `hunt-return-02.jsonl` closed at the same time limit after2746 frames. Both contain active9127, player inventory counts592–595 all zero throughout, no observed completion increase and no active4105/4106. The [bounded negative reports](../reference/hunt-return-negative-observations.json) retain source/raw hashes, first/last inventory and actual field transitions. They are valid observations of the pre-arrival state; neither passes `--require-return` or establishes completion timing.

After the second capture had closed, the operator continued ordinary movement to displayed map position `P:-265,134`. The [arrival screenshot](../reference/hunt-return-original-arrived.png) shows Bug Hunt completion, Jet Pack Script×1 notification and the player's4105 text before acknowledgment. Subsequent clicks displayed [Naoko4106](../reference/hunt-return-original-4106.png) and [Sajiva's9077 opening](../reference/hunt-return-original-next.png). Those are bounded physical UI observations, separate from the expired field capture. They do not supply exact reward inventory counts, precise coordinates or between-frame ordering; other reward IDs and the quest linkage remain source/archive-derived. The original exited through Quit, and no manual save was written during this return test.

A future instrumented comparison should prepare nearer the target before arming, while remaining outside its strict discovery radius, then record arrival and both ending pages inside one capture window. No additional launch is needed merely to describe the evidence already obtained.
