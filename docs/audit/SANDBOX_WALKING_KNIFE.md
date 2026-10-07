# Grounded walking Knife attacks

October6,2026. Source-derived ordinary root motion is now integrated into Adventure's existing female third-person blade path. No original or native graphics session was run. This closes the walking-admission implementation gap identified by [the source-first review](SOURCE_FIRST_REVIEW.md); ordinary four-hare completion and physical partial-progress reload remain unverified.

## Source extraction

[The root-channel proof](PLAYER_ROOT_CHANNELS.md) follows the pinned original's current/previous RootT sampling, orientation cancellation, horizontal subtraction, interval conversion, human scale and actor rotation through the public Animator deltaPosition getter. Ten pure tests and twelve isolated call-free native arithmetic cases support that bounded chain. It applies only to the exact common importer flags, FemaleAvatar777 and the selected unblended clips; it is not a full humanoid solver or controller transition implementation.

[export_sword_root_motion.py](../../rebuild/tools/export_sword_root_motion.py) requires that source proof to pass, then exports raw dense XZ knots for Attack6–9, source231/240/247/254. It retains the negative first-knot time for231 and subtracts interpolated authored time0 per clip. It does not manufacture60Hz samples or label source knots as engine observations. Human scale0.9932681322097778 is applied once; rootY and root rotation do not move the player.

The new2392-byte PESR1 group has SHA256`f0d8b2171c75b1098123e482115d5d1a576a309fc494807076e3d4330661f3fb`. Export/check uses the final proof and source hashes, stored in the adjacent ignored manifest. The tested group is installed at `rebuild/build/reference-assets/player-sword-motion/sword-root.pesr`. Existing captured408 PERM1 data and all pose groups are unchanged. Recovered motion assets are not committed.

```sh
rebuild/build/unity-tools/bin/python rebuild/tools/export_sword_root_motion.py \
  --output rebuild/build/FRESH_SWORD_MOTION
rebuild/build/unity-tools/bin/python rebuild/tools/export_sword_root_motion.py \
  --output rebuild/build/reference-assets/player-sword-motion --check
```

## Runtime behavior and ownership

[pe_sword_root_motion](../../rebuild/src/pe_sword_root_motion.h) owns one immutable four-curve allocation and validates the complete format, exact clip metadata and bounded finite knots. Exporter provenance checking is separate from runtime structural validation. The host also requires matching named body, weapon and incoming-defence clocks before enabling the group. Missing ordinary assets retain the previous explicit stationary/zero-root adapter; a present malformed or incompatible group fails initialization.

Ordinary attacks can now begin on supported ground at full XYZ speed≤4.5, including while holding forward, side or backward movement. The existing source selector still chooses running above4.5 through7 and rejects unsupported sprint, air/water and first-person branches. Holding movement or the attack's own root velocity does not reclassify/cancel an admitted ordinary action. Item identity, stamina debit, modal ownership and cancellation remain authoritative. This does not open the separately gated Wood Sword/Shield presentation or change the tutorial host.

The host advances combat once, retains its outgoing interval, derives one root request and sweeps it through shared collision before blade/defence contact. Ordinary stages use231/240/247/254; running stage0 retains captured408, and its later stages use240/247/254. A combo resetting its new clock to0 cannot discard or replay the outgoing displacement. Source event time ends the motion before the full clip's unused tail. Movement input, sprint and jump cannot add a second displacement beneath the action. Blocked distance is discarded rather than accumulated.

Motion curves belong to the host, not PeGame. PECSv16 is unchanged. Loading cancels transient action/interval state and rebinds the existing qualified templates; curve owners survive until host cleanup. Damage, creature HP, clothing wear and hunt rewards were not tuned.

## Validation and remaining limits

- Focused curve-reader tests pass optimized and ASan/UBSan with leak detection, including malformed/truncated files, negative-begin interpolation, exact metadata, terminal bounds, ownership and failure atomicity.
- [Walking integration](../../rebuild/tests/test_sword_walking.c) passes optimized and ASan/UBSan/leak checks with actual curves/body/blade/hare assets and no skips. It covers forward/back/side and threshold admission, held-input ownership, root-produced high speed, all outgoing combo intervals, one movement clock, wall collision/no deferred movement and a constructed post-sweep blade contact. This is arranged geometry, not ordinary play.
- [Actual host dispatch](../../rebuild/tests/test_sandbox_sword_motion_host.c) passes optimized and ASan/UBSan/leak checks with no skips, using both installed and explicit paths. It tests ordinary versus captured running dispatch, retained real combo intervals, no query clock mutation, invalid input and48 clip-clock comparisons across nine body parts, two Knife parts and defence. A deliberately missing path returns77.
- Four exporter tests and ten source-decoder/proof tests pass; exact regeneration passes. Build and existing policy/consumable/contact checks are recorded in [PLAYTEST](../PLAYTEST.md). These are focused checks, not a new full-core/graphics pass.

Reproduce integration with `make -C rebuild sword-walking-test sandbox-sword-motion-host-test`; `PE_SWORD_ROOT_MOTION` selects a fresh staged group. Missing-asset skips do not prove integration. Logs are under `rebuild/build/sword-root-check`, `sword-walking` and `sword-motion-host-check`.

The native fixed-step clock, double interpolation, source-frame timing adapter, swept collision and immediate action transitions remain explicit limits. Original Animator blending/target-facing logic, bit-identical root arithmetic and source PhysX motor behavior are not claimed. The source proof does not substitute for a graphical walk→attack/combo/wall check or earned hunt progress/reload. No desktop input was taken for this increment.
