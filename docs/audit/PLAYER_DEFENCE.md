# Source player defence capsules

The female player's repository prefab contains **16 enabled defence capsules**. They now have a separate compatible native actor and PEHB file, driven by all ten recovered source clips. This recovers the authored defence geometry for future retaliation work; it does not implement enemy attacks or establish that these exact component settings match the installed game's live player.

## Reproduce the export

After recovering the female starter rig and a complete isolated bake:

```sh
/tmp/pe-asset-tools/bin/python rebuild/tools/export_player_defence.py \
  "$HOME/.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl"
/tmp/pe-asset-tools/bin/python rebuild/tests/test_player_defence.py
```

The optional dependency is UnityPy 1.25.4. Output stays under `rebuild/build/player-defence`: `player-defence.peac`, `player-defence.pehb`, and `player-defence.json`. Arguments allow a different closed capture, source rig, source prefab and previously exported actor directory. Outputs are staged before replacement; the existing player scene and original assets are unchanged. Generated binaries are local, ignored recovery artifacts.

The manifest records hashes for the source prefab, rig, closed bake, original first-part actor and capsule/defence source classes, exact source IDs and bone paths, original settings, inverse-bind provenance, native skeleton fingerprint and per-clip point errors.

## Source settings and binding

The source is [FemaleModel.prefab](../../Assets/Resources/Prefab/PlayerPrefab/FemaleModel.prefab), `PEDefenceTrigger` component **11488762** on GameObject `Defence`, layer **25**. Its model root is Transform **400170**, and center bone **400264** is `Bip01 Spine2`. All capsule pointers are nonnull and resolve beneath that model hierarchy. Every part has defence type **Flesh (0)** and defence material **0**.

| Part | Radius | Height | Axis | Damage multiplier |
|---|---:|---:|---|---:|
| Head | 0.10 | 0.22 | −X | 2.0 |
| Left/right upper arm | 0.03 | 0.27 | −X | 0.8 |
| Left/right forearm | 0.03 | 0.27 | −X | 0.5 |
| Left/right hand | 0.03 | 0.10 | −X | 0.2 |
| Left/right thigh | 0.07 | 0.43 | −X | 0.9 |
| Left/right calf | 0.05 | 0.46 | −X | 0.9 |
| Left/right foot | 0.05 | 0.25 | +Y | 0.2 |
| Chest | 0.13 | 0.28 | +Z | 1.0 |
| Waist | 0.10 | 0.24 | +Z | 1.0 |
| Belly | 0.11 | 0.30 | +Z | 1.0 |

Values are authored bone-local lengths. The exported JSON retains all offsets, including head/feet/torso offsets. As in [PECapsuleTrigger.cs](../../Assets/Scripts/Attack/PECapsuleTrigger.cs), endpoints are `offset + radius × axis` and `offset + (height − radius) × axis`. Radius follows the transformed local-X radius vector, irrespective of capsule axis. No capsule is replaced by the movement controller's body or inferred from mesh bounds.

The existing first rendered actor contains only the torso's **17 joints**. Its skin cannot represent leg defence. The auxiliary actor therefore preserves that exact source torso vertex/index payload and the original joint order, then appends six source joints: left/right thigh, calf and foot. Their inverse binds come from source mesh **6078**, `Model/PlayerModel/female01-legs`. The resulting **23-joint PEAC2** has a distinct skeleton fingerprint. Its matching PEHB must be loaded with `player-defence.peac`, not with the original torso actor. Existing C actor/hitbox APIs need no format change.

Every capsule's full source bone path must match the Avatar-mapped bake path. Bind-space endpoints are calculated using the inverse of the selected source inverse bind, then driven by the same sampled `boneWorld × inverseBind` palette as the native skin. The extra joints are used for defence geometry; original torso vertex weights and indices are unchanged.

## Validation and limits

Four tests pass, including exact path rejection, preservation of original skin indices, source capsule counts/multipliers, repeatable output and native loading under AddressSanitizer/UndefinedBehaviorSanitizer. The native test poses **16 × 345 capsules** across NormalIdle, NormalWF, NormalRF, Jump, Fall, Land and Attack6–9. It also verifies that the first rendered torso cannot incorrectly accept this PEHB.

Serialized PEAC/PEHB point reconstruction is independently compared with direct bone-local capsule evaluation for every source sample. With capture 02, maximum point error is **1.87e-7 m**. The first six clips contain 62/34/22/31/22/34 samples; Attack6–9 contain 34/35/40/31. All matrices are checked for finite, invertible values; radius must remain positive. These are headless geometry checks. No graphical hitbox overlay or original incoming hit has been validated here.

Player defaults in repository `localData.bytes`, `initProp` numeric id 1, are DefaultPlayerID **1**, CampID **1**, DamageID **1**, and HP/HpMax **500**. These defaults do not alone authorize damage: [SkExpFuncSet.cs](../../Assets/Scripts/SkillSystem/SkExpFuncSet.cs) additionally checks reputation permission, faction conflict and `DamageData.GetValue`. Live tutorial overrides have not been sampled. Trigger activation, collision layers, attacker eligibility, per-skill deduplication, source swept-history checks and actual health changes remain separate host responsibilities.

The capsule settings come from the repository prefab, while body geometry/Avatar and sampled poses come from the installed original. This provenance split is explicit. No runtime defence settings were observed. Clip-only sampled poses omit Animator transitions, procedural IK, runtime morphs and root displacement; between samples the native actor uses affine palette interpolation. Capsule recovery does not remove the [bake's lifetime and root-motion limitations](ANIMATOR_BAKE.md), and it does not generalize female data to a male player.
