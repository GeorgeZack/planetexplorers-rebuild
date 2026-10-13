#ifndef PE_ANIMFOLLOW_DRIVE_H
#define PE_ANIMFOLLOW_DRIVE_H
#include "pe_world.h"
#include <stdbool.h>
#include <stdint.h>
#define PE_AF_DRIVE_BODIES 11u
/* Disconnected installed female drive requests, not a physics or pose solver.
 * Call the lifecycle controller FIRST, with the previous total_force_error;
 * then pass its new active/caps/drags/secondary_update here. No host calls this.
 * Quaternion ToAngleAxis, joint-space target rotation and nonrigid Quaternion
 * Lerp remain caller-owned. State is transient; never serialize it. */
typedef struct {
    bool initialized,torque_enabled,force_enabled,mimic_nonrigids;
    int32_t frame_counter;
    uint16_t joint_mask;
    float reciprocal_fixed_delta_time,joint_torque,joint_damping;
    float drive_spring,drive_damper,drive_maximum_force;
    PeVec3 torque_last_error[PE_AF_DRIVE_BODIES],force_last_error[PE_AF_DRIVE_BODIES];
    PeVec3 total_torque_error,total_force_error,torque_signal,force_signal;
} PeAnimFollowDrive;
typedef struct {
    PeVec3 master_position,slave_position;
    /* Engine result for master.worldRotation * inverse(slave.worldRotation).
     * Axis is retained literally; no invented normalization or quaternion. */
    float angle;PeVec3 axis;
} PeAnimFollowDriveBodyInput;
typedef struct {
    bool missing_assignments,active;
    int32_t secondary_update;
    float torque_cap,force_cap,joint_cap,joint_damping,drag,angular_drag;
    PeAnimFollowDriveBodyInput bodies[PE_AF_DRIVE_BODIES];
} PeAnimFollowDriveTick;
typedef struct {
    bool set_drag,apply_torque,apply_force,set_target_rotation;
    float drag,angular_drag;
    PeVec3 torque,force,force_position;
    unsigned force_mode; /* literal installed AddTorque/AddForceAtPosition mode2 */
} PeAnimFollowDriveBodyRequest;
typedef struct {
    bool apply;float spring,damper,maximum_force;unsigned mode;
} PeAnimFollowJointRequest;
typedef struct {
    bool driven,capture_nonrigid,write_nonrigid,blend_nonrigid;
    unsigned nonrigid_begin,nonrigid_end;float nonrigid_lerp;
    PeAnimFollowDriveBodyRequest bodies[PE_AF_DRIVE_BODIES];
    PeAnimFollowJointRequest joints[PE_AF_DRIVE_BODIES];
    PeVec3 total_torque_error,total_force_error;
} PeAnimFollowDriveUpdate;
typedef struct {unsigned master_transform,slave_transform,rigidbody,joint;} PeAnimFollowDriveBinding;
bool pe_animfollow_drive_binding(unsigned index,PeAnimFollowDriveBinding *out);
/* Awake caches reciprocal dt once and takes the last joint's SlerpDrive.
 * Initial one-argument SetJointTorque preserves that cached damper. */
bool pe_animfollow_drive_init(PeAnimFollowDrive *state,float fixed_delta_time);
bool pe_animfollow_drive_step(PeAnimFollowDrive *state,PeAnimFollowDriveTick input,PeAnimFollowDriveUpdate *out);
/* SetJointTorque overloads: with_damping=false preserves the shared cached
 * damper, not each joint's prior damper. Root0 and null-mask joints are skipped. */
bool pe_animfollow_drive_set_joints(PeAnimFollowDrive *state,float torque,bool with_damping,float damping,
                                   PeAnimFollowDriveUpdate *out);
/* Source angular X/Y/Z motion integers: enabled1, disabled2. Copy to each
 * present nonroot joint; linear motion is unchanged. */
bool pe_animfollow_drive_joint_limits(const PeAnimFollowDrive *state,bool enabled,uint16_t *mask,unsigned *motion);
/* SyncTransforms copies world position THEN rotation only for present body
 * and master pairs. This is admission, not a Transform or exception solve. */
bool pe_animfollow_drive_sync(uint16_t bodies,uint16_t masters,uint16_t *out);
/* Native finite-input/overflow checks fail atomically. Float arithmetic and
 * ClampMagnitude are adapters; no bit-exact Mono/Unity/PhysX claim. */
#endif
