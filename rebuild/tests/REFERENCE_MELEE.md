# Recorded-geometry melee fixture

`reference_melee_check.c` is headless native integration evidence, not an observed original hit or gameplay command. It combines supplied knife/hare PEAC/PEHB, PeCombat events and stationary contacts; it never launches graphics/input or moves geometry to force a hit.

```sh
make -C rebuild build/reference_melee_check
rebuild/build/reference_melee_check \
  --knife rebuild/build/pose-analysis/female-starter-knife-tp-attack6-v2/knife-recording-0.peac \
  --knife-hitboxes rebuild/build/pose-analysis/female-starter-knife-tp-attack6-v2/knife-recording-0.pehb \
  --target rebuild/build/reference-assets/hare.peac \
  --target-hitboxes rebuild/build/reference-assets/hare.pehb \
  --clock-map rebuild/tests/fixtures/reference_female_tp_attack6.pemc \
  --target-position 0 0 1 --target-yaw 180 --target-scale 1 \
  --target-clip idle --target-time 0 --gender female --view third --difficulty normal
```

Optional local recorded assets keep this outside default tests. Player root(0,0,0), yaw0/scale1; chosen hare(0,0,1), yaw180/idle, Normal scale1. This is controlled placement, not captured hare position. Geometry stays unchanged. Source stats: attack10+9=19, hare HP212×scale/defense29×scale; Easy halves hare attributes; scale allowed.8–1.3.

## Clock map

PEMC derives from `knife-reference-01.jsonl`, SHA256 `dc94b9c0ae1c2dffc99bffba2681c68b6ecf011c147bb8c3757d2de4bd3ba40e`, samples48–68, realtime origin1331.91162. Local report: `rebuild/build/pose-analysis/female-starter-tp-attack6-clock.json`.

Fitted entry is recording.15280761391068154/scaled1267.8645959386158. Subsequent rows use samples50–58; authored time is observed FullBody normalized time × Attack6 length1.06666672 (sample50 incoming, then current state). Scaled elapsed is not realtime×1.37; stable rate~1.370042/scaled second, transition rows retain measured clocks.

Format: `PEMC1`, then finite `recording_seconds authored_clip_seconds elapsed_scaled_seconds` triples. Initial clip/scaled clocks zero, recording strictly increasing, other clocks nondecreasing. Max1024 rows/256KiB. Each row is one explicit engine update, not invented60Hz subdivision; fitted first row precedes contact. Other recordings require their own map, never filename-specific offsets.

## Result and limits

Observed native fixture: one Head1 contact at recording.25/authored.106723234, scale2; damage15.041667, HP212→196.958328, matching19²/(29+19)×2. Eight closed-window checks, two separated misses and dedupe passed. Open windows also test a target100m aside; closed checks use fresh IDs. Immediate repeat must preserve HP/action/result; stationary guide permits at most one hit.

At target(0,0,10), zero contacts/HP212. Exit0 means hit and all invariants passed;2 means no contact, so contact-dependent proof unreached;1 means invalid input/invariant failure. Both placements passed ASan/UBSan/leak checks.

Original capture did not record trigger activation; source events supply windows. Pose resampling, captured transitions/IK, instantaneous rather than historical sweep, asserted eligibility and stationary controlled target limit this fixture. It proves neither full combat parity nor an original kill.
