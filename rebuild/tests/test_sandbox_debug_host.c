/* Actual host command dispatch: mock only the independently tested transaction. */
#define pe_debug_apply debug_apply_mock
#include "../src/pe_sandbox_view.c"
#undef pe_debug_apply
#include <assert.h>
static unsigned calls;
static bool transaction_ok=true;
bool debug_apply_mock(PeGame *game,PeDebugAction action,char *error,size_t capacity){
    calls++;assert(action!=PE_DEBUG_NONE);
    if(!transaction_ok){snprintf(error,capacity,"No safe hunting surface.");return false;}
    game->money++;snprintf(game->message,sizeof game->message,"Debug action committed.");return true;
}
static void hunt_controls(void){
    for(int mode=PE_SANDBOX_CAMERA_FREE;mode<=PE_SANDBOX_CAMERA_FIRST_PERSON;mode++){
        PeSandboxUI ui={.paused=true,.building=true,.free_cursor=true,.selected_slot=17};
        PeSandboxUI expected=ui;expected.building=false;expected.free_cursor=false;
        PeSandboxCameraControls camera={.mode=(PeSandboxCameraMode)mode,
            .right_tracking=true,.right_dragged=true,.right_seconds=.25f,.right_motion=3};
        bool third_person=mode!=PE_SANDBOX_CAMERA_FIRST_PERSON,build_mode=true;
        sandbox_debug_hunt_controls(&ui,&camera,&third_person,&build_mode);
        assert(third_person&&!build_mode&&!memcmp(&ui,&expected,sizeof ui));
        assert(camera.mode==PE_SANDBOX_CAMERA_FREE&&!camera.right_tracking&&!camera.right_dragged&&
            camera.right_seconds==0&&camera.right_motion==0);
        /* Reset does not admit actions while Settings/Pause owns input, nor
         * reinterpret the old held RMB gesture as a fresh free-camera drag. */
        PeSandboxCameraControlResult result;
        PeSandboxCameraControlInput input={.dt=1.f/60,.focused=true,.right_down=true};
        assert(pe_sandbox_camera_controls_step(&camera,input,&result));
        assert(result.third_person&&!result.world_input&&!result.capture&&!result.right_click);
        input.input_allowed=true;
        assert(pe_sandbox_camera_controls_step(&camera,input,&result));
        assert(result.third_person&&result.world_input&&!result.capture&&!result.right_click);
    }
}
int main(void){
    PeGame *g=calloc(1,sizeof *g);assert(g);g->mode=PE_GAME_ADVENTURE;
    PeUiSettings base={.open=true,.debug_available=true,.debug_enabled=true};
    for(unsigned i=0;i<6;i++){
        PeUiSettings s=base;s.debug_request=PE_DEBUG_MEDICINE;bool focused=true,paused=true;
        if(i==0)s.open=false;
        if(i==1)s.debug_available=false;
        if(i==2)s.debug_enabled=false;
        if(i==3)focused=false;
        if(i==4)paused=false;
        if(i==5)g->mode=PE_GAME_LEGACY;
        assert(sandbox_debug_command(&s,g,focused,paused,true)==PE_DEBUG_NONE);
        assert(!calls&&!g->money&&s.debug_request==PE_DEBUG_NONE);g->mode=PE_GAME_ADVENTURE;
    }
    PeUiSettings s=base;s.debug_request=PE_DEBUG_START_HUNT;
    assert(sandbox_debug_command(&s,g,true,true,false)==PE_DEBUG_NONE&&!calls&&!g->money);
    assert(strstr(s.debug_notice,"assets")&&s.open&&s.debug_enabled&&s.debug_request==PE_DEBUG_NONE);
    for(PeDebugAction action=PE_DEBUG_MEDICINE;action<=PE_DEBUG_START_HUNT;action++){
        s=base;s.debug_request=action;unsigned before=calls,money=g->money;
        assert(sandbox_debug_command(&s,g,true,true,true)==action&&calls==before+1&&g->money==money+1);
        assert(s.debug_request==PE_DEBUG_NONE&&s.open&&s.debug_enabled&&!strcmp(s.debug_notice,g->message));
        assert(sandbox_debug_command(&s,g,true,true,true)==PE_DEBUG_NONE&&calls==before+1&&g->money==money+1);
    }
    s=base;s.debug_request=PE_DEBUG_START_HUNT;transaction_ok=false;unsigned money=g->money;
    assert(sandbox_debug_command(&s,g,true,true,true)==PE_DEBUG_NONE&&g->money==money);
    assert(s.debug_request==PE_DEBUG_NONE&&strstr(s.debug_notice,"surface")&&s.open);
    hunt_controls();
    free(g);puts("Sandbox debug host: admission, one-shot requests, asset gate, failure reporting and paused hunt controls reset pass");return 0;
}
