#include "pe_sandbox_actor.h"
#include "pe_actor_scene_view.h"
#include "pe_ik_drive.h"
#include "pe_player_locomotion.h"
#include "pe_shield.h"
#include "../generated/pe_torch_source.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(PE_LOCOMOTION_COUNT<=PE_ACTOR_BLEND_MAX,"locomotion blend capacity");

enum { BODY, KNIFE_STOWED, SHOVEL_STOWED, SHOVEL_HELD, AXE_HELD, WOOD_AXE_HELD,
       PICKAXE_STOWED, PICKAXE_HELD, TORCH_STOWED, TORCH_HELD, KNIFE_HELD, DIG_BODY, DIG_TOOL, FELL_BODY, FELL_TOOL, WOOD_FELL_TOOL,
       MINE_BODY, MINE_TOOL, WOOD_SWORD_STOWED, WOOD_SWORD_HELD, WOOD_SHIELD,
       REACTION_BODY, REACTION_SWORD_STOWED, REACTION_SWORD_HELD, REACTION_SHIELD, VIEW_COUNT };
static const char *const folders[VIEW_COUNT]={"player","player-knife-stowed","player-shovel-stowed",
    "player-shovel","player-axe","player-wood-axe","player-pickaxe-stowed","player-pickaxe",
    "player-torch-stowed","player-torch","player-knife-drawn",
    "player-dig","player-shovel-dig","player-cut","player-axe-cut","player-wood-axe-cut","player-mine","player-pickaxe-mine","player-wood-sword-stowed","player-wood-sword-drawn","player-wood-shield",
    "player-reaction","player-reaction-wood-sword-stowed","player-reaction-wood-sword-drawn","player-reaction-wood-shield"};
static const char *const action_names[3][3]={{"Spade1","Spade2","Spade3"},{"Felling1","Felling2","Felling3"},{"Mine1","Mine2","Mine3"}};
/* Exact female source clocks. Shovel/Felling native captures09/08 and installed
 * female Mine clips221/228/237. Mine poses require their separate recovered
 * assets: equal durations do not make Spade and Mine interchangeable. PEAC2
 * terminal timestamps are required throughout. */
static const double action_duration[3][3]={{.6999969482421875,1.3333358764648438,.8333358764648438},
    {.6000003814697266,1.100006103515625,.6000003814697266},
    {.6999969482421875,1.3333358764648438,.8333358764648438}};
static const unsigned action_frames[3][3]={{22,42,27},{20,35,20},{22,42,27}};
static const char *const melee_names[]={"Attack6","Attack7","Attack8","Attack9"};
static const double melee_duration[]={1.0666667222976685,1.100000262260437,1.2666668891906738,.9666671752929688};
static const unsigned melee_frames[]={34,35,40,31};
static const double running_duration=1.8333396911621094; /* female clip408 */
/* Grounded TP only. Sideways leaves are the exact shared third-person turns;
 * distinct backward441/708 are never substituted by NormalWB/RB. Air, slope
 * and FP selectors are outside this presentation capability. */
static const char *const sword_ground_names[PE_LOCOMOTION_COUNT]={
    "AttackIdle","SwordWF","SwordRF",NULL,NULL,NULL,"NormalWalkTurnLeft","NormalWalkTurnRight","SwordWB",
    "NormalRunTurnLeft","NormalRunTurnRight","SwordRB"};
static const char *const sword_leaf_names[]={"AttackIdle","SwordWF","SwordRF","SwordSprint","SwordJump","SwordFall","SwordLand","SwordRunJump","SwordRunFall","SwordRunLand","SwordWB","SwordRB"};
static const double sword_leaf_duration[]={1.7333335876464844,1.0999984741210938,.6666679382324219,.6000001430511475,1.0333404541015625,.6666717529296875,1.066680908203125,1.4000015258789062,1.0000038146972656,.23333740234375,.8000030517578125,.6666717529296875};
static const unsigned sword_leaf_frames[]={54,34,22,20,33,22,34,44,32,9,26,22};
static const bool sword_leaf_loop[]={true,true,true,true,false,false,false,false,true,false,true,true};
/* Capture female-reaction-01 in bake order. Index0 is the NormalIdle anchor and
 * must equal the body's own clip; the rest are exact female source clocks.
 * Loop flags are the source clips', including the unlooped FowardRightFast. */
enum { REACTION_CLIPS=25 };
static const char *const reaction_names[REACTION_CLIPS]={"NormalIdle","IdleBeHit","SwordBeHit",
    "ForwardFast","FowardRightFast","RightFast","BackRightFast","BackFast","ForwardLeftFast","LeftFast","BackLeftFast",
    "Forward","ForwardRight","Right","BackRight","Back","ForwardLeft","Left","BackLeft","BlockRepulsed",
    "GetUpBackMirror","GetUpFrontMirror","GetUpBack","GetUpFront","SprintSwordAttack"};
static const double reaction_duration[REACTION_CLIPS]={0,.9333336353302002,.9333419799804688,
    1.0000009536743164,1.0000009536743164,1.0000019073486328,1,1.0000009536743164,1.0000009536743164,1.0000019073486328,1,
    1.9666669368743896,1.9666671752929688,.9666671752929688,1.9666681289672852,1.9666669368743896,1.9666671752929688,.9666671752929688,1.9666681289672852,1.0000152587890625,
    1.4666709899902344,1.0666694641113281,1.4666709899902344,1.0666694641113281,1.8000068664550781};
static const unsigned reaction_frames[REACTION_CLIPS]={0,30,30,32,32,32,31,32,32,32,31,61,61,31,61,61,61,31,61,32,46,34,46,34,56};
static const bool reaction_loop[REACTION_CLIPS]={true,false,false,true,false,true,true,true,true,true,true,
    true,true,true,true,true,true,true,true,true,false,false,false,false,false};
