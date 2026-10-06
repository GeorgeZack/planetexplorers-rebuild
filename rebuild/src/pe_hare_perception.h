#ifndef PE_HARE_PERCEPTION_H
#define PE_HARE_PERCEPTION_H
#include "pe_world.h"

#define PE_HARE_VISION_PERIOD 20u
#define PE_HARE_HEARING_PERIOD 16u
#define PE_HARE_TARGET_PERIOD 15u
/* PEPerception.Awake: SceneStatic11, VFVoxelTerrain12, Building13,
 * TreeStatic14, Unwalkable16, NearTreePhysics21. Not the camera query mask. */
#define PE_HARE_PERCEPTION_BLOCK_MASK ((1u<<11)|(1u<<12)|(1u<<13)|(1u<<14)|(1u<<16)|(1u<<21))
#define PE_HARE_REPUTATION_NEUTRAL 4

typedef struct { bool present; int level; } PeHareReputation;
typedef struct {
    PeHareReputation forward,reverse;
    bool adventure,force_conflict;
    float initial_threat,damage_value; /* Source camp/DamageID table lookups. */
} PeHarePermissionInput;
typedef struct { bool can_attack,can_damage; } PeHarePermissions;
/* PEUtil gates differ: neutral blocks new attack acquisition; damage allows
 * neutral and, in Adventure, even higher reputation. Neither boolean alone
 * authorizes an attack/HP change. Invalid input leaves output untouched. */
bool pe_hare_perception_permissions(PeHarePermissionInput input,PeHarePermissions *out);

typedef struct { float radius,angle_degrees; } PeHareVisionConfig;
typedef struct { float radius; bool ignore_block; } PeHareHearingConfig;
typedef struct {
    bool present,has_view,alive,is_snake;
    PeVec3 position,center;
    float max_radius;
} PeHarePerceptionTarget;
/* The host supplies actual animated sensor origin and transformed local axis;
 * no root offset, yaw-only head mount or database sensor default is inferred. */
typedef struct { PeVec3 position,axis; } PeHareVisionPose;
typedef struct {
    const void *context;
    /* True means query succeeded; writes whether any source-mask obstruction
     * lies on origin->target. Must include real live tree blockers. Synthetic
     * crop planes and the camera's tree-excluding ray are not replacements. */
    bool (*blocked)(const void *,PeVec3 origin,PeVec3 target,uint32_t layer_mask,bool *out);
} PeHarePerceptionQueries;
typedef struct {
    bool ready,visible,heard;
    uint64_t self_id,target_id,vision_updates,hearing_updates,target_updates;
} PeHarePerception;
typedef struct {
    bool self_has_view,self_alive,local_controller,scan,can_search;
    bool already_enemy,carrier_allowed;
} PeHareCollectionGate;
typedef struct {
    bool tick_due,collection_ran,visible,heard,collection_candidate;
    PeHarePermissions permissions;
    uint64_t target_id;
    float initial_threat;
} PeHareAcquisition;
/* One bounded, stable sole-player identity. New source components start their
 * independent frame counters at0. Reinitialization resets cached sensor lists. */
bool pe_hare_perception_init(PeHarePerception *state,uint64_t self_id,uint64_t target_id);
/* Each call is ONE corresponding enabled component Update, independent of
 * simulation dt. Host supplies actual component execution order. Never call
 * once per catch-up simulation tick. Lists refresh every20/16 calls, remain
 * cached otherwise, and do not filter death (source sensor behavior). Callback
 * failure/invalid input is transactional. No callback occurs when not due or
 * an earlier radius/angle/view gate excludes the entity. Vision checks Snake
 * after its block ray, exactly as source; hearing does not check Snake. */
bool pe_hare_perception_vision(PeHarePerception *state,PeHareVisionConfig config,
    PeHareVisionPose pose,PeHarePerceptionTarget target,const PeHarePerceptionQueries *queries);
bool pe_hare_perception_hearing(PeHarePerception *state,PeHareHearingConfig config,
    PeVec3 sensor_position,PeHarePerceptionTarget target,const PeHarePerceptionQueries *queries);
/* One TargetCmpt.OnUpdate. Only each15th call consumes cached sensor membership.
 * collection_candidate models CollectEnemies/CanSelectEntity for a new target,
 * not Enemy.Hatred/CanDelete/SelectEnemy or persistent combat target memory.
 * Negative initial threat can also be collected; source escape arbitration is
 * deliberately not replaced with aggression. Host owns all final AI selection,
 * attack execution, target lifetime and damage. Required output is replaced on
 * valid calls (tick_due false off cadence); invalid calls change nothing. */
bool pe_hare_perception_collect(PeHarePerception *state,PeHarePerceptionTarget target,
    PeHareCollectionGate gate,PeHarePermissionInput relations,PeHareAcquisition *out);
#endif
