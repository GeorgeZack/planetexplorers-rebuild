# Sandbox alignment: original reference fixture

This audit records the original-game reference for the sandbox: the repeatable Adventure and Single Player Build fixtures and what was observed in them on October 3, 2026. The current goal order is in [HANDOFF](../HANDOFF.md). Implemented behaviour is owned by the feature audits linked from [FEATURE_CONTRACTS](../FEATURE_CONTRACTS.md), and observed acceptance is indexed in [PLAYTEST](../PLAYTEST.md). The superseded October 3–6 work order and checkpoint log are in git history.


## Evidence and scope

Three complementary source/C audits define the implementation details:

- [Gameplay, items, crafting and survival](SANDBOX_GAMEPLAY.md), including the exact Adventure starter package and supply quest.
- [World, collision, construction and persistence](SANDBOX_WORLD.md).
- [Actors, UI, materials and encounters](SANDBOX_PRESENTATION.md).

These audits cite local source and database evidence. The observations below are separately established by operating the original executable through its normal UI. The [session manifest](../reference/sandbox/session.json) records launch/profile settings and hashes of78 captures and14 preserved save archives. Existing C test results remain documented in [PLAYTEST](../PLAYTEST.md); they do not prove general Adventure parity.

Adventure Normal is the initial survival reference. The optional question about Adventure versus Single Player Build was unanswered when testing began. Build is a distinct source mode and must not inherit Adventure quests or ambient enemies by accident. Entering construction with B inside Adventure is also different from selecting Build at the main menu.

## Repeatable original fixture

Original: Steam-installed native Linux **V1.1.4**, Unity **5.2.4f1**, installed at `/home/admin/.local/share/Steam/steamapps/common/Planet Explorers`. Testing uses the direct native client through the isolated launcher, with Steam running normally. Ordinary Steam Play and Proton startup are not certified fixed by this session.

Profile: `/home/admin/.local/share/planet-explorers-sandbox-reference`. This is separate from the user's ordinary saves and the earlier tutorial reference profile. Window 1280×720, CPU terrain extraction, antialiasing disabled. Launch from the repository root:

```sh
mkdir -p /home/admin/.local/share/planet-explorers-sandbox-reference/tmp
TMPDIR=/home/admin/.local/share/planet-explorers-sandbox-reference/tmp \
  python3 rebuild/tools/launch_reference.py \
  --user-data /home/admin/.local/share/planet-explorers-sandbox-reference
```

The first launch failed with `SqliteSyntaxException: no such table: skEffect`. Its preceding log records a disk-quota error writing the temporary database under `/tmp`. `LocalDatabase.LoadDb` copies the bundled database to `Path.GetTempFileName()` before opening it. Redirecting only this process's `TMPDIR` to the profile directory allowed the main menu, Adventure creation and gameplay to load. No database, Unity project or game executable was patched. This is the observed workaround for this machine's temporary-file failure, not a general SQLite fix. See [startup error capture](../reference/sandbox/original-startup-temp-error.png) and [runtime investigation](../REFERENCE_RUNTIME.md).

| Fixture field | Selected value |
|---|---|
| Mode | Single Player → Adventure → Normal |
| Avatar | Default female, name `audit` |
| Seed text | `Planet Maria` |
| Major biome / climate | Grassland / Dry |
| Maximum height / map | 128m / 2km × 2km |
| Number of AIs | 3, automatically clamped by the smaller map setting |
| River density / width | 25 / 10 |
| Plain height / flatness / bridge maximum height | 15 / 25 / 0 |
| Skill Tree / All Scripts Available | Both unchecked |
| Initial minimap readout | P:606,340; 09:12 in the first captured welcome frame |
| Manual save | Slot1, character `audit`, Adventure, seed `Planet Maria` |

The UI originally offered 512m/8km/6AIs. Only map size and height were dragged down before confirming. [Selected generation settings](../reference/sandbox/original-world-config-selected.png) are the authority for the tested fixture, not the earlier default screenshot. Resolved integer seed, internal generation stages and exact terrain bytes were not extracted; the seed string alone is insufficient for deterministic C parity.

