#ifndef PE_ANIMFOLLOW_H
#define PE_ANIMFOLLOW_H
#include "pe_hitbox.h"
#include <stdbool.h>
#include <stdint.h>
/* Disconnected installed female AnimFollow controller requests. No host calls
 * this. Physics/PD error, Quaternion alignment, actual Animator evaluation,
 * water/ray queries, transforms/joints and scene scheduling remain caller-owned.
 * This is not a ragdoll solve or a pose. All state is transient and unsaved. */
typedef enum {
    PE_AF_UNKNOWN,PE_AF_GROUND_MOVE,PE_AF_WATER_MOVE,PE_AF_MONSTER_IDLE,
    PE_AF_FRONT,PE_AF_BACK,PE_AF_FRONT_MIRROR,PE_AF_BACK_MIRROR,
    PE_AF_FRONT_WATER,PE_AF_BACK_WATER,PE_AF_FRONT_MIRROR_WATER,PE_AF_BACK_MIRROR_WATER
} PeAnimFollowState;
typedef enum {PE_AF_NO_SIGNAL,PE_AF_FRONT_SIGNAL,PE_AF_BACK_SIGNAL,PE_AF_FRONT_MIRROR_SIGNAL,PE_AF_BACK_MIRROR_SIGNAL} PeAnimFollowSignal;
typedef struct {
    bool initialized,active,wrapper_active,get_up_ready,shot_by_bullet;
    bool falling,getting_up,orientate,orientated,delayed_get_up_done,joint_limits,fell_on_speed;
    bool get_up_state,transition_to_get_up,was_transition_to_get_up;
    uint64_t calls;
    float torque,force,joint_torque,drag,angular_drag,contact_time,no_contact_time;
    unsigned secondary_update;
} PeAnimFollow;
typedef struct {
    float fixed_delta_time,limb_error_magnitude,root_speed,collision_speed;
    unsigned collision_count;
    bool missing_assignments,animator_present,controller_present,in_get_up_transition;
    PeAnimFollowState current_state;
    /* Predicate inputs from original world queries. See the audit for ray
     * mask6144, root-.5 water check and height+.5 downward-ray distance. */
    bool master_entity_animator_present,root_under_water,no_ground_below;
    PeVec3 root_position;float entity_height;
    float forward_dot_down; /* (ragdollRotation * cachedAlignment * forward) dot down */
} PeAnimFollowTick;
typedef struct {
    bool capture_root_alignment,align_master_horizontal,set_master_position;
    PeVec3 master_position;
    bool set_water_bool,water_bool;
    bool on_fall_begin,on_fall_finished,on_get_up_begin,on_get_up_finished;
    bool active_changed,active,sync_transforms,renderer_swap,body_policy_changed;
    bool restore_friction,zero_friction,set_joint_limits,joint_limits,clear_collision_speed;
    bool set_joint_torque;float joint_torque_request;
    bool set_animator_speed;float animator_speed;
    PeAnimFollowSignal signal; /* compiled SetBool(hash,true), not an assumed SetTrigger */
    bool source_missing_animator_fault;
} PeAnimFollowUpdate;
/* Exact full-path/name classifiers from installed HashIDs strings. These
 * native enums do not reproduce Unity's StringToHash or evaluate an Animator. */
bool pe_animfollow_state(const char *full_path,PeAnimFollowState *out);
bool pe_animfollow_get_up_transition(const char *name);
const char *pe_animfollow_signal_name(PeAnimFollowSignal signal);
const char *pe_animfollow_lock_tag(void);
/* Fresh qualified female snapshot: serialized settings plus constructor/Awake
 * state. Assignment validation, friction snapshots and root alignment belong
 * to the caller. Existing state can be reset on host creation/load. */
bool pe_animfollow_init(PeAnimFollow *state);
/* PERagdollController.Activate: a repeated request still sets shot/readiness;
 * its active setter only resyncs/swaps body policy when the value changes. */
bool pe_animfollow_activate(PeAnimFollow *state,bool get_up_ready,PeAnimFollowUpdate *out);
bool pe_animfollow_deactivate(PeAnimFollow *state,bool immediately,PeAnimFollowUpdate *out);
/* One DoRagdollControl call, before the actual PD drive. It reads the previous
 * drive's totalForceError, even when AnimFollow is inactive; no native 60Hz
 * scheduling or synthetic error decay is supplied. */
bool pe_animfollow_step(PeAnimFollow *state,PeAnimFollowTick input,PeAnimFollowUpdate *out);
/* Per-body active setter policy. RagdollLock bodies stay kinematic, but their
 * colliders follow the same active admission as other bodies. */
typedef struct {bool kinematic,collider_trigger,collider_enabled;} PeAnimFollowBodyPolicy;
PeAnimFollowBodyPolicy pe_animfollow_body_policy(bool active,bool ragdoll_lock);
/* Invalid input/arithmetic leaves state and output unchanged. */
#endif
