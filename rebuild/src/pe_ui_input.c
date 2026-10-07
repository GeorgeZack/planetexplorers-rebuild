#include "pe_ui_input.h"

bool pe_ui_pointer_route(PeUiPointerGate *gate,bool *world,bool *actions,unsigned buttons){
    if(!gate||!world||!actions)return true;
    if((*world||*actions)&&buttons){
        if(!gate->waiting_for_release)gate->actions_only=!*world;
        else if(*world)gate->actions_only=false;
        gate->waiting_for_release=true;
    }
    bool blocked=gate->waiting_for_release;
    if(blocked){if(gate->actions_only)*actions=true;else *world=true;}
    if(!buttons){gate->waiting_for_release=false;gate->actions_only=false;}
    return blocked;
}
PeUiChromeResult pe_ui_chrome_click(const PeTrainingUILayout *l,float x,float y,
    bool pressed,bool menu_open,bool hud_shortcuts){
    PeUiChromeResult r={.menu_open=menu_open,.action=-1};
    if(!l||!pressed)return r;
    if(menu_open){
        r.action=pe_training_ui_hit(l->menu_items,PE_UI_MENU_ITEMS,x,y);
        r.menu_open=false;r.consume_press=true;
    }else if(pe_training_ui_contains(l->menu_button,x,y)){
        r.menu_open=true;r.consume_press=true;
    }else if(hud_shortcuts){
        if(pe_training_ui_contains(l->health,x,y))r.action=1;
        else if(pe_training_ui_contains(l->mission_button,x,y))r.action=3;
        else if(pe_training_ui_contains(l->history_button,x,y))r.action=5;
        r.consume_press=r.action>=0;
    }
    return r;
}
