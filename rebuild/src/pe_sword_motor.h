#ifndef PE_SWORD_MOTOR_H
#define PE_SWORD_MOTOR_H
#include "pe_human_phy.h"

/* Source consumption of Action_SwordAttack root motion on the native tick.
 *
 * The original does not move the body by the clip's root delta directly. Per
 * frame: FixedUpdate replaces the body's velocity with the request latched by
 * the previous LateUpdate; Update turns the PREVIOUS animator update's root
 * delta (AnimatorCmpt.m_LastMove) into a new request through HumanPhyCtrl,
 * which holds it to 10 m/s; the animator then advances; LateUpdate latches.
 * A root delta therefore reaches the body two frames after its pose, and the
 * deltas still in flight when the attack ends are cancelled, not applied.
 *
 * One native tick stands for one original frame with one FixedUpdate. That
 * pacing, and coasting without PhysX contact friction, are native adapters.
 * Collision stays with the caller. State is transient: reset on host
 * creation, load, and whenever the blade action is cancelled. */
typedef struct {
    PeHumanPhy phy;
    bool engaged;            /* a blade action owned the previous tick */
    bool has_last_move;
    double last_move[3];     /* world-space root delta of the previous animator update */
} PeSwordMotor;
typedef struct {
    bool owns_motion;        /* false: the ordinary movement step runs this tick */
    PeVec3 velocity;         /* horizontal velocity for the collision step; y is 0 */
    bool coasting;           /* no request latched: source drag only */
} PeSwordMotorStep;

void pe_sword_motor_reset(PeSwordMotor *motor);
/* FixedUpdate half. action_active is the blade action's state entering this
 * tick. Returns the horizontal velocity the body moves with this tick. */
bool pe_sword_motor_consume(PeSwordMotor *motor,bool action_active,PeVec3 current_velocity,
                            bool grounded,float dt,PeSwordMotorStep *step);
/* Update, animator and LateUpdate halves, after the collision step.
 * skill_running is the action's state entering this tick; while it holds, the
 * previous root delta becomes a request (12 per-update correction against
 * current_velocity, then the 10 m/s hold). world_delta is this tick's
 * yaw-rotated root delta, or NULL when no authored interval advanced. When
 * skill_running is false the source OnEndAction cancels request and delta. */
bool pe_sword_motor_update(PeSwordMotor *motor,bool skill_running,const double world_delta[3],
                           PeVec3 current_velocity,double dt);
/* Invalid input leaves state and output unchanged. */
#endif
