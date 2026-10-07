# October 7 development-test crash review

The three user-supplied reports cover four assertion-triggered aborts during development, before the final checkpoints. Reviewed against systemd records, the retained development transcript, current source and newly executed focused tests at baseline `6a4b652d6308e3adb8e660d4d93484232655b6ca`. No game window or desktop input was used. The original reports and system-managed cores remain untouched.

| Test / incident (US/Central) | Finding | Current disposition |
|---|---|---|
| Camera environment, PID3304645, 00:04:09 | Exact-equality fixture used activity2 while the reference target was literal1.8f. Those are different float targets. | Fixture changed to activity1 at00:04:17; committed in `b8cbc9ae`. Current focused test passes. |
| Town Defense planner, PID3306478, 00:05:01 | Fixture required a southward50m sample from Z134 to lie below crop Z64. Its expected Z is84. | Current tests explicitly check Z84 and use a separate eastward80m out-of-crop sample. Committed in `b8cbc9ae`; current focused test passes. |
| Inventory discard, PID3328847 at00:15:43 and diagnostic rerun3330397 at00:16:18 | Fixture rejected `consume_actions=true` even though `consume_world=true` and all save/load/NPC/camera shortcuts were blocked. These ownership flags can coexist. | The helper retains the world/modal and shortcut assertions without forbidding additional action ownership. Committed in `18006848`; current focused test passes. |

## Camera boundary qualification

The retained execution transcript directly records the original activity2 fixture and its subsequent activity2→activity1 edit. The exact-equality assertion was retained, not relaxed. Source Follow computes its target using float `desired_distance * clamp(activity * .15f, .15f, 1)`, followed by range clamping. At activity2, desired distance6 produces `1.80000007` (`0x3fe66667`); literal1.8f is `1.79999995` (`0x3fe66666`). At activity1, the lower computed target clamps to that literal minimum.

A new isolated CPU probe reproducing the old fixture against current camera code finds the first difference on frame3: distance remains identical (`0x40bdabaf`), while velocities differ by one float step (`0xc030181b` versus `0xc030181c`). This explains why the old exact-equality expectation was invalid without changing correct source arithmetic. It is a current-code reproduction, not recovery of historical core locals: the historical executable's build ID differs from the rebuilt binary. Probe source/output remain ignored under `rebuild/build/crash-review-20261007`.

## Independent recheck and limits

Executed from the repository root with the documented compiler temporary directory:

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" make -s -C rebuild -j2 \
  camera-environment-test town-defense-spawn-test inventory-discard-test
```

All three exit0; output is retained at `rebuild/build/crash-reports-20261007-recheck.log`. No assertions were removed and no runtime code changed during this review. Earlier focused sanitizer results remain their separate recorded evidence; this review did not repeat the full suite or claim a new sanitizer run.

`coredumpctl info` confirms SIGABRT for the four incident PIDs. The camera and defense records contain `__assert_fail` stacks; the inventory records have no stored core, with their exact failed predicate established by the retained stderr/diagnostic transcript. The queried kernel journal for23:55–00:30 contains no OOM entries. No later coredump for these tests appeared in the queried October7 list. These were headless fixtures with no player-save writes; no save loss is indicated. No evidence implicates Omarchy, a graphics driver or a plugin.

Setting a test process's core-size limit to zero can limit stored core files, but systemd can still record its abnormal termination and show a crash notification. The two inventory records demonstrate this. Do not treat missing stored cores as successful execution or suppress assertion failures to hide notifications. Preserve stderr, revision/build identity and exit status when investigating any recurrence; a new failure must be diagnosed independently.

The reports identify already corrected test expectations, not a newly demonstrated runtime regression. The remaining camera geometry, live Town Defense and rendered inventory acceptance boundaries are unchanged by these passing tests.
