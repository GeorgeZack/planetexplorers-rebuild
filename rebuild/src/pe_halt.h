#ifndef PE_HALT_H
#define PE_HALT_H
#include "pe_hitbox.h"
#include <stdbool.h>
#include <stdint.h>

/* Source Action_Halt and its IKMoveEffect foot-stop effect for the female
 * player. Hosts reach it only through pe_halt_host.
 *
 * The original holds the Halt mask, which forbids Move and Jump, for as long
 * as the stop effect runs: .2 to .9 s chosen from the speed at the stop. The
 * effect itself is a set of additive effector offsets handed to FinalIK's
 * full-body solver; this module reports the offsets and never a pose.
 * Caller owns MotionMgr admission, the multiplayer branch, foot positions and
 * the solver. All state is transient; zero-initialize on host creation/load. */
#define PE_HALT_LINKS 7u
typedef struct {uint64_t forbidden_masks,end_immediately_actions;} PeHaltRelation;
typedef struct {
    bool running,mask;
    bool effect_running,right_foot;
    PeVec3 direction; /* normalized horizontal velocity at the stop; zero for a standing stop */
    float weight,step_time,remaining;
} PeHalt;
typedef struct {
    bool effect_present,motor_present; /* IKAnimEffectCtrl with its solver, and HumanPhyCtrl */
    PeVec3 velocity;                   /* controller velocity at DoAction */
    PeVec3 left_foot,right_foot;       /* effector bone world positions */
} PeHaltStart;
typedef struct {
    float delta_time;
    bool effect_present,motor_present;
    bool effect_enabled;  /* isActiveAndEnabled */
    float master_weight;  /* IKOffsetModifier.m_Weight; serialized 1 */
} PeHaltTick;
typedef struct {unsigned effector; PeVec3 offset;} PeHaltOffset;
typedef struct {
    bool started,ended,repeat_ignored,set_mask,mask;
    /* FinalIK FullBodyBipedEffector ordinal and the world-space amount to add
     * to its positionOffset this frame. */
    unsigned offset_count; PeHaltOffset offsets[PE_HALT_LINKS];
    float progress; /* elapsed/step_time used for the curves, 0 when none ran */
} PeHaltUpdate;

PeHaltRelation pe_halt_relation(void);
/* Row 50 dependencies; the caller separately applies CanDoAction's aim-state refusal. */
bool pe_halt_source_admissible(uint64_t source_masks);
/* Weight and duration for a horizontal speed. Weight is 0 through 4.5 m/s. */
bool pe_halt_profile(float speed,float *weight,float *step_time);
/* DoAction. A running Halt ignores the request (inherited no-op ResetAction). */
bool pe_halt_begin(PeHalt *action,PeHaltStart start,PeHaltUpdate *update);
/* One frame: the Update test of moveEffectRunning, then the solver's
 * pre-update offset pass. The offsets use the time elapsed before this frame;
 * the remaining time then drops by delta_time. A non-positive master weight or
 * zero delta_time skips the offset pass without consuming time, as
 * IKOffsetModifier does. */
bool pe_halt_step(PeHalt *action,PeHaltTick tick,PeHaltUpdate *update);
/* EndImmediately: stops the effect and clears the mask. */
bool pe_halt_cancel(PeHalt *action,PeHaltUpdate *update);
/* Invalid input leaves state and output unchanged. */
#endif
