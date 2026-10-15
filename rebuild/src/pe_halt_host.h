#ifndef PE_HALT_HOST_H
#define PE_HALT_HOST_H
#include "pe_halt.h"
/* Host adapter for Action_Halt, shared by the sandbox and tutorial hosts:
 * one call per fixed movement step, before the step consumes its input.
 *
 * Source rules (ActionRelation rows 1, 2, 4 and 50, Motion_Move_Human):
 * releasing movement while Move or Sprint runs requests Halt; Halt is refused
 * in the air, in water, dead, or with hand equipment drawn; Death ends it at
 * once. Jumping, losing the
 * ground and action masks end Move immediately, without a Halt.
 *
 * Adapters, not original behavior: the fixed step stands in for the original's
 * frame, an uninterrupted Halt updates before movement admission, a Halt
 * begun in a step takes its zero first offset pass in that step, and a jump
 * requested with a release wins. By user choice, renewed movement (including
 * sprint) or jump input cancels Halt before that step's offsets and admission;
 * the source's Move/Jump refusal is not applied. Other action locks still own
 * admission. State is transient and never saved; zero-initialize. */
typedef struct {
    PeHalt action;
    bool moving;                       /* Move or Sprint is running */
    unsigned offset_count;             /* this step's additive world-space effector offsets */
    PeHaltOffset offsets[PE_HALT_LINKS];
} PeHaltHost;
typedef struct {
    float delta_time;
    bool move_requested,jump_requested;  /* raw player requests for this step */
    bool move_admissible;                /* grounded and no action mask that forbids Move */
    bool grounded,in_water,alive,equipment_held;
    PeVec3 velocity;                     /* controller velocity before this step */
    PeVec3 left_foot,right_foot;         /* foot bone positions, both in one frame; equal when unknown */
} PeHaltHostInput;
typedef struct {bool block_move,block_jump,started,ended;} PeHaltHostOutput;
/* Invalid input leaves state and output unchanged. */
bool pe_halt_host_step(PeHaltHost *host,PeHaltHostInput input,PeHaltHostOutput *out);
/* Whether the Halt mask is set after the latest step. Row 50 also lists Gather
 * and PickUpItem; a host refuses a herb Gather request while this is true. The
 * player's PickUpItem action has no caller in the source. */
bool pe_halt_host_masked(const PeHaltHost *host);
/* Load, respawn, teleport or mode change: EndImmediately, and Move is not running. */
void pe_halt_host_reset(PeHaltHost *host);
#endif