static int melee_view(int item){return item==33?KNIFE_HELD:item==1?WOOD_SWORD_HELD:-1;}
static const int action_bodies[3]={DIG_BODY,FELL_BODY,MINE_BODY};
static int action_kind(int item){return item==12?0:(item==26||item==1527)?1:item==19?2:-1;}
struct PeSandboxActor {
    PeActorSceneView views[VIEW_COUNT];
    PeIkDrive *ik; /* Optional always-on solve; NULL keeps the baked presentation. */
    size_t clips[VIEW_COUNT][PE_LOCOMOTION_COUNT],sword_clips[VIEW_COUNT][PE_LOCOMOTION_COUNT];
    uint32_t shield_instance;
    PePlayerLocomotion locomotion;
    unsigned locomotion_seen;
    PeVec3 previous;
    bool have_previous,was_jumping,ok,have_facing,free_facing,facing_locked;
    float body_yaw;
    double elapsed;
    size_t melee_body[4],melee_knife[4];
    bool running_ready,sprint_ready;
    unsigned long long ik_draws;
    bool ik_offsets;PeVec3 ik_offset[PE_FBBIK_EFFECTORS]; /* world space, Halt stop effect only */
    bool grounding;PeGrounderPose ground_pose;PeVec3 ground_offset[PE_FBBIK_EFFECTORS]; /* model space, grounder */
    /* Native 150ms natural-end pose recovery; never an action/contact clock. */
    struct {bool was_active,active;int item;uint32_t instance;size_t body_clip,tool_clip;double seconds,age;} recovery;
    struct {bool active;size_t body_clip,knife_clip;double seconds;} melee;
    struct { int item_id,phase;uint32_t instance_id;bool held;double seconds; } action;
};
static void error_set(char *error,size_t cap,const char *message){if(error&&cap)snprintf(error,cap,"%s",message);}
static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static bool clocks_match(const PeActorClip *a,const PeActorClip *b){return a&&b&&!strcmp(a->name,b->name)&&a->fps==b->fps&&a->duration==b->duration&&a->frames==b->frames&&a->loop==b->loop&&a->terminal_at_duration==b->terminal_at_duration;}
void pe_sandbox_actor_unload(PeSandboxActor *a){if(a){for(int i=0;i<VIEW_COUNT;i++)pe_actor_scene_view_unload(&a->views[i]);pe_ik_drive_free(a->ik);free(a);}}
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
    bool torch_group=present[TORCH_STOWED]||present[TORCH_HELD];
    bool wood_pair=present[WOOD_SWORD_STOWED]||present[WOOD_SWORD_HELD]||present[WOOD_SHIELD];
    bool reaction_group=false,reaction_complete=true;
    for(int v=REACTION_BODY;v<VIEW_COUNT;v++){reaction_group=reaction_group||present[v];reaction_complete=reaction_complete&&present[v];}
    if(reaction_group&&!reaction_complete){error_set(error,cap,"Reaction assets must form the complete body/sword/shield group");goto fail;}
    if((present[KNIFE_HELD]&&!present[KNIFE_STOWED])||
       (torch_group&&!(present[TORCH_STOWED]&&present[TORCH_HELD]))||
       (wood_pair&&!(present[WOOD_SWORD_STOWED]&&present[WOOD_SWORD_HELD]&&present[WOOD_SHIELD]))||
       (shovel_group&&!(present[SHOVEL_STOWED]&&present[SHOVEL_HELD]&&present[DIG_BODY]&&present[DIG_TOOL]))||
       (axe_group&&!(present[AXE_HELD]&&present[FELL_BODY]&&present[FELL_TOOL]))||
       (wood_group&&!(present[WOOD_AXE_HELD]&&present[FELL_BODY]&&present[WOOD_FELL_TOOL]))||
       (pickaxe_group&&!(present[PICKAXE_STOWED]&&present[PICKAXE_HELD]&&present[MINE_BODY]&&present[MINE_TOOL]))) {
        error_set(error,cap,"Sandbox tool action assets must form a complete body/held/tool group");goto fail;
    }
    /* Decode every group and compare source clocks before uploading anything.
       Actor-scene retains the exact validated bytes through GPU upload. */
    for(int v=0;v<VIEW_COUNT;v++)if(present[v]) {
        if(!pe_actor_scene_view_prepare(&a->views[v],paths[v],error,cap))goto fail;
        if(v>=REACTION_BODY)continue; /* Validated below as one ordered group. */
        if(v<DIG_BODY||v>=WOOD_SWORD_STOWED)for(int i=0;i<PE_LOCOMOTION_COUNT;i++) {
            a->clips[v][i]=pe_actor_scene_view_find_clip(&a->views[v],pe_player_locomotion_name(i));
            const PeActorClip *clip=pe_actor_scene_view_clip_at(&a->views[v],a->clips[v][i]);
            if(!clip){if(error&&cap)snprintf(error,cap,"Sandbox %s is missing %s; install the matched directional player assets",folders[v],pe_player_locomotion_name(i));goto fail;}
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
    if(present[KNIFE_HELD])for(unsigned i=0;i<4;i++){
        a->melee_body[i]=pe_actor_scene_view_find_clip(&a->views[BODY],melee_names[i]);
        a->melee_knife[i]=pe_actor_scene_view_find_clip(&a->views[KNIFE_HELD],melee_names[i]);
        const PeActorClip *body=pe_actor_scene_view_clip_at(&a->views[BODY],a->melee_body[i]);
        const PeActorClip *knife=pe_actor_scene_view_clip_at(&a->views[KNIFE_HELD],a->melee_knife[i]);
        if(!clocks_match(body,knife)||body->loop||!body->terminal_at_duration||body->fps!=30||
           body->frames!=melee_frames[i]||fabs(body->duration-melee_duration[i])>1e-6){
            error_set(error,cap,"Knife melee scene differs from recovered female Attack6..9 clocks");goto fail;
        }
    }
    /* Running is an optional coherent extension. A partially installed body,
     * held or stowed knife group must fail before any GPU resources upload. */
    const int running_views[]={BODY,KNIFE_HELD,KNIFE_STOWED};
    const PeActorClip *running[3]={0};unsigned running_count=0;
    PeCombat running_pose={.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,.variant=PE_COMBAT_RUNNING};
    const char *running_name=pe_combat_pose_name(&running_pose);
    for(unsigned i=0;i<3;i++)if(present[running_views[i]]){
        PeActorSceneView *view=&a->views[running_views[i]];
        running[i]=pe_actor_scene_view_clip_at(view,pe_actor_scene_view_find_clip(view,running_name));
        if(running[i])running_count++;
    }
    if(running_count&&(present[KNIFE_HELD]||present[KNIFE_STOWED]||!wood_pair)){
        if(running_count!=3||!clocks_match(running[0],running[1])||!clocks_match(running[0],running[2])||
           running[0]->loop||!running[0]->terminal_at_duration||running[0]->fps!=30||running[0]->frames!=57||
           fabs(running[0]->duration-running_duration)>1e-6){
            error_set(error,cap,"RunSwordAttack requires matched female408 body/held/stowed knife scenes");goto fail;
        }
        a->running_ready=true;
    }
    const int sprint_views[]={BODY,KNIFE_HELD,KNIFE_STOWED};
    const PeActorClip *sprint[3]={0};unsigned sprint_count=0;
    for(unsigned i=0;i<3;i++)if(present[sprint_views[i]]){
        PeActorSceneView *v=&a->views[sprint_views[i]];
        sprint[i]=pe_actor_scene_view_clip_at(v,pe_actor_scene_view_find_clip(v,"SprintSwordAttack"));
        sprint_count+=sprint[i]!=NULL;
    }
    if(sprint_count){
        if(sprint_count!=3||!clocks_match(sprint[0],sprint[1])||!clocks_match(sprint[0],sprint[2])||
           sprint[0]->loop||!sprint[0]->terminal_at_duration||sprint[0]->fps!=30||sprint[0]->frames!=56||
           sprint[0]->duration!=1.8000068664550781){
            error_set(error,cap,"SprintSwordAttack requires exact female401 body/held/stowed Knife clocks");goto fail;
        }
        a->sprint_ready=true;
    }
    if(wood_pair){
        const PeActorClip *run=pe_actor_scene_view_clip_at(&a->views[BODY],pe_actor_scene_view_find_clip(&a->views[BODY],"RunSwordAttack"));
        if(!run||run->loop||!run->terminal_at_duration||run->fps!=30||run->frames!=57||fabs(run->duration-running_duration)>1e-6){error_set(error,cap,"Wood sword running408 clock differs");goto fail;}
        a->running_ready=true;
        const int wood_views[]={BODY,WOOD_SWORD_STOWED,WOOD_SWORD_HELD,WOOD_SHIELD};
        for(unsigned v=0;v<4;v++){
            PeActorSceneView *view=&a->views[wood_views[v]];
            if(pe_actor_scene_view_clip_count(view)!=33){error_set(error,cap,"Wood sword/shield require the coherent33-clip body/attachment group");goto fail;}
            for(size_t i=0;i<33;i++)if(!clocks_match(pe_actor_scene_view_clip_at(&a->views[BODY],i),pe_actor_scene_view_clip_at(view,i))){
                error_set(error,cap,"Wood sword/shield ordered body clocks differ");goto fail;}
            for(unsigned i=0;i<12;i++){
                const PeActorClip *clip=pe_actor_scene_view_clip_at(view,pe_actor_scene_view_find_clip(view,sword_leaf_names[i]));
                if(!clip||fabs(clip->duration-sword_leaf_duration[i])>1e-6||clip->fps!=30||clip->frames!=sword_leaf_frames[i]||clip->loop!=sword_leaf_loop[i]||!clip->terminal_at_duration){
                    error_set(error,cap,"Wood sword/shield differs from exact recovered Sword leaf clock");goto fail;}
            }
            for(int i=0;i<PE_LOCOMOTION_COUNT;i++){
                a->sword_clips[wood_views[v]][i]=sword_ground_names[i]?pe_actor_scene_view_find_clip(view,sword_ground_names[i]):PE_ACTOR_BIND_POSE;
                if(sword_ground_names[i]&&a->sword_clips[wood_views[v]][i]==PE_ACTOR_BIND_POSE){error_set(error,cap,"Wood sword TP turn/backward leaf missing");goto fail;}
            }
            const char *turn_names[]={"NormalWalkTurnLeft","NormalWalkTurnRight","NormalRunTurnLeft","NormalRunTurnRight"};
            const double turn_duration[]={1.1000022888183594,1.100006103515625,.6666717529296875,.6666717529296875};
            for(unsigned i=0;i<4;i++){
                const PeActorClip *clip=pe_actor_scene_view_clip_at(view,pe_actor_scene_view_find_clip(view,turn_names[i]));
                if(!clip||!clip->loop||!clip->terminal_at_duration||clip->fps!=30||clip->frames!=(i<2?35u:22u)||fabs(clip->duration-turn_duration[i])>1e-6){error_set(error,cap,"Wood sword shared TP turn clock differs");goto fail;}
            }
            for(unsigned i=0;i<4;i++){
                const PeActorClip *clip=pe_actor_scene_view_clip_at(view,pe_actor_scene_view_find_clip(view,melee_names[i]));
                if(!clip||clip->loop||!clip->terminal_at_duration||clip->fps!=30||clip->frames!=melee_frames[i]||fabs(clip->duration-melee_duration[i])>1e-6){error_set(error,cap,"Wood sword melee clock differs");goto fail;}
            }
        }
    }
    if(reaction_group){
        const PeActorClip *idle=pe_actor_scene_view_clip_at(&a->views[BODY],a->clips[BODY][PE_LOCOMOTION_IDLE]);
        for(int v=REACTION_BODY;v<VIEW_COUNT;v++){
            PeActorSceneView *view=&a->views[v];
            if(pe_actor_scene_view_clip_count(view)!=REACTION_CLIPS){error_set(error,cap,"Reaction group requires exactly the25 captured clips");goto fail;}
            for(size_t i=0;i<REACTION_CLIPS;i++){
                const PeActorClip *clip=pe_actor_scene_view_clip_at(view,i);
                if(!clip||strcmp(clip->name,reaction_names[i])){error_set(error,cap,"Reaction group clip names/order differ from the capture");goto fail;}
                if(i?clip->fps!=30||clip->frames!=reaction_frames[i]||fabs(clip->duration-reaction_duration[i])>1e-6||clip->loop!=reaction_loop[i]||!clip->terminal_at_duration
                    :!clocks_match(idle,clip)){
                    error_set(error,cap,i?"Reaction group differs from exact recovered female source clock":"Reaction group NormalIdle anchor differs from the body");goto fail;}
                if(v!=REACTION_BODY&&!clocks_match(pe_actor_scene_view_clip_at(&a->views[REACTION_BODY],i),clip)){
                    error_set(error,cap,"Reaction body and attachment clocks differ");goto fail;}
            }
        }
    }
    if(!pe_player_locomotion_init(&a->locomotion,durations)){error_set(error,cap,"Invalid sandbox locomotion durations");goto fail;}
    /* Staged solver poses and joint drivers are optional as a pair; a present group must load. */
    if(pe_ik_drive_present(root)&&!(a->ik=pe_ik_drive_load(root,error,cap)))goto fail;
    for(int v=0;v<VIEW_COUNT;v++)if(a->views[v].manifest&&!pe_actor_scene_view_upload(&a->views[v],error,cap))goto fail;
    if(a->ik)for(int v=0;v<VIEW_COUNT;v++)if(a->views[v].loaded)pe_actor_scene_view_bind_ik(&a->views[v],a->ik,folders[v]);
    a->action.phase=-1;a->ok=true;return a;
fail:pe_sandbox_actor_unload(a);return NULL;
}
bool pe_sandbox_actor_ok(const PeSandboxActor *a){return a&&a->ok;}
unsigned pe_sandbox_actor_locomotion_seen(const PeSandboxActor *a){return a?a->locomotion_seen:0;}
unsigned long long pe_sandbox_actor_ik_draws(const PeSandboxActor *a){return a?a->ik_draws:0;}
bool pe_sandbox_actor_action_ready(const PeSandboxActor *a,int item) {
    if(!a||!a->ok||(a->shield_instance&&item!=1))return false;
    if(item==1)return a->views[WOOD_SWORD_HELD].loaded&&a->views[WOOD_SWORD_STOWED].loaded&&a->views[WOOD_SHIELD].loaded&&a->views[BODY].loaded;
    if(item==33)return a->views[KNIFE_HELD].loaded&&a->views[KNIFE_STOWED].loaded&&a->views[BODY].loaded;
    if(item==12)return a->views[SHOVEL_HELD].loaded&&a->views[SHOVEL_STOWED].loaded&&a->views[DIG_BODY].loaded&&a->views[DIG_TOOL].loaded;
    if(item==26)return a->views[WOOD_AXE_HELD].loaded&&a->views[FELL_BODY].loaded&&a->views[WOOD_FELL_TOOL].loaded;
    if(item==1527)return a->views[AXE_HELD].loaded&&a->views[FELL_BODY].loaded&&a->views[FELL_TOOL].loaded;
    if(item==19)return a->views[PICKAXE_STOWED].loaded&&a->views[PICKAXE_HELD].loaded&&a->views[MINE_BODY].loaded&&a->views[MINE_TOOL].loaded;
    return false;
}
bool pe_sandbox_actor_hold_ready(const PeSandboxActor *a,int item) {
    if(item==10)return a&&a->ok&&a->views[TORCH_STOWED].loaded&&a->views[TORCH_HELD].loaded;
    return pe_sandbox_actor_action_ready(a,item);
}
bool pe_sandbox_actor_running_ready_for_item(const PeSandboxActor *a,int item){return (item==1||item==33)&&pe_sandbox_actor_action_ready(a,item)&&a->running_ready;}
bool pe_sandbox_actor_running_ready(const PeSandboxActor *a){return pe_sandbox_actor_running_ready_for_item(a,33);}
bool pe_sandbox_actor_shield_ready_for_item(const PeSandboxActor *a,int item){return a&&a->ok&&a->views[WOOD_SHIELD].loaded&&(item==0||item==1);}
bool pe_sandbox_actor_sprint_ready(const PeSandboxActor *a){return a&&a->ok&&a->sprint_ready;}
bool pe_sandbox_actor_reaction_ready(const PeSandboxActor *a){
    if(!a||!a->ok)return false;
    for(int v=REACTION_BODY;v<VIEW_COUNT;v++)if(!a->views[v].loaded)return false;
    return true;
}
bool pe_sandbox_actor_reaction_defence_ready(const PeSandboxActor *a,const PeActor *defence){
    if(!pe_sandbox_actor_reaction_ready(a)||!defence||pe_actor_stats(defence).clips!=REACTION_CLIPS)return false;
    for(size_t i=0;i<REACTION_CLIPS;i++)if(!clocks_match(pe_actor_scene_view_clip_at(&a->views[REACTION_BODY],i),pe_actor_clip(defence,i)))return false;
    return true;
}
bool pe_sandbox_actor_shield_ready(const PeSandboxActor *a){return pe_sandbox_actor_shield_ready_for_item(a,0);}
bool pe_sandbox_actor_set_shield(PeSandboxActor *a,uint32_t instance){
    if(!a||!a->ok||(instance&&(!pe_sandbox_actor_shield_ready(a)||(a->action.held&&!pe_sandbox_actor_shield_ready_for_item(a,a->action.item_id)))))return false;
    a->shield_instance=instance;return true;
}
bool pe_sandbox_actor_sword_ground_ready(const PeSandboxActor *a,const PeGame *g){
    if(!pe_sandbox_actor_action_ready(a,1)||!g||!g->grounded||g->jumping||g->sprinting)return false;
    PePlayerLocomotionPose p;if(!pe_player_locomotion_snapshot(&a->locomotion,&p))return false;
    for(int i=PE_LOCOMOTION_JUMP;i<=PE_LOCOMOTION_LAND;i++)if(p.weights[i]>0)return false;
    return true;
}
bool pe_sandbox_actor_set_action(PeSandboxActor *a,const PeSandboxActionPose *p) {
    if(!a||!a->ok)return false;
    if(!p){memset(&a->recovery,0,sizeof a->recovery);memset(&a->action,0,sizeof a->action);a->action.phase=-1;memset(&a->melee,0,sizeof a->melee);return true;}
    if(!isfinite(p->seconds)||p->seconds<0||p->seconds>1000000||p->item_id<0||
       (p->held&&(!p->instance_id||!pe_sandbox_actor_hold_ready(a,p->item_id))))return false;
    if(a->shield_instance&&p->held&&!pe_sandbox_actor_shield_ready_for_item(a,p->item_id))return false;
    int phase=-1;
    if(p->clip) {
        if(!p->held||!pe_sandbox_actor_action_ready(a,p->item_id))return false;
        int kind=action_kind(p->item_id);
        if(kind<0)return false;
        for(int i=0;i<3;i++)if(!strcmp(p->clip,action_names[kind][i]))phase=i;
        if(phase<0||(phase!=1&&p->seconds>action_duration[kind][phase]+1e-6))return false;
    }
    if(!p->item_id&&(p->instance_id||p->held||p->clip))return false;
    if(p->clip||(p->item_id&&(!p->held||p->instance_id!=a->recovery.instance||p->item_id!=a->recovery.item)))
        memset(&a->recovery,0,sizeof a->recovery);
    a->action.item_id=p->item_id;a->action.instance_id=p->instance_id;a->action.held=p->held;
    a->action.phase=phase;a->action.seconds=p->seconds;memset(&a->melee,0,sizeof a->melee);return true;
}

bool pe_sandbox_actor_melee_assets_for_item(const PeSandboxActor *a,int item,const PeActor **body,const PeActor **knife){
    int view=melee_view(item);
    if(!body||!knife||view<0||!pe_sandbox_actor_action_ready(a,item)||!a->views[BODY].count||!a->views[view].count||
       !a->views[BODY].parts||!a->views[view].parts||!a->views[BODY].parts[0].actor||!a->views[view].parts[0].actor)return false;
    *body=a->views[BODY].parts[0].actor;*knife=a->views[view].parts[0].actor;return true;
}
bool pe_sandbox_actor_set_melee_item(PeSandboxActor *a,int item,uint32_t instance,bool held,const PeCombat *combat){
    int view=melee_view(item);
    if(!a||!a->ok||view<0||(held&&(!instance||!pe_sandbox_actor_hold_ready(a,item))))return false;
    bool active=combat&&combat->active;
    if(combat&&(!isfinite(combat->clip_time)||combat->clip_time<0||combat->stage>=4||
       combat->gender!=PE_COMBAT_FEMALE||combat->view!=PE_COMBAT_THIRD_PERSON||!pe_combat_pose_name(combat)))return false;
    size_t body_clip=0,knife_clip=0;
    if(active){
        if(!held||(combat->variant==PE_COMBAT_RUNNING&&!pe_sandbox_actor_running_ready_for_item(a,item))||
           (combat->variant==PE_COMBAT_SPRINT&&(item!=33||!pe_sandbox_actor_sprint_ready(a))))return false;
        const char *name=pe_combat_pose_name(combat);
        body_clip=pe_actor_scene_view_find_clip(&a->views[BODY],name);
        knife_clip=pe_actor_scene_view_find_clip(&a->views[view],name);
        const PeActorClip *body=pe_actor_scene_view_clip_at(&a->views[BODY],body_clip);
        const PeActorClip *knife=pe_actor_scene_view_clip_at(&a->views[view],knife_clip);
        if(!clocks_match(body,knife)||combat->clip_time>body->duration+1e-6)return false;
    }
    PeSandboxActionPose pose={.item_id=instance?item:0,.instance_id=instance,.held=held};
    bool natural=!active&&combat&&combat->event_index==5&&a->recovery.was_active&&held&&
        a->recovery.item==item&&a->recovery.instance==instance;
    if(!pe_sandbox_actor_set_action(a,&pose))return false;
    if(active){
        a->recovery.was_active=true;a->recovery.active=false;a->recovery.item=item;a->recovery.instance=instance;
        a->recovery.body_clip=body_clip;a->recovery.tool_clip=knife_clip;a->recovery.seconds=combat->clip_time;
    }else{
        a->recovery.was_active=false;
        if(natural){a->recovery.active=true;a->recovery.age=0;a->recovery.seconds=combat->clip_time;}
        else if(!held||!combat||a->recovery.item!=item||a->recovery.instance!=instance)memset(&a->recovery,0,sizeof a->recovery);
    }
    a->melee.active=active;
    if(active){a->melee.body_clip=body_clip;a->melee.knife_clip=knife_clip;a->melee.seconds=combat->clip_time;}
    return true;
}

bool pe_sandbox_actor_melee_assets(const PeSandboxActor *a,const PeActor **body,const PeActor **knife){return pe_sandbox_actor_melee_assets_for_item(a,33,body,knife);}
bool pe_sandbox_actor_set_melee(PeSandboxActor *a,uint32_t instance,bool held,const PeCombat *combat){return pe_sandbox_actor_set_melee_item(a,33,instance,held,combat);}

float pe_sandbox_actor_yaw(const PeSandboxActor *a,const PeGame *g){
    return a&&a->have_facing?a->body_yaw*RAD2DEG:g?180-g->yaw*RAD2DEG:0;
}
bool pe_sandbox_actor_face_yaw(PeSandboxActor *a,float yaw){
    if(!a||!a->ok||!isfinite(yaw))return false;
    a->body_yaw=remainderf(yaw,360)*DEG2RAD;a->have_facing=true;return true;
}
void pe_sandbox_actor_facing_mode(PeSandboxActor *a,bool free_pointer){if(a)a->free_facing=free_pointer;}
void pe_sandbox_actor_facing_lock(PeSandboxActor *a,bool locked){if(a)a->facing_locked=locked;}
void pe_sandbox_actor_face_camera(PeSandboxActor *a,const PeGame *g){
    if(a&&g&&isfinite(g->yaw)){a->body_yaw=remainderf(PI-g->yaw,2*PI);a->have_facing=true;}
}
void pe_sandbox_actor_reset_motion(PeSandboxActor *a,const PeGame *g){
    if(!a||!g||!finite3(g->player)||!isfinite(g->yaw))return;
    double durations[PE_LOCOMOTION_COUNT];memcpy(durations,a->locomotion.duration,sizeof durations);
    pe_player_locomotion_init(&a->locomotion,durations);a->previous=g->player;a->have_previous=true;
    a->was_jumping=g->jumping;a->facing_locked=false;a->shield_instance=0;pe_sandbox_actor_set_action(a,NULL);pe_sandbox_actor_face_camera(a,g);
    a->ik_offsets=a->grounding=false;
}
bool pe_sandbox_actor_set_grounding(PeSandboxActor *a,const PeGrounderHost *host){
    if(!a)return false;
    if(!host||!host->active){a->grounding=false;return true;}
    if(!finite3(host->pose.pelvis_offset))return false;
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)if(!finite3(host->effector_offset[e]))return false;
    a->ground_pose=host->pose;memcpy(a->ground_offset,host->effector_offset,sizeof a->ground_offset);a->grounding=true;return true;
}
bool pe_sandbox_actor_set_ik_offsets(PeSandboxActor *a,const PeHaltOffset *offsets,unsigned count){
    if(!a||count>PE_HALT_LINKS||(count&&!offsets))return false;
    PeVec3 next[PE_FBBIK_EFFECTORS]={{0}};
    for(unsigned i=0;i<count;i++){
        if(offsets[i].effector>=PE_FBBIK_EFFECTORS||!finite3(offsets[i].offset))return false;
        next[offsets[i].effector]=offsets[i].offset;
    }
    memcpy(a->ik_offset,next,sizeof next);a->ik_offsets=count>0;return true;
}
static float turn_toward(float current,float target,float dt){
    float delta=remainderf(target-current,2*PI);
    /* Serialized player MoveParam rate6. The complete desired-direction filter
     * is unported; preserve its >150-degree body-right turn convention. */
    if(fabsf(delta)>150*DEG2RAD)delta=PI*.5f;
    return remainderf(current+delta*fminf(1,6*dt),2*PI);
}
void pe_sandbox_actor_update(PeSandboxActor *a,float dt,const PeGame *g){
    if(!a||!a->ok||!g||!isfinite(dt)||dt<0||dt>1||!finite3(g->player)||!finite3(g->velocity)||!isfinite(g->yaw))return;
    if(dt==0)return;
    if(a->recovery.active){
        a->recovery.age+=dt;a->recovery.seconds+=dt;
        const PeSlot *slot=pe_game_equipped_tool(g);
        if(a->recovery.age>=.15||g->hp<=0||!g->grounded||!slot||
           slot->item_id!=a->recovery.item||slot->instance_id!=a->recovery.instance)memset(&a->recovery,0,sizeof a->recovery);
    }
    PeVec3 measured={0,g->velocity.y,0};bool reset=false;
    if(!a->have_facing)pe_sandbox_actor_face_camera(a,g);
    if(a->have_previous){
        float dx=g->player.x-a->previous.x,dz=g->player.z-a->previous.z;
        double displacement=hypot((double)dx,dz);
        /* Max source sprint is8m/s; this allows frame jitter but not a load or
           imported-world relocation to become an enormous gait speed. */
        if(displacement>12.0*dt+.5||fabsf(g->player.y-a->previous.y)>20*dt+2)reset=true;
        else{measured.x=dx/dt;measured.z=dz/dt;}
    }
    if(reset){double durations[PE_LOCOMOTION_COUNT];memcpy(durations,a->locomotion.duration,sizeof durations);pe_player_locomotion_init(&a->locomotion,durations);measured=(PeVec3){0};}
    if(reset){memset(&a->recovery,0,sizeof a->recovery);pe_sandbox_actor_face_camera(a,g);}
    if(!a->facing_locked&&!a->melee.active&&a->action.phase<0){
        if(!a->free_facing)pe_sandbox_actor_face_camera(a,g);
        else if(hypotf(measured.x,measured.z)>.05f)
            a->body_yaw=turn_toward(a->body_yaw,atan2f(measured.x,measured.z),dt);
    }
    PePlayerLocomotionInput input={.velocity=measured,.grounded=g->grounded,.walk=hypotf(measured.x,measured.z)<=PE_WALK_SPEED+.25f,
        .sprint=g->sprinting,.jump_started=g->jumping&&!a->was_jumping&&!reset,
        .directional=true,
        .local_right=measured.x*cosf(a->body_yaw)-measured.z*sinf(a->body_yaw),
        .local_forward=measured.x*sinf(a->body_yaw)+measured.z*cosf(a->body_yaw)};
    if(pe_player_locomotion_step(&a->locomotion,input,dt)){
        a->locomotion_seen|=1u<<a->locomotion.clip;
        a->elapsed=fmod(a->elapsed+dt,3600);a->previous=g->player;a->have_previous=true;a->was_jumping=g->jumping;
    }
}
typedef struct {int body,tool;size_t body_clip,tool_clip;double seconds;bool locomotion,sword,held;} SelectedPose;
static bool select_pose(const PeSandboxActor *a,const PeGame *g,PePlayerLocomotionClip clip,double seconds,bool preview,SelectedPose *out) {
    if(!a||!a->ok||!g||clip<0||clip>=PE_LOCOMOTION_COUNT||!isfinite(seconds)||seconds<0)return false;
    const PeSlot *equipped=pe_game_equipped_tool(g);
    bool bound=equipped&&equipped->instance_id==a->action.instance_id&&equipped->item_id==a->action.item_id;
    bool held=bound&&a->action.held;
    int body=BODY,tool=-1,phase=a->action.phase;bool locomotion=!preview;
    if(equipped&&equipped->item_id==33){
        if(!isfinite(g->hp)||g->hp<=0)held=false;
        tool=held?KNIFE_HELD:KNIFE_STOWED;
    }
    else if(equipped&&equipped->item_id==1){
        if(!isfinite(g->hp)||g->hp<=0)held=false;
        tool=held?WOOD_SWORD_HELD:WOOD_SWORD_STOWED;
    }
    else if(equipped&&equipped->item_id==12)tool=held?SHOVEL_HELD:SHOVEL_STOWED;
    else if(equipped&&equipped->item_id==1527&&held)tool=AXE_HELD;
    else if(equipped&&equipped->item_id==26&&held)tool=WOOD_AXE_HELD;
    else if(equipped&&equipped->item_id==19)tool=held?PICKAXE_HELD:PICKAXE_STOWED;
    else if(equipped&&equipped->item_id==10)tool=held?TORCH_HELD:TORCH_STOWED;
    bool sword=held&&equipped&&equipped->item_id==1;
    if(sword&&!preview&&!a->melee.active&&!pe_sandbox_actor_sword_ground_ready(a,g))return false;
    size_t body_clip=sword?a->sword_clips[BODY][clip]:a->clips[BODY][clip];
    size_t tool_clip=tool>=0?(sword?a->sword_clips[tool][clip]:a->clips[tool][clip]):0;
    if(sword&&(preview||!a->melee.active)){const PeActorClip *c=pe_actor_scene_view_clip_at(&a->views[BODY],body_clip);if(!c)return false;seconds=seconds/a->locomotion.duration[clip]*c->duration;}
    int kind=equipped?action_kind(equipped->item_id):-1;
    if(!preview&&held&&phase>=0&&kind>=0&&pe_sandbox_actor_action_ready(a,equipped->item_id)) {
        locomotion=false;body=action_bodies[kind];tool=equipped->item_id==26?WOOD_FELL_TOOL:body+1;
        body_clip=a->clips[body][phase];tool_clip=a->clips[tool][phase];seconds=a->action.seconds;
    }
    if(!preview&&held&&(equipped->item_id==33||equipped->item_id==1)&&a->melee.active){
        locomotion=false;body_clip=a->melee.body_clip;tool_clip=a->melee.knife_clip;seconds=a->melee.seconds;
    }
    *out=(SelectedPose){body,tool,body_clip,tool_clip,seconds,locomotion,sword,held};return true;
}
static bool recovery_bound(const PeSandboxActor *a,const PeGame *g){
    const PeSlot *slot=pe_game_equipped_tool(g);
    return a->recovery.active&&slot&&slot->item_id==a->recovery.item&&slot->instance_id==a->recovery.instance&&
        a->action.held&&a->action.instance_id==slot->instance_id&&g->hp>0&&g->grounded;
}
static size_t locomotion_samples(const PeSandboxActor *a,int view,bool sword,bool recover,PeActorBlendSample samples[PE_ACTOR_BLEND_MAX]){
    PePlayerLocomotionPose pose;if(!pe_player_locomotion_snapshot(&a->locomotion,&pose))return 0;
    size_t count=0;
    for(int i=0;i<PE_LOCOMOTION_COUNT;i++)if(pose.weights[i]>0){
        size_t clip=sword?a->sword_clips[view][i]:a->clips[view][i];
        const PeActorClip *definition=pe_actor_scene_view_clip_at(&a->views[view],clip);if(!definition)return 0;
        double seconds=sword?pose.seconds[i]/a->locomotion.duration[i]*definition->duration:pose.seconds[i];
        samples[count++]=(PeActorBlendSample){clip,seconds,(float)pose.weights[i]};
    }
    if(recover&&(view==BODY||view==melee_view(a->recovery.item)||view==WOOD_SHIELD)){
        const PeActorClip *source=pe_actor_scene_view_clip_at(&a->views[BODY],a->recovery.body_clip);
        size_t clip=source?pe_actor_scene_view_find_clip(&a->views[view],source->name):PE_ACTOR_BIND_POSE;
        if(clip==PE_ACTOR_BIND_POSE||count>=PE_ACTOR_BLEND_MAX)return 0;
        float attack=(float)fmax(0,1-a->recovery.age/.15);
        for(size_t i=0;i<count;i++)samples[i].weight*=1-attack;
        samples[count++]=(PeActorBlendSample){clip,a->recovery.seconds,attack};
    }
    return count;
}
bool pe_sandbox_actor_body_pose(const PeSandboxActor *a,const PeGame *g,PeSandboxBodyPose *out){
    if(!out||!g||!finite3(g->player)||!isfinite(g->yaw))return false;
    SelectedPose p;
    if(!select_pose(a,g,a?a->locomotion.clip:PE_LOCOMOTION_IDLE,a?a->locomotion.seconds:0,false,&p))return false;
    const PeActorClip *clip=pe_actor_scene_view_clip_at(&a->views[p.body],p.body_clip);
    if(!clip||!clip->name[0]||!isfinite(clip->duration)||clip->duration<=0||!isfinite(p.seconds)||p.seconds<0)return false;
    PeSandboxBodyPose result={.clip=clip->name,.seconds=p.seconds,.duration=clip->duration,.loop=clip->loop};
    result.yaw=pe_sandbox_actor_yaw(a,g);
    if(p.locomotion){
        PeActorBlendSample samples[PE_ACTOR_BLEND_MAX];result.blend_count=locomotion_samples(a,p.body,p.sword,recovery_bound(a,g),samples);
        if(!result.blend_count)return false;
        for(size_t i=0;i<result.blend_count;i++){
            const PeActorClip *definition=pe_actor_scene_view_clip_at(&a->views[p.body],samples[i].clip);
            if(!definition)return false;
            result.blend[i].clip=definition->name;result.blend[i].seconds=samples[i].seconds;
            result.blend[i].weight=samples[i].weight;result.blend[i].duration=definition->duration;result.blend[i].loop=definition->loop;
        }
    }
    *out=result;return true;
}
static bool shield_bound(const PeSandboxActor *a,const PeGame *g){
    const PeSlot *primary=pe_game_equipped_tool(g);
    if(!a->shield_instance||g->shield_instance_id!=a->shield_instance||!pe_sandbox_actor_shield_ready_for_item(a,primary?primary->item_id:0))return false;
    const PeSlot *slot=pe_shield_equipped(g);
    return slot&&slot->instance_id==a->shield_instance;

}
/* The original's FullBodyBipedIK runs every frame near the camera with no
 * effector weights, stops while ragdolled, and loses its arm mapping while
 * hand equipment is held. Every scene drawn for the pose must be bound, and
 * every blended clip staged, or the whole pose stays undriven. */
