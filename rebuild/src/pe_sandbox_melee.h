#ifndef PE_SANDBOX_MELEE_H
#define PE_SANDBOX_MELEE_H
#include "pe_knife_contact.h"
#include "pe_sword.h"
#define PE_SANDBOX_MELEE_TARGETS 6u
#define PE_SANDBOX_MELEE_DT (1.0/60.0)
/* Authored mountMain Attack6..9 assets, borrowed for the host lifetime. PEAC
 * cannot establish attachment/gender provenance; the asset host verifies it. */
typedef struct {
    const PeActor *body,*drawn_weapon;
    const PeHitboxes *drawn_capsule;
    PeCombatGender gender;
    int weapon_item_id; /* Exact qualified attachment: Knife33 or Wood Sword1. */
    uint64_t player_id;
    float attack,stamina_reduce_percent;
    /* Explicit host qualification of immutable female408 root data; default
     * false keeps running disabled. Does not load or own a motion curve. */
    bool running_motion_ready;
    /* Source-qualified entity collider-bounds center. The general helper keeps
     * instantaneous compatibility unless the caller opts into this contract. */
    bool history_enabled;
    PeVec3 history_local_center;
} PeSandboxMeleeSetup;
typedef struct {
    bool alive,weapon_owned,grounded,third_person,action_allowed,move_requested,in_water;
    int equipped_item_id;
    uint32_t equipped_instance_id,drawn_instance_id;
    PeVec3 velocity;
} PeSandboxMeleeGate;
typedef struct {
    PeSandboxMeleeGate gate;
    PeVec3 player_position;
    float player_yaw,player_scale;
    bool attacker_eligible,critical[PE_SANDBOX_MELEE_TARGETS];
} PeSandboxMeleeFrame;
typedef struct {
    bool ready;
    PeSandboxMeleeSetup setup;
    PeCombat action;
    size_t body_clip[4],weapon_clip[4];
    size_t running_body_clip,running_weapon_clip;
    uint32_t instance_id;
    bool tick_pending,tick_cancelled;
    PeCombatAdvance tick_advance;
    PeCombatInterval tick_interval;
    PeCapsuleHistory history;
    bool history_active; /* Trigger window, independent of skill dedupe. */
} PeSandboxMelee;
typedef struct {
    PeCombatAdvance advance;
    PeCombatInterval interval;
    bool cancelled,weapon_valid;
    PeHitboxCapsule weapon;
    size_t target_count;
    PeKnifeContactStatus status[PE_SANDBOX_MELEE_TARGETS];
    PeKnifeContactResult contact[PE_SANDBOX_MELEE_TARGETS];
} PeSandboxMeleeUpdate;
/* Four identical body/tool clip clocks, source event coverage, one enabled
 * right-hand attack capsule. Atomic failure, no allocation or asset ownership. */
bool pe_sandbox_melee_init(PeSandboxMelee *host,const PeSandboxMeleeSetup *setup);
/* Stationary grounded third-person Knife33 or Wood Sword1: horizontal speed<=.05,
 * no requested movement. Binds the action to exact equipped/drawn instance.
 * Opted-in running uses actual full velocity magnitude >4.5 and <=7. Speed7
 * still runs; >7 is unsupported sprint. Other moving stationary starts remain
 * unsupported. Running eligibility is selected once; its own changing speed
 * and held movement do not reclassify/cancel the admitted skill.
 * Source low-stamina starts remain allowed; unsupported starts debit nothing. */
bool pe_sandbox_melee_begin(PeSandboxMelee *host,PeSandboxMeleeGate gate,float *stamina);
bool pe_sandbox_melee_queue(PeSandboxMelee *host,PeSandboxMeleeGate gate);
void pe_sandbox_melee_cancel(PeSandboxMelee *host);
/* Split one tick for the host: advance ONCE, sample the outgoing interval and
 * sweep its motion, then resolve using the resulting world pose. A pending
 * tick rejects another advance/begin/queue. Resolution never advances time or
 * debits stamina; success consumes the pending tick, invalid data preserve it
 * for retry. Host may stage a value copy for an outer atomic transaction. */
bool pe_sandbox_melee_advance(PeSandboxMelee *host,PeSandboxMeleeGate gate,float *stamina);
bool pe_sandbox_melee_interval(const PeSandboxMelee *host,PeCombatInterval *interval);
bool pe_sandbox_melee_resolve(PeSandboxMelee *host,const PeSandboxMeleeFrame *frame,
                             PeKnifeContactTarget *targets,size_t count,PeSandboxMeleeUpdate *output);
/* Optional exact Wood Sword bag-slot transaction. NULL preserves setup.attack
 * and legacy no-wear behavior. Non-NULL requires item/setup/frame/action identity
 * agreement; base_attack excludes sword buff. Recompute attack before each
 * target and charge source wear5 on each HIT notification, not miss/duplicate.
 * The slot and every other output commit together. No new persistent owner.
 * Caller supplies valid bag ownership and uses legacy/no-wear path in Build. */
bool pe_sandbox_melee_resolve_with_weapon(PeSandboxMelee *host,const PeSandboxMeleeFrame *frame,
    PeKnifeContactTarget *targets,size_t count,PeSlot *weapon,float base_attack,PeSandboxMeleeUpdate *output);
bool pe_sandbox_melee_step_with_weapon(PeSandboxMelee *host,const PeSandboxMeleeFrame *frame,
    PeKnifeContactTarget *targets,size_t count,PeSlot *weapon,float base_attack,float *stamina,PeSandboxMeleeUpdate *output);
/* One scaled60Hz tick advances ONE clock for every host-ordered target. The
 * caller supplies current HP, real pose and full source permissions; only HP
 * is mutated. Never pass duplicate entity IDs. Zero targets is valid.
 * Invalid data leave host, every HP, stamina and output unchanged, even when
 * an earlier target would hit. A closed action_allowed gate or changed exact
 * equipment identity cancels before contact. All referenced assets are borrowed.
 * Death events occur only on positive->zero HP; caller owns persistence/death
 * dispatch and must preserve returned HP. No spawn/objective/lesson grants.
 * Opted-in history uses source samples at this native60Hz clock; activation
 * pins both endpoints at the resolved pose (coalesced activation/LateUpdate
 * adapter). Closed windows retain endpoints but retire active ownership. No
 * AI, hit-stop, draw transitions or FP attacks. Running
 * displacement belongs to the qualified host's split path, not this wrapper;
 * running advance/step is unavailable without the explicit setup opt-in. Render
 * from action.stage/clip_time; suppress stamina recovery while action.active. */
bool pe_sandbox_melee_step(PeSandboxMelee *host,const PeSandboxMeleeFrame *frame,
                           PeKnifeContactTarget *targets,size_t count,float *stamina,
                           PeSandboxMeleeUpdate *output);
#endif
