#ifndef PE_GROUNDER_HOST_H
#define PE_GROUNDER_HOST_H
#include "pe_grounder.h"
/* Host adapter for the female player's GrounderFBBIK, shared by the sandbox
 * and tutorial hosts: one call per drawn frame, where the original runs it in
 * LateUpdate, before the full-body solve.
 *
 * Source rules: pe_grounder is the update; Motion_Move_Human writes the foot
 * rotation limit from HumanPhyCtrl.forwardGroundAngle every frame; IKCmpt
 * zeroes the spine bend while hand equipment holds its spine mask.
 *
 * Adapters, not original behavior: the ray is the host's own geometry, not
 * Unity colliders on the serialized layers; the ground angle comes from one
 * ray under the root standing in for the capsule's contact normals, used
 * while the host's controller reports ground; the root axes are the drawn
 * body yaw about world up. Presentation only: nothing here moves the
 * controller, a capsule or a contact query. State is transient and never
 * saved; zero-initialize. */
typedef struct {PeVec3 pelvis_offset; PeFbbikQuat foot_rotation[PE_GROUNDER_LEGS];} PeGrounderPose;
typedef struct {
    PeGrounder grounder;
    bool started;
    float forward_ground_angle;
    /* The latest update, in the drawn model's frame (yaw removed). */
    bool active;                                  /* false: draw without grounding */
    PeGrounderPose pose;                          /* applied to the animated pose before the solve */
    PeVec3 effector_offset[PE_FBBIK_EFFECTORS];   /* added to positionOffset: feet and spine */
    bool grounded,root_grounded;
} PeGrounderHost;
typedef struct {
    float time,delta_time;             /* drawn-frame clock; delta_time > 0 */
    PeVec3 root; float yaw_degrees;    /* drawn actor root and body yaw */
    PeVec3 foot[PE_GROUNDER_LEGS];     /* animated foot bones in the world, before any IK */
    bool controller_grounded;          /* the host controller stands on something */
    bool spine_effect;                 /* no hand equipment held */
    PeGrounderRay ray; void *context;
} PeGrounderHostInput;
/* Invalid input or a failed update clears active and keeps the stored state. */
bool pe_grounder_host_update(PeGrounderHost *host,const PeGrounderHostInput *input);
/* Load, respawn, teleport, death or mode change: the next update initiates again. */
void pe_grounder_host_reset(PeGrounderHost *host);
#endif
