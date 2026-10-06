# Town Defense9077 observation contract

October6,2026. **Offline-qualified tooling, not a live observation.** The separate [managed probe](../../rebuild/tools/ReferenceTownDefenseProbe.cs) and [launcher](../../rebuild/tools/reference_town_defense.py) compile against the installed Unity5.2.4 assemblies. Fifteen [offline tests](../../rebuild/tests/test_reference_town_defense.py) pass, including incomplete captures, changed membership, missing species, late observations, malformed values and launch defaults. No original process was launched during this work, and native defense remains disabled. The [defense audit](ADVENTURE_TOWN_DEFENSE.md) owns source metadata, archived center/protectedNPC, schedule and implementation blockers.

## Read-only boundary

The launcher reuses the existing opt-in main-thread shim in [reference_pose](../../rebuild/tools/reference_pose.py); default invocation only compiles into `rebuild/build/town-defense-probe`. Explicit `--launch` starts a separate original client using the isolated profile and normal Steam initialization. It never attaches to the current game. Only the designated desktop operator loads saves, arms, moves, clicks Ready or quits.

The probe creates one hidden observation component and samples at `WaitForEndOfFrame`. It reads the existing9077/7507 mission point, its `_entityBcn` and `_towerData`, then the beacon's `_agents`, `_idxWave`, `_preTime`, `_position`, `_forward` and `_uiData`. Exact private field names/types were checked against installed Mono metadata and are checked again before arming; the offline inventory is `rebuild/build/town-defense-probe/installed-fields.txt`. Source [`EntityMonsterBeacon`](../../Assets/Scripts/Scene/EntityMonsterBeacon.cs) and [`SceneEntityMissionPointTowerDefence`](../../Assets/Scripts/Scene/SceneEntityMissionPointTowerDefense.cs) explain these fields.

Protected NPC IDs come from the actual tower data, not an invented fixed population. Entity lookup reads the already-existing `PeSingleton<EntityMgr>.instance` backing field and its dictionary. It deliberately avoids `EntityMgr.Instance`, whose getter can construct/register gameplay state. Null/missing entities remain null. The source's simple tower UI getters are read only; no property setters, event subscriptions, RNG, actor creation, input, health/stat changes, skill calls, animation APIs or save operations occur. The observer does affect process allocation/performance; an instrumented launch is not an ordinary uninstrumented playtest.

Each frame records:

- scaled and real clocks, Unity frame number, mission presence and literal `TDMONS0`;
- player position/rotation/scale and existing HP/maxHP/attack/defense/camp/damage/default-player attributes;
- beacon ID, wave index, preparation clock, center/forward, and existing displayed kill goal/count/waves;
- protected NPC identity and the same existing entity snapshot, including HP;
- each actual beacon member's source-list order, agent ID, encoded prototype, creation state, anchor, scale/rotation and riding flag, separately from the resolved entity's species, moving pose and attributes.

`agentPosition` is the agent anchor, possibly still carrying deferredY until resolution; it is never replaced by the moving entity position. The encoded agent prototype is not the resolved species ID. The quest counter is never treated as an actor ID. No killer identity or exact damage cause is measured.

## Bounds and capture lifecycle

Fresh JSONL and `.arm`, `.stop`, `.launch.json`, `.exit.json` paths are required inside the isolated profile. Bounds are1–3600 sampled frames,1–120 real capture seconds,1–1800 seconds waiting to arm,64 actors/NPCs,64KiB per frame and16MiB total. The writer reserves space for closure. A stop file, elapsed limit or frame/byte bound closes output and destroys only the observation component. End-of-frame scheduling means paused/unfocused clients can delay checks and cleanup; the operator remains responsible for ordinary UI shutdown. The launcher records process return code separately from probe closure; neither implies the other succeeded.

The header includes the probe-source hash; the launch manifest hashes the managed probe, shim and installed engine/game assemblies, records the command/profile and process-local Mesa environment when present. Full captures and managed binaries remain ignored local artifacts. Reports are derived evidence, not installed C cohorts.

## Operator commands

From the repository root, offline only:

```sh
python3 rebuild/tools/reference_town_defense.py
python3 rebuild/tests/test_reference_town_defense.py
```

Before a supervised launch, preserve/check the existing manual archive hashes, especially manual8 (`PlanetExplorers/GameSave/save7`) and manual9 (`save8`), against their [active](../reference/town-defense-original-active.json) and [complete](../reference/town-defense-original-complete.json) reports. Keep normal saves and all existing isolated checkpoints unchanged. The observer does not write saves; the original can still run normal autosave behavior. Do not overwrite a checkpoint to test partial progress: use a fresh manual slot deliberately if that later observation is authorized.

