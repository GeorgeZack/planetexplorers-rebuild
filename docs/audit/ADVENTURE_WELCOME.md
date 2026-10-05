# Adventure welcome and Arrival handoff

The native Adventure host now runs Westina's initial conversation before Arrival. This is a bounded replication of source dialogue and one archived NPC appearance, not the procedural settlement generator.

## Source evidence

The read-only database chain is9027→9028→9029→9030→9031. The source Adventure initialization resolves `missionStartNpcEntityId`, assigns the current NPC and opens9027 (`MissionManager.AdventureInit`). `UINPCTalk` advances the opening/ending lines and next quests. Earlier original-client captures show the player question as an ordinary dialogue page, including [the resources question](../reference/sandbox/original-welcome-next.png) and [Westina's settlement explanation](../reference/sandbox/original-welcome-settlement.png).

| Page | Talk | Speaker |
| --- | --- | --- |
| 0 | 1949 | Welcome NPC (source current NPC0) |
| 1 | 4081 | Player |
| 2 | 4082 | Welcome role1 |
| 3 | 4083 | Player |
| 4 | 4084 | Welcome role1 |
| 5 | 4085 | Player |
| 6 | 4086 | Welcome role1 |
| 7 | 4087 | Player |
| 8 | 4088 | Welcome role1; directs player to role2 |

`export_adventure_dialogue.py` verifies quest links, automatic replies, source speakers and English localization before generating the C table. The source planet token is the configured seed string; the bounded C host displays its existing numeric world seed. Westina/Eitai/Annot are bindings from this archived reference, not universal generated names.

## Native contract

Fresh Adventure has `welcome_page=0`, no active Arrival mission and the unchanged starter inventory. Each acknowledged page commits through `pe_game_welcome_advance`; an incorrect expected page, dead player, incompatible mode or existing progression is rejected. Final4088 acknowledgment sets page9 and mission9031. Only subsequent eligible proximity to Eitai completes Arrival and permits the existing supplier dialogue. Final4094 still owns the atomic supply grant.

The introduction automatically opens once when a pending player is nearby and the host can accept NPC input. Esc closes it without discarding acknowledged replies; E/right mouse on Westina resumes it. Completed Westina offers the source close-only menu. All dialogue input, including the closing frame, consumes world actions. The host selects the nearest eligible NPC ray hit, turns the selected NPC toward the player and uses its own recovered portrait. Both NPCs appear on the minimap.

PECSv6 appends the four-byte page counter. Valid pages0..8 require mission0, no Arrival and no supplies. Page9 requires the existing9031/9032 state relationship. Save/load preserves each page; v4/v5 Adventure saves infer completed welcome and retain their quest/inventory state. Legacy saves retain page0. Existing v4 tree-overlap migration is unchanged. NPC placement/yaw and open dialogue windows remain transient.

Westina uses supported local placement around X28/Z35; Eitai remains around X32/Z27. Neither is the archived world position. Both use the compatible source female query capsule and remain nonblocking to the player. [Appearance provenance](ADVENTURE_WELCOME_APPEARANCE.md) records Westina's exact nine parts, saved armor, sword/shield and62-sample NormalIdle.

## Verification and remaining scope

The C desktop harness verifies automatic1949, modal input, cancellation/page3 resume, page8 save/reload, final acknowledgment, completed close-only interaction and another reload. All saved inventories match the starter inventory, no early Arrival/supplies occur, and counters remain zero tool swings/resource changes. The final long paragraph fits the recovered1280×720 dialogue panel. See [playtest evidence](../PLAYTEST.md#october-5-adventure-welcome).

Headless tests cover all nine source pages/speakers, every-page persistence, invalid progression, old-save migration and supplier handoff. Actual native skinning evaluates all558 part/frame combinations (9×62), totaling7,991 vertices. Export tests validate role identity, helmet slot replacement, equipment compatibility and deterministic output; the existing supplier export remains unchanged.

Still unimplemented: original settlement layout/population generation, source AdvPlot side effects and three colony-site map markers, travel to Annot, general NPC AI/gestures, player dialogue portrait, original portrait lighting and string-named world configuration. Saved partial-dialogue resume is a native compatibility policy, not claimed as original save semantics. No new Steam run was needed for this slice; source/archive evidence and previously captured original UI were compared with the C host.

```sh
TMPDIR="$PWD/rebuild/build/compiler-tmp" make -C rebuild all build/adventure_supplier_fixture
python3 rebuild/tools/playtest_adventure_welcome.py
python3 rebuild/tools/playtest_adventure_supplier.py
```

Desktop scripts require an unlocked session and exclusive operation of their own C window. They use isolated fixture/result saves and close their owned process. Do not rebuild the executable during an input run: the PID-scoped driver validates its executable identity.
