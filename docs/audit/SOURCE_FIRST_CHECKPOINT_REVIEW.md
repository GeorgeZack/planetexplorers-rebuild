# Source-first audit after the walking-attack checkpoint

Subsequent implementation: [vendor wallets and normal-purchase credit](TOWN_WALLETS.md) now have a source exporter, runtime owner and PECSv17 persistence. The recommendations below describe the audit baseline; [selling/buyback](TOWN_SELL_BUYBACK.md) now add source-priced transactions and shop tabs, with graphical acceptance pending.

October 6, 2026. Baseline `3791d89e763586a5cd3bc7684c6462592288572a` was committed and verified on private `rebuild/main`; the working tree was clean before these audit documents. The checkpoint includes offline ordinary-root extraction and walking Knife integration. Focused optimized, ASan/UBSan/leak, content, host and23 CLI checks passed. Neither original nor native gameplay was launched for this audit.

## Decision

Continue development from source, serialized assets and preserved saves. Another broad original-game playthrough is not a prerequisite for the next increments. The walking implementation demonstrates the practical benefit: existing clips and a bounded native arithmetic proof supplied the missing root motion without another capture.

This is a targeted parity review by three parallel agents and a coordinating review, not an exhaustive correctness certification of the whole game. Each detailed report identifies original call sites, current C behavior and the remaining evidence boundary:

- [Combat, animation and creature behavior](SOURCE_AUDIT_COMBAT_FOLLOWUP.md).
- [Mission progression, wallets and services](SOURCE_AUDIT_PROGRESSION_FOLLOWUP.md).
- [Inventory, construction, camera, gathering and recording](SOURCE_AUDIT_SYSTEMS_FOLLOWUP.md).

## October7 follow-through

The audit recommendations below are historical. Wallets and selling/buyback, category inventory operations and confirmed discard, construction history, source harvesting transactions, camera activity/recovery, isolated pounce, defense wave/member planning, typed9077–9084 events and follow-up recipe definitions are now implemented at their documented boundaries. [Sandbox alignment](SANDBOX_ALIGNMENT.md) and [PLAYTEST](../PLAYTEST.md) track the individual evidence.

The existing starting-town [repair machine](CAMP_REPAIR.md) is now qualified: source entity20006/prototype145 is present in the recovered settlement and route, with camp binding and empty archived jobs. Its detached item/material/counter owner passes focused checks. The next live step is saved ownership, actual machine interaction and modal UI integration. Remaining camera near-plane geometry likewise needs projection/near-clip and overlap semantics; activity/recovery alone is not the full clamp. Neither investigation needs a new broad original playthrough. Gather humanoid poses, masked shield composition and live pounce/defense integration retain their separate gates.

## What to extract next

| Workstream | Offline result available | Boundary before calling it playable parity |
|---|---|---|
| Vendor wallets, then selling/buyback | Decode actual archived purses; trace purchase credit, refill clock, condition pricing and exact-instance transfers | Add one saved wallet owner and transactional tests before exposing selling. Shop UI integration then needs a short native check. |
| Inventory capacity and operations | Trace original category capacities, stack split/merge/reorder and quickslot identity rules | This changes a foundational saved structure; design migration and audit all fixed-slot loops before increasing capacity. |
| Hare pounce | Export the generic TRS clip and source controller/skill metadata; port the bounded action lifecycle into isolated tests | Do not immediately make the hunt more aggressive. Arbitration, collision, outgoing/incoming poses and ordinary difficulty acceptance remain separate. |
| Quest graph and defense planning | Recover prerequisites, acceptance grants versus completion rewards, role bindings and spawn decisions | A planner does not enable Town Defense. Species, allies, supported geometry and instance/save behavior must also exist. |
| Construction undo/redo | Port the source five-action history and debit/refund callbacks | Keep geometry, inventory rollback and invalidated collision/render snapshots coherent; verify both Adventure and Build semantics. |
| Camera and gathering | Decode the remaining camera graph and source species/scale yield rules | Port bounded math and transactions first; visual feel and geometry-dependent behavior still need acceptance. |
| Moving shield presentation | Continue solving against existing poses, or prepare a bounded engine evaluation batch | Full humanoid limbs/masked composition are still unresolved. Another long manual block recording is not the default next step. |

The smallest new gameplay foundation is the vendor wallet owner: purchases currently debit the player without crediting a modeled vendor wallet. It can be implemented and tested offline before selling/buyback. In parallel, the next combat extraction is pounce data and its isolated lifecycle, without changing live encounter aggression. Inventory expansion is higher impact but wider in scope because of saves, aliases, UI pagination and every fixed-slot consumer. These priorities are recommendations; this audit does not enable the proposed features.

Existing native acceptance remains outstanding for walking attacks and Revival Shot, four ordinary hare kills and actual partial-progress save/reload. Those checks validate the rebuild and cannot be replaced by footage of the original. They need not block unrelated source-backed development.

## A thirty-minute user session

Yes: user-operated gameplay recorded for later review is a useful way to replace repeated broad observation sessions. Use ordinary game-window or selected-region video, timestamped notes and preserved start/end saves. The user remains the sole input operator. A short recording/playback preflight should establish encoding performance, output location and clean finalization before the full session. No recorder has been started by this audit.

A useful approximate route is3 minutes for mode/stats/equipment,6 for walk/run/camera/attack/block transitions,11 for ordinary combat, retreat and partial-progress save/reload,7 for reachable quest/shop/crafting interactions, and3 for final state and a fresh save. These are priorities rather than a demand to finish a mission or survive. Death, awkward transitions and failed interactions are useful evidence too.

Review the recording afterward by question and timestamp; short periodic observations during play can supplement it, but are not a continuous frame-by-frame analysis guarantee. Keep exact hidden constants, action ownership and transaction rules source-derived. Do not infer simulation timing or identities from video alone.

The full-pose probe is unsuitable for thirty continuous minutes: its current bound is120 seconds or3600 display frames, with a256MiB output cap. Increasing that cap is not a recording plan. Existing captures already support most remaining offline animation investigations. If a genuinely missing engine state remains, prepare one short focused probe separately. See [the recording/tool assessment](SOURCE_CAPTURE_REVIEW.md) and the systems follow-up for current local options and limits.

The immediate original-session recommendation is therefore optional broad video, not another mandatory replay. Use native testing first for the finished movement/revival changes; use one original recording when the user wants to demonstrate ordinary play and transitions. New specialized captures should answer a named unresolved question that the source, assets and existing archives cannot answer.

## Validation

Read-only source/data/tool comparisons and local-link checks accompany the detailed reports. No new runtime qualification, full test-suite pass, graphic check or recording is claimed by this documentation change. The walking checkpoint's separate evidence is in [PLAYTEST](../PLAYTEST.md) and [the implementation audit](SANDBOX_WALKING_KNIFE.md).
