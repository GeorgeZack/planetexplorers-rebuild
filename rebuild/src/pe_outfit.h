#ifndef PE_OUTFIT_H
#define PE_OUTFIT_H
#include <stdbool.h>
#include <stdint.h>
#define PE_OUTFIT_SLOTS 3
/* Bounded source starter clothes, separate from bag and held-tool ownership. */
typedef struct { int item_id; uint32_t instance_id; float durability; } PeWornItem;
typedef struct { PeWornItem items[PE_OUTFIT_SLOTS]; } PeOutfit;
struct PeGame;
/* Standalone combat may supply an entirely empty outfit. Partial/malformed
 * outfits are invalid; the supported nonempty order is female95/131/192. */
bool pe_outfit_valid(const PeOutfit *outfit);
/* Fresh zero outfit only. Adventure allocates after existing inventory IDs;
 * Legacy remains zero. Failures preserve outfit and allocator exactly. */
bool pe_outfit_init(struct PeGame *game);
/* Adventure requires all three source pieces, including exhausted instances;
 * Legacy requires zero. Reject worn aliases with every other owner/reference. */
bool pe_outfit_state_valid(const struct PeGame *game);
/* Base Defence stays separate. Exhausted pieces contribute0; invalid→NAN. */
float pe_outfit_defense(const PeOutfit *outfit);
/* One accepted hit's calculated damage, before remaining-HP clamping. Each
 * piece loses max(0, damage*source factor), clamped at0. Negative finite damage
 * is a no-op, never repair. Invalid input is atomic. Caller owns Build bypass. */
bool pe_outfit_wear(PeOutfit *outfit,float calculated_damage);
#endif
