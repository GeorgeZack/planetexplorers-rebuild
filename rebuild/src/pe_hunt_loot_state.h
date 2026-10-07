#ifndef PE_HUNT_LOOT_STATE_H
#define PE_HUNT_LOOT_STATE_H
#include "pe_world.h"
#define PE_HUNT_LOOT_CAPACITY 64u
/* World-owned stack identity. Bag stackables deliberately use instance0. */
typedef struct {
    uint32_t instance_id;
    int item_id,count;
    PeVec3 position;
    double created_seconds;
} PeHuntLootDrop;
typedef struct {uint32_t count;PeHuntLootDrop drops[PE_HUNT_LOOT_CAPACITY];} PeHuntLootState;
#endif
