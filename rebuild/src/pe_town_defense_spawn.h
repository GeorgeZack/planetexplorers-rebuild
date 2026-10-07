#ifndef PE_TOWN_DEFENSE_SPAWN_H
#define PE_TOWN_DEFENSE_SPAWN_H
#include "pe_world.h"
/* Disconnected source9077 planner and transient member owner. No PeGame,
 * acceptance, geometry queries, actor creation, clock or save integration. */
#define PE_TOWN_DEFENSE_WAVE_CAPACITY 11
#define PE_TOWN_DEFENSE_AGENT_CAPACITY 22

typedef struct {
    void *context;
    bool (*integer)(void *context,int minimum,int maximum_exclusive,int *value);
    bool (*real)(void *context,float minimum,float maximum_inclusive,float *value);
} PeTownDefenseDraws;
typedef struct {
    int spawn_type,prototype;
    bool can_ride; /* Source beacon agents always forbid riding. */
    float angle_degrees,radius;
    PeVec3 anchor; /* Source PosYTBD is0: anchor.y is deliberately unresolved. */
} PeTownDefenseSpawnRequest;
typedef struct {
    unsigned wave,count;
    PeVec3 center,forward;
    PeTownDefenseSpawnRequest requests[PE_TOWN_DEFENSE_WAVE_CAPACITY];
} PeTownDefenseWavePlan;
/* Draw order: count for group0, each angle then uniform radius, count for group1,
 * its angles/radii. Integers exclude max; reals include endpoints. The caller
 * owns callback state: stage its RNG if draw failure must roll that state back.
 * Output remains unchanged on failure. Retain a successful plan across geometry
 * retries; do not reroll, clamp or silently substitute unsupported anchors.
 * Source9077 uses forward +Z; nonzero supplied XZ directions exercise AiUtil. */
bool pe_town_defense_plan_wave(unsigned wave,PeVec3 center,PeVec3 forward,
                               PeTownDefenseDraws draws,PeTownDefenseWavePlan *out);
bool pe_town_defense_plan_valid(const PeTownDefenseWavePlan *plan);
typedef enum {
    PE_TOWN_DEFENSE_AGENT_EMPTY,
    PE_TOWN_DEFENSE_AGENT_PENDING,
    PE_TOWN_DEFENSE_AGENT_CREATED,
    PE_TOWN_DEFENSE_AGENT_DEAD
} PeTownDefenseAgentState;
typedef struct {
    uint32_t agent_id,entity_id;
    unsigned wave;
    PeTownDefenseAgentState state;
    PeTownDefenseSpawnRequest request;
    PeVec3 position; /* Only CREATED/DEAD records have supported positions. */
    int default_player_id,camp_id; /* Assigned on actual creation, never DamageID. */
} PeTownDefenseMember;
typedef struct {
    uint32_t beacon_id;
    unsigned waves_admitted,count,deaths;
    bool complete;
    PeVec3 center,forward;
    PeTownDefenseMember members[PE_TOWN_DEFENSE_AGENT_CAPACITY];
} PeTownDefenseSpawn;
bool pe_town_defense_spawn_init(PeTownDefenseSpawn *spawn,uint32_t beacon_id,
                                PeVec3 center,PeVec3 forward);
bool pe_town_defense_spawn_valid(const PeTownDefenseSpawn *spawn);
/* Atomic native whole-wave admission; source adds scene agents sequentially.
 * Admit waves0 then1 once, without waiting for any earlier members to die.
 * AgentIDs1..22 are stable within this beacon, distinct from created entityIDs. */
bool pe_town_defense_spawn_admit(PeTownDefenseSpawn *spawn,const PeTownDefenseWavePlan *plan);
/* Caller supplies qualified geometry support and the actually created living
 * actor with its source SkAlive-equivalent death capability.
 * Failed support leaves the same pending request. X/Z must be unchanged;
 * duplicate entity IDs, wrong species, rebinding, and dead members are rejected.
 * No source support-ray algorithm, retry timeout, scaled stats or poses claimed. */
bool pe_town_defense_spawn_created(PeTownDefenseSpawn *spawn,uint32_t agent_id,
    uint32_t entity_id,int prototype,PeVec3 position,bool supported);
typedef enum {
    PE_TOWN_DEFENSE_DEATH_REJECTED,
    PE_TOWN_DEFENSE_DEATH_CREDITED,
    PE_TOWN_DEFENSE_DEATH_COMPLETED
} PeTownDefenseDeathResult;
/* Only a created member of this exact beacon can receive credit, once. Killer
 * identity is deliberately irrelevant. At15 latch completion and preserve all
 * survivor/pending records for a future host cleanup policy; no despawn here.
 * Once complete, further death notices are rejected rather than extending the
 * completed target. Does not grant/complete a gameplay quest. */
PeTownDefenseDeathResult pe_town_defense_spawn_death(PeTownDefenseSpawn *spawn,
                                                   uint32_t beacon_id,uint32_t entity_id);
#endif
