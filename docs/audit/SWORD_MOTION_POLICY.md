# Sword branch selection and shared movement request

Subsequent checkpoint: [grounded walking attacks](SANDBOX_WALKING_KNIFE.md) now qualify, export and integrate ordinary Attack6–9 root motion, including running continuations. The remaining-extraction statements below describe this earlier policy checkpoint; visual acceptance and ordinary hunt completion remain open.

October 6, 2026. Source-derived and implemented in C, with offline numerical and recovered-asset tests. Neither game was launched. This advances the walking-attack work in [the source-first review](SOURCE_FIRST_REBUILD.md); slow-walking attacks are not enabled by this increment.

## One source selector

[pe_sword_variant](../../rebuild/src/pe_sword_variant.h) separates the source branch from native presentation availability. [Action_Sword.ApplySkill](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs) selects air first, then water when the motor exists and its spine is in water. Otherwise it uses the movement component's full XYZ velocity magnitude: strict greater than7 selects sprint, strict greater than4.5 selects running, and all other speeds select ordinary. Missing movement selects ordinary on land. These thresholds come from the instantiated EntityPlayer action; class defaults4/6 are not the installed values.

[export_sword_variant.py](../../rebuild/tools/export_sword_variant.py) guards active source methods and component binding and recovers those serialized thresholds. [Generated provenance](../../rebuild/generated/pe_sword_variant_source.json) records the inputs. Both the native display-frame running-press check and core action admission now use this selector; unsupported air/water/sprint branches never silently become ordinary attacks. Native finite-input validation remains a separate bound.

Float rounding matters at strict boundaries. For velocity `(-2.2190206050872803, -2.0396177768707275, 3.341542959213257)`, the former all-double magnitude is4.500000274421847. The installed managed Vector3 operation returns float4.5 and selects ordinary. The C selector reproduces the inspected wider sum-of-products, float square argument, square root and float return. [The offline numerical report](../reference/sword-variant-numerics.json) covers4,101 inputs against the installed managed method without initializing a scene. This is a qualification of that runtime's scalar operation, not every Unity/compiler combination.

## Movement ownership

[pe_sword_motion](../../rebuild/src/pe_sword_motion.h) now owns the common horizontal request calculation. Given qualified world displacement for one action update, it divides by delta time, projects onto XZ and limits the difference from current horizontal velocity to12 per update. This is neither a total speed cap nor acceleration multiplied by time. Root Y and delta rotation are not applied. The running wrapper retains its existing yaw transform and delegates to this helper with unchanged double arithmetic; Unity's float motor arithmetic is not claimed bitwise equivalent.

The source uses this rule for every running sword skill. Its action relations end Move/Sprint/Rotate on admission and exclude Move while SwordAttack is masked. Holding movement input is therefore not a source cancellation rule. Immediate cancellation clears horizontal velocity and inertia; normal end cancels the request without that explicit velocity reset. Native collision/controller behavior remains an adapter to the source motor.

The existing native ordinary stationary path still supplies zero horizontal request and restricts movement. Changing only its admission condition would preserve the wrong displacement. Attack6–9 require their own source-root extraction, qualified against existing engine evidence, before this restriction is removed. Likewise running combos' later ordinary stages need the corresponding ordinary curves. The retained outgoing `PeCombatInterval`, rather than the new/reset action stage, must select the movement curve; movement is swept once before contact resolution. Never replay a display delta on every fixed catch-up tick.

## Verification and next dependency

`make -C rebuild sword-policy-test` covers the selector, shared request, existing running request, sandbox melee and recovered running collision/contact pipeline. Optimized and ASan/UBSan runs with leak detection pass with0 C failures and no asset skips. The actual near-threshold core admission test verifies no incorrect running start or stamina debit. The executable build, existing content checks and23 CLI cases pass. No new full-core or graphical pass is claimed.

`make -C rebuild sword-source-check` runs exact regeneration and all15 exporter tests using the local UnityPy interpreter, with no skips. Override `UNITY_PYTHON` when using a different environment. Ordinary system-Python exporter tests skip four optional prefab cases; those skips are not passes. The new prefab-dependent check is separate from the existing dependency-light `check-content` target.

Logs are under ignored `rebuild/build/sword-motion-check`. The accompanying [root-channel investigation](PLAYER_ROOT_CHANNELS.md) now supplies a reproducible source decoder, static native flag/scale evidence and comparison against2,595 existing running samples. It leaves the final root-delta composition/basis connection unproven for the ordinary clips. Static pose-root coordinates and sums of paused-seek delta diagnostics do not establish sequential displacement. Four-hare completion and partial reload were accepted separately ([hunt acceptance](HUNT_ORDINARY_ACCEPTANCE.md)); graphical revival acceptance remains open.
