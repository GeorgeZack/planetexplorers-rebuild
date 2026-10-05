#ifndef PE_PLAYER_LOCOMOTION_H
#define PE_PLAYER_LOCOMOTION_H
#include "pe_world.h"
#include <stdbool.h>
typedef enum { PE_LOCOMOTION_IDLE, PE_LOCOMOTION_WALK, PE_LOCOMOTION_RUN,
    PE_LOCOMOTION_JUMP, PE_LOCOMOTION_FALL, PE_LOCOMOTION_LAND, PE_LOCOMOTION_COUNT } PePlayerLocomotionClip;
typedef struct {
    double duration[PE_LOCOMOTION_COUNT], seconds, air_time;
    PePlayerLocomotionClip clip;
    bool ready, airborne;
} PePlayerLocomotion;
typedef struct {
    PeVec3 velocity; /* Actual host velocity, meters/second, not raw key input. */
    bool grounded, walk, sprint;
    bool jump_started; /* One accepted physical jump edge, not held Space. */
} PePlayerLocomotionInput;
const char *pe_player_locomotion_name(PePlayerLocomotionClip clip);
/* Six actual loaded clip durations in enum order. No hard-coded actor indices.
 * Atomic rejection of missing/nonfinite durations; no allocations/graphics. */
bool pe_player_locomotion_init(PePlayerLocomotion *state,const double duration[PE_LOCOMOTION_COUNT]);
/* Finite dt [0,1] seconds; zero is a no-op. State outputs clip + seconds.
 * Source-backed .85 Jump exit, .7 Land exit, .3s ledge-fall delay, .1s jump
 * ground grace. Discrete forward-only selection, speed-scaled WF/RF, sprint
 * RF fallback and lack of blends/root motion are provisional, not Animator
 * parity. No attacks or combat events. Invalid input leaves state unchanged. */
bool pe_player_locomotion_step(PePlayerLocomotion *state,PePlayerLocomotionInput input,double dt);
#endif
