#ifndef PE_DEBUG_H
#define PE_DEBUG_H
#include "pe_game.h"

typedef enum {
    PE_DEBUG_NONE,
    PE_DEBUG_MEDICINE,
    PE_DEBUG_COMBAT_SUPPLIES,
    PE_DEBUG_START_HUNT
} PeDebugAction;

/* Explicit native cheats, not source quest transactions. The host owns the
 * session-only opt-in and must preflight encounter assets before START_HUNT.
 * Successful hunt travel requires the host's ordinary load-style transient
 * reset; medicine/supplies must retain the current encounter and action clocks.
 * No save is written. Failure preserves the entire game, including messages,
 * RNG, pending world loot, repair ownership and construction history. */
bool pe_debug_apply(PeGame *game,PeDebugAction action,char *error,size_t capacity);
#endif
