#ifndef PE_SHIELD_H
#define PE_SHIELD_H
#include "pe_game.h"
/* Bounded Wood Shield34 selection. Inventory is the sole item/durability owner;
 * selection and a matching quickslot may reference the same bag instance. */
#define PE_SHIELD_ITEM 34
bool pe_shield_slot_valid(const PeSlot *slot);
bool pe_shield_state_valid(const PeGame *game);
/* Borrowed bag slot; NULL for no selection or invalid ownership. */
const PeSlot *pe_shield_equipped(const PeGame *game);
/* Reference-only transactions: no bag transfer, allocation or capacity debit.
 * Primary equipment is independent. Exhausted instances remain selectable but
 * grant no defence/protection. Normal UI use stays gated until presentation. */
bool pe_shield_equip(PeGame *game,int inventory_slot);
bool pe_shield_unequip(PeGame *game);
/* NULL means absent for stat queries; malformed non-NULL slots return NAN.
 * Protection is the source attribute, not an unconditional damage multiplier:
 * the caller must supply a supported source block/action/damage-path gate. */
float pe_shield_defense(const PeSlot *slot);
float pe_shield_protection(const PeSlot *slot);
/* Pure slot-copy operation for an atomic combat transaction. Pass calculated
 * damage before remaining-HP clamp, once per accepted hit. Build bypass belongs
 * to the caller. Invalid data is atomic; negative damage never repairs. */
bool pe_shield_wear(PeSlot *slot,float calculated_damage);
/* Ordinary inventory removal only: discard a reference whose instance is gone.
 * Existing malformed instances remain invalid rather than being repaired. */
void pe_shield_clear_missing(PeGame *game);
#endif
