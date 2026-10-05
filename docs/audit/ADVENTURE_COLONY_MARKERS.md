# Adventure colony candidates and Annot destination

This October 5 audit is **source-derived and read from the preserved original archive**. No original client or C graphics session was run. It does not establish that three colony markers were visible in the original. The source has three generated empty towns, but the inspected map paths explicitly exclude them. **Annot is the name of town4, not an NPC.** The active supply mission has a recovered destination at source `(-284,25,134)`.

## Welcome side effects and mission boundaries

The read-only database is [localData.bytes](../../Assets/Resources/DataBase/localData.bytes), tables `AdRandomQuest_List`, `AdvPlot`, `AdQuest_EnterArea`, `AdRandNPCMission`, `Scenario` and `Translation`.

| Event | Database relation | Source effect |
| --- | --- | --- |
| Accept9028 after its opening4082 | `AdvPlot=1_1` | Queue plot1: create Adventure roles2 and5 at player-relative radii10m and15m. |
| Acknowledge4088 / accept9031 | Arrival target4502 | Enables the existing arrival-to-supplier mission; no separate three-marker plot is declared. |
| Complete9031 | `AdvPlot=3_3` | Queue plot3: create roles3 and4 around town4 at radii10m and20m. |
| Accept9032 after final4094 | Target4503, `AdvPlot=0` | Grant the source supplies and generate the town4 discovery destination. |

`Story_Info` is Get=1, Ing=2, Complete=3, Fail=4 in [Mission.cs](../../Assets/Scripts/Mission/Mission.cs). [AdRandomMission.cs](../../Assets/Scripts/Mission/AdRandomMission.cs) parses the `event_plot` pairs. [PlayerMission.cs](../../Assets/Scripts/Mission/PlayerMission.cs), `GetMission` and `CompleteMission`, dispatch those events. [UINPCTalk.cs](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINPCTalk.cs) attaches mission triggers to dialogue and calls `SetGetTakeMission` on advancing their final page.

[StroyManager.cs](../../Assets/Scripts/Mission/StroyManager.cs), `PushAdStoryList`/`UpdateAdvPlot`, queues and consumes each plot. Creation uses `Random.insideUnitCircle.normalized * radius`, adds the XZ offset to the reference position, obtains initial Y from `VFDataRTGen.GetPosTop`, then assigns the NPC fixed point from the town-aware terrain height. This is a point on the radius, not uniform sampling inside a disk. Source terrain support remains necessary.

Exact rows:

```text
AdvPlot1 CreNPC=1_0_10_2;1_0_15_5
AdvPlot3 CreNPC=2_4_10_3;2_4_20_4
```

Here `ReferToType.Player=1`, `Town=2`, `Npc=3` in [StoryRepository.cs](../../Assets/Scripts/Mission/StoryRepository.cs). Neither plot contains marker creation. Plot2 installs a town4 proximity trigger for plot3, but9027–9032 do not select plot2; the active chain directly selects plot3 on9031 completion.

## Three colony candidates: geometry exists, marker behavior is unverified

Westina4088 (`Translation3004088`) says three areas have been plotted for the player's colony. This text alone does not establish a UI marker or a marker activation event.

[VArtifactTownManager.cs](../../Assets/Scripts/PuJi/Town/VArtifactTownManager.cs), `GenTown`/`GenEmptyTown`, starts the town counter at0, creates the starting settlement, then creates exactly three successful empty towns before the next populated town. Their IDs are1,2,3; the next populated town is4. Each candidate is a level0 template with `townNameId=0`, player alliance, `isEmpty=true`, `isMainTown=false`, artifact units and terrain integration. Empty towns skip `RenderTown`'s buildings/NPCs. They are reserved generated terrain, not three arbitrary local points.

The implementation has a significant detail: `GenCampByZone` first samples a proposed point using the configured empty-town distance range and checks its map bounds. The actual town position then comes from a separate `VATownGenerator.GenTownPos(genAreaId,myRand)` call. Do not claim the final centers obey the nominal radial range. [TownGenData.cs](../../Assets/Scripts/PuJi/Town/Data/TownGenData.cs) supplies count3 and small/middle/large nominal ranges300–800/500–1000/1000–1500. The source PRNG, terrain eligibility and consumed random sequence matter for reproduction.

In [RandomMapIconMgr.cs](../../Assets/Scripts/PuJi/Town/Tool/RandomMapIconMgr.cs), `AddTownIcon`, `AddNativeIcon` and `AddDestroyedTownIcon` all return immediately for `vat.isEmpty`. The unknown-town discovery loops in `VArtifactTownManager` also exclude empty towns. `VATownGenerator.GetEmptyTown` has no call site in the inspected C# source. `ColonyLabel` is a player-built colony/assembly label, created by colony management paths; it is not evidence for preexisting candidate-site icons.

