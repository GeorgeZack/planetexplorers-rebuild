#ifndef PE_PASSIVE_RECOVERY_H
#define PE_PASSIVE_RECOVERY_H
#include <stdbool.h>
#include <stdint.h>

/* Process/host time, never saved with an entity. Call once per display frame.
 * The host supplies scaled time; fixed-step catch-up must not advance it. */
typedef struct {
    double seconds;
    uint64_t frame, attribute_frame;
} PeAttributeClock;
typedef struct {
    float now;
    uint64_t frame;
    bool attributes_due;
} PeAttributeFrame;
bool pe_attribute_clock_step(PeAttributeClock *clock,double dt,PeAttributeFrame *out);

/* Buff phase and last-hurt timestamp are transient in the original serializer.
 * Zero means fresh creation; load sets only loaded=true. HP stays game-owned. */
typedef struct {
    float created, mounted_at, last_hurt, last_time;
    uint64_t evaluations, frame;
    bool initialized, mounted, initialization_checked, loaded;
} PePassiveRecovery;
/* Initial mount, delayed source CheckInitAttr remount, death and buff evaluation.
 * Call once per unpaused display frame, before that frame's combat contacts.
 * Invalid inputs preserve both the state and HP. */
bool pe_passive_recovery_frame(PePassiveRecovery *state,PeAttributeFrame frame,float *hp,float max_hp);
/* A committed damage notification, including an admitted zero-damage hit.
 * The caller has already validated/frame-updated the owner. No second clock. */
/* State_Revive remounts immediately without resetting entity timestamps or
 * its pending initial CheckInitAttr timer. Same-frame operation, no buff tick. */
bool pe_passive_recovery_revive(PePassiveRecovery *state,PeAttributeFrame frame);
void pe_passive_recovery_hurt(PePassiveRecovery *state,float hp);
#endif
