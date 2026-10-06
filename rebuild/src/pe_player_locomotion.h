#ifndef PE_PLAYER_LOCOMOTION_H
#define PE_PLAYER_LOCOMOTION_H
#include "pe_world.h"
#include <stdbool.h>
typedef enum { PE_LOCOMOTION_IDLE, PE_LOCOMOTION_WALK, PE_LOCOMOTION_RUN,
    PE_LOCOMOTION_JUMP, PE_LOCOMOTION_FALL, PE_LOCOMOTION_LAND,
    PE_LOCOMOTION_WALK_LEFT, PE_LOCOMOTION_WALK_RIGHT, PE_LOCOMOTION_WALK_BACK,
    PE_LOCOMOTION_RUN_LEFT, PE_LOCOMOTION_RUN_RIGHT, PE_LOCOMOTION_RUN_BACK,
    PE_LOCOMOTION_COUNT } PePlayerLocomotionClip;
/* Native presentation adapter, not the recovered Animator blend graph. */
#define PE_LOCOMOTION_BLEND_TAU .12
/* Enum-indexed authored times and normalized nonnegative pose weights. */
typedef struct {
    double weights[PE_LOCOMOTION_COUNT], seconds[PE_LOCOMOTION_COUNT];
} PePlayerLocomotionPose;
typedef struct {
    double duration[PE_LOCOMOTION_COUNT], seconds, air_time;
    PePlayerLocomotionPose blend;
    PePlayerLocomotionClip blend_gait;
    PePlayerLocomotionClip clip;
    bool ready, airborne;
} PePlayerLocomotion;
typedef struct {
    PeVec3 velocity; /* Actual host velocity, meters/second, not raw key input. */
    bool grounded, walk, sprint;
    bool jump_started; /* One accepted physical jump edge, not held Space. */
    bool directional; /* False retains legacy forward-only selection. */
    float local_right, local_forward; /* Actual planar velocity in authored
        * body space: +X right, +Z forward; ignored when directional=false. */
} PePlayerLocomotionInput;
const char *pe_player_locomotion_name(PePlayerLocomotionClip clip);
/* Twelve actual loaded clip durations in enum order. No hard-coded actor indices.
 * Atomic rejection of missing/nonfinite durations; no allocations/graphics. */
bool pe_player_locomotion_init(PePlayerLocomotion *state,const double duration[PE_LOCOMOTION_COUNT]);
/* Finite dt [0,1] seconds; zero is a no-op. State outputs clip + seconds.
 * Source-backed .85 Jump exit, .7 Land exit, .3s ledge-fall delay, .1s jump
 * ground grace. Optional discrete cardinal selection uses the dominant local
 * axis (diagonal ties choose forward/back), preserving phase across all gaits.
 * Selected clip/seconds retain their original discrete semantics. The separate
 * continuous pose snapshot smooths weights with a .12s exponential time constant
 * and advances outgoing clips, keeping active gaits phase-aligned. Speed-scaled
 * gaits, sprint fallback and this blend adapter are not full Animator/root-motion
 * parity. No attacks or combat events. Invalid input leaves state unchanged. */
bool pe_player_locomotion_step(PePlayerLocomotion *state,PePlayerLocomotionInput input,double dt);
/* Read-only normalized twelve-clip snapshot; no actor dependency or advancement.
 * Init/reset snaps to idle. Hosts map enum slots to exact compatible body/held/
 * defence clips, using this same snapshot for each. Action clips override the
 * whole locomotion pose; never add attacks/tools to this blend. Invalid input
 * leaves output unchanged. The snapshot clocks, not legacy selected.seconds,
 * are authoritative when rendering a blended locomotion pose. */
bool pe_player_locomotion_snapshot(const PePlayerLocomotion *state,PePlayerLocomotionPose *pose);
#endif
