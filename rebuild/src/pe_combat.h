#ifndef PE_COMBAT_H
#define PE_COMBAT_H
#include <stdbool.h>
#include <stdint.h>

/* Knife20110011 and female TP running20110014. No pose, range, faction, AI, hit-stop,
 * Animator retargeting or collision queries are supplied by this module. */
typedef enum { PE_COMBAT_FEMALE, PE_COMBAT_MALE } PeCombatGender;
typedef enum { PE_COMBAT_THIRD_PERSON, PE_COMBAT_FIRST_PERSON } PeCombatView;
/* SPRINT is the source timing of skill20110013 (clip401, then Attack7..9). Its
 * poses and root curve are not recovered; hosts keep it unavailable until then. */
typedef enum { PE_COMBAT_STATIONARY, PE_COMBAT_RUNNING, PE_COMBAT_SPRINT } PeCombatVariant;
typedef enum {
    PE_COMBAT_HIT_START, PE_COMBAT_HIT_END, PE_COMBAT_COMBO_START,
    PE_COMBAT_COMBO_END, PE_COMBAT_ACTION_END
} PeCombatEventType;
typedef struct { double time; PeCombatEventType type; } PeCombatEvent;
typedef struct { int path_id; PeCombatEvent events[5]; } PeCombatClip;
#define PE_COMBAT_TARGET_CAPACITY 64u
typedef struct {
    bool active, contact_open, combo_window, combo_queued, pending_close;
    PeCombatGender gender;
    PeCombatView view;
    PeCombatVariant variant;
    unsigned stage, event_index, target_count; /* Combo stage0..3. */
    uint64_t update_index, opened_update;
    double clip_time, wall_time; /* wall_time retains its API name: scaled Time.time action age. */
    float stamina_reduce_percent;
    uint64_t targets[PE_COMBAT_TARGET_CAPACITY];
} PeCombat;
typedef enum {
    PE_COMBAT_INVALID=-1, PE_COMBAT_IDLE=0, PE_COMBAT_ADVANCED,
    PE_COMBAT_NEXT_STAGE, PE_COMBAT_ENDED
} PeCombatAdvance;
/* Outgoing authored interval, even when the new action has ended or advanced
 * to another stage at time0. Invalid/idle intervals have valid=false. Running
 * stage0 is the only newly supported root curve; other stages keep their
 * existing host movement policy. No unconsumed displacement is accumulated. */
typedef struct {
    bool valid;
    PeCombatVariant variant;
    PeCombatGender gender;
    PeCombatView view;
    unsigned stage;
    double from,to;
} PeCombatInterval;

void pe_combat_init(PeCombat *action);
/* Each start/combo charges5*reduction, clamped at0. Low stamina never rejects
 * an attack. Host verifies weapon held/ownership/alive/action compatibility. */
bool pe_combat_begin(PeCombat *action, PeCombatGender gender, PeCombatView view,
                     float stamina_reduce_percent, float *stamina);
bool pe_combat_begin_variant(PeCombat *action,PeCombatGender gender,PeCombatView view,
                            PeCombatVariant variant,float stamina_reduce_percent,float *stamina);
void pe_combat_stop(PeCombat *action);
bool pe_combat_queue_combo(PeCombat *action);
const PeCombatClip *pe_combat_clip(PeCombatGender gender, PeCombatView view, unsigned stage);
const PeCombatClip *pe_combat_variant_clip(PeCombatGender gender,PeCombatView view,PeCombatVariant variant,unsigned stage);
/* Common render/contact asset name, independent of per-asset clip indices. */
const char *pe_combat_pose_name(const PeCombat *action);
/* Source knife animation speed1.37, thresholds50/20 each multiply.9, clamp
 * [.5,1.5]. Returns0 for invalid stamina. This is not an FP blend time scale. */
float pe_combat_animation_speed(float stamina);
/* Exact supported weapon rates: Knife33 or Wood Sword1. Unsupported items and
 * invalid stamina return0. Event timestamps remain authored clip seconds. */
float pe_combat_item_animation_speed(int item_id,float stamina);
/* Stable stationary knife blend endpoints: TP clip rate equals Animator.speed;
 * FP equals .65*Animator.speed. Source controller leaf timing, calibrated with
 * female TP/FP Attack6..9 runtime captures. Excludes transitions, hit response,
 * moving/air attacks and partial FirstPerson blends. Invalid input returns0. */
double pe_combat_stationary_clip_rate(PeCombatView view,double animator_speed);
/* IMPORTANT: clip_delta is authored CLIP seconds, wall_delta is elapsed scaled
 * GAME seconds (source Time.time action age), not realtimeSinceStartup. Host supplies the evaluated Animator clock, or the calibrated
 * stationary endpoint rate when its preconditions hold. Weapon metadata
 * frequency does not determine the clock.
 * Each valid call represents one engine update, even with zero clock deltas.
 * EndAttack retains contact for at least2 updates after StartAttack. EndAction
 * obeys the source .3s scaled-game-time guard. A combo resets both clocks and discards
 * surplus clip time: the host starts the new clip on NEXT_STAGE.
 * Invalid inputs leave both action and stamina unchanged. No regeneration is
 * applied; the host suppresses its stamina recovery while action.active. */
PeCombatAdvance pe_combat_advance(PeCombat *action, double clip_delta,
                                 double wall_delta, float *stamina);
PeCombatAdvance pe_combat_advance_interval(PeCombat *action,double clip_delta,
                                          double wall_delta,float *stamina,PeCombatInterval *interval);
/* Host calls only after an authorized, spatially verified contact. Entity ID
 * must be stable/nonzero. SkInst's per-stage target deduplication is preserved.
 * Capacity is a native bound; overflow refuses additional contacts safely. */
bool pe_combat_record_contact(PeCombat *action, uint64_t target_id);
/* Pure numeric expression from trigger493/502/503/237. No HP, XP, RNG or
 * permission side effects. Critical is an already-resolved source RNG result.
 * A nonpositive/tiny source contact scale is ignored, as in TryGetHurt.
 * Invalid/nonfinite input leaves *damage unchanged. */
bool pe_combat_damage(float attack, float defense, bool shield_block,
                      float shield_protection, bool critical, float contact_scale,
                      float *damage);
#endif
