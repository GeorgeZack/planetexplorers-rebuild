/* Actual host routing with owned source/captured curves, no raylib execution.
 * Compile with function/data sections and linker garbage collection. */
#include "pe_test_paths.h"
#include "../src/pe_sandbox_view.c"

static unsigned failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sandbox sword motion host:%d %s\n",__LINE__,#c);failures++;}}while(0)
static bool readable(const char *path){FILE *f=path?fopen(path,"rb"):NULL;if(!f)return false;fclose(f);return true;}
static PeCombatInterval interval(PeCombatVariant variant,unsigned stage,double from,double to){
    return (PeCombatInterval){.valid=true,.variant=variant,.gender=PE_COMBAT_FEMALE,
        .view=PE_COMBAT_THIRD_PERSON,.stage=stage,.from=from,.to=to};
}
static void routed(const PeRootMotion *running,const PeSwordRootMotion *ordinary,PeCombatInterval span){
    PeCombatInterval before=span;PeVec3 current={.3f,-2,.5f},out={99,88,77},expected={0};
    bool is_running=span.variant==PE_COMBAT_RUNNING&&span.stage==0;
    CHECK(is_running?pe_running_attack_motion_request(running,&span,current,37,.125,&expected):
        pe_sword_root_motion_request(ordinary,&span,current,37,.125,&expected));
    CHECK(sandbox_melee_motion(running,ordinary,&span,current,37,.125,&out));
    CHECK(!memcmp(&out,&expected,sizeof out)&&!memcmp(&span,&before,sizeof span));
    PeVec3 repeat={0};CHECK(sandbox_melee_motion(running,ordinary,&span,current,37,.125,&repeat));
    CHECK(!memcmp(&repeat,&out,sizeof out)); /* Pure query does not consume/advance the interval. */
}
static void routing(const PeRootMotion *running,const PeSwordRootMotion *ordinary){
    bool distinct=false;
    for(unsigned sample=0;sample<12;sample++){
        double from=.05*sample,to=from+.04;
        PeCombatInterval normal=interval(PE_COMBAT_STATIONARY,0,from,to),run=interval(PE_COMBAT_RUNNING,0,from,to);
        PeVec3 a={0},b={0};
        CHECK(sandbox_melee_motion(running,ordinary,&normal,(PeVec3){0},0,1,&a));
        CHECK(sandbox_melee_motion(running,ordinary,&run,(PeVec3){0},0,1,&b));
        if(hypot((double)a.x-b.x,(double)a.z-b.z)>1e-4)distinct=true;
    }
    CHECK(distinct); /* A stage-only dispatcher must not send ordinary0 to408. */
    for(unsigned stage=0;stage<4;stage++){
        routed(running,ordinary,interval(PE_COMBAT_STATIONARY,stage,.1,.12));
        routed(running,ordinary,interval(PE_COMBAT_RUNNING,stage,.1,.12));
    }
    PeCombatInterval normal=interval(PE_COMBAT_STATIONARY,2,.1,.12),run=interval(PE_COMBAT_RUNNING,0,.1,.12);
    PeVec3 out={99,88,77};
    CHECK(sandbox_melee_motion(running,NULL,&normal,(PeVec3){2,3,4},37,.125,&out)&&out.x==0&&out.y==0&&out.z==0);
    normal.variant=PE_COMBAT_RUNNING;
    CHECK(sandbox_melee_motion(running,NULL,&normal,(PeVec3){2,3,4},37,.125,&out)&&out.x==0&&out.y==0&&out.z==0);
    PeVec3 expected={0};CHECK(pe_running_attack_motion_request(running,&run,(PeVec3){0},37,.125,&expected));
    CHECK(sandbox_melee_motion(running,NULL,&run,(PeVec3){0},37,.125,&out)&&!memcmp(&out,&expected,sizeof out));
    out=(PeVec3){99,88,77};PeVec3 before=out;
    CHECK(!sandbox_melee_motion(NULL,ordinary,&run,(PeVec3){0},37,.125,&out)&&!memcmp(&out,&before,sizeof out));
}
static void retained_combo(const PeRootMotion *running,const PeSwordRootMotion *ordinary,PeCombatVariant variant){
    PeCombat action;pe_combat_init(&action);float stamina=100;PeCombatInterval span={0};
    CHECK(pe_combat_begin_variant(&action,PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,variant,1,&stamina));
    bool queued=false;unsigned steps=0;PeCombatAdvance status=PE_COMBAT_IDLE;
    while(action.active&&action.stage==0&&steps++<200){
        status=pe_combat_advance_interval(&action,.01,.01,&stamina,&span);
        CHECK(status!=PE_COMBAT_INVALID);
        if(!queued&&action.combo_window){CHECK(pe_combat_queue_combo(&action));queued=true;}
    }
    CHECK(queued&&steps<200&&status==PE_COMBAT_NEXT_STAGE&&action.stage==1&&action.clip_time==0);
    CHECK(span.valid&&span.stage==0&&span.from>0&&span.to>span.from&&span.variant==variant);
    PeCombat action_before=action;PeCombatInterval before=span;float energy=stamina;
    routed(running,ordinary,span);
    CHECK(!memcmp(&action,&action_before,sizeof action)&&!memcmp(&span,&before,sizeof span)&&stamina==energy);
    /* The NEXT update consumes Attack7's own clock from0, not a reversed
     * subtraction against the preceding source clip. */
    CHECK(pe_combat_advance_interval(&action,.01,.01,&stamina,&span)==PE_COMBAT_ADVANCED);
    CHECK(span.valid&&span.stage==1&&span.from==0&&span.to==.01);
    routed(running,ordinary,span);
}
static void invalid(const PeRootMotion *running,const PeSwordRootMotion *ordinary){
    for(unsigned k=0;k<12;k++){
        PeCombatInterval span=interval(PE_COMBAT_STATIONARY,0,.1,.12);PeVec3 velocity={0},out={7,8,9},before=out;
        double yaw=0,dt=.125;
        if(k==0)span.valid=false;
        if(k==1)span.stage=4;
        if(k==2)span.from=.2;
        if(k==3)span.to=NAN;
        if(k==4)span.from=-.1;
        if(k==5)span.to=2;
        if(k==6)span.gender=PE_COMBAT_MALE;
        if(k==7)span.view=PE_COMBAT_FIRST_PERSON;
        if(k==8)yaw=INFINITY;
        if(k==9)dt=0;
        if(k==10)dt=NAN;
        if(k==11)velocity.z=NAN;
        PeCombatInterval old;memcpy(&old,&span,sizeof old);
        CHECK(!sandbox_melee_motion(running,ordinary,&span,velocity,yaw,dt,&out));
        CHECK(!memcmp(&out,&before,sizeof out)&&!memcmp(&span,&old,sizeof old));
        if(k!=5&&k!=6&&k!=7){
            CHECK(!sandbox_melee_motion(running,NULL,&span,velocity,yaw,dt,&out));
            CHECK(!memcmp(&out,&before,sizeof out)&&!memcmp(&span,&old,sizeof old));
        }
    }
    PeVec3 out={7,8,9},before=out;PeCombatInterval span=interval(PE_COMBAT_STATIONARY,0,.1,.12);
    CHECK(!sandbox_melee_motion(running,ordinary,NULL,(PeVec3){0},0,1,&out)&&!memcmp(&out,&before,sizeof out));
    CHECK(!sandbox_melee_motion(running,ordinary,&span,(PeVec3){0},0,1,NULL));
}
static const char *pose_paths[]={
    "player-defence/player-defence.peac",
    "player/player-part-0-material-0.peac","player/player-part-1-material-0.peac",
    "player/player-part-2-material-0.peac","player/player-part-3-material-0.peac",
    "player/player-part-4-material-0.peac","player/player-part-4-material-1.peac",
    "player/player-part-5-material-0.peac","player/player-part-6-material-0.peac",
    "player/player-part-7-material-0.peac",
    "player-knife-drawn/knife-drawn-part-0-material-0.peac",
    "player-knife-drawn/knife-drawn-part-1-material-0.peac"
};
static bool pose_files_present(void){
    char path[2048];
    for(size_t i=0;i<sizeof pose_paths/sizeof *pose_paths;i++){
        pe_test_format(path,sizeof path,"%s/rebuild/build/reference-assets/%s",pe_test_root(),pose_paths[i]);
        if(!readable(path)){fprintf(stderr,"sandbox sword motion host: SKIP77 missing %s\n",path);return false;}
    }
    return true;
}
static void pose_clocks(const PeSwordRootMotion *ordinary){
    static const char *names[]={"Attack6","Attack7","Attack8","Attack9"};
    PeActorClip reference[4]={0};char path[2048],error[256];
    for(size_t i=0;i<sizeof pose_paths/sizeof *pose_paths;i++){
        pe_test_format(path,sizeof path,"%s/rebuild/build/reference-assets/%s",pe_test_root(),pose_paths[i]);
        PeActor *actor=pe_actor_load(path,error,sizeof error);CHECK(actor);
        if(!actor){fprintf(stderr,"%s: %s\n",path,error);continue;}
        for(unsigned stage=0;stage<4;stage++){
            const PeActorClip *clip=pe_actor_clip(actor,pe_actor_find_clip(actor,names[stage]));
            const PeCombatClip *source=pe_combat_clip(PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,stage);
            CHECK(clip&&source);if(!clip||!source)continue;
            CHECK(clip->duration==pe_sword_root_motion_duration(ordinary,(uint32_t)source->path_id));
            CHECK(!clip->loop&&clip->terminal_at_duration&&clip->fps==30);
            if(!i)reference[stage]=*clip;
            else CHECK(clip->duration==reference[stage].duration&&clip->fps==reference[stage].fps&&
                clip->frames==reference[stage].frames&&clip->loop==reference[stage].loop&&
                clip->terminal_at_duration==reference[stage].terminal_at_duration);
        }
        pe_actor_free(actor);
    }
}
int main(void){
    const char *ordinary_path=getenv("PE_SWORD_ROOT_MOTION");
    if(!ordinary_path)ordinary_path=pe_test_path("rebuild/build/reference-assets/player-sword-motion/sword-root.pesr");
    const char *running_path=pe_test_path("rebuild/build/reference-assets/player-running-motion/running-knife.perm");
    if(!ordinary_path||!readable(ordinary_path)||!readable(running_path)){
        fprintf(stderr,"sandbox sword motion host: SKIP77 requires installed/PE_SWORD_ROOT_MOTION PESR1 and installed captured408 PERM1\n");return 77;
    }
    if(!pose_files_present())return 77;
    char error[256];PeSwordRootMotion *ordinary=pe_sword_root_motion_load(ordinary_path,error,sizeof error);
    if(!ordinary){fprintf(stderr,"sandbox sword motion host: %s\n",error);return 1;}
    PeRootMotion *running=pe_root_motion_load(running_path,error,sizeof error);
    if(!running){fprintf(stderr,"sandbox sword motion host: %s\n",error);pe_sword_root_motion_free(ordinary);return 1;}
    pose_clocks(ordinary);routing(running,ordinary);retained_combo(running,ordinary,PE_COMBAT_STATIONARY);
    retained_combo(running,ordinary,PE_COMBAT_RUNNING);invalid(running,ordinary);
    pe_root_motion_free(running);pe_sword_root_motion_free(ordinary);
    printf("Sandbox sword motion host: %s (%u failures; no graphics or asset skips)\n",failures?"FAILED":"PASS",failures);
    return failures?1:0;
}
