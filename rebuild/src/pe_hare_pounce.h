#ifndef PE_HARE_POUNCE_H
#define PE_HARE_POUNCE_H
#include "pe_world.h"
/* Disconnected BTPounce owner. Nothing here selects an attack, advances an
 * Animator/skill, changes actor position, or activates production hunt AI. */
typedef struct {
    int skill_id,trigger_id;
    float weight,cooldown,min_range,max_range,start_time,end_time,stop_time;
    float skill_duration,window_start,window_end;
} PeHarePounceDef;
typedef enum { PE_HARE_POUNCE_FAILURE,PE_HARE_POUNCE_RUNNING,PE_HARE_POUNCE_SUCCESS } PeHarePounceStatus;
typedef struct {
    bool initialized,active,started,stopped,faced;
    float last_cooldown,start_time,stop_time,rotate_time,speed,saved_gravity,last_time;
} PeHarePounce;
typedef struct {
    float now,gravity,sqr_distance_logic;
    PeVec3 position,target_position,forward,direction_xz;
    /* Actual accepted attack identity, target existence and source IsScopeAngle
     * result (inclusive ±15 with PEUtil's degenerate cross-product tolerance).
     * sqr_distance_logic is source bounds-gap geometry, never center distance. */
    bool selected,target_present,in_scope,blocked;
} PeHarePounceInput;
typedef struct {
    PeHarePounceStatus status;
    bool gravity_set,clear_movement_effect,stop_move,speed_set,animator_set;
    bool move_direction_set,face_direction_set,pursue_run,start_skill;
    float gravity,speed;
    bool animator_attack5;
    int skill_id;
    PeVec3 move_direction,face_direction,pursuit_target;
    /* Deliberately no StopSkill command: source Reset leaves it running. */
} PeHarePounceRequests;
const PeHarePounceDef *pe_hare_pounce_def(void);
void pe_hare_pounce_init(PeHarePounce *state);
bool pe_hare_pounce_ready_cd(const PeHarePounce *state,float now,bool skill_runnable);
bool pe_hare_pounce_ready_geometry(PeHarePounceInput input);
/* Ordinary Animator attack trigger window, independent of action elapsed and
 * skill guide eligibility. Both bounds inclusive; no damage/contact side effect. */
bool pe_hare_pounce_window(float normalized_time);
/* bool return reports valid arguments, not behavior success. A valid rejected
 * Init/Tick emits FAILURE. Host must call Reset after success/failure; it owns
 * applying emitted requests through real collision/motor/Animator/skill owners.
 * Begin does not repeat selection's cooldown test, matching non-ATTACK_OLD Init.
 * Absolute scaled Time.time-style floats must be nondecreasing. Invalid calls
 * leave state and requests unchanged; all successful calls stage atomically. */
bool pe_hare_pounce_begin(PeHarePounce *state,PeHarePounceInput input,PeHarePounceRequests *requests);
bool pe_hare_pounce_tick(PeHarePounce *state,PeHarePounceInput input,PeHarePounceRequests *requests);
bool pe_hare_pounce_reset(PeHarePounce *state,PeHarePounceRequests *requests);
#endif
