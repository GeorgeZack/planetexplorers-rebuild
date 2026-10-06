# Starter Wood Sword and Wood Shield: source and implementation status

October6, 2026. The initial read-only audit following [hunt difficulty](HUNT_DIFFICULTY.md) and [starter outfit](STARTER_OUTFIT.md) has progressed to source metadata, independent shield ownership with PECSv15 persistence, bounded headless combat support, a successful ten-leaf Sword pose capture, and staged modern equipment assets. **The ordinary native Sword/Shield gameplay feature is still gated; these foundations are not installed presentation or physical combat parity.** Supplies9032 already grant both items. The work implements their source contracts rather than changing Knife or hare balance. [Pose staging](STARTER_SWORD_POSE_GROUP.md), [ownership/save](SHIELD_OWNERSHIP.md), and [masked-layer qualification](SHIELD_LAYER_CAPTURE.md) record the separate evidence.

## Exact equipment and ownership

| Property | Wood Sword1 | Wood Shield34 |
| --- | --- | --- |
| Modern resource | `Prefab/Item/Equip/Weapon/Sword/wood_sword` | `Prefab/Item/Equip/Armor/Shield/wood_shield` |
| Source class | `PeSword` | `PESheild` (original spelling) |
| Equip mask / replacement mask |16 /0|512 /0|
| Item function/components |5, `EquipAttack`|18, `EquipDefence`|
| Item attack / defence |10 /0|0 /9|
| Equipment buff |30200087|30200093|
| Full durability / wear factor |2369 /5|4312 /.1|
| ShieldMeleeProtect |0|.22|

These are `PrototypeItem`, `item_class` and `skBuff` rows, not the legacy `playerstatic` balance table. Player `initProp` row1 has base attack10, defence0, SwordMultiple1 and ShieldMultiple1. Thus intact sword attack is20 before other modifiers; intact shield adds9 defence, independently of whether block is held. Together with intact starter clothing, the bounded initial defence would be21. This calculation does not establish a matched physical hunt outcome.

[`EquipmentCmpt`](../../Assets/Scripts/PeEntity/Cmpt/EquipmentCmpt.cs) replaces equipment whose actual masks intersect. Sword16 and shield512 coexist; replacing the right-hand Knife is independent of equipping the shield. The native foundation now uses a separate `shield_instance_id` reference alongside the primary reference, while the existing inventory slot remains the sole item/durability owner. A matching quickslot may reference that same bag instance; duplicate bag identities or aliases with primary equipment, outfit or deployed ownership are rejected. Atomic equip/unequip works with a full bag because this native representation changes references, not item capacity. PECSv15 retains the literal v14 payload and appends only the4-byte shield reference; older saves infer no selected shield. This representation is not proof of source equipment-receiver transfer behavior. Neither selection nor drawing clones an instance or restores durability. See [the exact ownership contract](SHIELD_OWNERSHIP.md).

[`ItemObj.EquipAttack/EquipDefence`](../../Assets/Scripts/Items/ItemObj.cs) and [`SkAliveEntity.OnHurtSb/OnGetHurt`](../../Assets/Scripts/SkillSystem/SkAliveEntity.cs) establish wear. Sword wear5 is dispatched on a successful damage notification through `Battle_EquipAttack`, not once per swing or once per animation frame. Multiple damaged targets can therefore cause multiple wear events. Shield defence wear is `max(0, resolver_damage*.1)` for each received hit, including damage exceeding remaining HP, through the existing all-equipped-item path. Merely holding block does not debit durability. Exhaustion removes the item's property buff; Build bypasses equipment wear. Do not charge the shield twice through both outfit and held-equipment paths.

## Modern visual binding and collision

The repository modern prefabs are [wood_sword](../../Assets/Resources/Prefab/Item/Equip/Weapon/Sword/wood_sword.prefab) and [wood_shield](../../Assets/Resources/Prefab/Item/Equip/Armor/Shield/wood_shield.prefab). Installed `mainData` ResourceManager resolves these same resources into `PE_Client_Data/resources.assets`:

