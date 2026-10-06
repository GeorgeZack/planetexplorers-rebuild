# Contact-responsive attack playback

2026-10-06. Source-derived and implemented in the C sandbox; no game launch or new physical playtest. This closes the numerical contact-speed gap identified by the [source-first combat audit](SOURCE_FIRST_COMBAT.md). [Shield action admission](SHIELD_ACTION.md) is a separate, disconnected foundation; ordinary shield activation stays gated.

## Recovered contract

[Action_Sword.UpdateAnimSpeed](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs) applies the weapon's speed and its stamina thresholds, clamps that base to0.5–1.5, then multiplies by the contact curve while `Time.time - animScaleStartTime < animScaleTime`. There is no second clamp. The exact player prefab overrides the class-default0.15s with **0.6499999761581421s**. The [exporter](../../rebuild/tools/export_attack_playback.py) resolves Motion_Equip's `m_SwordAttack` on EntityPlayer, retains all five keys/tangents and raw endpoint modes, and cross-checks the installed modern ResourceManager payload. [Generated provenance](../../rebuild/generated/pe_attack_playback_source.json) records source hashes, component identity and guarded caller methods.

The curve starts at1, falls to0.01 around normalized0.1, stays there until0.25, rises to1.3 at0.75, and finishes at0.9999892115592957 at0.9995110630989075. These are normalized curve coordinates, not wall seconds. Its serialized endpoint mode2 is an internal clamp value, distinct from public WrapMode2. The [offline evaluator audit](UNITY5_CURVE_EVALUATOR.md) traces deserialization, enum conversion, segment selection and float polynomial evaluation in the exact installed executable. A bounded arithmetic-only replay agrees with the C curve at4,109 inputs, including exact keys and adjacent floats. No ELF initialization, Unity object, game process, save access or graphics was needed. This establishes the supported finite-key arithmetic on this Linux x86-64 build, not every Unity curve/platform.

## Notification is separate from damage

[PEAttackTrigger.CheckHit/OnHit](../../Assets/Scripts/Attack/PEAttackTrigger.cs) establishes geometric overlap first. A geometric hit enters its effected-trigger list even if `OnHit` subsequently rejects damage permission. A permitted non-self defence-trigger contact invokes Action_Sword's callback **before** the skill collision/guide dedupe and HP change. Collider-only contacts without a defence trigger do not invoke this callback. Therefore HP loss is not a valid substitute for contact notification, and repeatedly overlapping a previously effected trigger must not restart the clock every tick.

The C [read-only geometry probe](../../rebuild/src/pe_knife_contact.h) separates that query from damage. The sandbox owner has a transient effected-entity set distinct from PeCombat's skill-guide set. Targets carry separate candidate, trigger-permission and skill-eligibility inputs. A permitted contact can restart playback even when the damage guide ignores it; an impermissible geometric hit is consumed without notification or damage. The bounded hunt maps one defence trigger to each actor identity; source PhysX candidate discovery, multiple triggers per entity and callback ordering remain outside this mapping.

Both lists reset with the relevant action/contact-window lifecycle. Capacity failures and invalid later targets roll back staged contact timestamps, earlier damage, durability and outputs together. This preserves the native transactional contract rather than reproducing partial source side effects after a malformed input.

## Shared clock and lifecycle

[pe_attack_playback](../../rebuild/src/pe_attack_playback.h) owns only transient absolute scaled-time response state. It uses the sandbox's existing display clock, never accelerated world/day time or per-combo wall time. Same-display fixed catch-up ticks retain the pre-contact response snapshot; notifications affect the next display timestamp. The existing per-tick stamina base-rate calculation and fixed60Hz event/contact scheduler remain native adapters, not Unity Update/Animator/LateUpdate equivalence.

The source timestamp starts at0 and survives combo starts, cancellation, draw/stow and equipment changes on the same character Action_Sword. Native cancellation and equipment-template rebinding preserve it; a successful load/new host creates fresh transient response state. Returning through a recreated graphics host also resets it, an explicit native lifecycle boundary. No new save payload is added; PECSv16 remains unchanged. Failed loads retain existing state.

The resulting multiplier feeds the **one authoritative attack interval** already used by body/tool pose, defence pose, authored contact events and recovered running408 root motion. Rendering does not maintain a second response timer. Other actors' action clocks are independent. Recovered attack poses need no rebake for this clock correction. Shared display-clock pause/focus/capped-delta limits remain as documented in [passive recovery](PASSIVE_RECOVERY.md).

## Verification and remaining acceptance

[Pure clock/curve tests](../../rebuild/tests/test_attack_playback.c) cover independent native golden float values, exact interval end, low stamina, no final clamp, same-display snapshot behavior, timestamp preservation and invalid rollback. [Sandbox tests](../../rebuild/tests/test_sandbox_melee.c) cover geometry without permission, notification without damage, separate dedupe, new-target reset, capacity rollback and actual recovered running assets. The actual frontend equipment helper checks playback opt-in and preserved timestamp across template rebinding. Exporter tests check source binding and malformed metadata; installed payload comparison is separate evidence.

Run the offline metadata checks from the root:

```sh
make -C rebuild attack-playback-source-check ASSET_PYTHON="$PWD/rebuild/build/unity-tools/bin/python"
```

The optional arithmetic replay and its exact installed-binary prerequisite are documented in [the evaluator audit](UNITY5_CURVE_EVALUATOR.md). Current run results are recorded in [PLAYTEST](../PLAYTEST.md). Tests do not establish visible hit response, ordinary four-hare completion, partial-kill physical reload, shield pose fidelity or overall difficulty parity. Those remain focused native acceptance checks after source-backed integration, not reasons to repeatedly explore the original for already extracted values.
