# Ordinary native hunt acceptance — October 7, 2026

The C rebuild now has observed ordinary-input coverage of an earned first kill, partial-progress save/load, four total kills, encounter cleanup, return travel, one-time rewards and resumed ending dialogue. This closes the remaining ordinary hunt gate in [the acceptance plan](HUNT_ACCEPTANCE.md). It does not establish original-game difficulty or animation parity.

## Execution and provenance

The unmodified production runtime was commit `2ea0f288013836c95a4b07d2e718bef1cb5eea42`, executable SHA256 `824cc5a82f7a05e900b490e6a9175368a1e99647982a88b166e2c5196a53c88a`. Two dedicated1280×720 Xvfb sessions used software rendering and ordinary OS keyboard/mouse input. Neither the user's desktop nor the Steam original was controlled. Commands used only `--sandbox --load COPY --save COPY` and the existing read-only combat trace; no smoke mode, changed stats, teleports, arranged enemies or edited quest counters were used.

Session02 began from the preserved earned v13 checkpoint whose SHA256 is `2b7f763da6bd453da8fb8102a914983e1913c625211803f5151648120ee0ca6f`. The ordinary current loader applied its documented older-save migrations. The player had Knife33, starter clothing, eight Medpacks, ten Juices and ten Revival Shots. The agent traveled from Annot to the accepted hunt, using WASD, sprint, captured third person, fresh left-button attack pulses and ordinary consumables/revivals. Session03 continued from session02's earned one-kill save, with the same production binary. Reload intentionally recreates the pinned full-HP cohort while retaining quest kills; surviving individual injuries are not saved.

The [checkpoint manifest](../reference/hunt-native-20261007/checkpoints.json) includes hashes and current-loader numeric inspections. [Session02 inputs](../reference/hunt-native-20261007/session02-inputs.jsonl), [session02 trace](../reference/hunt-native-20261007/session02-native.log), [session03 inputs](../reference/hunt-native-20261007/session03-inputs.jsonl) and [session03 trace](../reference/hunt-native-20261007/session03-native.log) preserve the sequence, including failed attempts and ordinary reloads. Runtime trace frame numbers restart by host and its positions are world-local; source positions in the inspector include crop origin(-640,0,64). Saves and recovered assets remain ignored local files. A local video covers only an earlier562.4-second portion of session02, not the completed kill sequence; no continuous-video claim is made.

## Observed results

| Gate | Actual result |
|---|---|
| Earned first kill | Session02 frame35693, contact73: actor2146483648 HP14.073910→0, quest0→1. |
| Partial save/load | Pause→Save, Pause→Load, then Save while still paused. Both files have SHA256 `222bd1add75a11731c449ea08879a6ab0fa1cee767b0038054e32e7fae4a2057`; complete schema2 inspections match, including1kill,223.993683HP, inventory/quickslots, clothing identity/wear and clocks. [Loaded UI](../reference/hunt-native-20261007/partial-loaded.png) reports “Game loaded” and1/4. |
| Remaining earned kills | Session03 resumes that checkpoint. Its combat trace advances1→2→3→4 through actual blade contacts; final frame2881/contact89 kills actor2146483651. Death/revival occurs during the run. |
| Cleanup and return objective | The fourth-kill event clears all six presence flags, including surviving positive-HP actors. [Rendered result](../reference/hunt-native-20261007/four-kills.png) shows4/4 and return coordinates. Saved state3 has4kills,256.5HP, four Medpacks, six Juices, six Revival Shots, and no reward scripts yet. |
| Ordinary return and reward order | Sprint/walk reaches Naoko's return radius from source(-394,245). [First reply](../reference/hunt-native-20261007/reward-before-ack.png) appears with the reward receipt message before acknowledgment. Number1 advances4105→4106. |
| Pending dialogue reload | Escape closes4106; a second Escape opens Pause. Save records state5, pending4106 and exactly one each of592–595. Load→Resume [reopens4106](../reference/hunt-native-20261007/resumed-4106.png). |
| Final acknowledgment | Number1 completes reporting. Save records state6,4kills, no pending reply and still exactly one each of592–595. Its SHA256 is `9a28960b3894bab1b89c1128b78772d98ce49ebac5b98b3fec1b9e8a9e49e0d2`. A subsequent isolated normal launch displays completed Bug Hunt with Town Defense unavailable. |

F9 during an open ending dialogue did not save; the resulting local file named `reward-4105.pesave` is merely the earlier four-kill snapshot and is **not reward evidence**. The table uses the later confirmed Pause save. Save/load of the large world takes several seconds; file copies were accepted only after the actual atomic-save timestamp changed. Early inputs during startup/loading are not evidence of successful UI actions.

## Failures and remaining limits

Early attempts died or reloaded without kills. A captured-camera absolute pointer warp also changed facing; future attacks use nonwarping button input. After revival, the blade is stowed and requires separate draw/attack presses. The death/get-up modal owns number2 as Load; do not assume it means healing until the modal closes. Successful revival restores166.5HP/full stamina, consumes a shot and retains current injuries, but preserves medicine cooldowns. Pausing does not advance these clocks. Retained trace clip/stage values after action cancellation do not prove a frozen active animation.

The session required consumables, revival and repeated attempts. It is a functional acceptance result, not evidence that the six-hare balance, AI, shield support, attack blending or camera presentation match the original. Walking/running visual-quality gates remain separate. General Town Defense9077 is still disabled.

## Test tooling

The save inspector now scans all1680 Adventure slots instead of the legacy first32, and reports category and player orientation/motion. Two formatter regressions cover categorized and Legacy layouts. Current-loader checks on the preserved real checkpoint find22 occupied slots,13 outside the old range, without changing its hash.

The [private-input supervisor](NATIVE_PRIVATE_INPUT.md) adds guarded normal-game launch/input, immutable baseline copies, bounded sessions and input journals. Independent review fixed HUP/orphan cleanup and watchdog publication/completion races. All19 pure/subprocess helper tests pass; no runtime combat code or save format changed in this tooling increment. Session03 qualified its ordinary input path before the final watchdog race fixes; a fresh final-helper session loaded the completed checkpoint, accepted Pause/menu input, captured the owned display and stopped with baseline unchanged. Explicit stops terminate the private processes; these runs do not claim clean application-quit exits. The final helper's process-loss/watchdog cases are covered by its dedicated tests.
