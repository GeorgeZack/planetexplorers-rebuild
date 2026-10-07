#ifndef PE_HARVEST_H
#define PE_HARVEST_H
#include "pe_game.h"
#define PE_HARVEST_ALTERNATIVES 4
#define PE_HARVEST_TREE_PROTOTYPE_ID 48u
typedef struct { int item_id; float threshold; } PeHarvestDrop;
typedef struct { float chance; unsigned count; PeHarvestDrop drops[PE_HARVEST_ALTERNATIVES]; } PeHarvestExtra;
typedef struct {
    uint32_t prototype_id;
    float duration,fixed_count,self_count;
    unsigned primary_count;
    PeHarvestDrop primary[PE_HARVEST_ALTERNATIVES];
    PeHarvestExtra optional,rare;
} PeHarvestSource;
const PeHarvestSource *pe_harvest_source(uint32_t prototype_id);
/* Source HP begins at255, separately from the resource hardness multiplier.
 * Pure calculations accept finite positive scales; current rendered instances
 * remain unit scale. Output is untouched for invalid/unbounded inputs. */
bool pe_harvest_damage(uint32_t prototype_id,float width,float height,float hp,float damage,float *remaining);
/* Source fixed/self volume, bonus, cumulative primary cutoffs, optional float
 * gate and ceil loop; skill rare branch only when explicitly requested. Uses
 * saved game xorshift stream, not Unity's engine RNG sequence. Stages the whole
 * bag plus RNG: any invalid input or capacity failure leaves both untouched. */
bool pe_harvest_grant(PeInventory *inventory,uint32_t *rng,uint32_t prototype_id,float width,float height,float bonus,bool rare);
#endif
