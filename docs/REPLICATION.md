# Replication scope

Rebuild Planet Explorers as a native C11 program using the pinned Unity 5.2.4f1 source, databases, serialized assets and packaged Linux V1.1.4 runtime as evidence. Preserve original numeric IDs and provenance. Unity and omitted plugins are reference inputs, not runtime dependencies. Pathea's [terms](../README.md#original-upstream-terms) remain binding.

## Evidence rules

Distinguish **source-derived**, **observed in the original**, **implemented in C** and **provisional**. A passing C test establishes native behavior; a screenshot establishes visible state; neither proves original timing, hidden IDs or full parity. Native substitutions must be named adapters. Recover active call chains and existing archives before requesting another original session.

[Feature contracts](FEATURE_CONTRACTS.md) own supported behavior and compatibility. [PLAYTEST](PLAYTEST.md) owns observations and acceptance limits. [Handoff](HANDOFF.md) owns the next goal. Do not duplicate their running history here.

## Delivered native runtime

| Area | Supported boundary |
|---|---|
| Application | Main menu, separate Tutorial and sandbox hosts, shared recovered presentation |
| Tutorial | Supported lesson chain through construction and receiver puzzle; creation lesson 756 is the boundary |
| Adventure world | Captured settlement/416×64×416 route, editable terrain, Block45 construction, shared geometry queries; no procedural-world equivalence |
| Population and quests | Archived residents, eight shops/284 offers, supplies, Annot, medicine 9072, bounded hunt 9075/9126 and return 9127 ending pages 4105/4106 |
| Inventory and economy | Four 420-cell categories, exact item wear/identity, recipes, atomic purchases/sales/buyback, confirmed discard and camp repair |
| Combat | Bounded female third-person Knife33 ordinary/walking/running/sprint branches, shared root/pose/contact clocks, six pinned hares, actual death credit and drops |
| Survival | HP/stamina/comfort recovery, supported healing/food, hunger/starvation rules and revival; no full environmental survival |
| Presentation | Recovered sampled actors/equipment/materials, directional locomotion, presentation IK/Grounder/Halt, shared HUD/panels/history/settings |
| Persistence | PECSv23 append-only native format and documented earlier migrations; not Unity-save compatibility |

Ordinary native acceptance includes earned hunt completion/partial reload, trade, food use, camp repair and inventory/construction operations. Each observation remains limited to its recorded fixture and inputs.

## Closed or incomplete scope

Normal Sword/Shield readiness and shield input, Town Defense 9077–9084, hare pounce/aggression and reaction stun/knockback remain disconnected foundations. Assets, tables, clocks or headless tests do not open these gates. General Gather still uses the immediate native herb adapter while engine composition and commit timing are qualified.

Full Animator transitions/retargeting, PhysX response, shaders/effects/audio, procedural generation/streaming, population scheduling, colony systems, creations/vehicles and multiplayer remain incomplete or absent. The original's omitted middleware and unavailable lobby service are separate limitations. A working packaged client does not restore its Unity project or online services.

## Architecture and acceptance

Keep simulation/parsing independent of raylib where practical; Makefile CORE/FRONTEND define the boundary. Mode hosts adapt to shared presentation and own their gameplay transactions. World/game heap ownership uses clone/move/destroy. Actions share authoritative clocks across visible pose, motion and contact; geometry changes update render/query/movement/placement/save validation together.

Recover assets through exporters, preserve complete groups/backups and reject malformed present metadata. Keep transient clocks, UI/GPU state, histories and jobs out of saves. Each feature advances through source contract, focused native proof, required regression/export checks, then its appropriate physical/original comparison. Never widen automated coverage into visual or difficulty acceptance.
