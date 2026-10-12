#ifndef PE_RAGDOLL_ACTION_H
#define PE_RAGDOLL_ACTION_H
#include "pe_hitbox.h"
#include <stdbool.h>
#include <stdint.h>
/* Disconnected admitted-action requests. No host calls this. Caller owns
 * MotionMgr admission, paused-action arbitration, exact parameter-entity bone
 * lookup, view state, deferred callbacks, ragdoll physics and every pose.
 * State is transient and unsaved. Initialize with .kind set to one of these. */
typedef enum {PE_RAGDOLL_WENTFLY=7,PE_RAGDOLL_KNOCKED=8,PE_RAGDOLL_GET_UP=10} PeRagdollKind;
typedef struct {uint64_t required_masks,forbidden_masks,pause_actions,end_actions,end_immediately_actions;} PeRagdollRelation;
typedef struct {
    PeRagdollKind kind;
    bool running,mask,freeze_requested,auto_get_up;
    float remaining;
} PeRagdollAction;
typedef enum {
    PE_RAGDOLL_SET_MASK, PE_RAGDOLL_FREEZE, PE_RAGDOLL_ACTIVATE,
    PE_RAGDOLL_DEACTIVATE, PE_RAGDOLL_INJURED,
    PE_RAGDOLL_CLEAR_DESIRED_MOVEMENT, PE_RAGDOLL_CANCEL_MOVE_REQUEST,
    PE_RAGDOLL_REQUEST_GET_UP
} PeRagdollOpType;
/* ACTIVATE value means has hit info (get-up-ready is always false here).
 * DEACTIVATE value means immediately. INJURED delay replaces any prior
 * pending view Invoke, and toggles defence-trigger admission, not an HP value.
 * GET_UP always means DoActionImmediately: bypass normal mask admission. */
typedef struct {PeRagdollOpType type;bool value;float delay;} PeRagdollOp;
#define PE_RAGDOLL_MAX_OPS 8u
typedef enum {
    PE_RAGDOLL_FAULT_NONE,PE_RAGDOLL_FAULT_PARAMETER,
    PE_RAGDOLL_FAULT_ENTITY_VIEW,PE_RAGDOLL_FAULT_UPDATE_VIEW
} PeRagdollFault;
typedef struct {
    bool view_present,ragdoll_active,motor_present;
    bool parameter_present,entity_present,entity_view_present,bone_present;
    /* Wentfly uses the parameter entity's exact ragdoll bone and its point.
     * Direction is multiplied by force without normalizing. */
    PeVec3 direction,hit_point;
    float force;
} PeRagdollStart;
typedef struct {float delta_time;bool view_present,ragdoll_active,ready_get_up;} PeRagdollTick;
typedef struct {
    bool started,reset,repeated_ignored,manager_complete;
    PeRagdollFault source_fault; /* reports source null dereferences, never performs them */
    unsigned op_count;PeRagdollOp ops[PE_RAGDOLL_MAX_OPS]; /* exact call order */
    PeVec3 hit_force,hit_normal,hit_point; /* only with ACTIVATE value true */
    uint64_t continue_actions; /* only on manager removal */
} PeRagdollUpdate;
bool pe_ragdoll_action_relation(PeRagdollKind kind,PeRagdollRelation *out);
/* Normal admission only; repeat and immediate dispatch bypass it in source. */
bool pe_ragdoll_action_source_admissible(PeRagdollKind kind,uint64_t masks);
bool pe_ragdoll_action_begin(PeRagdollAction *action,PeRagdollStart input,PeRagdollUpdate *out);
bool pe_ragdoll_action_step(PeRagdollAction *action,PeRagdollTick input,PeRagdollUpdate *out);
/* Wentfly graceful end clears mask/freeze and requests GetUp, leaving manager
 * removal to that dispatch. Other graceful ends use inherited EndImmediately.
 * Immediate end/model teardown never requests GetUp. */
bool pe_ragdoll_action_end(PeRagdollAction *action,bool view_present,bool immediately,PeRagdollUpdate *out);
/* Source helper only: caller supplies inherited settledSpeed and actual root
 * Rigidbody speed. Strict <; no invented settling clock. */
bool pe_ragdoll_ready_get_up(bool falling,float speed,float settled_speed,bool *out);
/* Call only when AddForce's next-FixedUpdate coroutine actually resumes and
 * finds a Rigidbody. This returns hitForce*100 for AddForceAtPosition with
 * default Force mode; it neither schedules nor applies a velocity impulse. */
bool pe_ragdoll_deferred_force(PeVec3 hit_force,PeVec3 *out);
/* Pure source class-keyed freeze reduction for these three owners. The native
 * three-bit representation also retains caller-supplied other class requests,
 * system freeze and network freeze. Repeated class requests are idempotent. */
typedef struct {unsigned classes;bool other_classes,system,network;} PeRagdollFreeze;
typedef struct {bool model_physics,ragdoll_physics;} PeRagdollPhysics;
bool pe_ragdoll_freeze_set(PeRagdollFreeze *state,PeRagdollKind kind,bool value,PeRagdollPhysics *out);
/* Invalid input/arithmetic leaves state and output unchanged. A source_fault
 * is a valid result retaining the source's partial requests up to the fault. */
#endif