| Installed object | Sword | Shield |
| --- | ---: | ---: |
| Root GameObject / Transform |20555 /124967|18653 /123070|
| Script component |PeSword324284|PESheild323303|
| Mesh / material |6335 /855|6440 /769|
| Mesh child / Transform |23465 /127879|20517 /124930|

The standalone installed `AssetBundles/Item/wood_sword.unity3d` contains legacy `MeleeWeapon` data with old skill IDs; the corresponding shield bundle uses legacy `Equipment`. Their names alone do not qualify them as the modern equipped prefab. Recover from the verified ResourceManager binding and preserve its hierarchy/material transforms.

Sword source `m_HandChangeAttr` binds **mountMain when held, mountBack when stowed**, with `SwordPutOn`/`SwordPutOff`, active action13, inactive16 and Sword move style1. The source `m_RemoveEndAction` includes SwordAttack14. Its root/model/blade transforms are identity. Shield [`PESheild.InitEquipment`](../../Assets/Scripts/Items/PEEquipment/PESheild.cs) binds `Bone L Forearm`; there is no separate shield-back stow in this class. Visibility follows `hideEquipInactive`. Source female Model hierarchy provides mountMain400208 beneath right hand, mountBack400268 beneath Spine3, and Bone L Forearm400258 beneath left forearm. Ragdoll duplicates are not the rendered Model chain. The new `export_starter_pair_assets.py` resolves the exact main/back/animated-forearm chains rather than assuming the older three-mount helper covers them. The resulting source-bound rigid scenes remain staged; see [modern attachment recovery](STARTER_SWORD_POSE_GROUP.md).

Shield mesh child local position is `(-.191,-.052,.006)`, quaternion approximately `(-3.248e-8,.9799247384,-.1993679404,-1.5965e-7)`, scale1. Preserve that transform and the forearm mount's own rotation; neither an identity forearm mount nor a screen-space offset is equivalent.

Sword installed PEAttackTrigger344244 on `SingleHandSword` Transform126042 matches the source trigger11412292/Transform400004 exactly: one form0 capsule, radius `.20000000298`, height `1.79999995232`, local offset `(0,0,-.10000000149)`, axis2/Z, layer mask34154496. Its broadphase BoxCollider300648 has size `(.6,.6,2.5)`, center `(0,0,1)`, trigger=true, initially disabled. The capsule, broadphase and event gate have different roles; neither mesh length nor AI range is the contact shape. Pose both blade and player defence from the same authoritative action interval after movement.

**The modern shield hierarchy has no Collider or PEDefenceTrigger.** Source shield mitigation is an attribute/action rule on an eligible player hit, not a shield-mesh collision test. Do not invent a physical blocking disk/capsule.

## Attack clocks and block rules

Sword's serialized PeSword attack skills are stationary20110011, moving20110014, sprint20110013, air20110012 and water20110005. It uses stamina5 per accepted stage and base animation speed **1.0**, compared with Knife1.37. Source [`Action_Sword.UpdateAnimSpeed`](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs) multiplies by.9 at stamina≤50 and again at≤20, clamps [.5,1.5], and also contains the separately unported hit-responsive animation curve. Reuse the source event schedule, not the Knife playback constant. Its sole AI attack-mode row has range0–1.8, switch range0–3, ±30° and frequency.5; these fields do not replace event timing or define shield protection.

