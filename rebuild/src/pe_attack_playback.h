#ifndef PE_ATTACK_PLAYBACK_H
#define PE_ATTACK_PLAYBACK_H
#include "pe_combat.h"
/* Character Action_Sword lifetime, independent of weapon instance/combo. Zero
 * matches source animScaleStartTime=0; do not reset on action start/cancel. */
typedef struct {
    float last_contact,time,frame_contact;
    bool initialized;
} PeAttackPlayback;
typedef struct {
    float base_speed,elapsed,curve_time;
    bool curve_active;
} PeAttackPlaybackSample;
/* Shared scaled display timestamp, never authored animation or world/day time.
 * Same-time queries retain the response snapshot from before frame contacts;
 * contact notifications affect the next display timestamp. No allocation or save
 * owner. Returns base weapon/stamina speed and the original strict response
 * branch/normalized curve input. Curve evaluation is separate from clock state.
 * Invalid input leaves state and required result unchanged. */
bool pe_attack_playback_sample(PeAttackPlayback *state,float now,int weapon_item,
    float stamina,PeAttackPlaybackSample *sample);
/* Caller supplies an already permitted, nonself PEAttackTrigger notification,
 * before skill-guide damage/dedupe. Uses the current validated frame timestamp;
 * no HP-change requirement. Must not be called on every overlapping frame. */
bool pe_attack_playback_contact(PeAttackPlayback *state);
/* Exact supported five-key curve on [0,1], native Hermite coefficients/Horner
 * and serialized internal clamp2. Qualified from installed offline native code;
 * not a generic AnimationCurve/wrap-mode/weighted-tangent implementation. */
bool pe_attack_playback_curve(float normalized_time,float *value);
/* After qualified curve evaluation, multiply the clamped base WITHOUT another
 * .5..1.5 clamp. Outside the response, curve_value is ignored. */
bool pe_attack_playback_speed(PeAttackPlaybackSample sample,float curve_value,float *speed);
#endif
