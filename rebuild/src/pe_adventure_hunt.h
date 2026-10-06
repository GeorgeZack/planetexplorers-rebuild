#ifndef PE_ADVENTURE_HUNT_H
#define PE_ADVENTURE_HUNT_H
#include "pe_game.h"
enum { PE_HUNT_NONE=0, PE_HUNT_TRAVEL=1, PE_HUNT_ENCOUNTER_PENDING=2, PE_HUNT_RETURN_PENDING=3,
    PE_HUNT_RETURN_TALK1=4, PE_HUNT_RETURN_TALK2=5, PE_HUNT_COMPLETE=6 };
/* Saved source coordinates remain valid across explicit world imports. A
 * missing reference never enables acceptance at placeholder coordinates. */
bool pe_adventure_hunt_state_valid(const PeGame *game);
bool pe_adventure_hunt_reference(PeVec3 *source_destination);
/* Cheap profile/prerequisite/bounds eligibility. Acceptance additionally tests
 * the authoritative collision snapshot for supported arrival terrain. */
bool pe_adventure_hunt_available(const PeGame *game);
bool pe_adventure_hunt_accept(PeGame *game);
/* Strict15m XZ discovery, no items; stage2 enables the bounded9126 encounter. */
bool pe_adventure_hunt_step(PeGame *game);
bool pe_adventure_hunt_return_reference(PeVec3 *source_destination);
/* Strict5m XZ discovery at the archived4525 anchor, nonnegative sourceY.
 * Commits all four script rewards once BEFORE entering4105. Capacity failure
 * is an explicit native retry adapter: inventory/progress remain unchanged.
 * Does not execute original plot4 NPC movement, skill-tree buffs or9077 waves. */
bool pe_adventure_hunt_return_step(PeGame *game);
/* Durable presentation progress only. Reopening/loading never grants again;
 * expected talk ID rejects stale/replayed acknowledgments. */
int pe_adventure_hunt_return_talk(const PeGame *game);
bool pe_adventure_hunt_return_advance(PeGame *game,int expected_talk);
#endif
