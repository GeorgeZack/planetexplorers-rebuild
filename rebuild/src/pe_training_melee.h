#ifndef PE_TRAINING_MELEE_H
#define PE_TRAINING_MELEE_H
#include "pe_knife_contact.h"
#define PE_TRAINING_MELEE_DT (1.0/60.0)
typedef struct {
    /* Borrowed for host lifetime. These MUST be verified authored mountMain
     * poses, not stowed mountWaist or observational recording timelines.
     * PEAC itself does not encode gender or attachment provenance. */
    const PeActor *body,*drawn_knife,*target_actor;
    const PeHitboxes *drawn_capsule,*target_defence;
    PeCombatGender gender;
    uint64_t player_id,target_id;
    float attack,stamina_reduce_percent,target_hp,target_max_hp,target_defense;
} PeTrainingMeleeSetup;
typedef struct {
    bool alive,knife_owned,knife_equipped,knife_drawn,grounded,third_person;
    bool action_allowed,move_requested;
    PeVec3 velocity; /* Measured physical velocity, not requested movement. */
} PeTrainingMeleeGate;
typedef struct {
    PeTrainingMeleeGate gate;
    PeVec3 player_position,target_position;
    float player_yaw,player_scale,target_yaw,target_scale;
    size_t target_clip;
    double target_seconds; /* Exact current rendered hare clip/time. */
    /* Host supplies actual source layer/faction/skill-guide/trigger permission
     * and resolved critical RNG. Geometry alone cannot authorize a hit. */
    bool attacker_eligible,target_eligible,critical,shield_block;
    float shield_protection;
} PeTrainingMeleeFrame;
typedef struct {
    bool ready;
    PeTrainingMeleeSetup setup;
    PeCombat action;
    size_t body_clip[4],knife_clip[4],weapon_index;
    float target_hp;
} PeTrainingMelee;
typedef struct {
    PeCombatAdvance advance;
    PeKnifeContactStatus contact_status;
    bool cancelled,weapon_valid;
    PeHitboxCapsule weapon;
    PeKnifeContactResult contact; /* died only for actual positive->zero HP. */
} PeTrainingMeleeUpdate;
/* Validates four nonlooping Attack6..9 clocks match body/knife exactly and cover
 * the chosen source event schedules, one active knife capsule and enabled
 * target defences. Atomic failure; no allocations or asset ownership. */
bool pe_training_melee_init(PeTrainingMelee *host,const PeTrainingMeleeSetup *setup);
/* Source stationary TP slice only: alive/held/owned/drawn/grounded/action gate,
 * no movement request and horizontal speed<=.05. Unsupported cases fail without
 * spending stamina. Low stamina remains allowed by the source combat core. */
bool pe_training_melee_begin(PeTrainingMelee *host,PeTrainingMeleeGate gate,float *stamina);
bool pe_training_melee_queue(PeTrainingMelee *host);
void pe_training_melee_cancel(PeTrainingMelee *host);
/* Exactly one scaled60Hz update. Body render uses action.stage + clip_time;
 * knife capsule uses the same clock, current root/yaw and supplied target pose.
 * Unsupported gate changes cancel immediately before contact. Invalid inputs
 * leave host/stamina/output unchanged. Target HP lives here, not in PeHare's
 * max_hp field. Host suppresses stamina recovery while action.active.
 * No swept collision, hit-stop, AI reaction, skill XP or mission dispatch.
 * Consume contact.died once in the host's source-objective/death dispatcher. */
bool pe_training_melee_step(PeTrainingMelee *host,const PeTrainingMeleeFrame *frame,
                            float *stamina,PeTrainingMeleeUpdate *output);
#endif
