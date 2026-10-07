#ifndef PE_CAMP_REPAIR_STATE_H
#define PE_CAMP_REPAIR_STATE_H
#include "pe_item_slot.h"
#include <stdbool.h>
/* Exactly one reserved ordinary item; no pointers or host state. */
typedef struct { PeSlot item; int64_t remaining_ticks; bool running; } PeCampRepair;
#endif
