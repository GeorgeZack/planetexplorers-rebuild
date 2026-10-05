#include "pe_sandbox_actor.h"
#include "pe_actor_scene_view.h"
#include "pe_player_locomotion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BODY, KNIFE_STOWED, SHOVEL_STOWED, SHOVEL_HELD, AXE_HELD, WOOD_AXE_HELD,
       PICKAXE_STOWED, PICKAXE_HELD, DIG_BODY, DIG_TOOL, FELL_BODY, FELL_TOOL, WOOD_FELL_TOOL,
       MINE_BODY, MINE_TOOL, VIEW_COUNT };
static const char *const folders[VIEW_COUNT]={"player","player-knife-stowed","player-shovel-stowed",
    "player-shovel","player-axe","player-wood-axe","player-pickaxe-stowed","player-pickaxe",
    "player-dig","player-shovel-dig","player-cut","player-axe-cut","player-wood-axe-cut","player-mine","player-pickaxe-mine"};
static const char *const action_names[3][3]={{"Spade1","Spade2","Spade3"},{"Felling1","Felling2","Felling3"},{"Mine1","Mine2","Mine3"}};
/* Exact female source clocks. Shovel/Felling native captures09/08 and installed
 * female Mine clips221/228/237. Mine poses require their separate recovered
 * assets: equal durations do not make Spade and Mine interchangeable. PEAC2
 * terminal timestamps are required throughout. */
static const double action_duration[3][3]={{.6999969482421875,1.3333358764648438,.8333358764648438},
    {.6000003814697266,1.100006103515625,.6000003814697266},
    {.6999969482421875,1.3333358764648438,.8333358764648438}};
