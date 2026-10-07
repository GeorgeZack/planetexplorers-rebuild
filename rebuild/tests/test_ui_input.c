#include "pe_ui_input.h"
#include <assert.h>
#include <stdio.h>

int pe_test_ui_input(void){
    const float sizes[][2]={{1280,720},{640,360},{954,1048},{2560,1080}};
    for(unsigned n=0;n<sizeof sizes/sizeof sizes[0];n++){
        PeUIWindows windows={0};PeTrainingUILayout l;
        assert(pe_training_ui_windows_layout(sizes[n][0],sizes[n][1],&windows,&l));
        PeTrainingUIRect targets[]={l.health,l.mission_button,l.menu_button};
        for(int k=0;k<3;k++){
            PeTrainingUIRect t=targets[k];float x=t.x+t.w*.5f,y=t.y+t.h*.5f;
            PeUiChromeResult r=pe_ui_chrome_click(&l,x,y,true,false,true);
            assert(r.consume_press&&r.action==(k==0?1:k==1?3:-1)&&r.menu_open==(k==2));
            r=pe_ui_chrome_click(&l,x,y,false,false,true);assert(!r.consume_press&&r.action==-1);
            if(k<2){r=pe_ui_chrome_click(&l,x,y,true,false,false);assert(!r.consume_press&&r.action==-1);}
        }
        for(int k=0;k<PE_UI_MENU_ITEMS;k++){
            PeTrainingUIRect t=l.menu_items[k];
            PeUiChromeResult r=pe_ui_chrome_click(&l,t.x+t.w*.5f,t.y+t.h*.5f,true,true,true);
            assert(r.consume_press&&!r.menu_open&&r.action==k);
        }
        /* Closing a menu over another affordance must not activate it. */
        PeUiChromeResult r=pe_ui_chrome_click(&l,l.health.x+1,l.health.y+1,true,true,true);
        assert(r.consume_press&&!r.menu_open&&r.action==-1);
        assert(l.mission_tracker.y-7*l.scale>=l.minimap.y+l.minimap.h+8*l.scale);
    }
    PeUiPointerGate gate={0};bool world=false,actions=true;
    assert(pe_ui_pointer_route(&gate,&world,&actions,1)&&!world&&actions&&gate.actions_only);
    world=actions=false;assert(pe_ui_pointer_route(&gate,&world,&actions,1)&&!world&&actions);
    /* Modal escalation cannot be downgraded by a later action-only frame. */
    world=true;actions=false;assert(pe_ui_pointer_route(&gate,&world,&actions,3)&&!gate.actions_only);
    world=false;actions=true;assert(pe_ui_pointer_route(&gate,&world,&actions,2)&&world);
    world=actions=false;assert(pe_ui_pointer_route(&gate,&world,&actions,0)&&world&&!gate.waiting_for_release);
    world=actions=false;assert(!pe_ui_pointer_route(&gate,&world,&actions,0)&&!world&&!actions);
    /* A UI close press and each held/release frame stay owned. */
    for(unsigned button=1;button<=2;button++){
        world=true;actions=false;assert(pe_ui_pointer_route(&gate,&world,&actions,button));
        for(int frame=0;frame<3;frame++){
            world=actions=false;assert(pe_ui_pointer_route(&gate,&world,&actions,button)&&world);
        }
        world=actions=false;assert(pe_ui_pointer_route(&gate,&world,&actions,0)&&world);
        world=actions=false;assert(!pe_ui_pointer_route(&gate,&world,&actions,button)&&!world&&!actions);
    }
    puts("shared UI input: chrome actions and held/release ownership passed");return 0;
}
#ifdef PE_UI_INPUT_TEST_MAIN
int main(void){return pe_test_ui_input();}
#endif
