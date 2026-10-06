#ifndef PE_HARE_H
#define PE_HARE_H
#include "pe_scene_collision.h"
#define PE_HARE_DT (1.0f/60.0f)
#define PE_HARE_PROTOTYPE 13u
#define PE_HARE_SPAWN_ID 261u

typedef enum { PE_HARE_IDLE, PE_HARE_PATROL, PE_HARE_APPROACH, PE_HARE_RETREAT, PE_HARE_HOLD } PeHareMode;
typedef enum { PE_HARE_MELEE, PE_HARE_POUNCE } PeHareApproach;
typedef struct {
    /* Caller supplies the result of perception/reputation/hostility selection.
     * This module does not invent a sensor cone, damage event or aggro timer. */
    bool accepted;
    PeVec3 position, velocity;
    float radius, height, yaw_degrees;
    PeHareApproach approach;
} PeHareTarget;
typedef struct {
    bool present, unobstructed, melee_geometry, pounce_geometry;
    float gap, facing_degrees;
} PeHareEngagement;

/* Borrowed query snapshot; never stored in PeHare or a save. Sweep/overlap use
 * creature movement geometry; ray is real support/LOS geometry (no synthetic
 * crop planes). Refresh the sandbox snapshot before calling any query API. */
typedef struct {
    const void *context;
    bool (*overlap)(const void *,PeVec3,float,float);
    PeSceneHit (*sweep)(const void *,PeVec3,float,float,PeVec3);
    PeSceneHit (*ray)(const void *,PeVec3,PeVec3,float);
    bool require_real_support;
} PeHareQueries;
typedef struct {
    float minimum_radius,maximum_radius;
    bool spawn_center,unchecked_fallback;
} PeHarePatrol;
typedef struct { PeVec3 spawn; float yaw_degrees; PeHarePatrol patrol; } PeHareConfig;
struct PeSandboxCollision;
PeHareQueries pe_hare_queries_scene(const PeSceneCollision *scene);
PeHareQueries pe_hare_queries_sandbox(const struct PeSandboxCollision *collision);
PeHareConfig pe_hare_tutorial_config(void);
/* Source Adventure defaults: current center, radial distance uniform15..30.
 * Reject unsupported/out-of-crop candidates rather than the source raw final
 * fallback; bounded native navigation policy, not full BTPatrol/A* parity. */
PeHareConfig pe_hare_adventure_config(PeVec3 spawn,float yaw_degrees);

typedef struct {
    bool ready, grounded, goal_valid;
    PeVec3 spawn, position, velocity, patrol_goal;
    float scale, radius, height, max_hp, attack, defense;
    float yaw_degrees, mode_age, mode_duration, stuck_seconds, retarget_age;
    uint64_t rng;
    uint32_t decisions;
    PeHareMode mode;
    PeHarePatrol patrol;
} PeHare;

/* General fixed-step foundation. Explicit spawn is grounded within5m without
 * horizontal relocation/lift or any mission-specific spawn-height adjustment.
 * Query/configuration failure is transactional; accepted targets remain the
 * host's decision. No attack, current HP, perception or mission ownership here. */
bool pe_hare_init_query(PeHare *hare,const PeHareQueries *queries,const PeHareConfig *config,
                        uint64_t seed,float scale,bool easy);
bool pe_hare_step_query(PeHare *hare,const PeHareQueries *queries,const PeHareTarget *target);
PeHareEngagement pe_hare_engagement_query(const PeHare *hare,const PeHareQueries *queries,
                                         const PeHareTarget *target);

/* Transactional initial spawn (15,2,12), grounded using the scene capsule.
 * scale==0 chooses deterministic uniform [.8,1.3); explicit values must lie in
 * that source interval. Easy halves scaled HP/Atk/Def only. No current HP here. */
bool pe_hare_init(PeHare *hare,const PeSceneCollision *scene,uint64_t seed,
                  float scale,bool easy);
/* One fixed 60Hz step, no allocation. NULL or unaccepted target means leisure.
 * Source speeds2.5/6.5/retreat2, yaw150deg/s, patrol radius30/time10..20,
 * retarget distance2/stuck2s; source idle6..8 and patrol acceptance.6.
 * The leisure scheduler/PRNG, grounding/slide response, capsule bounds proxy
 * and LOS midpoint are explicit C approximations, not a full behaviour tree.
 * A bad argument/state leaves the hare unchanged. Never deals damage or emits
 * a kill/mission event. Reaching attack geometry merely holds position. */
bool pe_hare_step(PeHare *hare,const PeSceneCollision *scene,const PeHareTarget *target);
/* Geometry eligibility only; excludes attack cooldowns/skill availability and
 * animated hit windows. Bounds-point gap follows source TargetCmpt's method
 * using yaw-rotated capsule AABBs as the available entity-bounds proxy. */
PeHareEngagement pe_hare_engagement(const PeHare *hare,const PeSceneCollision *scene,
                                   const PeHareTarget *target);
#endif
