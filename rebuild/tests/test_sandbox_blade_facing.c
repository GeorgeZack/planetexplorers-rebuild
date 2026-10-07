/* Real blade clocks and host transaction. No window, GPU or desktop input. */
#include "../src/pe_sandbox_view.c"
static unsigned failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"blade facing host:%d %s\n",__LINE__,#c);failures++;}}while(0)
static bool angle(float a,float b){return fabsf(remainderf(a-b,360))<.001f;}
static PeSandboxMeleeGate gate(void){return (PeSandboxMeleeGate){.alive=true,.weapon_owned=true,.grounded=true,.third_person=true,.action_allowed=true,.equipped_item_id=33,.equipped_instance_id=91,.drawn_instance_id=91};}
static bool tick(PeSandboxMelee *m,SandboxBladeFacing *f,PeHuntEncounter *e,PeSandboxMeleeGate g,bool press,PeVec3 move,float *yaw,float *stamina,bool *began){
    PeCombatInterval span;bool rejected=false;
    bool ok=sandbox_blade_tick(m,f,e,g,press,move,(PeVec3){0},*yaw,stamina,&span,yaw,began,&rejected);
    CHECK(ok&&!rejected);if(!ok)return false;
    PeSandboxMeleeFrame frame={.gate=g,.player_yaw=*yaw,.player_scale=1};PeSandboxMeleeUpdate u;
    CHECK(pe_sandbox_melee_resolve(m,&frame,NULL,0,&u));return true;
}
int main(void){
    char error[256];
    const char *body_path=PE_SOURCE_ROOT "/rebuild/build/reference-assets/player/player-part-0-material-0.peac";
    const char *blade_path=PE_SOURCE_ROOT "/rebuild/build/reference-assets/player-knife-drawn/knife-drawn-part-0-material-0.peac";
    const char *box_path=PE_SOURCE_ROOT "/rebuild/build/reference-assets/player-knife-drawn/knife-drawn.pehb";
    PeActor *body=pe_actor_load(body_path,error,sizeof error),*blade=pe_actor_load(blade_path,error,sizeof error);
    PeHitboxes *boxes=blade?pe_hitboxes_load(box_path,blade,error,sizeof error):NULL;
    if(!body||!blade||!boxes){fprintf(stderr,"blade facing host: required assets unavailable: %s\n",error);pe_hitboxes_free(boxes);pe_actor_free(body);pe_actor_free(blade);return 77;}
    PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=body,.drawn_weapon=blade,.drawn_capsule=boxes,.gender=PE_COMBAT_FEMALE,.player_id=1,.attack=19,.stamina_reduce_percent=1,.ordinary_motion_ready=true};
    PeSandboxMelee m;CHECK(pe_sandbox_melee_init(&m,&setup));SandboxBladeFacing facing={0};PeHuntEncounter e={0};
    PeSandboxMeleeGate g=gate();float yaw=37,stamina=100;bool began=false;
    PeVec3 request;
    CHECK(pe_melee_facing_request((PeVec3){0},false,true,0,&request));
    CHECK(tick(&m,&facing,&e,g,true,request,&yaw,&stamina,&began));
    CHECK(began&&stamina==95&&angle(yaw,37)&&facing.active&&!facing.target.selected);
    /* Changing movement does not steer an already admitted stage. */
    CHECK(tick(&m,&facing,&e,g,false,(PeVec3){-1,0,0},&yaw,&stamina,&began));
    CHECK(!began&&angle(yaw,37)&&stamina==95);
    bool queued=false,redirected=false,saw_old_interval=false,saw_next=false;
    for(unsigned i=0;i<150&&m.action.active&&!saw_next;i++){
        bool first=!queued&&m.action.combo_window;
        bool redirect=queued&&!redirected&&m.action.stage==0;
        CHECK(tick(&m,&facing,&e,g,first||redirect,first?(PeVec3){-1,0,0}:(PeVec3){1,0,0},&yaw,&stamina,&began));
        if(first)queued=true;
        if(redirect)redirected=true;
        if(m.action.stage==1&&facing.stage==0){saw_old_interval=true;CHECK(angle(yaw,37));}
        if(facing.stage==1){saw_next=true;CHECK(angle(yaw,90)&&!facing.queued);}
    }
    CHECK(queued&&redirected&&saw_old_interval&&saw_next&&stamina==90);
    g.action_allowed=false;CHECK(tick(&m,&facing,&e,g,false,(PeVec3){0},&yaw,&stamina,&began));
    CHECK(!m.action.active&&!facing.active&&stamina==90);
    g=gate();g.equipped_instance_id=g.drawn_instance_id=92;
    CHECK(pe_melee_facing_request((PeVec3){1,0,0},false,true,0,&request));
    CHECK(tick(&m,&facing,&e,g,true,request,&yaw,&stamina,&began));
    CHECK(began&&facing.instance==92&&angle(yaw,90)&&stamina==85);
    pe_sandbox_melee_cancel(&m);facing=(SandboxBladeFacing){0};
    /* Camera-forward is only a requested stage basis. The admitted stage is
     * frozen, and continuation presses replace the queued request without
     * changing the outgoing interval's heading. No target is present here. */
    yaw=37;stamina=100;
    CHECK(pe_melee_facing_request((PeVec3){1,0,0},true,true,0,&request));
    CHECK(tick(&m,&facing,&e,g,true,request,&yaw,&stamina,&began));
    CHECK(began&&angle(yaw,180)&&stamina==95);
    CHECK(pe_melee_facing_request((PeVec3){0},true,true,PI/2,&request));
    CHECK(tick(&m,&facing,&e,g,false,request,&yaw,&stamina,&began));
    CHECK(!began&&angle(yaw,180)&&stamina==95);
    queued=false;redirected=false;saw_old_interval=false;saw_next=false;
    for(unsigned i=0;i<150&&m.action.active&&!saw_next;i++){
        bool first=!queued&&m.action.combo_window;
        bool redirect=queued&&!redirected&&m.action.stage==0;
        /* First press aims +X; the most recent continuation press aims -X.
         * Later unpressed camera changes back to +Z do not replace it. */
        float camera_yaw=first?PI/2:redirect?-PI/2:PI;
        CHECK(pe_melee_facing_request((PeVec3){0},true,true,camera_yaw,&request));
        CHECK(tick(&m,&facing,&e,g,first||redirect,request,&yaw,&stamina,&began));
        if(first)queued=true;
        if(redirect)redirected=true;
        if(m.action.stage==1&&facing.stage==0){saw_old_interval=true;CHECK(angle(yaw,180));}
        if(facing.stage==1){saw_next=true;CHECK(angle(yaw,-90)&&!facing.queued);}
    }
    CHECK(queued&&redirected&&saw_old_interval&&saw_next&&stamina==90);
    pe_sandbox_melee_cancel(&m);facing=(SandboxBladeFacing){0};
    /* The same preference is inert in captured/first-person camera modes. */
    yaw=37;
    CHECK(pe_melee_facing_request((PeVec3){1,0,0},true,false,0,&request));
    CHECK(tick(&m,&facing,&e,g,true,request,&yaw,&stamina,&began));
    CHECK(began&&angle(yaw,90)&&stamina==85);
    pe_sandbox_melee_cancel(&m);facing=(SandboxBladeFacing){0};
    /* Invalid target query must undo start AND its stamina debit. */
    PeSandboxMelee prior=m;SandboxBladeFacing old=facing;float energy=stamina,out_yaw=999;
    PeCombatInterval span={.stage=99},span_before=span;bool rejected=true;began=true;
    CHECK(!sandbox_blade_tick(&m,&facing,&e,g,true,(PeVec3){1,0,0},(PeVec3){NAN,0,0},yaw,&stamina,&span,&out_yaw,&began,&rejected));
    CHECK(!memcmp(&m,&prior,sizeof m)&&!memcmp(&facing,&old,sizeof old)&&stamina==energy&&out_yaw==999&&began&&rejected&&!memcmp(&span,&span_before,sizeof span));
    g.action_allowed=false;
    CHECK(sandbox_blade_tick(&m,&facing,&e,g,true,(PeVec3){0},(PeVec3){0},yaw,&stamina,&span,&out_yaw,&began,&rejected));
    CHECK(!began&&rejected&&!m.action.active&&!facing.active&&stamina==energy);
    pe_hitboxes_free(boxes);pe_actor_free(body);pe_actor_free(blade);
    printf("Blade facing host: %s (%u failures, real blade assets)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
