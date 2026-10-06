#ifndef PE_HUNT_ENCOUNTER_H
#define PE_HUNT_ENCOUNTER_H
#include "pe_hare.h"
#include "pe_sandbox_melee.h"
#include "pe_sandbox_collision.h"
#define PE_HUNT_ENCOUNTER_COUNT 6u
#define PE_HUNT_REQUIRED_KILLS 4u
/* Transient pinned capture adapter, not saved population or source RNG. */
typedef struct {
    bool present;
    uint32_t source_agent_id;
    uint64_t entity_id;
    int prototype,camp_id,damage_id,default_player_id;
    PeHare motion;
    float hp; /* Sole current-HP owner; motion.max_hp is the maximum only. */
    size_t clip;
    double seconds;
} PeHuntEncounterActor;
typedef struct {
    bool ready,active;
    const PeActor *actor;const PeHitboxes *defence; /* Borrowed. */
    size_t idle_clip,walk_clip,run_clip;
    PeHuntEncounterActor actors[PE_HUNT_ENCOUNTER_COUNT];
    unsigned starting_kills,credited_count; /* Transient stale-counter guard. */
    uint64_t credited_entities[PE_HUNT_REQUIRED_KILLS];
} PeHuntEncounter;
typedef struct {
    PeHareTarget target; /* Perception/hostility already resolved by the host. */
    bool movement_locked;
    size_t locked_clip;
    double locked_seconds; /* Authoritative attack pose when locked. */
} PeHuntEncounterMotion;
typedef struct {
    uint64_t entity_id;
    int prototype;
    float hp_before,hp_after; /* Actual dispatcher evidence, positive->zero. */
} PeHuntEncounterDeath;
typedef enum {PE_HUNT_DEATH_INVALID=-1,PE_HUNT_DEATH_IGNORED=0,
              PE_HUNT_DEATH_CREDITED,PE_HUNT_DEATH_COMPLETE} PeHuntDeathResult;
/* Require original route dimensions/origin and a CURRENT shared collision
 * snapshot. Stage all six supported original XZ/scale/stat identities before
 * committing; no invented placement or horizontal relocation. Borrowed assets
 * must outlive encounter. Kill progress is caller-owned/saved, never reset here.
 * Reload explicitly re-prepares a fresh full-HP PINNED cohort, an adapter for
 * source transient-agent recreation, not recovery of its new random cohort. */
bool pe_hunt_encounter_prepare(PeHuntEncounter *encounter,const PeWorld *world,
    const PeSandboxCollision *collision,const PeActor *actor,const PeHitboxes *defence,
    unsigned kills,char *error,size_t capacity);
void pe_hunt_encounter_clear(PeHuntEncounter *encounter);
/* One fixed tick. Caller provides six target/attack-lock decisions. Locked
 * actors keep position and use the exact supplied pose; no private attack clock.
 * Others use general PeHare motion and matching idle/walk/run time. All six
 * update atomically; dead/not-present records do not move. No damage or credit. */
bool pe_hunt_encounter_step(PeHuntEncounter *encounter,const PeWorld *world,
    const PeSandboxCollision *collision,const PeHuntEncounterMotion motion[PE_HUNT_ENCOUNTER_COUNT]);
/* Runs the real capsule resolver with one authoritative action clock. Stages
 * encounter HP, melee action, stamina and caller-owned kill counter together.
 * An already pending melee tick is resolved at the supplied post-movement pose
 * without a second advance/debit. Otherwise the legacy combined step runs.
 * Failure preserves the pending tick for retry and all caller-owned state.
 * Each eligibility flag must represent actual source permission, not proximity.
 * Caller supplies frame critical decisions and attacker permission. A fourth
 * actual death marks only this owned cohort absent and sets completed=true;
 * caller then advances its saved quest state. Invalid calls change nothing. */
bool pe_hunt_encounter_melee(PeHuntEncounter *encounter,PeSandboxMelee *melee,
    const PeSandboxMeleeFrame *frame,const bool eligible[PE_HUNT_ENCOUNTER_COUNT],
    float *stamina,unsigned *kills,PeSandboxMeleeUpdate *update,bool *completed);
/* Same transaction with optional exact Wood Sword per-contact stat/wear.
 * Stages the bag slot until death dispatch also succeeds; NULL is legacy. */
bool pe_hunt_encounter_melee_with_weapon(PeHuntEncounter *encounter,PeSandboxMelee *melee,
    const PeSandboxMeleeFrame *frame,const bool eligible[PE_HUNT_ENCOUNTER_COUNT],
    PeSlot *weapon,float base_attack,float *stamina,unsigned *kills,PeSandboxMeleeUpdate *update,bool *completed);
/* Active target1518 counts ANY prototype13 real death, without player-last-hit,
 * cohort-membership or radius restrictions. Host passes only actual death
 * notifications. Owned HP must match hp_before; external entities stay owned
 * by their own system. Duplicate credited IDs are ignored. No synthetic kill
 * API, damage calculation, XP or item rewards. Completed/ignored events cannot
 * increment again. Counter mismatch/invalid evidence is rejected atomically. */
PeHuntDeathResult pe_hunt_encounter_death(PeHuntEncounter *encounter,
    unsigned *kills,PeHuntEncounterDeath death);
#endif