static const unsigned action_frames[3][3]={{22,42,27},{20,35,20},{22,42,27}};
static const int action_bodies[3]={DIG_BODY,FELL_BODY,MINE_BODY};
static int action_kind(int item){return item==12?0:(item==26||item==1527)?1:item==19?2:-1;}
struct PeSandboxActor {
    PeActorSceneView views[VIEW_COUNT];
    size_t clips[VIEW_COUNT][PE_LOCOMOTION_COUNT];
    PePlayerLocomotion locomotion;
    PeVec3 previous;
    bool have_previous,was_jumping,ok;
    double elapsed;
    struct { int item_id,phase;uint32_t instance_id;bool held;double seconds; } action;
};
static void error_set(char *error,size_t cap,const char *message){if(error&&cap)snprintf(error,cap,"%s",message);}
static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static bool clocks_match(const PeActorClip *a,const PeActorClip *b){return a&&b&&!strcmp(a->name,b->name)&&a->fps==b->fps&&a->duration==b->duration&&a->frames==b->frames&&a->loop==b->loop&&a->terminal_at_duration==b->terminal_at_duration;}
void pe_sandbox_actor_unload(PeSandboxActor *a){if(a){for(int i=0;i<VIEW_COUNT;i++)pe_actor_scene_view_unload(&a->views[i]);free(a);}}
PeSandboxActor *pe_sandbox_actor_load(const char *root,char *error,size_t cap){
    if(error&&cap)*error=0;
    if(!root||!root[0]||!IsWindowReady()){error_set(error,cap,"Sandbox actor requires assets and an active graphics context");return NULL;}
    PeSandboxActor *a=calloc(1,sizeof *a);if(!a){error_set(error,cap,"Sandbox actor allocation failed");return NULL;}
    double durations[PE_LOCOMOTION_COUNT]={0};
    bool present[VIEW_COUNT]={0};char paths[VIEW_COUNT][4096];
    for(int v=0;v<VIEW_COUNT;v++) {
        int n=snprintf(paths[v],sizeof paths[v],"%s/%s/recording.peas",root,folders[v]);
        if(n<0||(size_t)n>=sizeof paths[v]){error_set(error,cap,"Sandbox actor path too long");goto fail;}
        present[v]=v==BODY||FileExists(paths[v]);
    }
    bool shovel_group=present[SHOVEL_HELD]||present[DIG_BODY]||present[DIG_TOOL];
    bool axe_group=present[AXE_HELD]||present[FELL_TOOL];
    bool wood_group=present[WOOD_AXE_HELD]||present[WOOD_FELL_TOOL];
    bool pickaxe_group=present[PICKAXE_STOWED]||present[PICKAXE_HELD]||present[MINE_BODY]||present[MINE_TOOL];
    if((shovel_group&&!(present[SHOVEL_STOWED]&&present[SHOVEL_HELD]&&present[DIG_BODY]&&present[DIG_TOOL]))||
       (axe_group&&!(present[AXE_HELD]&&present[FELL_BODY]&&present[FELL_TOOL]))||
       (wood_group&&!(present[WOOD_AXE_HELD]&&present[FELL_BODY]&&present[WOOD_FELL_TOOL]))||
       (pickaxe_group&&!(present[PICKAXE_STOWED]&&present[PICKAXE_HELD]&&present[MINE_BODY]&&present[MINE_TOOL]))) {
        error_set(error,cap,"Sandbox tool action assets must form a complete body/held/tool group");goto fail;
    }
    /* Decode every group and compare source clocks before uploading anything.
       Actor-scene retains the exact validated bytes through GPU upload. */
    for(int v=0;v<VIEW_COUNT;v++)if(present[v]) {
        if(!pe_actor_scene_view_prepare(&a->views[v],paths[v],error,cap))goto fail;
        if(v<DIG_BODY)for(int i=0;i<PE_LOCOMOTION_COUNT;i++) {
            a->clips[v][i]=pe_actor_scene_view_find_clip(&a->views[v],pe_player_locomotion_name(i));
            const PeActorClip *clip=pe_actor_scene_view_clip_at(&a->views[v],a->clips[v][i]);
            if(!clip){error_set(error,cap,"Sandbox actor is missing a required locomotion clip");goto fail;}
            if(v==BODY)durations[i]=clip->duration;
            else if(!clocks_match(pe_actor_scene_view_clip_at(&a->views[BODY],a->clips[BODY][i]),clip)) {
                error_set(error,cap,"Sandbox attachment locomotion clock differs from body");goto fail;
            }
        }
        else {
            int kind=v>=MINE_BODY?2:v>=FELL_BODY?1:0,body=action_bodies[kind];
            if(pe_actor_scene_view_clip_count(&a->views[v])!=3){error_set(error,cap,"Tool action scene must contain exactly three source clips");goto fail;}
            for(int i=0;i<3;i++) {
                a->clips[v][i]=pe_actor_scene_view_find_clip(&a->views[v],action_names[kind][i]);
                const PeActorClip *clip=pe_actor_scene_view_clip_at(&a->views[v],a->clips[v][i]);
                if(!clip||fabs(clip->duration-action_duration[kind][i])>1e-6||clip->fps!=30||
                   clip->frames!=action_frames[kind][i]||clip->loop!=(i==1)||!clip->terminal_at_duration) {
                    error_set(error,cap,"Tool action scene differs from recovered female source clock");goto fail;
                }
                if(v!=body&&!clocks_match(pe_actor_scene_view_clip_at(&a->views[body],a->clips[body][i]),clip)) {
                    error_set(error,cap,"Tool action body and attachment clocks differ");goto fail;
                }
            }
        }
    }
    if(!pe_player_locomotion_init(&a->locomotion,durations)){error_set(error,cap,"Invalid sandbox locomotion durations");goto fail;}
    for(int v=0;v<VIEW_COUNT;v++)if(a->views[v].manifest&&!pe_actor_scene_view_upload(&a->views[v],error,cap))goto fail;
    a->action.phase=-1;a->ok=true;return a;
fail:pe_sandbox_actor_unload(a);return NULL;
}
bool pe_sandbox_actor_ok(const PeSandboxActor *a){return a&&a->ok;}
bool pe_sandbox_actor_action_ready(const PeSandboxActor *a,int item) {
    if(!a||!a->ok)return false;
    if(item==12)return a->views[SHOVEL_HELD].loaded&&a->views[SHOVEL_STOWED].loaded&&a->views[DIG_BODY].loaded&&a->views[DIG_TOOL].loaded;
    if(item==26)return a->views[WOOD_AXE_HELD].loaded&&a->views[FELL_BODY].loaded&&a->views[WOOD_FELL_TOOL].loaded;
    if(item==1527)return a->views[AXE_HELD].loaded&&a->views[FELL_BODY].loaded&&a->views[FELL_TOOL].loaded;
    if(item==19)return a->views[PICKAXE_STOWED].loaded&&a->views[PICKAXE_HELD].loaded&&a->views[MINE_BODY].loaded&&a->views[MINE_TOOL].loaded;
    return false;
}
bool pe_sandbox_actor_set_action(PeSandboxActor *a,const PeSandboxActionPose *p) {
    if(!a||!a->ok)return false;
    if(!p){memset(&a->action,0,sizeof a->action);a->action.phase=-1;return true;}
    if(!isfinite(p->seconds)||p->seconds<0||p->seconds>1000000||p->item_id<0||
       (p->held&&(!p->instance_id||!pe_sandbox_actor_action_ready(a,p->item_id))))return false;
    int phase=-1;
    if(p->clip) {
        if(!p->held||!pe_sandbox_actor_action_ready(a,p->item_id))return false;
        int kind=action_kind(p->item_id);
        if(kind<0)return false;
        for(int i=0;i<3;i++)if(!strcmp(p->clip,action_names[kind][i]))phase=i;
        if(phase<0||(phase!=1&&p->seconds>action_duration[kind][phase]+1e-6))return false;
    }
    if(!p->item_id&&(p->instance_id||p->held||p->clip))return false;
    a->action.item_id=p->item_id;a->action.instance_id=p->instance_id;a->action.held=p->held;
    a->action.phase=phase;a->action.seconds=p->seconds;return true;
}

