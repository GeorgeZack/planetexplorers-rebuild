# Archived Adventure population

This bounded recovery reads the preserved manual slot1 (`GameSave/save0`) without modifying it. The identities, modular appearance, equipment, saved transforms and gender below are **archive/source-derived**. Exported actors are native asset implementations; exporting them does not establish original AI behavior or a graphical verification.

[Population exporter](../../rebuild/tools/export_adventure_population.py) emits [C metadata](../../rebuild/generated/pe_adventure_population.h) and [JSON provenance](../../rebuild/generated/pe_adventure_population.json). It pins the original13 RandomNpc identities and appends twelve observed Annot residents from a separate save, verifies the saved five-role map against the database, validates finite normalized poses, and requires positive serialized HP. Source file hashes are checked before and after recovery. Database role candidates remain provenance rather than invented mission assignments.

| Entity | RandomNpc | Name | Saved sex | Bound role |
| --- | --- | --- | --- | --- |
| 20002 | 141 | Cherish Mercer | Female | Weapon shop prototype; NPCstore28 |
| 20008 | 17 | Westina Nangle | Female | Adventure1 |
| 20009 | 101 | Dikeledi Sehgal | Female | No saved plot role |
| 20010 | 106 | Jaden Okeke | Male | No saved plot role |
| 20011 | 151 | Virginia Cannan | Female | Supply shop prototype; NPCstore38 |
| 20012 | 111 | Kiri Abraham | Female | No saved plot role |
| 20013 | 126 | Archibald Sanchez | Male | No saved plot role |
| 20014 | 146 | Andre Dale | Male | Armor shop prototype; NPCstore33 |
| 20015 | 136 | Jailekha Eric | Female | No saved plot role |
| 20016 | 22 | Eitai Takayama | Female | Adventure2 |
| 20017 | 23 | Winnie Jakande | Female | Adventure5 |
| 20018 | 24 | Naoko Ueda | Male | Adventure3, Annot |
| 20019 | 25 | Sajiva Victory | Male | Adventure4, Annot |

The original saved town0/role3/4 roster above is retained exactly. A separate original route session loaded Annot and saved manual slot3 (`planet-explorers-route-reference/PlanetExplorers/GameSave/save2`), adding these twelve living residents:

| Entity | RandomNpc | Name | Saved sex |
| --- | --- | --- | --- |
| 20027 | 123 | Terry Sibu | Male |
| 20028 | 153 | Xing Zhang | Male |
| 20029 | 103 | Qiang Zhang | Male |
| 20030 | 113 | Maddison Atanasof | Female |
| 20031 | 158 | Sherley Hansen | Female |
| 20032 | 163 | Abe Okorie | Male |
| 20033 | 143 | Ntombifikile Petros | Female |
| 20034 | 133 | Abhati Msm | Female |
| 20035 | 173 | Aatmaja Olsen | Female |
| 20036 | 108 | Aadrika Stewart | Female |
| 20037 | 148 | Tammana Chu | Female |
| 20038 | 118 | Neil Nguyen | Male |

The extension validates their saved `NpcCmpt.FixedPointPos` against the live captured town4 bounds X[-331,-237], Z[87,181]. It preserves their actual saved transforms and appearance, while retaining the original13 snapshot poses despite later wandering or streaming resets. Fresh entity20020 Nnyadzeni Obi is outside those bounds and the route, at source(-711,24.498039,535), and is explicitly excluded. The completed route capture records town0 **Haleiwa**, nameID8500180, and town4 **Annot**, nameID8500008; `town-names.json`/`town-name.txt` retain capture hash provenance. Names were observed, not inferred from NPC names or source dialogue placeholders.

The population module owns appearances and role/placement metadata. [Town interactions](ADVENTURE_TOWN_INTERACTIONS.md) separately integrates the original17 Town0 offers plus267 Annot offers across eight shops, discovery and supported follow-up quests. General NPC storage/mission-chain simulation is not implied by appearance recovery.

The generated metadata preserves the nonbreaking space within Dikeledi's archived family name. Names do not determine sex. Annot's two lazily loaded `AvatarCmpt` records have empty prefab strings but complete saved nude `MaleModel` selections, all27 morph weights and four colors, corroborated by `CommonCmpt.sex=2`. The recovery uses those records, never a female template or tutorial NPC appearance.

