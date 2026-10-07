#ifndef PE_ITEM_SLOT_H
#define PE_ITEM_SLOT_H
#include <stdint.h>
/* Value identity shared by bag and detached machine ownership. */
typedef struct { int item_id, count; uint32_t instance_id; float durability; } PeSlot;
#endif
