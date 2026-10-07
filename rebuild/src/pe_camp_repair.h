#ifndef PE_CAMP_REPAIR_H
#define PE_CAMP_REPAIR_H
#include "pe_game.h"
/* Disconnected ordinary camp repair. This owns a removed bag instance, never a
 * copy still owned by inventory. Not part of PeGame or PECS. Zero initializes.
 * Source equipment-origin drops are intentionally unsupported in this adapter. */
typedef struct {
    PeSlot item;
    int64_t remaining_ticks;
    bool running;
} PeCampRepair;
/* Mandatory host guard: true only if this instance has no equipment, outfit,
 * deployment, buyback or other external owner/reference preventing transfer.
 * Bag membership is checked here separately. Guard must be read-only. */
typedef struct {
    PeInventory *inventory;
    bool (*unbound)(const void *context,uint32_t instance_id);
    const void *context;
} PeCampRepairAccess;
#define PE_CAMP_REPAIR_COSTS 8
typedef struct { unsigned count; PeIngredient materials[PE_CAMP_REPAIR_COSTS]; } PeCampRepairCost;
/* Reject unknown/component-unsupported profiles and unmodeled instance state.
 * Full and zero-maximum profiles may be inserted but cannot start a job. */
bool pe_camp_repair_item_supported(const PeSlot *item);
bool pe_camp_repair_quote(const PeSlot *item,PeCampRepairCost *cost);
bool pe_camp_repair_valid(const PeCampRepair *repair,const PeCampRepairAccess *access);
/* Expected slot must match all fields, protecting against stale UI selection.
 * Busy machine rejects swaps. Same-category swaps use the incoming bag slot;
 * cross-category swaps first require room to return the old item. */
bool pe_camp_repair_insert(PeCampRepair *repair,const PeCampRepairAccess *access,int slot,const PeSlot *expected);
bool pe_camp_repair_start(PeCampRepair *repair,const PeCampRepairAccess *access);
bool pe_camp_repair_cancel(PeCampRepair *repair,const PeCampRepairAccess *access);
bool pe_camp_repair_take(PeCampRepair *repair,const PeCampRepairAccess *access,int *destination);
/* Call once per active display frame. CounterScript truncates EACH float dt at
 * 100000 ticks/second. enabled=false suspends the counter, not item ownership.
 * Completion output is true exactly once; invalid calls preserve all outputs. */
bool pe_camp_repair_update(PeCampRepair *repair,const PeCampRepairAccess *access,float dt,bool enabled,bool *completed);
float pe_camp_repair_seconds(const PeCampRepair *repair);
/* Value snapshot only: caller must restore the corresponding inventory and
 * other owners first. Destination must be empty; rejected snapshots leave it
 * unchanged. No on-disk parser, host load hook or original mid-job oracle. */
bool pe_camp_repair_restore(PeCampRepair *repair,const PeCampRepair *snapshot,const PeCampRepairAccess *access);
#endif
