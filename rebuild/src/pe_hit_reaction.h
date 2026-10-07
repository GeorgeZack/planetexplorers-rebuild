#ifndef PE_HIT_REACTION_H
#define PE_HIT_REACTION_H
#include <stdbool.h>

/* Source Motion_Beat effect selection, disconnected from both gameplay hosts.
 * Caller owns permission, supported female model, current summed threshold
 * attributes and Vector3.Angle(forward,forceDir). Selecting an action is not
 * admission or execution: no masks, buffs, damage, animation or motion change. */
typedef enum {
    PE_HIT_REACTION_NONE, PE_HIT_REACTION_WHACKED, PE_HIT_REACTION_REPULSED,
    PE_HIT_REACTION_WENTFLY, PE_HIT_REACTION_KNOCKED
} PeHitReactionEffect;
typedef struct {
    float angle_degrees, force, thresholds[4];
    bool in_air;
} PeHitReactionInput;
typedef struct {
    PeHitReactionEffect effect;
    unsigned requested_action; /* Source PEActionType: None0, Whacked5, Repulsed6, Wentfly7. */
    float scaled_force, threshold_scale;
    bool request_hit_ik; /* No IK curve, randomness or pose is evaluated here. */
} PeHitReactionDecision;

/* Exported snapshots only; `shield_buff_mounted` is not a request to block.
 * This does not infer buff presence from selected shield, wear or key state. */
bool pe_hit_reaction_player_thresholds(bool shield_buff_mounted,float out[4]);
/* Exact supported hare event force; unknown IDs fail without changing output. */
bool pe_hit_reaction_hare_force(int trigger_id,float *force);
/* All outputs remain unchanged on failure. Angle must be finite in[0,180],
 * force/thresholds finite and nonnegative. Preserve source comparison order;
 * callers need not sort thresholds. */
bool pe_hit_reaction_decide(PeHitReactionInput input,PeHitReactionDecision *out);
#endif
