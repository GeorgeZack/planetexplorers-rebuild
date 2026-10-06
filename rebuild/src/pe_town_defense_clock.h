#ifndef PE_TOWN_DEFENSE_CLOCK_H
#define PE_TOWN_DEFENSE_CLOCK_H
#include <stdbool.h>
/* Source9077 beacon scheduling only. No quest acceptance, actors, random counts,
 * death credit, rewards or save format. The graphical host does not use this. */
typedef enum {
    PE_TOWN_DEFENSE_INITIAL_PREP,
    PE_TOWN_DEFENSE_COOLDOWN,
    PE_TOWN_DEFENSE_SECOND_PREP,
    PE_TOWN_DEFENSE_CLOCK_FINISHED
} PeTownDefenseClockPhase;
typedef struct {
    PeTownDefenseClockPhase phase;
    unsigned seconds_remaining;
    double wait_remaining;
} PeTownDefenseClock;
typedef struct {
    /* -1 means no wave;0/1 requests that source wave exactly once. It is not a
     * generated cohort, random count, entity ID or completion notification. */
    int wave;
} PeTownDefenseClockEvent;
void pe_town_defense_clock_init(PeTownDefenseClock *clock);
bool pe_town_defense_clock_valid(const PeTownDefenseClock *clock);
/* Ready zeros current prep, preserving the outstanding one-second wait.
 * Returns false without mutation during cooldown/finished or invalid state. */
bool pe_town_defense_clock_ready(PeTownDefenseClock *clock);
/* Call once per display frame with scaled elapsed seconds0..1. Paused/dt0
 * freezes. One coroutine wake maximum per call; overshoot is discarded and a
 * new wait begins at that frame, explicitly adapting Unity WaitForSeconds
 * quantization. Never feed simulation catch-up ticks or resume wallclock debt.
 * Invalid input leaves clock AND event untouched. Accepted calls output wave=-1
 * unless a prep wake emits0/1. Caller controls any subsequent transactional spawn.
 * Finished means both waves were requested, not that a defense was completed. */
bool pe_town_defense_clock_step(PeTownDefenseClock *clock,double dt,bool paused,
                               PeTownDefenseClockEvent *event);
/* UI preparation countdown; remains0 throughout the independent cooldown. */
unsigned pe_town_defense_clock_prep(const PeTownDefenseClock *clock);
unsigned pe_town_defense_clock_waves_remaining(const PeTownDefenseClock *clock);
#endif
