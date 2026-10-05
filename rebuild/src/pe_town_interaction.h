#ifndef PE_TOWN_INTERACTION_H
#define PE_TOWN_INTERACTION_H
#include "pe_game.h"
typedef struct {
    uint32_t entity_id;
    int prototype_id, store_id, item_id;
    unsigned price, limit, refresh_seconds;
} PeTownOffer;
/* One preserved settlement: source NPCstore.adven_single, supported catalog
 * only. Entity identity, not mission role or building ID, selects a vendor. */
size_t pe_town_shop_count(uint32_t entity_id);
const PeTownOffer *pe_town_shop_offer(uint32_t entity_id,size_t index);
/* Opening refreshes exhausted entries after strict source interval, resets the
 * creation timestamp of nonempty entries as UIShopWnd.UpdataShop does. */
bool pe_town_shop_open(PeGame *game,uint32_t entity_id);
unsigned pe_town_shop_stock(const PeGame *game,int store_id);
/* Call only for a currently eligible NPC interaction; the headless transaction
 * owns money/stock/inventory atomically. Caller owns physical range and modal UI. */
bool pe_town_shop_buy(PeGame *game,uint32_t entity_id,int store_id,unsigned count);
/* Save validator: rejects malformed clocks/stock and impossible progression. */
bool pe_town_state_valid(const PeGame *game);
/* Source4503 strict15m XZ in original world coordinates; no reward/next quest.
 * Requires source-backed world, supplies acceptance and above-ground player. */
bool pe_adventure_annot_step(PeGame *game);
#endif
