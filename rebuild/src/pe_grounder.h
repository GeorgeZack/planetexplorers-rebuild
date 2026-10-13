#ifndef PE_GROUNDER_H
#define PE_GROUNDER_H
#include "pe_fbbik.h"
#define PE_GROUNDER_LEGS 2u
/* Disconnected installed FinalIK GrounderFBBIK update for the female player:
 * Grounding.Update (root ray, both legs, pelvis), then the pelvis, foot and
 * spine requests GrounderFBBIK.OnSolverUpdate hands to the full-body solver.
 * It covers the serialized mode only: world-up grounding (rotateSolver off)
 * with the three-ray foot plane (quality Simple).
 *
 * No host calls this. Rays are a caller callback; the original casts against
 * Unity colliders on the serialized layers. Quaternion FromToRotation/Slerp
 * and Vector3 RotateTowards are conventional float adapters, and Leg.r starts
 * at identity where the original leaves a zero quaternion. Nothing here has
 * been compared with the original on uneven ground. State is transient and
 * unsaved; zero-initialize and call pe_grounder_init on host creation/load. */
typedef struct {
    float max_step,foot_speed,foot_radius,prediction;
    float foot_rotation_weight,foot_rotation_speed,max_foot_rotation_angle; /* degrees */
    float pelvis_speed,pelvis_damper,lower_pelvis_weight,lift_pelvis_weight;
    float spine_bend,spine_speed;
    /* GrounderFBBIK.spine by FullBodyBipedEffector ordinal; absent entries are zero. */
    float spine_horizontal[PE_FBBIK_EFFECTORS],spine_vertical[PE_FBBIK_EFFECTORS];
} PeGrounderSettings;
typedef struct {PeVec3 point,normal; float distance;} PeGrounderHit;
/* Physics.Raycast(origin, direction, out hit, max_distance, layers). Return
 * false for a miss and leave *hit alone. */
typedef bool (*PeGrounderRay)(void *context,PeVec3 origin,PeVec3 direction,float max_distance,PeGrounderHit *hit);
typedef struct {
    float last_time,ik_offset,height_from_ground;
    bool grounded;
    PeVec3 last_position,ik_position;
    PeFbbikQuat r,rotation_offset;
} PeGrounderLeg;
typedef struct {
    PeGrounderSettings settings;
    bool initiated;
    PeGrounderLeg leg[PE_GROUNDER_LEGS];
    float pelvis_last_time,pelvis_damper,pelvis_height;
    PeVec3 pelvis_last_root,spine_offset;
} PeGrounder;
typedef struct {
    float delta_time;                  /* Time.deltaTime at Initiate; the original stores it as lastTime */
    PeVec3 root_position,foot[PE_GROUNDER_LEGS];
} PeGrounderStart;
typedef struct {
    float time,delta_time;             /* Time.time and Time.deltaTime */
    bool enabled; float weight;        /* component enabled; Grounder.weight before its clamp */
    PeVec3 root_position,root_forward,root_right,root_up; /* BipedReferences.root axes */
    PeVec3 foot[PE_GROUNDER_LEGS];     /* left, right foot effector bone positions */
    PeGrounderRay ray; void *context;
} PeGrounderInput;
typedef struct {
    bool ran;                          /* false when disabled or weight <= 0: nothing is requested */
    bool grounded,root_grounded;
    PeVec3 pelvis_offset;              /* add to the pelvis bone's world position */
    /* Add to the foot effectors' positionOffset. Measured from the foot after
     * the pelvis move, taken as a pure translation of the leg. */
    PeVec3 foot_offset[PE_GROUNDER_LEGS];
    PeFbbikQuat foot_rotation[PE_GROUNDER_LEGS];  /* premultiply the foot bone's world rotation */
    PeVec3 effector_offset[PE_FBBIK_EFFECTORS];   /* spine requests; add to positionOffset */
} PeGrounderOutput;

const PeGrounderSettings *pe_grounder_female_settings(void);
bool pe_grounder_init(PeGrounder *grounder,const PeGrounderSettings *settings,PeGrounderStart start);
/* One GrounderFBBIK.OnSolverUpdate. Invalid or nonfinite input, or a
 * nonfinite result, leaves state and output unchanged. */
bool pe_grounder_update(PeGrounder *grounder,const PeGrounderInput *input,PeGrounderOutput *output);
#endif
