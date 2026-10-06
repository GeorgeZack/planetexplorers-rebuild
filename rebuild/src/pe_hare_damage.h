#ifndef PE_HARE_DAMAGE_H
#define PE_HARE_DAMAGE_H
#include "pe_hare_attack.h"
#include "pe_hitbox.h"

typedef struct {
    const PeActor *actor;
    const PeHitboxes *hitboxes;
    PeVec3 position;
    float yaw,scale;
} PeHareDamagePose;
typedef struct {
    uint64_t entity_id;
    PeHareDamagePose pose;
    float attack;
    unsigned window,guide,bone; /* One actual forearm trigger, RIGHT or LEFT. */
    /* Alive attacker, active collision-enabled skill guide, faction/reputation,
     * DamageID, layer and external trigger-dispatch eligibility belong to host. */
    bool eligible,critical;
} PeHareDamageAttack;
typedef struct {
    uint64_t entity_id;
    PeHareDamagePose pose;
    size_t clip;
    double seconds;
    /* Zero uses legacy clip/seconds; otherwise exact visible normalized blend. */
    PeActorBlendSample blend[PE_ACTOR_BLEND_MAX];
    size_t blend_count;
    float defense,max_hp,shield_protection;
    bool eligible,shield_block; /* Active defence trigger/skill target gates. */
} PeHareDamageTarget;
typedef struct {
    uint64_t target_id;
    size_t attack_index,defence_index;
    uint32_t attack_form,defence_form,defence_material;
    PeHitboxContact contact;
    float contact_scale,damage,applied_damage,hp_before,hp_after;
    bool died;
} PeHareDamageResult;
typedef enum {
    PE_HARE_DAMAGE_INVALID=-1,PE_HARE_DAMAGE_IGNORED=0,
    PE_HARE_DAMAGE_MISS,PE_HARE_DAMAGE_HIT
} PeHareDamageStatus;
/* One host-ordered forearm trigger against one target. Uses the action's exact
 * normalized clock and matching nonlooping attack0..4 clip. Caller must render
 * that same pose. PEHB must retain authored trigger-part order/provenance.
 * Checks all active attack/defence poses before committing the first overlap.
 * A successful source guide contact atomically records dedupe and reduces the
 * caller's authoritative HP; no private HP copy/state, mission or save owner.
 * Two forearms cannot hit twice in one guide; attack2 guide0/1 remain distinct.
 * Host supplies real guide index independently from Animator window index;
 * this helper invents no skill-guide timing, target selection or retaliation AI.
 * INVALID/IGNORED/MISS leave action, HP and required result untouched. HIT may
 * have zero damage (still consumes the contact); died is positive-to-zero only.
 * No hurt animation, rigid force, XP, shield skill or collision-history sweep. */
PeHareDamageStatus pe_hare_damage_resolve(PeHareAttack *action,
    const PeHareDamageAttack *attack,const PeHareDamageTarget *target,
    float *hp,PeHareDamageResult *result);
#endif
