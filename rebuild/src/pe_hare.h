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
typedef struct {
    bool ready, grounded, goal_valid;
    PeVec3 spawn, position, velocity, patrol_goal;
    float scale, radius, height, max_hp, attack, defense;
    float yaw_degrees, mode_age, mode_duration, stuck_seconds, retarget_age;
    uint64_t rng;
    uint32_t decisions;
    PeHareMode mode;
} PeHare;

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