## Live Adventure observations

| Operation | Observed result and evidence | Limit |
|---|---|---|
| Spawn and welcome | A populated settlement, player clothing, named NPCs, quest markers, terrain and vegetation rendered. Westina Nangle introduced the planet and directed the player to Eitai. [Spawn](../reference/sandbox/original-spawn.png), [settlement explanation](../reference/sandbox/original-welcome-settlement.png). | Generated names/appearance/positions are this fixture; the tutorial's nine fixed NPCs are not its population. |
| Starting inventory | Consumable page showed10 Herbal Juice and10 Revival Shot, with four scanner upgrade objects. Equipment page showed knife, safety axe, headlamp, water pitcher and battery. Resources showed400 meat and200 wood. [Items](../reference/sandbox/original-starting-inventory.png), [equipment](../reference/sandbox/original-starting-equipment-bag.png), [resources](../reference/sandbox/original-starting-resources.png). | Names/IDs are resolved against the source database in the gameplay audit; pictures alone are insufficient. Only first pages were inspected, not every empty page. |
| Starting recipes | Replicator opened with an empty All list. Right-clicking the first scanner upgrade opened Scan, not a crafting recipe. [Empty Replicator](../reference/sandbox/original-replicator-before-learning.png), [Scan](../reference/sandbox/original-learn-script.png). | The four starter scanner objects must not be treated as recipe books. |
| Vitals | Character window showed HP500/500, stamina100/100, hunger284/500, comfort468/500, oxygen500/500, shield0/0, energy0/0, attack10, defense12. Later hunger and comfort had declined while alive. [Character](../reference/sandbox/original-character-stats.png). | These are sampled values after time in the world, not exact initial hunger/comfort or measured decay rates. Death, swimming and exhaustion were not tested. |
| Arrival | Q showed “Ask Eitai for the supplies”; moving near Eitai completed Arrival, then right-click interaction exposed the supply conversation. [Mission](../reference/sandbox/original-arrival-mission.png), [interaction](../reference/sandbox/original-eitai-interact.png). | Proximity completion is observed in this run; all mission target rules are not inferred from it. |
| Supply acceptance | Finishing Eitai's conversation started **The Delayed Supplies** and added tools and actual recipe books. The tracker then said “Go to Annot.” [Acceptance](../reference/sandbox/original-supplies-accept.png), [new books](../reference/sandbox/original-supplied-scripts.png). | The trip to Annot was not completed. Source quest 9032 grants the package on acceptance, not on finishing that trip. |
| Learning/crafting | Right-clicking the Wood Shovel Script produced one learned Wood Shovel entry. Selecting it showed10 wood per shovel. One timed craft produced a second shovel and reduced wood200→190. [Learned](../reference/sandbox/original-learned-shovel.png), [recipe](../reference/sandbox/original-shovel-recipe.png), [progress](../reference/sandbox/original-shovel-crafting.png), [two tools](../reference/sandbox/original-tools-after-crafting.png). | The run verifies delivery and cost, not exact elapsed delivery timing or cancellation behavior. Source timing remains documented separately. |
| Equipping | Right-clicking the newly crafted shovel moved it from the bag, displayed it on the character, occupied both hand indicators and changed attack10→33. [Equipped](../reference/sandbox/original-shovel-equipped.png). | Two hand icons represent one equipped instance, not two owned shovels. Independent wear was not numerically inspected. |
| Digging | Bounded ordinary mouse actions with the shovel drew the blue target box and excavated the nearby hillside. Dirt notification and inventory showed6 dirt, alongside190 wood and one yellow byproduct icon. [Excavation](../reference/sandbox/original-dig-removal.png), [inventory](../reference/sandbox/original-gathered-resources.png). | Exact accepted swing count, hidden density changes, yellow-item identity/probability and per-swing timing/wear were not established. |
| Building in Adventure | B opened construction, source controls appeared, and a small dirt block was placed beside the player. Dirt6→5. [Build UI](../reference/sandbox/original-build-entry.png), [placed block](../reference/sandbox/original-built-block.png), [remaining inventory](../reference/sandbox/original-after-building-inventory.png). | Preview and committed placement required successive clicks in this operator sequence; this does not establish a universal two-click brush rule. Brush size/packed cells were not instrumented. |
| Save/reload | Saving empty manual slot1 reported success. Loading slot1 returned to the settlement with the active quest, equipped shovel, learned recipe,190 wood,5 dirt and the dirt block still present. [Save](../reference/sandbox/original-saved-slot1.png), [load metadata](../reference/sandbox/original-load-slot1.png), [resources](../reference/sandbox/original-reloaded-resources.png), [recipe](../reference/sandbox/original-reloaded-recipe.png), [construction](../reference/sandbox/original-reloaded-construction.png). | This is a normal UI round-trip, not byte comparison, restart recovery or complete archive validation. |

