#ifndef PE_SWORD_VARIANT_H
#define PE_SWORD_VARIANT_H
#include <stdbool.h>
#include "pe_world.h"
/* Source branch, not presentation availability or action permission. Callers
 * must reject unsupported AIR/WATER/SPRINT explicitly, never substitute NORMAL. */
typedef enum {
    PE_SWORD_VARIANT_ORDINARY,
    PE_SWORD_VARIANT_RUNNING,
    PE_SWORD_VARIANT_SPRINT,
    PE_SWORD_VARIANT_AIR,
    PE_SWORD_VARIANT_WATER
} PeSwordVariant;
typedef struct {
    PeVec3 velocity;
    bool movement_present,in_air,motor_present,spine_in_water;
} PeSwordVariantInput;
/* Full XYZ, as Action_Sword.move.velocity.magnitude. Wider sum-of-products,
 * float round at Mathf.Sqrt argument, double sqrt then float result follows
 * inspected installed Mono math; no flat-speed or squared-threshold shortcut.
 * Reject nonfinite components or overflow of float squared length. Atomic output. */
bool pe_sword_variant_speed(PeVec3 velocity,float *speed);
/* Air first, then motor-present spine water, then strict >7 sprint, >4.5
 * running, otherwise ordinary (including walking and exactly4.5). Missing
 * movement chooses ordinary on ground. Inputs remain finite even if their
 * branch would not read velocity; this is a deliberate native validity bound.
 * No clock, input, stamina, first-person capability or equipment state changes. */
bool pe_sword_variant_select(PeSwordVariantInput input,PeSwordVariant *variant);
#endif