Jailekha's saved position is `(-588.5618896484375,24.232267379760742,286.7139892578125)`, south of the original starting crop. Her metadata and appearance are retained; placement must not relocate her into that crop. Annot actor positions remain separate from discovery4503's destination.

## Appearance and animation

The shared [supplier recovery](../../rebuild/tools/export_adventure_supplier.py) now accepts the saved male or female base model, original skin weights, signed morph channels, material masks/colors and equipment slot replacements. Rigid tools use each source prefab's stowed `m_PutOffBone`, validated against the corresponding MaleModel/FemaleModel hierarchy: swords3/5/shovel12 `mountBack`, axe28/knife33 `mountWaist`, bows43/45 `Bow`, handgun54 `Pistol`, rifle55 `Long_Gun`; shields35/36/38 use source `Bone L Forearm`. Geometry and material identities are retained per person. Source `IgnoreCaseOfBoneName` binding maps male mesh `Bip01 mouth` onto Avatar `Bip01 Mouth`. Missing hair children (for example `m_hair_23/hair_0B`) follow `GetPartInfos`' null-Smr omission and are listed in rig provenance; no replacement hair is invented.

Female actors reuse the already validated FemaleAvatar NormalIdle capture. Male actors require a distinct MaleAvatar NormalIdle capture; dedicated tutorial NPC Avatars are rejected by the strict mapper. [Animator probe](../../rebuild/tools/reference_animator_bake.py) adds `--source-entity` to read a loaded entity through `EntityMgr.Instance.Get(int)` while retaining the strict gender/Avatar/controller/clip checks. It copies transforms into an isolated graph, records the selected source entity in launch and skeleton provenance, and does not modify the live actor or create a substitute player. Default capture still reads the main player.

Both sexes use source physical capsule center `(0,.875,0)`, radius`.37`, height`1.75`. Their Defence triggers differ: female radius`.5`, male radius`.6`, both centerY1/height2. [Collider exporter](../../rebuild/tools/export_adventure_npc_collider.py) preserves the selected prefab and component IDs. These are query shapes, not a reason to add player/NPC blockers; source layer10 self-collision remains disabled.

Artifacts publish to `rebuild/build/reference-assets/adventure-population/<entity>/recording.peas`, with individual source rig, bake report, collider and provenance manifest. Existing welcome/supplier scenes remain separately owned. The full metadata has25 actors, of which23 use the new population asset directory. Idle is a sampled source clip with approximate diffuse shading; controller transitions, IK, gestures, root motion, schedules, recruitment, wandering and combat are outside this recovery. Archived item ownership does not preserve transient held/stowed action visibility.

## Reproduction and checks

```sh
python3 rebuild/tools/export_adventure_population.py
python3 rebuild/tools/export_adventure_population.py --check
python3 -m unittest discover -s rebuild/tests -p test_adventure_population.py -v
python3 -m unittest discover -s rebuild/tests -p test_reference_animator_bake.py -v
```

`--assets NEW_DIRECTORY --entity ID` exports selected actors with the optional UnityPy/Pillow environment. Male exports additionally require `--male-bake CLOSED_CAPTURE.jsonl`. Existing actor directories are rejected; originals, saves and other recovered scenes remain intact. A full export omits the separately owned welcome/supplier actors. Pass the fresh manual-slot3 directory with `--save-dir` to reproduce the extended25-actor manifest. Subsequent default regeneration/`--check` replays the extension's recorded archive/capture paths; explicit original `--save-dir` selects only the baseline13. The optional original-archive test explicitly skips when the preserved save is unavailable; such a skip is not an archive validation pass.

Five population regressions pass here, including the actual13-actor archive, malformed identity/dead-actor rejection, lazy male prefab recovery, mismatched Common/equipment sex rejection, source case-insensitive mouth-bone binding, and deterministic read-only output. Existing supplier7/welcome4 and animator launcher14 regressions pass. Offline C# compilation passes with the installed Unity assemblies. The original MaleAvatar capture completed70 samples using entity20010; native male asset recovery uses those poses. Its separate exit record reports signal9, so closed capture completion does not establish normal process teardown. Graphical integration is recorded separately by the runtime integration work.

