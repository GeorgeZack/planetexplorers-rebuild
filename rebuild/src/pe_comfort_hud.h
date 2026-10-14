#ifndef PE_COMFORT_HUD_H
#define PE_COMFORT_HUD_H
#include <stdbool.h>
/* The Adventure HUD's comfort presentation: UIMainMidCtrl.UpdatePlayerAtrbute's
 * slider, drop flash and face (export_comfort_hud.py). Transient presentation
 * state owned by a host UI, never by PeGame, never saved.
 *
 * Source order per frame: the slider is Comfort/ComfortMax (0 when the maximum is
 * not positive); the fall check compares with the previous frame's comfort only
 * (rise: store; drop of more than 10: start the curve at time 0 and store; else
 * store); while the curve plays, its time advances by the frame's delta, alpha is
 * the curve at that time, and play stops once alpha >= 1; the face is face3
 * below 0.2, face2 below 0.5, else face1. Alpha keeps its last value after play.
 *
 * Adapters, not original behavior: the slider value is clamped to 0..1 (UISlider
 * is not decompiled); the curve is the float Hermite polynomial over the
 * serialized keys with clamped ends, not qualified engine output; the host's reset
 * on load and its pause/focus freeze are native lifecycle choices. */
typedef enum { PE_COMFORT_FACE1 = 1, PE_COMFORT_FACE2, PE_COMFORT_FACE3 } PeComfortFace;
typedef struct {
    float last_comfort;  /* m_lastComfort */
    float curve_time;    /* m_CurveTotalTime */
    float alpha;         /* mComfortForeground.alpha */
    float slider;        /* mComfort_4.sliderValue */
    bool playing;        /* m_CurvePlay */
    PeComfortFace face;  /* mComfortSprite.spriteName */
} PeComfortHud;

/* A fresh component: the prefab's serialized values (last 0, alpha 1, slider 1, face3). */
void pe_comfort_hud_reset(PeComfortHud *hud);
/* One UpdatePlayerAtrbute. dt is that frame's delta (>= 0). Invalid input or
 * state, or a nonfinite result, returns false and changes nothing. */
bool pe_comfort_hud_frame(PeComfortHud *hud,float comfort,float comfort_max,float dt);
/* mComfortAlphaCurve.Evaluate(time); false for nonfinite time. */
bool pe_comfort_hud_curve(float time,float *alpha);
/* "face1".."face3", or NULL. */
const char *pe_comfort_hud_face_sprite(PeComfortFace face);
#endif