void pe_sandbox_actor_update(PeSandboxActor *a,float dt,const PeGame *g){
    if(!a||!a->ok||!g||!isfinite(dt)||dt<0||dt>1||!finite3(g->player)||!finite3(g->velocity))return;
    if(dt==0)return;
    PeVec3 measured={0,g->velocity.y,0};bool reset=false;
    if(a->have_previous){
        float dx=g->player.x-a->previous.x,dz=g->player.z-a->previous.z;
        double displacement=hypot((double)dx,dz);
        /* Max source sprint is8m/s; this allows frame jitter but not a load or
           imported-world relocation to become an enormous gait speed. */
        if(displacement>12.0*dt+.5||fabsf(g->player.y-a->previous.y)>20*dt+2)reset=true;
        else{measured.x=dx/dt;measured.z=dz/dt;}
    }
    if(reset){double durations[PE_LOCOMOTION_COUNT];memcpy(durations,a->locomotion.duration,sizeof durations);pe_player_locomotion_init(&a->locomotion,durations);measured=(PeVec3){0};}
    PePlayerLocomotionInput input={.velocity=measured,.grounded=g->grounded,.walk=hypotf(measured.x,measured.z)<=PE_WALK_SPEED+.25f,
        .sprint=g->sprinting,.jump_started=g->jumping&&!a->was_jumping&&!reset};
    if(pe_player_locomotion_step(&a->locomotion,input,dt)){
        a->elapsed=fmod(a->elapsed+dt,3600);a->previous=g->player;a->have_previous=true;a->was_jumping=g->jumping;
    }
}
static void draw_pose(PeSandboxActor *a,const PeGame *g,PePlayerLocomotionClip clip,double seconds,Vector3 position,float yaw,bool preview) {
    if(!a||!a->ok||!g||!isfinite(yaw))return;
    const PeSlot *equipped=pe_game_equipped_tool(g);
    bool bound=equipped&&equipped->instance_id==a->action.instance_id&&equipped->item_id==a->action.item_id;
    bool held=bound&&a->action.held;
    int body=BODY,tool=-1,phase=a->action.phase;
    if(equipped&&equipped->item_id==33)tool=KNIFE_STOWED;
    else if(equipped&&equipped->item_id==12)tool=held?SHOVEL_HELD:SHOVEL_STOWED;
    else if(equipped&&equipped->item_id==1527&&held)tool=AXE_HELD;
    else if(equipped&&equipped->item_id==26&&held)tool=WOOD_AXE_HELD;
    else if(equipped&&equipped->item_id==19)tool=held?PICKAXE_HELD:PICKAXE_STOWED;
    size_t body_clip=a->clips[BODY][clip],tool_clip=tool>=0?a->clips[tool][clip]:0;
    int kind=equipped?action_kind(equipped->item_id):-1;
    if(!preview&&held&&phase>=0&&kind>=0&&pe_sandbox_actor_action_ready(a,equipped->item_id)) {
        body=action_bodies[kind];tool=equipped->item_id==26?WOOD_FELL_TOOL:body+1;
        body_clip=a->clips[body][phase];tool_clip=a->clips[tool][phase];seconds=a->action.seconds;
    }
    bool ok=pe_actor_scene_view_draw_clip(&a->views[body],body_clip,seconds,position,yaw,1,WHITE);
    if(tool>=0&&a->views[tool].loaded)ok=pe_actor_scene_view_draw_clip(&a->views[tool],tool_clip,seconds,position,yaw,1,WHITE)&&ok;
    if(!ok){a->ok=false;fprintf(stderr,"Sandbox actor pose rendering failed\n");}
}
void pe_sandbox_actor_draw(PeSandboxActor *a,const PeGame *g){
    if(!g||!finite3(g->player)||!isfinite(g->yaw))return;
    draw_pose(a,g,a?a->locomotion.clip:PE_LOCOMOTION_IDLE,a?a->locomotion.seconds:0,(Vector3){g->player.x,g->player.y,g->player.z},180-g->yaw*RAD2DEG,false);
}
void pe_sandbox_actor_preview(void *context,const PeGame *g){
    PeSandboxActor *a=context;if(!a||!a->ok||!g)return;
    Camera3D camera={{0,1,3.8f},{0,.94f,0},{0,1,0},30,CAMERA_PERSPECTIVE};
    BeginMode3D(camera);draw_pose(a,g,PE_LOCOMOTION_IDLE,a->elapsed,(Vector3){0},0,true);EndMode3D();
}
