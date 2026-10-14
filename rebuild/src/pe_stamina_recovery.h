#ifndef PE_STAMINA_RECOVERY_H
#define PE_STAMINA_RECOVERY_H
#include "pe_passive_recovery.h"
/* Same initial-buff lifecycle as HP; independent source pulse indices.
 * Derived delay and consumption timestamp are not serialized. */
typedef struct {
    float last_consumption, summed_delay;
    uint64_t stamina_ticks, comfort_ticks, delay_ticks, frame;
    bool initialized;
    /* Revival writes raw comfort before the next sum/clamp pass. PeGame keeps
     * a bounded saved value; loading drops this transient pre-clamp remainder. */
    float pending_raw_comfort;
    bool comfort_pending;
} PeStaminaRecovery;
typedef struct {
    float stamina, maximum, recovery_speed, base_delay, comfort;
    /* Buff pre-adds to the two ReducePercent sums (starvation); zero otherwise.
     * recovery_speed and base_delay are already summed by the caller. */
    float reduce_percent_add, comfort_reduce_percent_add;
} PeStaminaVitals;
/* The parent before/after snapshots are from this frame's successful passive
 * lifecycle update. All formulas precede attribute recalculation/clamping. */
bool pe_stamina_recovery_frame(PeStaminaRecovery *state,
    const PePassiveRecovery *before,const PePassiveRecovery *after,
    PeAttributeFrame frame,PeStaminaVitals *vitals);
/* Only successful consumption or an active cost-owning action calls this.
 * now is the validated shared display timestamp, never a fixed-tick clock. */
/* Same-frame remount; preserves prior consumption/summed-delay/frame state. */
bool pe_stamina_recovery_revive(PeStaminaRecovery *state,PeAttributeFrame frame,float raw_comfort);
void pe_stamina_recovery_consume(PeStaminaRecovery *state,float now);
#endif
