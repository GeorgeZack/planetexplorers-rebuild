#ifndef PE_HARE_SKILL_H
#define PE_HARE_SKILL_H
#include <stdbool.h>
#define PE_HARE_SKILL_DT (1.0/60.0)
#define PE_HARE_SKILL_SKIP_PRE 1u
#define PE_HARE_SKILL_SKIP_MAIN 2u
#define PE_HARE_SKILL_SKIP_POST 4u
typedef struct {
    int skill_id;unsigned guide_count;
    double prepare_pre,prepare_post,pre[2],main[2],post[2],ending_pre,ending_post;
} PeHareSkillDef;
typedef enum {PE_HARE_SKILL_IDLE,PE_HARE_SKILL_PREPARE_PRE,PE_HARE_SKILL_PREPARE_POST,
    PE_HARE_SKILL_PRE,PE_HARE_SKILL_MAIN,PE_HARE_SKILL_POST,
    PE_HARE_SKILL_ENDING_PRE,PE_HARE_SKILL_ENDING_POST,PE_HARE_SKILL_DONE} PeHareSkillPhase;
typedef struct {
    bool initialized,active,completed,contact_enabled;
    unsigned attack,guide,skip_flags;
    PeHareSkillPhase phase;
    double seconds,phase_seconds;
} PeHareSkill;
const PeHareSkillDef *pe_hare_skill_def(unsigned attack);
void pe_hare_skill_init(PeHareSkill *skill);
bool pe_hare_skill_begin(PeHareSkill *skill,unsigned attack);
/* One scaled60Hz update of source pre/main/post/ending waits, independent of
 * Animator normalized time. Positive waits consume full ticks and discard
 * surplus on transition; zero phases run immediately. This is a bounded native
 * coroutine scheduling adapter, not measured Unity frame-order parity.
 * Current guide is zero-based; contact_enabled mirrors ext-collision execution
 * ONLY during main. Host must also require the actual Animator capsule window.
 * Explicit skip requests latch for this guide and reset at the next guide;
 * only guide pre/main/post are skippable. BTMelee callers pass0: source BTMelee
 * does not issue Sword/Gun skip flags. Animator MonsterEndAttack is not a skip.
 * Invalid calls are atomic. Completion does not end Animator or erase separate
 * PeHareAttack cooldowns; host stops both on actual action interruption. */
bool pe_hare_skill_step(PeHareSkill *skill,unsigned skip_requests);
void pe_hare_skill_cancel(PeHareSkill *skill);
#endif
