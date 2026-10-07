/* Actual sandbox health/request/UI helpers under CPU section GC. No window or
 * desktop input; the source no-ragdoll next-display-frame adapter is explicit. */
#include "../src/pe_sandbox_view.c"
#include <unistd.h>
static unsigned failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sandbox revival host:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static PeSandboxUIInput ui_input(void){return (PeSandboxUIInput){.focused=true,.mouse_x=640,.mouse_y=350,.quickslot=-1};}
static PeSandboxUIInput ui_click(PeTrainingUIRect r){PeSandboxUIInput i=ui_input();i.mouse_x=r.x+r.w/2;i.mouse_y=r.y+r.h/2;i.left_pressed=true;return i;}
static bool fresh_dead(PeGame*g){pe_game_destroy(g);if(!pe_game_init_mode(g,944,PE_GAME_ADVENTURE))return false;g->hp=0;g->stamina=0;g->comfort=100;return pe_inventory_add(&g->inventory,937,2);}
static void camera_blocked(PeSandboxUIResult r){
    PeSandboxCameraControls camera;pe_sandbox_camera_controls_init(&camera);
    PeSandboxCameraControlResult out={0};
    CHECK(pe_sandbox_camera_controls_step(&camera,(PeSandboxCameraControlInput){.dt=.1f,.focused=true,.input_allowed=!r.consume_world,.cycle_pressed=true,.right_pressed=true,.right_down=true,.mouse_dx=20},&out));
    CHECK(!out.capture&&!out.look&&!out.world_input&&!out.cycle_accepted);
}
static void ui_and_lifecycle(PeGame*g){
    CHECK(fresh_dead(g));PeSandboxUI ui;pe_sandbox_ui_init(&ui,1280,720);
    PeAttributeClock clock={.seconds=100};int shots=pe_inventory_count(&g->inventory,937);
    PeInventory inventory=g->inventory;PeSandboxUIInput i=ui_input();i.response=1;
    PeSandboxUIResult r=pe_sandbox_ui_route(&ui,g,i);
    CHECK(ui.revival_open&&r.revive&&r.consume_world&&r.cursor_visible&&!r.camera_cycle_allowed);
    CHECK(g->hp==0&&!memcmp(&inventory,&g->inventory,sizeof inventory));camera_blocked(r);
    CHECK(!sandbox_revive_request(g,&clock)); /* Health owners must match frame. */
    CHECK(sandbox_health_frame(&clock,g,0,true,false));
    float x=g->player.x,z=g->player.z;PeRevival dead=g->revival;
    for(unsigned n=0;n<20;n++)pe_game_step(g,(PeInput){.move_x=1,.move_z=1,.jump=true,.sprint=true},PE_FIXED_DT);
    CHECK(g->player.x==x&&g->player.z==z&&!g->sprinting&&!g->jumping&&g->hp==0&&g->stamina==0);
    CHECK(!memcmp(&dead,&g->revival,sizeof dead));
    i=ui_click(pe_sandbox_ui_revival_button(&ui,0));r=pe_sandbox_ui_route(&ui,g,i);
    CHECK(r.revive&&r.consume_world);
    PeSandboxPointerGate pointer={0};CHECK(pe_sandbox_pointer_route(&pointer,&r,1));
    CHECK(sandbox_revive_request(g,&clock));
    CHECK(g->hp==g->max_hp*.333f&&g->stamina==g->stamina_max&&g->comfort==75);
    CHECK(g->revival.phase==PE_REVIVAL_GETTING_UP&&pe_revival_damage_disabled(&g->revival));
    CHECK(sandbox_hunt_runs(&ui,r));
    PeSandboxUI other_ui=ui;other_ui.revival_open=false;CHECK(!sandbox_hunt_runs(&other_ui,r));
    CHECK(pe_inventory_count(&g->inventory,937)==shots-1&&!sandbox_revive_request(g,&clock));
    PeRevival getting_up=g->revival;PeAttributeClock oldclock=clock;
    for(unsigned n=0;n<30;n++)pe_game_step(g,(PeInput){.move_x=1,.jump=true},PE_FIXED_DT);
    CHECK(!memcmp(&getting_up,&g->revival,sizeof getting_up)&&!memcmp(&oldclock,&clock,sizeof clock));
    CHECK(g->player.x==x&&g->player.z==z);
    i=ui_input();i.response=1;r=pe_sandbox_ui_route(&ui,g,i);CHECK(ui.revival_open&&!r.revive&&r.consume_world);
    CHECK(sandbox_health_frame(&clock,g,.25,true,true));CHECK(!memcmp(&getting_up,&g->revival,sizeof getting_up));
    CHECK(sandbox_health_frame(&clock,g,.25,false,false));CHECK(!memcmp(&getting_up,&g->revival,sizeof getting_up));
    CHECK(clock.seconds==oldclock.seconds);
    CHECK(sandbox_health_frame(&clock,g,.125,true,false));
    CHECK(g->revival.phase==PE_REVIVAL_GRACE&&g->revival.enable_at==(float)clock.seconds+1.5f);
    i=ui_input();r=pe_sandbox_ui_route(&ui,g,i);CHECK(!ui.revival_open&&!r.revive);
    CHECK(pe_sandbox_pointer_route(&pointer,&r,1)&&r.consume_world);camera_blocked(r);
    r=pe_sandbox_ui_route(&ui,g,ui_input());CHECK(pe_sandbox_pointer_route(&pointer,&r,0)&&r.consume_world);
    r=pe_sandbox_ui_route(&ui,g,ui_input());CHECK(!pe_sandbox_pointer_route(&pointer,&r,0)&&!r.consume_world);
    for(unsigned n=0;n<11;n++)CHECK(sandbox_health_frame(&clock,g,.125,true,false));
    CHECK(pe_revival_damage_disabled(&g->revival));
    CHECK(sandbox_health_frame(&clock,g,.125,true,false));CHECK(g->revival.phase==PE_REVIVAL_IDLE);
    CHECK(!sandbox_revive_request(g,&clock)&&pe_inventory_count(&g->inventory,937)==shots-1);
}
static void dead_load_pause_and_rollback(PeGame*g){
    CHECK(fresh_dead(g));PeSandboxUI ui;pe_sandbox_ui_init(&ui,1280,720);
    PeAttributeClock clock={.seconds=200};CHECK(sandbox_health_frame(&clock,g,0,true,false));
    PeSandboxUIInput i=ui_input();i.escape=true;PeSandboxUIResult r=pe_sandbox_ui_route(&ui,g,i);
    CHECK(ui.paused&&r.paused&&r.consume_world&&!r.revive);
    i=ui_input();i.response=1;r=pe_sandbox_ui_route(&ui,g,i);CHECK(!r.revive&&g->hp==0);
    ui.paused=false;i=ui_click(pe_sandbox_ui_revival_button(&ui,1));r=pe_sandbox_ui_route(&ui,g,i);
    CHECK(r.load&&!r.revive&&r.consume_world&&g->hp==0);
    i=ui_input();i.response=1;i.load=true;r=pe_sandbox_ui_route(&ui,g,i);CHECK(r.load&&!r.revive);
    PeGame *loaded=calloc(1,sizeof *loaded);CHECK(loaded);if(!loaded)return;
    char path[]="build/revival-host-save-XXXXXX",error[256];int fd=mkstemp(path);CHECK(fd>=0);
    if(fd<0){free(loaded);return;}close(fd);
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));
    PeRevival zero={0};CHECK(loaded->hp==0&&!memcmp(&loaded->revival,&zero,sizeof zero));
    CHECK(sandbox_health_frame(&clock,loaded,.125,true,false));
    int shots=pe_inventory_count(&loaded->inventory,937);CHECK(sandbox_revive_request(loaded,&clock));
    CHECK(!sandbox_revive_request(loaded,&clock)&&pe_inventory_count(&loaded->inventory,937)==shots-1);
    PeRevival before=loaded->revival;PeAttributeClock saved=clock;float hp=loaded->hp;
    loaded->comfort=NAN;CHECK(!sandbox_health_frame(&clock,loaded,.125,true,false));
    CHECK(!memcmp(&before,&loaded->revival,sizeof before)&&!memcmp(&saved,&clock,sizeof saved)&&loaded->hp==hp);
    loaded->comfort=75;CHECK(sandbox_health_frame(&clock,loaded,.125,true,false));CHECK(loaded->revival.phase==PE_REVIVAL_GRACE);
    unlink(path);pe_game_destroy(loaded);free(loaded);
}
int main(void){PeGame*g=calloc(1,sizeof *g);CHECK(g);if(!g)return 1;ui_and_lifecycle(g);dead_load_pause_and_rollback(g);pe_game_destroy(g);free(g);if(failures)return 1;puts("sandbox revival host: real UI/request, modal ownership, death/get-up input freeze, frame/grace, pause/focus, load and rollback passed");return 0;}
