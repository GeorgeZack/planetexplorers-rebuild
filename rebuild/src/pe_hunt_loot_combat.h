#ifndef PE_HUNT_LOOT_COMBAT_H
#define PE_HUNT_LOOT_COMBAT_H
#include "pe_hunt_combat.h"
#include "pe_game.h"

/* Adventure host transaction: contact/HP/wear/kill state and independent loot
 * creation commit together. Only actual contact deaths create drops, including
 * additional deaths in the same contact batch as the fourth quest credit.
 * Borrowed assets remain borrowed; this never clones an owning PeGame/world.
 * Failure preserves game, combat, melee and every caller output. */
bool pe_hunt_loot_combat_step(PeGame *game,PeHuntCombat *combat,
    const PeSandboxCollision *collision,PeHuntCombatPlayer player,
    PeSandboxMelee *melee,const PeSandboxMeleeFrame *frame,
    PeSandboxMeleeUpdate *update,bool *completed);
#endif