Female third-person stationary leaves remain `FullBody.Attack.Attack6..9`, IDs231/240/247/254. Their captured body poses and authored event clocks are reusable because the modern sword references the same skill family. The headless `pe_combat_item_animation_speed` now selects exact Knife33 base1.37 or Wood Sword1 base1.0 with the source low-stamina reductions. `pe_sandbox_melee` binds its setup and admission gate to the exact supported item and equipped/drawn instance; Sword tests exercise its own clock and attack20 rather than merely widening an ID check. Running408 poses and the qualified authored root curve exist, with explicit running opt-in still required. The ordinary sandbox host remains on its qualified Knife setup: Per-hit Sword wear and optional item-specific actor selection are now implemented and tested, but asset installation and normal host authorization remain gated. See [wear](SWORD_WEAR.md) and [current actor groundwork](STARTER_SWORD_POSE_GROUP.md). Air, sprint, water and first-person attacks remain unsupported branches.

| Clip | Authored duration | Contact start → end | Action end |
| --- | ---: | --- | ---: |
| Attack6 /231 |1.0666667223|.0687236562 → .2994630337|.7091259360|
| Attack7 /240 |1.1000002623|.1731357425 → .4647010565|.8369432092|
| Attack8 /247 |1.2666668892|.1389591545 → .4254856706|.8732097149|
| Attack9 /254 |.9666671753|.0385129340 → .2535538673|.5349509716|

These are authored seconds, not wall-clock durations or full Unity Animator-transition recovery. Preserve existing combo windows, two-update contact minimum and end guard; do not multiply the event times by the weapon rate twice.

[`PeInputSettings`](../../Assets/Scripts/Input/PeInputSettings.cs) maps default Block to held **LeftControl**; [`MainPlayerCmpt.UpdateOtherAction`](../../Assets/Scripts/PeEntity/Cmpt/MainPlayerCmpt.cs) passes this to `Motion_Equip.HoldSheild`. [`Action_SheildHold`](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sheild.cs) sets Animator bool `SheildBlock`, HoldShield mask and EnableShieldBlock35=1; it mounts buff30200177. Releasing/ending clears these and cancels the buff. No stamina debit or stamina admission threshold exists in this hold action. Without EquipmentHold, blocking selects Sword movement style and restores base style when ended.

Source ActionRelation row14 immediately ends action15 when a sword attack begins. Row15 requires SwordAttack=false and Sprint=false, so blocking cannot begin during either. Equipped coexistence is therefore not simultaneous attack/block action eligibility. A continuously held key can request block again after the attack ends. UI consumption, death, load, unequip and action cancellation must clear transient blocking; input must not leave a saved permanent protection flag.

Buff30200093 adds itemDef×ShieldMultiple to defence27 and itemShieldMeleeProtect to attribute33. Hare trigger10100 applies `attack²/(defence+attack) * (EnableShieldBlock==1 ? 1-ShieldMeleeProtect : 1)` before crit and collision scaling. Its `colinfo` condition only requires a collision target; inspected `SkCond.TstColInfo` and `SkExpFuncSet.TryGetHurt` add no shield-facing check. For this exact hare path, intact held shield contributes a `.78` factor to an otherwise admitted hit. This is not authority to reduce every damage source globally. The `<30°` condition in [`Action_Beat`](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Beat.cs) selects block repulse reaction (`SheildBlockF`), not the damage reduction cone. Buff30200177 sets reaction thresholds70..73 to250/300/9999/9999; source hurt/stagger parity remains a separate meaningful part of the pair.

## Recovered assets and remaining gameplay gates

The installed `FemaleAnimator2`779 in sharedassets1.assets contains:

| Role | State / source leaf | ID | Duration / loop |
| --- | --- | ---: | --- |
| Ground sword idle/walk/run | Sword style leaves AttackIdle/SwordWF/SwordRF |90/133/435|1.7333335876 /1.0999984741 /.6666679382, loop|
| Draw | UpbodyOnly.SwordPutOn, InWater0 |270|.4666709900, no loop|
| Stow | UpbodyOnly.SwordPutOff, InWater0 |275|.4000015259, no loop|
| Normal-style block | LArmOnly.SheildBlock, MoveStyle0 |420 NormalSheildBlock|2.0000038147, loop|
| Sword-style block | LArmOnly.SheildBlock, MoveStyle1 |413 SwordSheildBlock|1.6666717529, loop|

