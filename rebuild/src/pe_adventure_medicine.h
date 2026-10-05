#ifndef PE_ADVENTURE_MEDICINE_H
#define PE_ADVENTURE_MEDICINE_H
#include "pe_game.h"
enum { PE_MEDICINE_NONE=0, PE_MEDICINE_ACTIVE=1, PE_MEDICINE_COMPLETE=2 };
/* Saved quest status, not dialogue position. Readiness is current possession,
 * so dropping/using the target items cannot leave a stale turn-in flag. */
bool pe_adventure_medicine_state_valid(const PeGame *game);
bool pe_adventure_medicine_ready(const PeGame *game);
/* Caller verifies the actual Naoko encounter and owns modal input. Acceptance
 * is the source4097 choice; completion is only the final4100 acknowledgment.
 * Inventory and progression commit together; failures permit a later retry. */
bool pe_adventure_medicine_accept(PeGame *game);
bool pe_adventure_medicine_complete(PeGame *game);
#endif
