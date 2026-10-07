#ifndef PE_TOWN_WALLET_H
#define PE_TOWN_WALLET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
struct PeGame;
typedef struct {
    uint32_t entity_id,initial_balance,serialized_max,daily_increase;
    bool initial_added;
} PeTownWalletSource;
/* Stable archive profiles, shared by transactions, persistence and exporters.
 * serialized_max is provenance only: source Update does not cap balances. */
const PeTownWalletSource *pe_town_wallet_source_at(size_t index);
/* Fresh/pre-v17 initialization only, with a zero wallet array. Atomic. The
 * zero guard cannot distinguish a legitimately depleted current save; its
 * caller must never invoke initialization as current-version repair. */
bool pe_town_wallet_init(struct PeGame *game);
bool pe_town_wallet_state_valid(const struct PeGame *game);
/* One observed source update using game_seconds and the configured26-hour day.
 * Morning resets the latch; noon/afternoon adds once. No elapsed-day catch-up.
 * All eight commit together; overflow beyond signed32-bit fails closed. */
bool pe_town_wallet_step(struct PeGame *game);
/* Exact archived entity identity; failure leaves output untouched. */
bool pe_town_wallet_balance(const struct PeGame *game,uint32_t entity_id,uint32_t *out);
#endif