With no competing desktop operator or original process, an explicit launch is:

```sh
env __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 \
  python3 rebuild/tools/reference_town_defense.py --launch \
  --single-threaded-rendering \
  --user-data /home/admin/.local/share/planet-explorers-hunt-reference \
  --output /home/admin/.local/share/planet-explorers-hunt-reference/pose-captures/town-defense-01.jsonl \
  --frames 3600 --seconds 120 --wait-seconds 900
```

Choose another fresh output name if any matching capture/sidecar exists. The Mesa variables apply only to this process tree and follow the successful live03 observation; they are not a proven general crash fix. Software rendering can make120 wallclock seconds cover fewer scaled gameplay seconds. There is no promise that both waves will fit before closure.

Load preserved manual8 through the original UI. Arm promptly once loading finishes and before the first wave, using the **printed path**; `.arm` replaces `.jsonl`:

```sh
touch /home/admin/.local/share/planet-explorers-hunt-reference/pose-captures/town-defense-01.arm
```

Let ordinary simulation advance. Ready input is optional and must be recorded if used; it cannot skip the ten-second inter-wave cooldown. Do not manipulate the player's position, HP, NPCs, RNG or mission state to manufacture evidence. The first wave is8–9 hares plus2 rhinos; the second is8–9 wolves plus2 rhinos. Capture all actual members, including creation gaps and deaths; do not force the goal15 into a cohort size.

To stop before its limit:

```sh
touch /home/admin/.local/share/planet-explorers-hunt-reference/pose-captures/town-defense-01.stop
```

After a recorded clean probe closure, analyze:

```sh
python3 rebuild/tools/analyze_reference_town_defense.py \
  /home/admin/.local/share/planet-explorers-hunt-reference/pose-captures/town-defense-01.jsonl \
  --require-waves --output rebuild/build/town-defense-probe/observed-waves.json
```

Omit `--require-waves` to retain a valid negative/partial observation report. An error closure, crash-truncated file, nonfinite geometry, duplicate identity, invalid source order or contradictory membership is rejected. Confirm the original remains responsive, quit through its UI, preserve its final Player.log and exit sidecar, and compare manual hashes again. No zero process-exit status or unchanged-archive claim follows merely from analyzer success.

## Analyzer interpretation

The [analyzer](../../rebuild/tools/analyze_reference_town_defense.py) requires a closed, source-hashed bounded recording. It separates beacon epochs on absence, changed ID, wave rollback or shrinking membership. Numerical IDs may repeat across reloads/processes; they are not global keys.

Source appends each complete wave synchronously before incrementing `_idxWave`. An observed index1 list therefore supplies the first-wave boundary; later appended members at index2 belong to the second wave. A recording starting at index2 cannot recover that boundary and leaves membership unknown. Wave qualification requires both lists in one epoch, resolved monster species matching both source groups, and all20–22 members represented. It never invents an omitted member or infers one from kill counts.

The report preserves the first observed resolved entity for each member, plus phase/change snapshots. That first pose may already have damage: `--require-waves` establishes complete **membership**, not full-HP initial spawns, exact creation timing, permissions parity or safe native support. Original source anchors remain separate from moving poses. Missing NPC snapshots, timer intervals or death attribution remain missing. Archived mission completion and prior preparation reload are independent evidence; this tool does not itself certify partial-progress reload semantics.

## Next creature-pose gate

The installed wolf7/rhino4 bundles and live mesh branches are inventoried in [species metadata](../reference/town-defense-species-assets.json). All wolf clips and rhino idle/attacks contain streamed curves rejected by the existing actor exporter. The player Animator sampler explicitly requires a humanoid Avatar. Neither restriction should be removed globally to make these assets appear supported.

Prepare a separate generic-creature skeleton sampler or a validated streamed-curve decoder. Verify installed generic Avatar/controller binding offline before launch; construct only a pure transform/Animator clone, without live AI, physics or skill components. Wolf requires an auxiliary transform for its pounce shape outside the skin palette; rhino requires four preserved submeshes/materials. Recover actual pose clocks, attack/Defence shapes, sensors and source skill guides before using them in native combat. Ground collision must support their source horizontal capsules. Capturing cohort anchors alone does not solve these requirements, NPC participation/failure, save policy or the source annulus extending beyond the installed route.
