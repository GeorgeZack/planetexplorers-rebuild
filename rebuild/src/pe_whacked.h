#ifndef PE_WHACKED_H
#define PE_WHACKED_H
#include <stdbool.h>
#include <stdint.h>

/* Disconnected source Action_Whacked lifecycle and its BeHit state clock.
 * No host calls this. The original has no stun timer: the action ends when the
 * animator leaves FullBody.BeHit and that state's AnimatorEventSender sends
 * HitAnimEnd. The clock here is that state's authored exit, in nominal form.
 *
 * Caller owns MotionMgr admission/pause order, the animator, MoveStyle
 * smoothing, layer mixing, hit IK and every pose. Clip numbers are installed
 * female clip identities, not poses; no reaction pose has been baked.
 * All state is transient; zero-initialize on host creation/load. */
typedef struct {uint64_t forbidden_masks,pause_actions,end_immediately_actions;} PeWhackedRelation;
typedef struct {
    bool running;    /* action held by the manager */
    bool mask;       /* PEActionMask.Whacked */
    bool presenting; /* animator is in the BeHit state */
    float normalized_time;
} PeWhacked;
typedef struct {
    float delta_time;
    bool animator_present;
    float move_style; /* current animator MoveStyle float, already smoothed by the caller */
} PeWhackedTick;
typedef struct {
    bool started,ended,repeat_ignored,set_mask,mask;
    bool set_trigger_be_hit,set_trigger_reset_full_body,reset_trigger_be_hit;
    bool hit_anim_end;
    /* One-dimensional MoveStyle blend: clip_a at 1-weight_b, clip_b at weight_b. */
    bool presenting; uint32_t clip_a,clip_b; float weight_b,state_length,normalized_time;
    float exit_blend; /* 0..1 progress of the authored exit into FullBody.DoNothing */
    /* Row 5 paused actions the manager resumes when the action leaves it. */
    uint64_t continue_actions;
} PeWhackedUpdate;

PeWhackedRelation pe_whacked_relation(void);
/* Admission only. Row 5 forbids the Whacked mask itself, but the manager
 * bypasses dependencies for a running action and calls inherited no-op
 * ResetAction: a second hit neither restarts nor extends the reaction. */
bool pe_whacked_source_admissible(uint64_t source_masks);
/* Blended BeHit leaves and state length for a MoveStyle value. */
bool pe_whacked_selection(float move_style,PeWhackedUpdate *selection);
/* DoAction: sets the mask always, the BeHit trigger only with an animator. */
bool pe_whacked_begin(PeWhacked *action,bool animator_present,PeWhackedUpdate *update);
/* One Update followed by that frame's animator evaluation. State time
 * advances by delta_time over the blended length; the exit begins at
 * normalized .9 and HitAnimEnd is reported once it reaches 1. Which display
 * frame Unity evaluates the entry and exit on is not qualified.
 * Without an animator the source Update reports completion and the manager
 * drops the action without EndImmediately: the mask stays set. */
bool pe_whacked_step(PeWhacked *action,PeWhackedTick tick,PeWhackedUpdate *update);
/* EndImmediately from the manager (another action's relation, or cleanup). */
bool pe_whacked_cancel(PeWhacked *action,bool animator_present,PeWhackedUpdate *update);
/* Invalid input leaves state and output unchanged. */
#endif