UpbodyOnly draw/stow state speed is1.2000000477; their authored PutOnEquipment/PutOffEquipment events occur at.1631838381/.2606496215. FullBody.Guard also references270/275 at speed1, so the actual action/layer path must be retained, not guessed from a clip name. `LArmOnly` is layer3, override mode0, default weight1 with body mask words `(0,41947128,1)`. It blends block leaves on MoveStyle0/1 and enters/exits on the `SheildBlock` bool. Entry transition duration.0999999642 and exit.1020938531 are **normalized**, not fixed seconds. A standalone unmasked clip bake does not reproduce this layered result.

The local capture inventory now contains qualified Attack6–9 (female-clips06), Normal directional movement (female-directional01), RunSwordAttack408 (female-running-pose01), tools, and **304 poses across the ten exact Sword leaves in female-sword-01**. The [capture record](../reference/starter-sword-bake-20261006.json) preserves source/hash,42-bone validation,120-update cleanup survival and the original Quit/Yes exit−9. [Torch Sword recovery](TORCH_SWORD_RECOVERY.md) records the completed preset and separate Torch integration limits. The four draw/block clips above remain outside the isolated sampler allowlist; standalone clip sampling still does not establish masked block over simultaneous locomotion.

The [unified03 stage](STARTER_SWORD_POSE_GROUP.md) contains33 body clips, matching33-clock modern Wood Sword drawn/stowed and Wood Shield forearm scenes, the exact Sword attack capsule and a single42-clip defence actor. It preserves the existing21 body/30 defence prefixes, including tool clips, and all installed groups remain untouched. Backward SwordWB441/SwordRB708 are captured and qualified; no additional backward capture is required. Shared third-person turn leaves can be reused only under the verified source branch conditions. Draw/stow, masked block, procedural IK and full directional blending remain unqualified.

The headless hunt combat adapter now accepts an optional validated shield slot and transient block request, stages shield wear atomically with outfit/HP/combat state, applies passive9 defence and conditional `.78` hare mitigation while intact, and recomputes protection after each accepted hit. Active and ending blade intervals suppress the block request. Tests cover real posed incoming contacts, broken/absent shields, same-tick breakage, overkill wear and late rollback. This optional core boundary does not activate LeftControl, equip UI, shield visuals, source block reactions or general damage mitigation in the ordinary host.

The remaining integration gates are:

1. Preserve the qualified staged33/42 group, including backward Sword leaves, and install only with a coherent supported host. Any additional exposed branch needs its own source contract; do not remove retained tool defence clips to fit an older clip limit.
2. Qualify the actual masked left-arm block over Sword idle/walk/run, release, draw/stow and block→attack→held-block restoration. [The layer audit](SHIELD_LAYER_CAPTURE.md) preserves exact source masks/events and the safety restriction against attaching callback-bearing original controllers to pure clones. The initial187-frame llvmpipe observation had zero block frames; the subsequent user-operated3600-frame Zink capture observes451 steady Sword block frames over genuine mixed idle/walk/run poses. A subsequent three-point callback-free controller replay matches actual states, clocks, parameters and blend weights exactly, with documented residual bone differences. Neither result is an authored masked runtime loop.
3. Integrate qualified body/attachment/defence snapshots and exact equipment identity into the ordinary host, including successful-hit Sword wear, transient block admission/cancellation and explicit unsupported-branch gates. Normal item34 UI use remains disabled until visible presentation is qualified. Source block reactions/hit-responsive animation behavior remain separate gaps.
4. Test original and native with the same visible pair/outfit and ordinary equip/draw/hold/attack/release inputs. Record incoming damage, wear, pose/reaction differences and save/reload independently. Formulas and headless asset/contact tests do not prove a successful or equivalent hunt.

Current `pe_actor` whole-pose blending is not evidence of Unity's masked humanoid layer semantics. Both original and native sessions are closed, and the user owns the desktop. Further qualification can be prepared offline; launching again requires coordinated access.

