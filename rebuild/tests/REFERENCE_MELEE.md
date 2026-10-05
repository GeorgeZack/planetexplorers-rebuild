# Optional recorded-geometry melee check

`reference_melee_check.c` is a headless integration fixture, not a gameplay command or evidence of an observed original-game hit. It combines an explicitly supplied recorded knife PEAC/PEHB, hare PEAC/PEHB, source `PeCombat` events and the stationary contact resolver. It never launches graphics, sends input, changes geometry, or dispatches a mission.

Build from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -Irebuild/src \
  rebuild/src/pe_actor.c rebuild/src/pe_hitbox.c rebuild/src/pe_combat.c \
  rebuild/src/pe_knife_contact.c rebuild/tests/reference_melee_check.c \
  -lm -o rebuild/build/reference_melee_check
```

`make -C rebuild build/reference_melee_check` builds the same fixture. It is separate from default tests because the recorded geometry is an ignored local artifact and requires prior original-runtime capture/export.

The verified controlled placement is:

```sh
rebuild/build/reference_melee_check \
  --knife rebuild/build/pose-analysis/female-starter-knife-tp-attack6-v2/knife-recording-0.peac \
  --knife-hitboxes rebuild/build/pose-analysis/female-starter-knife-tp-attack6-v2/knife-recording-0.pehb \
  --target rebuild/build/reference-assets/hare.peac \
  --target-hitboxes rebuild/build/reference-assets/hare.pehb \
  --clock-map rebuild/tests/fixtures/reference_female_tp_attack6.pemc \
  --target-position 0 0 1 --target-yaw 180 --target-scale 1 \
  --target-clip idle --target-time 0 --gender female --view third --difficulty normal
```

The player model root is `(0,0,0)`, yaw zero and scale one. The fixture's chosen hare root is `(0,0,1)`, facing the player at yaw 180, normal difficulty and scale one. This is a **controlled target placement**, not the original captured hare position. Hare geometry, its original idle pose and knife capsule geometry are unchanged. Effective baseline stats are sourced: knife/player attack `10+9=19`, hare HP `212*scale`, defense `29*scale`; Easy halves hare combat attributes. The fixture permits only the source hare scale interval .8–1.3.

The checked-in PEMC clock file contains facts derived from capture `knife-reference-01.jsonl`, SHA-256 `dc94b9c0ae1c2dffc99bffba2681c68b6ecf011c147bb8c3757d2de4bd3ba40e`. It matches the recording exported from capture samples 48–68, realtime origin `1331.91162`. The supporting generated report is `rebuild/build/pose-analysis/female-starter-tp-attack6-clock.json`:

- Its first synthetic row establishes the fitted zero-entry at recording second `.15280761391068154`, scaled game time `1267.8645959386158`.
- Subsequent rows use original samples 50–58. Recording seconds are original realtime minus the recording origin; authored clip seconds are **observed FullBody Animator normalized time times the actual Attack6 clip length `1.06666672`**, using sample50's incoming state and the current state thereafter.
- The third column is scaled game time since fitted entry. It deliberately does not multiply realtime by a constant 1.37; capture realtime and scaled-time steps differ.
- The stable measured TP clip rate was about `1.370042` per scaled game second. The last transition rows use their observed normalized clocks, not a constant-rate extrapolation.

PEMC format is `PEMC1` followed by three whitespace-separated finite values per row: `recording_seconds authored_clip_seconds elapsed_scaled_seconds`. The first clip/wall clocks must be zero. Recording time must increase strictly; clip and scaled elapsed time must not decrease. Maximum 1,024 rows and 256 KiB. Each row represents one explicit engine update for the event scheduler, not an invented fixed 60 Hz subdivision. The fitted first row is preparatory and precedes the first contact event. Other captures must supply their own matching map; the executable contains no special filename cases or implicit clock offsets.

Observed fixture result with the supplied map:

```text
CONTACT recording=0.25 authored=0.106723234 part=Head1 scale=2
 damage=15.041667 HP=212->196.958328
Reference melee: hits=1 closed=8 separated_misses=2 dedupe=1 result=PASS
```

The source formula is `19²/(29+19)*2` for that first head contact. Every open-window update also checks a separate target translated 100 metres to the side. Closed-window checks use a fresh target ID so per-target deduplication cannot conceal a bad activation gate. A repeated call immediately after the hit must leave HP, action and result unchanged. Full frame progression must yield at most one successful hit in the stationary guide.

Replacing only `--target-position 0 0 1` with `--target-position 0 0 10` gives zero contacts and unchanged HP212. Exit status **0** means a contact was reached and all closed-window/miss/dedup/HP checks passed; **2** means the geometry made no contact, so contact-dependent HP/dedup checks were not reached; **1** means invalid inputs or a failed invariant. A no-hit result is reported honestly rather than moving the hare automatically or expanding the weapon.

Both placements passed AddressSanitizer, UndefinedBehaviorSanitizer and leak checking. The actual capture did not record trigger activation; active windows here are reconstructed from source events. PEAC pose resampling, transition/IK state in captured transforms, instantaneous contact rather than source swept history, externally asserted eligibility, stationary target pose and controlled placement remain explicit limitations. This is integration evidence for the native geometry/event/damage pipeline, not complete combat parity or an original kill confirmation.
