#ifndef PE_UI_INPUT_H
#define PE_UI_INPUT_H
#include "pe_training_ui_layout.h"

/* Transient pointer ownership, shared by every gameplay host. */
typedef struct { bool waiting_for_release,actions_only; } PeUiPointerGate;
/* Preserves the release frame. Full modal ownership can escalate an action-only
 * press; action-only ownership must never pause movement or simulation. */
bool pe_ui_pointer_route(PeUiPointerGate *gate,bool *consume_world,bool *consume_actions,unsigned buttons_down);

typedef struct {
    bool menu_open,consume_press;
    int action; /* -1, or shared menu order: inventory, character, craft, mission, pause. */
} PeUiChromeResult;
/* Presentation actions only. Hosts retain inventory, quest and pause policy.
 * A popup owns outside dismissals; a newly opened panel cannot receive its
 * opening click. HUD shortcuts require no preexisting modal. */
PeUiChromeResult pe_ui_chrome_click(const PeTrainingUILayout *layout,float x,float y,
    bool pressed,bool menu_open,bool hud_shortcuts);
#endif
