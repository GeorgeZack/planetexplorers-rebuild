#ifndef PE_HUNT_LOOT_H
#define PE_HUNT_LOOT_H
#include "pe_game.h"
bool pe_hunt_loot_valid(const PeGame *game);
/* Actual prototype13 death positions, never mission credit. At most six deaths
 * per batch. Atomic: only loot, saved RNG and shared instance allocator change.
 * Native xorshift32 is a deterministic adapter, not System.Random parity. */
bool pe_hunt_loot_spawn(PeGame *game,const PeVec3 *positions,size_t count);
/* Host calls once per focused unpaused display. Expiry uses saved game seconds;
 * living players collect complete individual stacks at strict 3D distance<4.
 * Full inventory leaves that stack untouched. Counts are stacks, not units;
 * optional outputs and all state remain unchanged on validation failure. */
bool pe_hunt_loot_update(PeGame *game,unsigned *picked_up,unsigned *expired);
/* Explicit accepted lifecycle reset only; never called by rendering. */
void pe_hunt_loot_clear(PeGame *game);
#endif
