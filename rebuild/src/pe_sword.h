#ifndef PE_SWORD_H
#define PE_SWORD_H
#include "pe_game.h"
/* Exact source Wood Sword1. Inventory remains the sole persistent owner. */
bool pe_sword_slot_valid(const PeSlot *slot);
/* Base attack excludes this item's buff. Exhaustion removes item attack10;
 * malformed slot/base input returns NAN, never a substitute weapon stat. */
float pe_sword_attack(const PeSlot *slot,float base_attack);
/* One successful source damage notification, including zero/overkill damage.
 * Subtracts5 once and clamps0; misses/deduplicated contacts must not call this.
 * Pure slot operation for staged transactions. Build bypass belongs to host. */
bool pe_sword_wear(PeSlot *slot);
#endif
