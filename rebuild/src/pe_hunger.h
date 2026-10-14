#ifndef PE_HUNGER_H
#define PE_HUNGER_H
#include "pe_passive_recovery.h"
/* The player's source hunger (attribute 16). The Adventure sandbox drives it from
 * sandbox_health_frame; PeGame owns it and PECSv23 saves its value and condition.
 *
 * Source rules (export_hunger.py): initial buff 30200050 drains raw hunger by
 * speed * percent * 5 every 5 s on the shared eight-frame attribute pass, the
 * first time at mount; the summed value clamps to 0..HungerMax. AbnormalType 8
 * (starvation) checks a frame-time timer before accumulating it: while inactive,
 * every >= 1 s an alive player with hunger in [0,50] starts it; while active,
 * every >= 0.1 s hunger in [51,1000] ends it. Starting or ending clears both
 * timers. It is not ended by death. Both rates are exactly 1.
 *
 * Adapters, not original behavior: the abnormal update runs after the attribute
 * pass within a frame (script order unestablished); Unity's Random.value can
 * return 1.0, which would refuse a rate-1 roll, and that is ignored; the food
 * cooldown is a countdown of scaled time, as the medpack's is. Nutrients and the
 * 1% food poisoning are reported, not owned. State is plain data; the hunger
 * value and the starving flag are what a save would keep. */
typedef struct {
    float raw;                 /* written by drain and food; summed at the next attribute pass */
    float hunger;              /* summed, clamped: what the HUD and the condition read */
    uint64_t drain_ticks, frame;
    bool initialized, starving;
    float start_elapsed, end_elapsed;   /* the condition's two PEAT_Time timers */
    float food_cooldown;                /* seconds left in skill cooldown group 12 */
} PeHunger;
typedef struct {
    float stamina_reduce_percent, stamina_recover_speed, stamina_recover_delay,
          comfort_reduce_percent, oxygen_reduce_percent;
} PeHungerStarvation;
typedef struct { int item_id; float hunger; int nutrient; float nutrient_gain; } PeHungerMeal;
typedef enum { PE_HUNGER_NO_CHANGE, PE_HUNGER_STARTED, PE_HUNGER_ENDED } PeHungerEvent;

/* A fresh player: the prototype's 300 of 500, not starving. */
void pe_hunger_fresh(PeHunger *hunger);
/* A loaded player: the saved summed value and condition; timers and phase are transient. */
bool pe_hunger_loaded(PeHunger *hunger,float value,bool starving);
/* Once per unpaused display frame, after the passive lifecycle update of the same
 * frame (before/after are its snapshots). dt is that frame's scaled time.
 * Invalid input leaves the state unchanged. */
bool pe_hunger_frame(PeHunger *hunger,const PePassiveRecovery *before,const PePassiveRecovery *after,
    PeAttributeFrame frame,float dt,bool alive,PeHungerEvent *event);
/* One supported food (pe_hunger_source_foods): raw hunger rises by its gain and the
 * cooldown starts. Refused while cooling down, before the first frame, or for
 * any other item; a refusal changes nothing. */
bool pe_hunger_eat(PeHunger *hunger,int item_id,PeHungerMeal *meal);
bool pe_hunger_is_food(int item_id);
/* Starvation buff 30200113's pre-adds while starving, zeros otherwise. */
PeHungerStarvation pe_hunger_starvation(const PeHunger *hunger);
#endif
