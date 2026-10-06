#ifndef PE_CAPSULE_HISTORY_H
#define PE_CAPSULE_HISTORY_H
#include "pe_hitbox.h"

/* Native work bound, not an original sample limit. Exceeding it fails closed;
 * samples are never truncated, evenly redistributed, or replaced by a sweep. */
#define PE_CAPSULE_HISTORY_MAX_SAMPLES 4096u
typedef struct {
    PeVec3 previous_a, previous_b;
    PeHitboxCapsule current;
    PeVec3 parent_center; /* source entity centerPos, not a bone-parent origin */
} PeCapsuleHistory;
typedef enum {
    PE_CAPSULE_HISTORY_INVALID=-1,
    PE_CAPSULE_HISTORY_MISS=0,
    PE_CAPSULE_HISTORY_HIT=1
} PeCapsuleHistoryStatus;
typedef struct {
    PeHitboxContact contact; /* axis_distance is diagnostic; source distance=0 */
    PeVec3 hit_direction;
    PeHitboxCapsule sampled;
    unsigned sample_index, sample_count;
} PeCapsuleHistoryContact;

/* Literal PECapsuleTrigger capsule-vs-capsule sampling and WhiteCat closest
 * points using current attack radius and CURRENT target endpoints. Count1
 * samples previous attack endpoints. Radial interpolation uses Unity's vector
 * equality/normalization thresholds. First source-order overlap wins.
 * History is supplied, never advanced here. No trigger/guide/damage policy.
 * Required output is untouched on INVALID/MISS; nonfinite/out-of-bound inputs,
 * excess sample work or nonfinite final geometry return INVALID atomically. */
PeCapsuleHistoryStatus pe_capsule_history_contact(const PeCapsuleHistory *history,
    PeHitboxCapsule target,PeCapsuleHistoryContact *out);
#endif