**Result:** no source-backed instruction to add three colony-site map markers has been recovered. Do not place three invented markers on the bounded map or use `ColonyLabel`/PlayerBase merely because the dialogue mentions sites. Full candidate terrain recovery and any installed-client difference from this source remain separate work.

## Annot is town4 and target4503 is its fixed discovery anchor

The source strings are `Translation3102566 = Go to "Town%"004` and3102567's corresponding delayed-supplies description. Name parsing uses `VArtifactUtil.GetTownName(4)`. “Annot” is the resolved name in the preserved reference context. There is no Annot NPC role.

```text
AdQuest_EnterArea4503:
  DistPos=2_4_0_15
  TrackRadius=60
  NotForDungeon=1
  NpcRadius=0, Prompt=0, TalkID=0, ReceiveQuest=0, NpcNum=0
```

`PlayerMission.ProcessAdRandomMission`'s Discovery branch resolves Town4 through [VArtifactUtil.GetTownPos](../../Assets/Scripts/PuJi/Town/Tool/VArtifactUtil.cs), uses zero radial offset, truncates X/Z to integers and queries terrain height. [VArtifactTown.TransPos](../../Assets/Scripts/PuJi/Town/Model/VArtifactTown.cs) is `(PosCenter.x,height+2,PosCenter.y)`, but target Y is separately sampled terrain Y. Do not equate the saved destination Y with town height+2.

[MissionManager.cs](../../Assets/Scripts/Mission/MissionScript/MissionManager.cs), `UpdateSearchMission`, rejects this Adventure target while playerY<0 and tests strict XZ distance<15m for the random mission. The target must not already be complete, and the scene must match. `TrackRadius=60` is map tracking radius, not completion distance. The Discovery UI path uses `m_DistPos` plus that tracking radius. The `9032 ? 0 : MissionID` building-position expressions elsewhere in this file are in **Follow** code; target4503 is **Discovery**, so those expressions are not its destination.

Roles3/4 at town4 are RandomNpc24/25, recovered as Naoko Ueda and Sajiva Victory. The supply quest's town destination is not either actor's position and does not move with them.

## Saved reference evidence

Input: `~/.local/share/planet-explorers-sandbox-reference/PlanetExplorers/GameSave/save0` (preserved manual slot1). Read-only decoding used [read_reference_adventure.py](../../rebuild/tools/read_reference_adventure.py)'s `Reader`, `file_bytes` and root `records` helpers. `MainAdventure/index.arc` entries have `yird=true`, which root `records` deliberately rejects; an inline read-only index traversal validated GUID, bounded ranges and lengths against `MainAdventure/world.arc`. No parser/tool behavior was changed for this audit.

| Record | Recovered fields | Consequence |
| --- | --- | --- |
| Root `ArchiveKeyMisRepository` | Completed9027–9031; active9032 `STEP=0`; role map1→20008,2→20016,5→20017,3→20018,4→20019 | Confirms this archive is after Arrival and supply acceptance. |
| Root `ArchiveKeyMisRepository` | `mID_buildPos[0]=(-599.7844848632812,28.5,401.1063537597656)` | Starting building front door, not4503 or a colony site. |
| MainAdventure `ArchiveKeyAdRMRepository` | One mission9032, target4503, type4, present1, `m_DistPos=(-284,25,134)` | Exact persistent town4 discovery coordinate. |
| Root `ArchiveKeyCreature` | Westina20008 `(-606,27.50515365600586,349)`; Eitai20016 `(-598.5214233398438,22.978425979614258,341.3612365722656)` | Saved actor positions. |
| Root `ArchiveKeyCreature` | Naoko20018 `(-277.114990234375,26.5,126.74765014648438)`; Sajiva20019 `(-297.37042236328125,26.5,148.87387084960938)` | Saved town4 actor positions; do not substitute for the target. |
| Root `VArtifactTownArchiveMgr` | Version20160902; town0 state record; town0 alive buildings20001,20003–20007 | This68-byte record saves state/building entity IDs, not generated town centers or empty-site coordinates. |
| MainAdventure `PeMap.StaticPoint+Mgr`, `ArchiveKeyMapUserLabel`, `PeMap.TowerMark+Mgr` | Each is four bytes, integer0 | No saved static/user/tower marker entries in these records. Does not prove absence of every transient map label. |

