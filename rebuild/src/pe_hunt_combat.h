#ifndef PE_HUNT_COMBAT_H
#define PE_HUNT_COMBAT_H
#include "pe_outfit.h"
#include "pe_shield.h"
#include "pe_hunt_encounter.h"
#include "pe_hare_perception.h"
#include "pe_hare_damage.h"
#include "pe_hare_skill.h"
/* Bounded six-actor combat host; borrowed immutable assets, no GPU/save state.
 * Positive sole-player threat is retained until death. Melee-only arbitration,
 * three concurrent attackers, fixed guide/clip clocks and native bounds are
 * explicit adapters; this is not the original TargetCmpt/behavior tree. */
typedef struct {
    PeHuntEncounter encounter;
    PeHarePerception sensors[PE_HUNT_ENCOUNTER_COUNT];
    PeHareAttack attacks[PE_HUNT_ENCOUNTER_COUNT];
    PeHareSkill skills[PE_HUNT_ENCOUNTER_COUNT];
    PeHareDamageHistory histories[PE_HUNT_ENCOUNTER_COUNT];
    bool hostile[PE_HUNT_ENCOUNTER_COUNT];
    double attack_seconds[PE_HUNT_ENCOUNTER_COUNT],clock;
    size_t attack_clips[PE_HARE_ATTACK_COUNT];
    uint64_t rng,player_hits,hare_hits;
    const PeActor *player_actor;
    const PeHitboxes *player_defence;
} PeHuntCombat;
typedef struct {
    PeVec3 position,velocity;
    float yaw,radius,height,defense,max_hp;
    /* Optional owned outfit; defense above is the base value. Step stages wear
     * with HP/combat state, recomputing protection after every accepted hit. */
    PeOutfit *outfit;
    /* Optional authoritative selected bag slot; caller validates ownership.
     * Exact item34 is checked here and wear is staged with the entire tick.
     * A qualified transient block request never overrides a blade active at
     * tick entry or after advance, including a pending split ending interval. */
    PeSlot *shield;
    /* Optional selected Wood Sword1 bag slot. Base attack excludes its buff;
     * NULL preserves the legacy melee setup attack/no-wear path. Ownership
     * remains in inventory and wear commits with the entire hunt transaction. */
    PeSlot *weapon;
    float base_attack;
    bool shield_block;
    bool alive,grounded;
    bool defence_disabled; /* Actual collider grace; actor/AI clocks continue. */
    size_t clip; /* Exact visible NON-MELEE pose; active blade overrides below. */
    double seconds;
    /* Zero uses legacy clip/seconds; otherwise exact visible normalized blend. */
    PeActorBlendSample blend[PE_ACTOR_BLEND_MAX];
    size_t blend_count;
} PeHuntCombatPlayer;
bool pe_hunt_combat_prepare(PeHuntCombat *combat,const PeWorld *world,
    const PeSandboxCollision *collision,const PeActor *hare,const PeHitboxes *hare_boxes,
    const PeActor *player,const PeHitboxes *player_boxes,unsigned kills,char *error,size_t capacity);
/* One display-frame sensor update in vision/hearing/collection order. */
bool pe_hunt_combat_sense(PeHuntCombat *combat,const PeSandboxCollision *collision,PeHuntCombatPlayer player);
/* One fixed tick: creature motion/attacks, player's blade, then surviving
 * creatures' forearms. HP/stamina/kill progress and all clocks commit together.
 * Uses exact visible player pose; missing clips fail closed, not an idle proxy.
 * Pending melee ticks resolve only, at the supplied post-movement player pose;
 * legacy callers without a pending tick retain the combined advance/resolve.
 * A running opt-in requires matching RunSwordAttack body/defence clocks. Active
 * blade variants select their exact pe_combat_pose_name, overriding locomotion.
 * Paused/focus/modal suspension is a host policy; no contact runs when omitted. */
bool pe_hunt_combat_step(PeHuntCombat *combat,const PeWorld *world,const PeSandboxCollision *collision,
    PeHuntCombatPlayer player,PeSandboxMelee *melee,const PeSandboxMeleeFrame *frame,
    float *hp,float *stamina,unsigned *kills,PeSandboxMeleeUpdate *update,bool *completed);
#endif
