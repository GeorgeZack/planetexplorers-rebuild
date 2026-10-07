#ifndef PE_TOWN_TRADE_H
#define PE_TOWN_TRADE_H
#include "pe_game.h"
/* Pure source item quote. Truncate each condition-adjusted unit before quantity
 * multiplication. Unsupported source condition components fail closed. Outputs
 * are unchanged on failure; either output may be NULL. */
bool pe_town_trade_price(const PeSlot *slot,uint32_t *unit_sell,uint32_t *base_currency);
/* Session-only source repurchase ownership. Empty state is valid before old
 * save migration; nonempty records must be packed and uniquely owned. */
bool pe_town_trade_state_valid(const PeGame *game);
size_t pe_town_trade_count(const PeGame *game,uint32_t entity_id);
const PeSlot *pe_town_trade_at(const PeGame *game,uint32_t entity_id,size_t row);
/* Pure admission probes include both purses, quantity, bag capacity and exact
 * ownership. Sell uses a physical bag slot, never an aggregate item count.
 * Selected primary/shield equipment must first be unequipped. */
bool pe_town_trade_can_sell(const PeGame *game,uint32_t entity_id,int bag_slot,unsigned count);
bool pe_town_trade_can_buyback(const PeGame *game,uint32_t entity_id,size_t row,unsigned count);
/* Atomic transactions. Failure preserves all gameplay ownership and currency;
 * only the UI message may change. Success appends sold rows without merging;
 * exhausting a repurchase row compacts that vendor's list. Type shortcuts stay
 * assigned; sold exact-instance shortcuts clear and are not auto-restored. */
bool pe_town_trade_sell(PeGame *game,uint32_t entity_id,int bag_slot,unsigned count);
bool pe_town_trade_buyback(PeGame *game,uint32_t entity_id,size_t row,unsigned count);
#endif
