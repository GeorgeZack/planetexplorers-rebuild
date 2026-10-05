#ifndef PE_HARE_ATTACK_H
#define PE_HARE_ATTACK_H
#include <stdbool.h>
#include <stdint.h>

/* The whole BTMelee branch has weight.5; its five runnable skills are selected
 * uniformly, not with five independent .5 attack probabilities. No perception,
 * pursuit, pounce, bounds-gap/angle/HP/obstruction or other AI checks are invented. */
#define PE_HARE_MELEE_WEIGHT 0.5f
#define PE_HARE_ATTACK_COUNT 5u
#define PE_HARE_ATTACK_TARGET_CAPACITY 64u
enum { PE_HARE_RIGHT_FOREARM=1, PE_HARE_LEFT_FOREARM=2 };
typedef struct {
    const char *name;
    int64_t transform_path_id;
    unsigned palette_index;
    uint32_t path_crc32;
} PeHareAttackBone;
typedef struct {
    double start, end; /* Authoritative Animator normalized time, not seconds. */
    uint8_t bones;
    bool end_inclusive, emits_monster_end;
} PeHareAttackWindow;
typedef struct {
    int skill_id, trigger_id;
    double skill_cooldown, clip_duration;
    unsigned window_count, guide_count;
    PeHareAttackWindow windows[2];
} PeHareAttackDef;
typedef struct {
    bool initialized, active;
    unsigned attack;
    double last_melee_start, shared_ready, skill_ready[PE_HARE_ATTACK_COUNT];
    double normalized_time;
    uint8_t open_windows, ended_windows;
    unsigned target_count[2];
    uint64_t targets[2][PE_HARE_ATTACK_TARGET_CAPACITY];
} PeHareAttack;
typedef struct {
    uint8_t opened_windows, closed_windows, monster_end_windows;
    uint8_t open_windows, active_bones;
} PeHareAttackEvents;

void pe_hare_attack_init(PeHareAttack *action);
const PeHareAttackDef *pe_hare_attack_def(unsigned attack);
const PeHareAttackBone *pe_hare_attack_bone(unsigned bone_bit);
/* now is monotonic absolute host wall time using the source Time.time epoch.
 * Initial lastCDTime=0 means melee first becomes ready strictly after1s.
 * geometry_eligible incorporates the host's actual source eligibility checks.
 * runnable_mask supplies all additional skill-system gates, bits0..4.
 * Behavior cooldown elapsed>1 and own/shared cooldown deadlines are applied. */
uint8_t pe_hare_attack_candidates(const PeHareAttack *action, double now,
                                  bool geometry_eligible, uint8_t runnable_mask);
/* random_rank is the host's uniform integer in [0,popcount(candidates)).
 * Failed starts are atomic; successful starts retain prior skill cooldowns. */
bool pe_hare_attack_begin(PeHareAttack *action, double now, bool geometry_eligible,
                          uint8_t runnable_mask, unsigned random_rank);
/* Evaluated normalized Animator time is independent of wall cooldown time.
 * Nondecreasing finite time only; invalid calls leave action/events untouched.
 * Standard windows include both ends. EX windows exclude end, latch closed,
 * and emit MonsterEndAttack once even if the active interval was skipped.
 * No inferred state exit at normalized1: host must end on actual state exit. */
bool pe_hare_attack_update(PeHareAttack *action, double normalized_time,
                           PeHareAttackEvents *events);
void pe_hare_attack_end(PeHareAttack *action);
/* Event-only preparation: host verifies spatial contact, faction permission
 * and successful skill-event execution before committing a target. Animation
 * windows and wall-time skill guides are separate gates! Supply current guide
 * index0/1 (attack2 has2, others1). Deduplication is per target entity per guide,
 * so both forearms cannot double-hit in one guide. This applies no damage. */
bool pe_hare_attack_record_contact(PeHareAttack *action, unsigned window,
                                   unsigned skill_guide, uint64_t target_id);
#endif
