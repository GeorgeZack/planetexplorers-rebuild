# Grounded sprint Knife integration

October 12, 2026. **Source-derived, implemented in C, with bounded native
observation.** Grounded female third-person Knife33 now selects
skill20110013/SprintSwordAttack401 above source speed7, followed by the existing
Attack7–9 continuations. This does not enable reactions, shield, get-up,
airborne, water, first-person or Wood Sword sprint attacks.

## Assets and ownership

[Root qualification](SPRINT_ROOT_CHANNELS.md) supplies the separate 952-byte,
56-knot PESR1 curve. `pe_sprint_root_motion` validates its complete profile,
interpolates in double, subtracts the start and applies humanScale/body yaw
once. The ordinary four-record source and captured408 formats remain separate.
`sandbox_melee_delta` uses the retained outgoing stage0 interval; continuations
use ordinary roots. No zero-motion substitution is permitted.

`export_sprint_player.py` selects only SprintSwordAttack from the complete
25-clip body/Knife drawn/stowed/defence stages and appends it to the retained
21/30 baseline. Existing payloads, textures, materials and hitboxes remain
literal. Installed body/Knife scenes now have22 clips and defence31, all with
matching nonlooping30Hz/56-frame/1.8000068664550781s terminal clocks. Present
partial/malformed groups fail startup; an absent optional group preserves
before-debit rejection. Only Knife33 receives `sprint_motion_ready` after
body, attachments, defence and distinct root validation.

Local reproducible stage: `rebuild/build/sprint-player-06`; immutable baseline:
`rebuild/build/sprint-base-01`; replaced complete installed folders:
`rebuild/build/sprint-install-backup-01`. They are ignored recovered assets.
The installer checks current baseline hashes and qualified root metadata,
prepares whole folders and restores previous folders on rename failure.
Never overwrite an old backup or hand-edit an exported actor.

```sh
python3 rebuild/tools/export_sprint_player.py \
  --base rebuild/build/sprint-base-01 --output rebuild/build/sprint-player-06 --check
```

An initial installation also requires `--install` with a fresh backup path;
it intentionally refuses to repeat over an already installed sprint group.
`check-sources` regenerates and compares the stage. Historical tool and sword
composition tests use the preserved21/30 inputs, keeping exact payload/clock
assertions instead of comparing old exports with the expanded live body.

## Integration checks and native observation

`make -C rebuild sprint-attack-test` exercises actual exported body, Knife
blade and16 defence capsules, both an open lane and a blocking wall. It checks
four stages/three handoffs, shared collision and post-move sampling, once-only
contact advancement, motor ordering and stamina. Results:89 ticks,88 blade
samples, open finalX36.6212 and wallX33.6999 fromX32. These are C fixture
outcomes, not original distances or earned combat. Combat costs20 stamina;
the first ordinary sprint movement tick additionally costs about0.0833.

The production sandbox ran on newly owned private display108 with a copied
existing completed Adventure checkpoint. Free third person and the existing
camera-facing-attacks preference ON were retained. Inputs: move backward,
draw Knife, then hold `Shift_L+s` for1s, pulse LMB for.12s, and retain keys for
1.2s. The tracked nonwarping `key-button` driver released all controls. A
readonly4s/15fps video and [retained frame](../reference/sprint-native-20261012.png)
show the swing/lunge; the production log reports one sprint start and zero
outgoing/incoming contacts. The baseline already had4/4 kills: none were
newly earned. [Bounded result](../reference/sprint-native-20261012.json) records
hashes and limits. Pause returned to the main menu (sandbox result2); explicit
supervisor stop then terminated the process with-15. Do not claim clean
process exit0. Baseline integrity remained true; no save was performed.
A prior private startup smoke on display107 exited0 with the staged assets.

Native motor two-tick scheduling, collision, dropped ending/trailing motion,
vertex blends and cancellation remain adapters. Source skill20110013's exact
interruption flags and complete Animator/PhysX transition behavior are not
qualified. This check does not establish damage attribution, difficulty,
reload/progression, original blend parity or user visual acceptance of sprint.
Broader combat acceptance remains outstanding. The user's “feels good!”
accepted the earlier one-click standing/walking recovery qualitatively.