static size_t ik_samples(const PeSandboxActor *a,const PeGame *g,const SelectedPose *p,PeIkDriveSample samples[PE_ACTOR_BLEND_MAX]){
    size_t count=1;
    if(p->locomotion){
        PeActorBlendSample blend[PE_ACTOR_BLEND_MAX];count=locomotion_samples(a,p->body,p->sword,recovery_bound(a,g),blend);
        for(size_t i=0;i<count;i++){
            const PeActorClip *c=pe_actor_scene_view_clip_at(&a->views[p->body],blend[i].clip);if(!c)return 0;
            samples[i]=(PeIkDriveSample){c->name,blend[i].seconds,blend[i].weight};
        }
    }else{
        const PeActorClip *c=pe_actor_scene_view_clip_at(&a->views[p->body],p->body_clip);if(!c)return 0;
        samples[0]=(PeIkDriveSample){c->name,p->seconds,1};
    }
    return count;
}
static bool ik_frame(const PeSandboxActor *a,const PeGame *g,const SelectedPose *p,PeIkDriveFrame *out){
    if(!a->ik||!isfinite(g->hp)||g->hp<=0||!a->views[p->body].ik_bound||
       (p->tool>=0&&a->views[p->tool].loaded&&!a->views[p->tool].ik_bound)||(shield_bound(a,g)&&!a->views[WOOD_SHIELD].ik_bound))return false;
    PeIkDriveSample samples[PE_ACTOR_BLEND_MAX];size_t count=ik_samples(a,g,p,samples);
    if(!count)return false;
    if(!a->ik_offsets&&!a->grounding)return pe_ik_drive_solve(a->ik,samples,count,!p->held,out);
    /* Halt offsets are world vectors; the solver works in the drawn model's frame.
     * The grounder's requests add to them, as both add to positionOffset in the original. */
    PeVec3 offsets[PE_FBBIK_EFFECTORS]={{0}};float yaw=pe_sandbox_actor_yaw(a,g);
    if(a->ik_offsets)for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)offsets[e]=pe_ik_drive_model_vector(a->ik_offset[e],yaw);
    if(a->grounding)for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)
        offsets[e]=(PeVec3){offsets[e].x+a->ground_offset[e].x,offsets[e].y+a->ground_offset[e].y,offsets[e].z+a->ground_offset[e].z};
    return pe_ik_drive_solve_grounded(a->ik,samples,count,!p->held,offsets,a->grounding?&a->ground_pose:NULL,out);
}
bool pe_sandbox_actor_feet(const PeSandboxActor *a,const PeGame *g,PeVec3 *left,PeVec3 *right){
    if(!a||!a->ik||!g||!left||!right||!finite3(g->player)||!isfinite(g->yaw))return false;
    SelectedPose p;PeIkDriveSample samples[PE_ACTOR_BLEND_MAX];PeVec3 l,r;
    if(!select_pose(a,g,a->locomotion.clip,a->locomotion.seconds,false,&p))return false;
    size_t count=ik_samples(a,g,&p,samples);
    if(!count||!pe_ik_drive_feet(a->ik,samples,count,&l,&r))return false;
    float yaw=pe_sandbox_actor_yaw(a,g);l=pe_ik_drive_world_vector(l,yaw);r=pe_ik_drive_world_vector(r,yaw);
    *left=(PeVec3){g->player.x+l.x,g->player.y+l.y,g->player.z+l.z};*right=(PeVec3){g->player.x+r.x,g->player.y+r.y,g->player.z+r.z};
    return true;
}
static void draw_pose(PeSandboxActor *a,const PeGame *g,PePlayerLocomotionClip clip,double seconds,Vector3 position,float yaw,bool preview) {
    if(!isfinite(yaw))return;
    SelectedPose p;if(!select_pose(a,g,clip,seconds,preview,&p))return;
    int body=p.body,tool=p.tool;size_t body_clip=p.body_clip,tool_clip=p.tool_clip;seconds=p.seconds;
    bool ok;
    PeIkDriveFrame solved;const PeIkDriveFrame *frame=!preview&&ik_frame(a,g,&p,&solved)?&solved:NULL;
    pe_actor_scene_view_set_ik(&a->views[body],frame);pe_actor_scene_view_set_ik(&a->views[WOOD_SHIELD],frame);
    if(tool>=0)pe_actor_scene_view_set_ik(&a->views[tool],frame);
    if(frame)a->ik_draws++;
    if(p.locomotion){
        PeActorBlendSample samples[PE_ACTOR_BLEND_MAX];size_t count=locomotion_samples(a,body,p.sword,recovery_bound(a,g),samples);
        ok=count&&pe_actor_scene_view_draw_blend(&a->views[body],samples,count,position,yaw,1,WHITE);
        if(tool>=0&&a->views[tool].loaded){count=locomotion_samples(a,tool,p.sword,recovery_bound(a,g),samples);
            ok=count&&pe_actor_scene_view_draw_blend(&a->views[tool],samples,count,position,yaw,1,WHITE)&&ok;}
    }else{
        ok=pe_actor_scene_view_draw_clip(&a->views[body],body_clip,seconds,position,yaw,1,WHITE);
        if(tool>=0&&a->views[tool].loaded)ok=pe_actor_scene_view_draw_clip(&a->views[tool],tool_clip,seconds,position,yaw,1,WHITE)&&ok;
    }
    if(shield_bound(a,g)){
        if(p.locomotion){PeActorBlendSample samples[PE_ACTOR_BLEND_MAX];size_t count=locomotion_samples(a,WOOD_SHIELD,p.sword,recovery_bound(a,g),samples);
            ok=count&&pe_actor_scene_view_draw_blend(&a->views[WOOD_SHIELD],samples,count,position,yaw,1,WHITE)&&ok;
        }else{const PeActorClip *c=pe_actor_scene_view_clip_at(&a->views[body],body_clip);
            size_t shield_clip=c?pe_actor_scene_view_find_clip(&a->views[WOOD_SHIELD],c->name):PE_ACTOR_BIND_POSE;
            if(shield_clip!=PE_ACTOR_BIND_POSE)ok=pe_actor_scene_view_draw_clip(&a->views[WOOD_SHIELD],shield_clip,seconds,position,yaw,1,WHITE)&&ok;
        }
    }
    pe_actor_scene_view_set_ik(&a->views[body],NULL);pe_actor_scene_view_set_ik(&a->views[WOOD_SHIELD],NULL);
    if(tool>=0)pe_actor_scene_view_set_ik(&a->views[tool],NULL);
    if(!ok){a->ok=false;fprintf(stderr,"Sandbox actor pose rendering failed\n");}
}
static bool torch_point(const PeSandboxActor *a,const PeGame *g,const float point[3],PeVec3 *out){
    if(!out||!g||!finite3(g->player)||!isfinite(g->yaw)||!isfinite(g->hp)||g->hp<=0||
       !pe_sandbox_actor_hold_ready(a,10))return false;
    const PeSlot *equipped=pe_game_equipped_tool(g);
    if(!equipped||equipped->item_id!=10||equipped->instance_id!=a->action.instance_id||
       a->action.item_id!=10||!a->action.held)return false;
    const PeActorSceneView *view=&a->views[TORCH_HELD];
    if(!view->count||!view->parts||!view->parts[0].actor)return false;
    float local[3];PeActorBlendSample samples[PE_ACTOR_BLEND_MAX];size_t count=locomotion_samples(a,TORCH_HELD,false,false,samples);
    if(!count||!pe_actor_pose_joint_point_blend(view->parts[0].actor,samples,count,0,point,local))return false;
    /* The emitter follows the hand through the same solve as the visible torch. */
    SelectedPose p;PeIkDriveFrame solved;
    if(select_pose(a,g,a->locomotion.clip,a->locomotion.seconds,false,&p)&&p.tool==TORCH_HELD&&p.locomotion&&ik_frame(a,g,&p,&solved)&&
       !pe_actor_pose_joint_point_blend_driven(view->parts[0].actor,samples,count,view->parts[0].ik_driver,solved.delta[0],0,point,local))return false;
    float yaw=pe_sandbox_actor_yaw(a,g)*DEG2RAD,c=cosf(yaw),s=sinf(yaw);
    PeVec3 result={g->player.x+c*local[0]+s*local[2],g->player.y+local[1],
                  g->player.z-s*local[0]+c*local[2]};
    if(!finite3(result))return false;
    *out=result;return true;
}
bool pe_sandbox_actor_torch_light(const PeSandboxActor *a,const PeGame *g,PeVec3 *out){
    const float point[3]={PE_TORCH_LIGHT_POSITION_X,PE_TORCH_LIGHT_POSITION_Y,PE_TORCH_LIGHT_POSITION_Z};
    return torch_point(a,g,point,out);
}
bool pe_sandbox_actor_torch_flame(const PeSandboxActor *a,const PeGame *g,PeVec3 *out){
    const float point[3]={PE_TORCH_FLAME_POSITION_X,PE_TORCH_FLAME_POSITION_Y,PE_TORCH_FLAME_POSITION_Z};
    return torch_point(a,g,point,out);
}
void pe_sandbox_actor_draw(PeSandboxActor *a,const PeGame *g){
    if(!g||!finite3(g->player)||!isfinite(g->yaw))return;
    draw_pose(a,g,a?a->locomotion.clip:PE_LOCOMOTION_IDLE,a?a->locomotion.seconds:0,(Vector3){g->player.x,g->player.y,g->player.z},pe_sandbox_actor_yaw(a,g),false);
}
void pe_sandbox_actor_preview(void *context,const PeGame *g){
    PeSandboxActor *a=context;if(!a||!a->ok||!g)return;
    Camera3D camera={{0,1,3.8f},{0,.94f,0},{0,1,0},30,CAMERA_PERSPECTIVE};
    BeginMode3D(camera);draw_pose(a,g,PE_LOCOMOTION_IDLE,a->elapsed,(Vector3){0},0,true);EndMode3D();
}