The preserved Adventure manual save is under the isolated profile's `PlanetExplorers/GameSave/save0`, including `MainAdventure` terrain, water, construction and world archives. It is a reference checkpoint, not a C-compatible save. Autosaves also belong only to this isolated profile.

## Live Single Player Build comparison

A second character, default female `buildaudit`, was created through Single Player → Build in the same isolated profile. It used the same seed, biome, climate and numeric generation settings as Adventure; this setup exposed no Skill Tree/All Scripts checkboxes. [Selected settings](../reference/sandbox/original-build-world-config-selected.png).

The initial view showed terrain and vegetation without the Adventure settlement welcome or nearby NPC/quest markers. The minimap read P:593,377 and13:00. That clock remained13:00 throughout the inspection. [Spawn](../reference/sandbox/original-build-spawn.png). This small observed area does not prove the absence of every entity in the entire world.

Inventory started with many consumables, objects and resource stacks, most visible resource stacks at9,999. Replicator already contained learned formulas, including Herbal Juice and several medical/component entries. [Items](../reference/sandbox/original-build-starting-items.png), [resources](../reference/sandbox/original-build-starting-resources.png), [recipes](../reference/sandbox/original-build-recipes.png). Only the first visible pages were inspected, so this is not a complete live inventory enumeration or proof that every possible formula is known. Character still displayed health, stamina, hunger, comfort and oxygen; a frozen world clock must not be interpreted as disabling all survival state. [Character](../reference/sandbox/original-build-character.png).

B opened the same construction interface. Dirt was placed visibly, but its inventory count remained9,999. [Placed](../reference/sandbox/original-build-mode-placed.png), [count afterward](../reference/sandbox/original-build-after-placement-resources.png). The source explanation is specific: `SingleGameBuild.Load` enables `PlayerPackageCmpt.LockStackCount`; the construction path still checks available items, while the package destruction method returns success without debit when locked. This is separate from the `IsGod` debug bypass and `unlimitedRes`. `PlayerTypeLoader` sets day94.50 and `ElapseSpeed=0`, explaining13:00 in a26-hour day. The [world audit](SANDBOX_WORLD.md) contains exact source links and remaining boundary cases.

Build was saved successfully to previously empty manual slot2, preserving Adventure slot1. [Save confirmation](../reference/sandbox/original-build-saved-slot2.png). Its archive is the isolated profile's `PlanetExplorers/GameSave/save1`. Build reload, crafting debit, missing-item construction and deletion/undo were not tested. The original received its ordinary Quit Game command after both checkpoints were saved.

## Comparison still open

Before claiming core alignment, run the same bounded start → supply → learn → craft → equip → dig → build → save/load sequence in both programs, recording counts and the equipped instance's identity. Then measure tool event times and wear, sprint, exhaustion and recovery, airborne regeneration, slopes and steps, ordinary trees, low-stamina and no-resource attempts, crafting batches and cancellation, construction deletion and undo, and edited collision after reload. Take timing and internal quantities from captured clocks or state, never from still frames.
