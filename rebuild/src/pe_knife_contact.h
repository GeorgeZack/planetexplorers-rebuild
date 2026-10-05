#ifndef PE_KNIFE_CONTACT_H
#define PE_KNIFE_CONTACT_H
#include "pe_combat.h"
#include "pe_hitbox.h"
/* Stationary skill20110011, one actual knife capsule against one host-ordered
 * target. No broadphase, swept history, target perception or mission mutation. */
typedef struct {
    uint64_t entity_id;
    PeHitboxCapsule weapon;
    float attack;
    bool critical;
    /* Host verifies alive attacker, held knife, action compatibility, active
     * skill guide, collision layers and all remaining source permissions. */
    bool eligible;
} PeKnifeContactAttack;
typedef struct {
    uint64_t entity_id;
    const PeActor *actor;
    const PeHitboxes *hitboxes;
    size_t clip;
    double seconds;
    PeVec3 position;
    float yaw,scale;
    float defense,hp,max_hp;
    bool shield_block;
    float shield_protection;
    /* Host combines defence-trigger activity, skill target restriction,
     * reputation/faction/DamageID checks and external collision eligibility.
     * This includes source trigger-dispatch/effected-trigger bookkeeping,
     * which is distinct from PeCombat's successful skill-guide hit list. */
    bool eligible;
} PeKnifeContactTarget;
typedef struct {
    uint64_t target_id;
    size_t defence_index;
    uint32_t defence_form,defence_material;
    PeHitboxContact contact;
    float contact_scale,damage,applied_damage,hp_before,hp_after;
    bool died; /* Only an actual positive-to-zero HP transition. */
} PeKnifeContactResult;
typedef enum {
    PE_KNIFE_CONTACT_INVALID=-1,
    PE_KNIFE_CONTACT_IGNORED=0, /* Closed/blocked/dead/self/duplicate/full list. */
    PE_KNIFE_CONTACT_MISS,
    PE_KNIFE_CONTACT_HIT
} PeKnifeContactStatus;
/* Result required. INVALID/IGNORED/MISS leave action, target and result exactly
 * unchanged. HIT changes only target.hp, action's dedupe list and result.
 * Disabled/attack PEHB records are skipped. First overlapping enabled defence
 * record wins in source order, NOT nearest/deepest (source contact.distance=0).
 * Every enabled defence record must pose successfully before committing.
 * Critical is a host-resolved source random decision. Zero damage still records
 * the successfully executed contact. Result.died is evidence for an ordinary
 * death-event dispatcher, never permission to bypass mission requirements.
 * Single-target calls preserve supplied target order; Unity's global trigger
 * ordering is not reconstructed or replaced with a nearest-target sort. */
PeKnifeContactStatus pe_knife_contact_resolve(PeCombat *action,
    const PeKnifeContactAttack *attack,PeKnifeContactTarget *target,
    PeKnifeContactResult *result);
#endif
