#ifndef PE_REVIVAL_H
#define PE_REVIVAL_H
#include "pe_passive_recovery.h"

/* Transient source no-ragdoll lifecycle; never serialized. */
typedef enum { PE_REVIVAL_IDLE, PE_REVIVAL_GETTING_UP, PE_REVIVAL_GRACE } PeRevivalPhase;
typedef struct {
    PeRevivalPhase phase;
    float last_time, enable_at;
    uint64_t frame;
} PeRevival;
typedef struct {
    bool death_mask, fall_mask, ride_mask, getup_ready, item_skill_ready;
} PeRevivalGate;
struct PeGame;
/* Native atomic preflight: the source UI starts action11 before attempting937.
 * Requires already-updated recovery owners at this exact display frame. Only
 * inventory, supported vitals and transient action/recovery state are staged. */
bool pe_game_revive(struct PeGame *game,PeAttributeFrame frame,PeRevivalGate gate);
/* GETTING_UP ends on a strictly later display frame; source no-ragdoll Update
 * has no animation wait. The fixed next-frame order is a native adapter.
 * GRACE disables incoming defence contacts until now>=enable_at. */
bool pe_revival_frame(PeRevival *state,PeAttributeFrame frame,bool alive);
bool pe_revival_frozen(const PeRevival *state);
bool pe_revival_damage_disabled(const PeRevival *state);
#endif