## Input provenance and checks

The initial read-only UnityPy1.25.4 inspection verified ResourceManager identities, modern source/installed blade capsule equality, source mount hierarchies, controller states/clip identities and local capture names. Scratch inventory is ignored under `rebuild/build/starter-pair-{modern,pose}.json`; no source/runtime binaries were modified. That initial inspection ran no graphics or runtime suite; subsequent capture, headless asset and ownership/combat evidence is separated above and in the linked audits.

- `Assets/Resources/DataBase/localData.bytes`: `c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376`.
- Installed `PE_Client_Data/resources.assets`: `363d02dc06aa3cd5ecccbeb67ac417077e0552cb32d0173205e6950ba93a3cb5`.
- Installed `PE_Client_Data/sharedassets1.assets`: `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`.

## Source metadata foundation

The metadata increment added [export_starter_sword_shield.py](../../rebuild/tools/export_starter_sword_shield.py), generated [C contracts](../../rebuild/generated/pe_starter_sword_shield_source.h) and [source provenance](../../rebuild/generated/pe_starter_sword_shield_source.json). The metadata exporter itself enables neither item and produces no meshes/poses; the later staged visual exporter and runtime foundations are separate. The header exposes separate `pe_starter_sword_source`, `pe_starter_shield_source` and `pe_starter_sword_capsule_source` constants, with no runtime equipment or vector-type dependency.

Generation guards the exact item/class/buff rows; source action IDs and attribute indices; modern prefab script GUID bindings; held/back/forearm contracts; sword skill IDs, rate, stamina, capsule and broadphase; shield collider absence; attack/block exclusion; conditional hare damage expression; and successful-hit/received-damage wear, exhaustion and Build bypass. JSON retains full original item rows, relevant skill/action/buff rows, source component IDs and hashes for the database and18 source/prefab/meta inputs. It deliberately supplies no default block pose, invented defence collider or generic damage multiplier.

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_starter_sword_shield.py
rebuild/build/unity-tools/bin/python rebuild/tools/export_starter_sword_shield.py --check
rebuild/build/unity-tools/bin/python rebuild/tools/export_starter_sword_shield.py --check \
  --game "$HOME/.local/share/Steam/steamapps/common/Planet Explorers"
TMPDIR="$PWD/rebuild/build/compiler-tmp" \
  rebuild/build/unity-tools/bin/python rebuild/tests/test_starter_sword_shield_export.py
```

`--game` additionally verifies modern ResourceManager PeSword/PESheild bindings, installed sword capsule/broadphase and shield collider absence. It never loads a same-named legacy Item bundle as the modern equipment. This check is bounded: it does not assert that every serialized installed PeSword field or the full live Animator has been independently decoded. Generated metadata remains deterministic from repository inputs and contains no Steam installation paths. `--check` compares both files without rewriting either.

All15 focused tests passed with UnityPy and zero skips; these include temporary-copy mutations of item stats/slots/models, buffs, action masks, source wear and hold methods, script GUIDs, capsule transforms and invented shield colliders, plus deterministic regeneration, unchanged input hashes and a strict standalone C11 header compile/run. The installed-binding `--check` also passed. Without optional UnityPy, eight source/database tests run and seven prefab/generated-output tests explicitly skip; those skips are not asset-validation passes. No broader runtime or physical claim follows from those metadata-only checks. Sword locomotion capture has since completed; the backward branches are also qualified. Masked shield loop coverage and ordinary host integration remain the current gates.

The dedicated check is `make -C rebuild starter-equipment-check ASSET_PYTHON=/path/to/UnityPy/python`; it requires prefab validation to pass. The ordinary `make test` also runs this test file, with seven explicit optional skips when UnityPy is unavailable. The earlier full outfit test run had21 skips before this metadata suite was added; the new suite separately passed all15 checks with UnityPy.