## Wider route buildings

[Route asset exporter](../../rebuild/tools/export_adventure_route_assets.py) consumes a fresh original save after Annot has loaded. It selects living building IDs for towns0/4 from `VArtifactTownArchiveMgr`, resolves each exact `EntityDoodad` pose and unique `PrototypeDoodad.asset_path` bundle cross-checked against all matching `Townhouse` rows, and verifies all six starting buildings against the untouched baseline save. A save without Annot buildings fails; no town geometry is guessed. New save GUIDs are intentionally separate (source `ArchiveMgr` allocates one for each save).

It reuses the settlement mesh/material/collider recovery at origin `(-640,0,64)` and publishes `route.obj`, `route-collision.obj`, `route-manifest.json` plus their material/textures. Existing route terrain is preserved; existing geometry publication is rejected except during read-only `--check`. Three regressions verify compact triangle attributes, town/identity bounds, duplicate and malformed records, and the original archive's inability to supply invented Annot buildings. The six Annot buildings are entities20021–20026, using source HumanBuilding03/04/05 bundles. All twelve route buildings have positive saved HP; source living/death branch selection remains explicit in the manifest. Exact town names come from the separate live route observation, not this archive's town-state record.

## Test-only graphical save fixtures

[adventure_town_fixture.c](../../rebuild/tests/adventure_town_fixture.c) and `make -C rebuild adventure-town-fixtures` create funded, collision-supported native v8 saves for physical UI testing. Both use public welcome acknowledgments, Arrival and supply transactions, then an explicitly test-only10000 money grant. They validate actual actor scenes, find supported camera-selectable poses, and verify native save/load identity without mutating normal player saves. The shop fixture faces Cherish; the Annot fixture faces Naoko with `annot_complete=false` saved, and verifies completion only on a separate reloaded copy. Annot requires actual route terrain and building collision. Shop mode can use the old settlement while the route remains incompletely installed.

The produced `rebuild/build/adventure-shop-fixture.pesave` and `adventure-annot-fixture.pesave` passed source placement/selection and v8 roundtrips in the complete route. This is headless fixture validation, not physical shop/dialogue UI verification. `adventure_town_fixture --inspect SAVE` reports money, stock, inventory and progression after the root operator's graphical checks.

The25 actual actor scenes all pass the production native scene loader, in optimized and ASan/UBSan/leak-check builds. Full-route placement surveys now pass25/25 in optimized and ASan/UBSan/leak-check builds, including Maddison20030 at exact source(-310.378540,19.944292,150.424744). These are headless placement/selection checks, not a graphics pass. Naoko settles vertically from saved26.5 to22.501038 and Sajiva from26.5 to21.513512 while preserving original XZ. The old64×32×128 crop covers10 of25; its other15 are explicitly outside that crop.

The initial24/25 survey failed at Maddison. Read-only collision diagnosis located the failure in the generic walkable support filter, before the NPC center-ray guard: her exact saved capsule is clear, its first downward terrain contact is only0.000040m below the saved position with normalY0.597871, and all permitted vertical lifts meet the same contact. No route-building collider is hit. The centerline terrain ray instead has normalY0.865137. This is not evidence for relocating her to `FixedPointPos(-301,26,158)` or changing building collision masks; that fixed point selects town membership only. The source-only NPC refresh now first tries ordinary settling, then retains exact existing/archived XYZ only when the complete capsule is clear, a downward real-geometry sweep finds upward normalY≥0.1 contact within0.06m, and its center sees real walkable normalY≥0.65 floor within0.15m. Synthetic boundaries, unsupported air and overlap cannot qualify. General player collision, movement and walkable support remain unchanged. Asset-independent raised-edge tests verify retention across collision updates and rejection of distant contact, overlap, missing center floor, crop-bottom support and invalid bounds/NaN.

The route visual writer retains only referenced vertices within each material submesh, preserving exact triangle positions, normals, UVs and winding. This reduces the route OBJ from206MiB to25.7MB while keeping138544 visual triangles and4343 collision triangles; collision/materials/textures are byte-identical to the initial export.
