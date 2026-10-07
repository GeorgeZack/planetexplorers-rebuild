#ifndef PE_VEGETATION_H
#define PE_VEGETATION_H
#include "pe_game.h"
/* Source prototype/mesh bounds. Primary item/count remain presentation metadata;
 * pe_harvest owns randomized complete production, independent of placement. */
typedef struct { uint32_t prototype_id; int item_id; unsigned drop_count; bool harvestable; PeVec3 minimum,maximum; } PePlantSource;
const PePlantSource *pe_vegetation_source(uint32_t prototype_id);
bool pe_vegetation_state_valid(const PeGame *game);
void pe_vegetation_clear(PeGame *game);
/* Append2 existing recovered tree examples and8 fixed plants near finite anchor.
 * No RNG, inventory grants, original spawn claims, or partial installation.
 * Repeating an installed patch succeeds without replenishing harvested plants.
 * Uses supported terrain and shared collision; authored residents keep3m XZ. */
bool pe_vegetation_install_fixture(PeGame *game,PeVec3 anchor);
/* Source terrain picker clamps requested3m to inclusive2m XZ and scans nearby
 * floor-indexed vertical cells. Pointer selection uses source renderer AABBs.
 * obstacle_distance is +INFINITY or nearest obstructing ray distance. Outputs
 * remain unchanged on failure; decorations/depleted plants are never picked. */
bool pe_vegetation_pick(const PeGame *game,PeVec3 origin,PeVec3 direction,float obstacle_distance,
                        unsigned *index,float *distance);
/* Snapshot must describe the same game. Immediate collection remains a native
 * adapter: no unqualified Gather animation or movement freeze. Source production,
 * RNG, depletion and gathered count commit together. Failure may update message only. */
bool pe_vegetation_can_harvest(const PeGame *game,unsigned index,const PeSandboxCollision *collision);
bool pe_vegetation_harvest(PeGame *game,unsigned index,const PeSandboxCollision *collision);
#endif
