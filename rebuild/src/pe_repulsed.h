#ifndef PE_REPULSED_H
#define PE_REPULSED_H
#include "pe_hitbox.h"
#include <stdbool.h>
#include <stdint.h>

/* Disconnected source Action_Repulsed arithmetic and admitted-action requests.
 * No host calls this. Caller owns MotionMgr admission/pause ordering, original
 * Vector3.Angle/ProjectOnPlane, physics, animation, IK and force permission.
 * Neither equipment nor a selected reaction automatically enables shielding.
 * All state is transient; zero-initialize on host creation/load. */
typedef struct {
    float velocity,move_time,stop_time,wentfly_time;
} PeRepulsedProfile;
typedef struct {uint64_t forbidden_masks,pause_actions,end_immediately_actions;} PeRepulsedRelation;
typedef struct {
    bool running,blocked,stop_applied;
    PeVec3 direction; /* Retained after EndImmediately, as in the source. */
    float force,velocity,move_time,stop_time,wentfly_time,elapsed;
} PeRepulsed;
typedef struct {
    float force,previous_direction_angle;
    /* Source Angle(-entity.forward, old m_MoveDir), never the new hit direction.
     * The owner handles its zero direction as 90 using pinned Unity5 behavior;
     * caller evaluates the angle for a nonzero previous direction.
     * force is the already angle-scaled Motion_Beat request. Positions and hit
     * direction describe the active PEActionParamVVF.param singleton at DoAction,
     * rather than a parameter retained while the manager waits. */
    PeVec3 projected_direction,position,hit_position;
    bool shield_running,animator_present,move_motor_present;
} PeRepulsedStart;
typedef struct {
    float delta_time;
    bool context_present,animator_present,human_phy_present,move_motor_present;
    bool wentfly_bone_present;
} PeRepulsedTick;
typedef struct {
    bool started,ended,repeat_ignored,set_mask,mask;
    bool set_position; PeVec3 position;
    bool set_initial_block,blend_anim_parameters,reset_anim_parameters;
    float block_target,anim_blend_fraction;
    bool apply_move_request,apply_motor_force,cancel_move_request,clear_desired_direction;
    PeVec3 move_request,motor_force;
    bool request_halt,request_wentfly;
    PeVec3 wentfly_direction; /* Retained direction; caller normalizes again for Wentfly. */
    float wentfly_force;
    /* End resumes the exact row6 paused actions, even if Wentfly is denied. */
    uint64_t continue_actions;
} PeRepulsedUpdate;

PeRepulsedRelation pe_repulsed_relation(void);
/* Admission only; the source manager bypasses dependencies for a running
 * action, then invokes inherited no-op ResetAction. Masks are source ordinals. */
bool pe_repulsed_source_admissible(uint64_t source_masks);
bool pe_repulsed_profile(float force,PeRepulsedProfile *profile);
/* Called only when source admission/waits permit DoAction and the transform /
 * bound BeatParam exist. A repeated running request ignores incoming fields.
 * Direction is the caller's zero or normalized plane-projected hit direction.
 * begin emits only the generic-motor force; HumanPhy starts on Update. */
bool pe_repulsed_begin(PeRepulsed *action,PeRepulsedStart input,PeRepulsedUpdate *update);
/* Exactly one source Update: tests old elapsed, emits velocity requests, then
 * adds delta_time. No displacement or hidden catch-up ticks. Wentfly wins over
 * elapsed>=move_time, regardless of whether the larger manager admits it.
 * Missing context follows source immediate cleanup. */
bool pe_repulsed_step(PeRepulsed *action,PeRepulsedTick input,PeRepulsedUpdate *update);
bool pe_repulsed_cancel(PeRepulsed *action,PeRepulsedTick context,PeRepulsedUpdate *update);
/* Finite input/output and bounded direction validation fail atomically. */
#endif
