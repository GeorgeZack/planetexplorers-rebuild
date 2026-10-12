#ifndef PE_HIT_IK_H
#define PE_HIT_IK_H
#include "pe_hitbox.h"
#include <stdbool.h>
#define PE_HIT_IK_PARTS 16u
#define PE_HIT_IK_EFFECTORS 9u
/* Disconnected female IKHitReaction. Exact source transform IDs select parts;
 * IDs are reference identities, never runtime pointer values. Caller owns the
 * reaction decision, independent RNG, controller presence and FinalIK solve.
 * No host calls this. All state is transient and unsaved. */
typedef struct {
    PeVec3 direction,root_start;
    float weight,duration,remaining;
} PeHitIkEffect;
typedef struct {PeHitIkEffect parts[PE_HIT_IK_PARTS];} PeHitIk;
typedef struct {float randomized_force,weight,duration;} PeHitIkProfile;
typedef struct {
    unsigned transform_id;
    PeVec3 direction,root_position;
    float weight,duration,weight_scale;
    bool controller_present,solver_present;
} PeHitIkStart;
typedef struct {
    PeVec3 root_position;
    float delta_time,master_weight,weight_scale;
    bool controller_present,solver_present,enabled;
} PeHitIkTick;
typedef struct {
    unsigned matched_parts,active_parts;
    /* Sum of additive positionOffset requests in source part/link order. */
    PeVec3 offsets[PE_HIT_IK_EFFECTORS];
} PeHitIkUpdate;
/* draw is the caller's independent Random.Range(-.1,.1) result. force is
 * Motion_Beat's already angle-scaled force, not damage or a raw skill force. */
bool pe_hit_ik_profile(float force,float draw,PeHitIkProfile *out);
/* Starts/restarts all matching parts. Direction is retained, not normalized.
 * Missing controller/solver or an unknown transform is a successful no-op. */
bool pe_hit_ik_begin(PeHitIk *state,PeHitIkStart input,PeHitIkUpdate *out);
/* Evaluate old elapsed time, fade over .1s, then subtract dt. Skipped modifier
 * passes preserve clocks. Changing weight_scale affects ongoing offsets too. */
bool pe_hit_ik_step(PeHitIk *state,PeHitIkTick input,PeHitIkUpdate *out);
/* Invalid input or arithmetic overflow leaves both state and output unchanged. */
#endif