Serialization owners are [PeArchiveMgrs.cs](../../Assets/PeLauncher/PeArchiveMgrs.cs), `AdRMRepositoryArchiveMgr`/`RandomMapConfigArchiveMgr`; [AdRandomMission.cs](../../Assets/Scripts/Mission/AdRandomMission.cs), `Export`/`Import`; [PlayerMission.cs](../../Assets/Scripts/Mission/PlayerMission.cs), `Export`/`Import`; and `VArtifactTownManager.Export`/`Import`. Generated town geometry/centers are reconstructed from map configuration and source generators, rather than stored in the town-state record. The saved configuration includes seed and generation settings; this audit has not reconstructed candidate1–3 centers.

Input SHA256:

```text
save0/index.arc              939a80153919330eb873d555f944e70194c864ca7b7dcb7a0642efbadfe72cca
save0/world.arc              74e301a0744ee2e19cf93f2366b046a01f1413fd4be94664573d51d6765d9220
MainAdventure/index.arc     149860b7eaab236bd016b6b7a37242cdc10055f8aad60e5da6f70acf956b65ae
MainAdventure/world.arc     dcc22436b7c0652efb6174f9b135e2455a9ad11fdf3e1b1be51b7564cc3bfcf6
localData.bytes             c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376
```

## Bounded native recommendation

1. Recover the starting settlement at one consistent translation/scale with its authored structures and existing role1/role2 appearances. Keep the original coordinates/provenance alongside local coordinates. Correct documentation that describes Annot as an NPC.
2. Preserve the source4503 destination `(-284,25,134)` and15m completion/60m tracking semantics in exported metadata. Only expose an actionable destination when supported terrain, collision and actual travel include that transformed coordinate. A compressed nearby marker would invent the travel objective. A wider overview may truthfully show the recovered remote town destination if clearly outside the playable crop.
3. Treat the three colony candidates as unresolved generated terrain. Their IDs/count and generation process are established; their saved centers and marker visibility are not. Do not derive them from the two plot3 NPC positions, the start-building front door or an evenly spaced local triangle.
4. Reproduce only supported plot effects with appropriate event/idempotence ownership. Source role2/5 creation belongs after4082 acceptance; role3/4 creation belongs after Arrival completion. The existing always-present bounded NPCs are an explicit native limitation, not proof those plots already run.

Verification here is database/source cross-checking, bounded archive reads and local Markdown link checks. It is not a C implementation or a graphical pass. Existing [welcome](ADVENTURE_WELCOME.md) and [NPC](ADVENTURE_NPC.md) contracts remain the native behavior until their implementations are changed.

## Reproducible bounded settlement export

[export_adventure_settlement.py](../../rebuild/tools/export_adventure_settlement.py) now emits [pe_adventure_settlement.h](../../rebuild/generated/pe_adventure_settlement.h) and its [JSON provenance manifest](../../rebuild/generated/pe_adventure_settlement.json). The exporter checks both archive indexes/GUIDs/ranges, component versions and finite transforms, saved role/prototype bindings, source XML allocation and database identities. Its subworld reader validates every indexed file, including unused terrain records. Input hashes are checked again before returning the result.

All six buildings agree with Town_artifacts11's authored slots under one zero-rotation lot origin `(-623,16,327)`. The origin is derived from fixed shop slot0, then independently checked against the other five archived positions and rotations. Dimensions `55,100,14` are source X/Z/Y order, not XYZ. Exact archived quaternions are preserved; the manifest also records authored yaw and each Townhouse resource/size.

| Entity | Doodad prototype | Townhouse | Slot | Source yaw |
| --- | --- | --- | --- | --- |
| 20001 | 163 | 21 shop | 0 | 270° |
| 20003 | 164 | 22 notice board | 1 | 180° |
| 20004 | 162 | 20 hotel | 2 | 170° |
| 20005 | 161 | 19 dwelling | 3 | 250° |
| 20006 | 145 | 63 repair machine | 4 | 180° |
| 20007 | 146 | 64 solar plant | 5 | 180° |

The **source-derived initial player spawn** is `(-606,28,348)`: source `InitTownHeight` adds `(0,1,-1)` to lot-local `NPC_born[0]=(17,11,22)`. This is separate from the archived `Pathea.MainPlayer` saved pose, which reflects later player movement. Both are exported, as are role1/2 saved poses and the start-building front door. The header is data only; it neither reconstructs terrain nor adds markers.

Six focused [Python tests](../../rebuild/tests/test_adventure_settlement_export.py) pass against synthetic bounded archives and the repository source data, including malformed/truncated/GUID-mismatched subworlds, nonfinite poses, slot/role disagreements, changed XML/database schema, target type/trailing data and deterministic read-only output. The actual preserved archive also passes regeneration checking:

```sh
python3 rebuild/tools/export_adventure_settlement.py --check
python3 -m unittest discover -s rebuild/tests -p test_adventure_settlement_export.py -v
```
