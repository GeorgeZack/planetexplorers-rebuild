#ifndef PE_HUMAN_PHY_H
#define PE_HUMAN_PHY_H
#include "pe_hitbox.h"
#include <stdbool.h>
#include <stddef.h>

/* Disconnected source HumanPhyCtrl rules for the female controller: movement
 * request latch, ground/water/drag state and the per-FixedUpdate force
 * requests. Read from installed CIL; no host calls this.
 *
 * Nothing here integrates, collides or moves a body. The caller owns the
 * Rigidbody/capsule, contact generation, water queries and transform, and the
 * original callback order:
 *   FixedUpdate x n -> physics step -> Update -> LateUpdate.
 * A request made during Update is latched by that frame's LateUpdate and first
 * consumed by the following frame's FixedUpdate calls.
 * All state is transient; zero-fill then pe_human_phy_init on host creation/load. */
typedef struct {
    PeVec3 request_velocity; /* m_RequestSubVelocity: summed since the last LateUpdate */
    PeVec3 applied_velocity; /* m_ApplySubVelocity: what FixedUpdate consumes */
    PeVec3 inertia_velocity,desired_direction,gravity_up,water_acc,sub_acc;
    float move_speed,net_move_speed_scale,gravity,drag,forward_ground_angle;
    float impact_time,last_ground_time,water_to_root,water_height;
    int fixed_updates; /* since the last Update */
    bool is_controller,freeze_update,use_rope_gun;
    bool phy_ground,grounded,fall_ground,feet_in_water,spine_in_water,head_in_water;
} PeHumanPhy;
typedef struct {
    float time,delta_time; /* Time.time and Time.deltaTime */
    PeVec3 position,forward;
    /* Contact normals reported since the previous Update for colliders on the
     * source ground layers, in report order. */
    const PeVec3 *ground_normals; size_t ground_normal_count;
    bool rigidbody_kinematic;
    bool overlap_ground; /* Non-controller OverlapSphere result; unused by the controller. */
    /* PE.PointInWater at the root [0] and .5 above it [1]; the source samples
     * whichever matches the grounded state it has just computed. */
    float point_in_water[2];
    bool water_ray_hit; float water_ray_y; /* 2 m down from root+height on the water layer */
} PeHumanPhyFrame;
typedef struct {
    PeVec3 velocity; /* Rigidbody.velocity as read on entry; see pe_human_phy_fixed_update */
    PeVec3 position,up; /* transform.position and transform.up */
    bool rigidbody_kinematic;
    float fixed_delta_time;
} PeHumanPhyStep;
typedef struct {
    bool ran; /* false: frozen, or a third or later call since Update; nothing else is set */
    bool kinematic,zero_velocity; /* kinematic branch: Rigidbody.velocity=0, no forces */
    bool move_position; PeVec3 move_position_target; /* kinematic non-controller only */
    bool set_position; PeVec3 position; /* water-surface hold or below-world rescue */
    PeVec3 velocity_change; /* sum of ForceMode.VelocityChange requests, source order preserved */
    PeVec3 acceleration;    /* sum of ForceMode.Acceleration requests */
} PeHumanPhyForces;

void pe_human_phy_init(PeHumanPhy *phy);
/* ResetSpeed: floors at .1, then multiplies by serialized mSpeedTimes. */
bool pe_human_phy_reset_speed(PeHumanPhy *phy,float speed);
/* Sums into the pending request; a magnitude strictly above MaxSubSpeed 10 is
 * rescaled to exactly that length. Callers in one frame therefore add. */
bool pe_human_phy_apply_move_request(PeHumanPhy *phy,PeVec3 velocity);
/* Clears pending and applied requests at once; desired_direction is separate. */
bool pe_human_phy_cancel_move_request(PeHumanPhy *phy);
/* State half of ApplyImpact; the caller queues the impact as a VelocityChange. */
bool pe_human_phy_apply_impact(PeHumanPhy *phy,float time);
/* Update: clears the FixedUpdate counter, then ground, water and drag state in
 * source order. Ground uses the previous frame's spine_in_water. */
bool pe_human_phy_update(PeHumanPhy *phy,const PeHumanPhyFrame *frame);
/* LateUpdate: inertia=velocity, applied=pending, pending=0. An unrenewed
 * request therefore lasts exactly one frame's FixedUpdate calls. */
bool pe_human_phy_late_update(PeHumanPhy *phy,PeVec3 rigidbody_velocity);
/* One FixedUpdate. Every source velocity read uses step.velocity: that queued
 * forces do not alter Rigidbody.velocity within the call is an unqualified
 * PhysX assumption. Outputs are requests; integration and collision response
 * remain the caller's. Invalid input leaves state and output unchanged. */
bool pe_human_phy_fixed_update(PeHumanPhy *phy,PeHumanPhyStep step,PeHumanPhyForces *forces);
#endif
