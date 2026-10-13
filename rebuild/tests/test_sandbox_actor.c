/* Headless presentation contract. Scene/GPU boundaries are mocked; actual
 * PEAC/PEAS parsing and skin arithmetic have separate real-file tests. */
#include "pe_sandbox_actor.h"
#include "pe_actor_scene_view.h"
#include "pe_ik_drive.h"
#include "pe_player_locomotion.h"
#include "../generated/pe_torch_source.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do{if(!(e)){fprintf(stderr,"sandbox actor:%d: %s\n",__LINE__,#e);failures++;}}while(0)
enum { BODY,KNIFE,SHOVEL_STOWED,SHOVEL_HELD,AXE_HELD,WOOD_AXE_HELD,PICKAXE_STOWED,PICKAXE_HELD,
       TORCH_STOWED,TORCH_HELD,KNIFE_HELD,
       DIG_BODY,DIG_TOOL,FELL_BODY,FELL_TOOL,WOOD_FELL_TOOL,MINE_BODY,MINE_TOOL,LEGACY_VIEWS,WOOD_SWORD_STOWED=LEGACY_VIEWS,WOOD_SWORD_HELD,WOOD_SHIELD,
       REACTION_BODY,REACTION_SWORD_STOWED,REACTION_SWORD_HELD,REACTION_SHIELD,VIEWS };
static const char *const folders[]={"player","player-knife-stowed","player-shovel-stowed","player-shovel","player-axe","player-wood-axe",
    "player-pickaxe-stowed","player-pickaxe","player-torch-stowed","player-torch","player-knife-drawn","player-dig","player-shovel-dig","player-cut","player-axe-cut","player-wood-axe-cut","player-mine","player-pickaxe-mine","player-wood-sword-stowed","player-wood-sword-drawn","player-wood-shield",
    "player-reaction","player-reaction-wood-sword-stowed","player-reaction-wood-sword-drawn","player-reaction-wood-shield"};
static PeActorClip clips[VIEWS][33];
static bool window=true,optional=true,draw_failure=false,missing_clip=false,wood_pair=false,reaction_group=false;
static int missing_view=-1,bad_clock_view=-1,bad_source_view=-1;
static unsigned pickaxe_mask=15,running_mask,sprint_mask;
static bool running_in_view(int v){return (v==BODY&&(running_mask&1))||(v==KNIFE_HELD&&(running_mask&2))||(v==KNIFE&&(running_mask&4));}
static bool sprint_in_view(int v){return (v==BODY&&(sprint_mask&1))||(v==KNIFE_HELD&&(sprint_mask&2))||(v==KNIFE&&(sprint_mask&4));}
static int wrong_count_view=-1;
static int missing_direction_view=-1;
static int allocated,uploads,fail_upload,draw_count,mode_depth;
typedef struct {
    int view;
    /* Single draws record their actual clock. For blends draw() fills these
     * two legacy selection diagnostics from the independently queried pose;
     * actual render inputs are always retained and checked in samples[]. */
    size_t clip;double seconds;Vector3 position;float yaw;
    bool blended;size_t count;PeActorBlendSample samples[PE_ACTOR_BLEND_MAX];
    bool driven; /* The view held solved bone motions when it was drawn. */
} Draw;
static Draw draws[16];
static bool light_pose_failure;
static size_t light_clip;
static double light_seconds;
static size_t light_count;
static PeActorBlendSample light_samples[PE_ACTOR_BLEND_MAX];
static int path_view(const char *path) {
    for(int i=0;i<VIEWS;i++){char fragment[80];snprintf(fragment,sizeof fragment,"/%s/",folders[i]);if(strstr(path,fragment))return i;}
    return -1;
}
/* Staged solver poses and drivers: absent unless a test installs them. */
static bool ik_staged,ik_load_failure,ik_solve_failure,ik_arms_mapped;
static int ik_unbound_view=-1,ik_loaded,ik_solves,ik_light_moves;
static size_t ik_count;static PeIkDriveSample ik_samples[PE_ACTOR_BLEND_MAX];
bool pe_ik_drive_present(const char *root){(void)root;return ik_staged;}
PeIkDrive *pe_ik_drive_load(const char *root,char *error,size_t cap){
    (void)root;(void)error;(void)cap;if(ik_load_failure)return NULL;
    ik_loaded++;return (PeIkDrive*)(uintptr_t)1;
}
void pe_ik_drive_free(PeIkDrive *d){if(d)ik_loaded--;}
bool pe_ik_drive_solve(const PeIkDrive *d,const PeIkDriveSample *samples,size_t count,bool arms_mapped,PeIkDriveFrame *out){
    if(!d||!samples||!count||count>PE_ACTOR_BLEND_MAX||ik_solve_failure)return false;
    ik_solves++;ik_count=count;memcpy(ik_samples,samples,count*sizeof *samples);ik_arms_mapped=arms_mapped;
    memset(out,0,sizeof *out);return true;
}
bool pe_actor_scene_view_bind_ik(PeActorSceneView *v,const PeIkDrive *d,const char *group){
    int id=(int)(uintptr_t)v->manifest-1;
    v->ik_bound=d&&v->loaded&&id!=ik_unbound_view&&!strcmp(group,folders[id]);return v->ik_bound;
}
void pe_actor_scene_view_set_ik(PeActorSceneView *v,const PeIkDriveFrame *frame){v->ik_active=frame&&v->ik_bound;}
bool pe_actor_pose_joint_point_blend_driven(const PeActor *a,const PeActorBlendSample *samples,size_t count,const unsigned char *driver,
                                            const float *delta,size_t joint,const float p[3],float out[3]){
    (void)a;(void)samples;(void)count;(void)driver;(void)joint;(void)p;if(!delta)return false;
    ik_light_moves++;out[1]+=.25f;return true;
}
bool IsWindowReady(void){return window;}
bool FileExists(const char *path){
    int v=path_view(path),bit=v==PICKAXE_STOWED?0:v==PICKAXE_HELD?1:v==MINE_BODY?2:v==MINE_TOOL?3:-1;
    return v>=0&&(v<LEGACY_VIEWS||(v<REACTION_BODY?wood_pair:reaction_group))&&optional&&v!=missing_view&&(bit<0||(pickaxe_mask&(1u<<bit)));
}
void BeginMode3D(Camera3D camera){(void)camera;mode_depth++;}
void EndMode3D(void){mode_depth--;}
bool pe_actor_scene_view_prepare(PeActorSceneView *v,const char *path,char *error,size_t cap) {
    (void)error;(void)cap;int id=path_view(path);if(id<0)return false;
    v->manifest=(PeActorSceneManifest*)(uintptr_t)(id+1);v->count=1;
    v->parts=calloc(1,sizeof *v->parts);if(!v->parts)return false;
    v->parts[0].actor=(PeActor*)(uintptr_t)(id+1);allocated++;return true;
}
void pe_actor_scene_view_unload(PeActorSceneView *v){if(v&&v->manifest){free(v->parts);allocated--;memset(v,0,sizeof *v);}}
bool pe_actor_pose_joint_point(const PeActor *a,size_t clip,double seconds,size_t joint,const float p[3],float out[3]){
    if(light_pose_failure||(int)(uintptr_t)a!=TORCH_HELD+1||joint!=0)return false;
    light_clip=clip;light_seconds=seconds;
    out[0]=p[0]+.5f;out[1]=p[1]+1.0f;out[2]=p[2];return true;
}
bool pe_actor_pose_joint_point_blend(const PeActor *a,const PeActorBlendSample *samples,size_t count,size_t joint,const float p[3],float out[3]){
    if(!samples||!count||count>PE_ACTOR_BLEND_MAX||light_pose_failure||(int)(uintptr_t)a!=TORCH_HELD+1||joint!=0)return false;
    light_count=count;memcpy(light_samples,samples,count*sizeof *samples);
    return pe_actor_pose_joint_point(a,samples[0].clip,samples[0].seconds,joint,p,out);
}
bool pe_actor_scene_view_upload(PeActorSceneView *v,char *error,size_t cap) {
    (void)error;(void)cap;uploads++;if(uploads==fail_upload){pe_actor_scene_view_unload(v);return false;}v->loaded=true;return true;
}
size_t pe_actor_scene_view_clip_count(const PeActorSceneView *v){
    int id=(int)(uintptr_t)v->manifest-1;
    return id==wrong_count_view?4:id>=REACTION_BODY?25:wood_pair&&(id==BODY||id>=WOOD_SWORD_STOWED)?33:id>=DIG_BODY?3:((id==BODY||id==KNIFE_HELD)?PE_LOCOMOTION_COUNT+4:PE_LOCOMOTION_COUNT)+(running_in_view(id)?1:0)+(sprint_in_view(id)?1:0);
}
size_t pe_actor_scene_view_find_clip(const PeActorSceneView *v,const char *name) {
    int id=(int)(uintptr_t)v->manifest-1;
    for(size_t i=0;i<pe_actor_scene_view_clip_count(v);i++)if(!strcmp(name,clips[id][i].name))
        return (missing_clip&&i==PE_LOCOMOTION_FALL)||(id==missing_direction_view&&i==PE_LOCOMOTION_WALK_LEFT)?PE_ACTOR_BIND_POSE:i;
    return PE_ACTOR_BIND_POSE;
}
const PeActorClip *pe_actor_scene_view_clip_at(const PeActorSceneView *v,size_t i) {
    if(i>=pe_actor_scene_view_clip_count(v))return NULL;
    int id=(int)(uintptr_t)v->manifest-1;
    clips[id][i].fps=bad_clock_view==id?29:30;
    clips[id][i].terminal_at_duration=bad_source_view!=id;
    return &clips[id][i];
}
bool pe_actor_scene_view_draw_clip(PeActorSceneView *v,size_t clip,double seconds,Vector3 p,float yaw,float scale,Color tint) {
    (void)scale;(void)tint;if(draw_count<16)draws[draw_count]=(Draw){.view=(int)(uintptr_t)v->manifest-1,.clip=clip,.seconds=seconds,.position=p,.yaw=yaw,.driven=v->ik_active};draw_count++;return !draw_failure;
}
bool pe_actor_scene_view_draw_blend(PeActorSceneView *v,const PeActorBlendSample *samples,size_t count,Vector3 p,float yaw,float scale,Color tint){
    (void)scale;(void)tint;
    if(!samples||!count||count>PE_ACTOR_BLEND_MAX)return false;
    if(draw_count<16){
        Draw *d=&draws[draw_count];*d=(Draw){.view=(int)(uintptr_t)v->manifest-1,.clip=PE_ACTOR_BIND_POSE,.position=p,.yaw=yaw,.blended=true,.count=count,.driven=v->ik_active};
        memcpy(d->samples,samples,count*sizeof *samples);
    }
    draw_count++;return !draw_failure;
}
const PeSlot *pe_game_equipped_tool(const PeGame *g) {
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->equipped_instance_id&&g->inventory.slots[i].instance_id==g->equipped_instance_id)return &g->inventory.slots[i];
    return NULL;
}
const PeSlot *pe_shield_equipped(const PeGame *g){
    const PeSlot *found=NULL;
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->shield_instance_id&&g->inventory.slots[i].instance_id==g->shield_instance_id){
        if(found||g->inventory.slots[i].item_id!=34||g->inventory.slots[i].count!=1)return NULL;
        found=&g->inventory.slots[i];
    }
    return found;
}
static int pose_failures;
static bool same_pose(const PeSandboxBodyPose *a,const PeSandboxBodyPose *b){
    if(strcmp(a->clip,b->clip)||a->seconds!=b->seconds||a->duration!=b->duration||a->loop!=b->loop||a->yaw!=b->yaw||a->blend_count!=b->blend_count)return false;
    for(size_t i=0;i<a->blend_count;i++)if(strcmp(a->blend[i].clip,b->blend[i].clip)||a->blend[i].seconds!=b->blend[i].seconds||a->blend[i].duration!=b->blend[i].duration||a->blend[i].weight!=b->blend[i].weight||a->blend[i].loop!=b->blend[i].loop)return false;
    return true;
}
static bool same_samples(const PeActorBlendSample *a,const PeActorBlendSample *b,size_t count){
    for(size_t i=0;i<count;i++)if(a[i].clip!=b[i].clip||a[i].seconds!=b[i].seconds||a[i].weight!=b[i].weight)return false;
    return true;
}
static bool same_angle(float a,float b){return fabsf(remainderf(a-b,360))<.001f;}
static void draw(PeSandboxActor *a,PeGame *g){
    PeSandboxBodyPose before={0},after={0};bool available=pe_sandbox_actor_body_pose(a,g,&before);
    draw_count=0;pe_sandbox_actor_draw(a,g);
    if(draw_count){
        bool match=available&&before.yaw==draws[0].yaw;
        if(before.blend_count){
            for(int d=0;d<draw_count;d++){
                Draw *actual=&draws[d];double total=0;
                match=match&&actual->blended&&actual->count==before.blend_count&&actual->yaw==before.yaw&&actual->position.x==g->player.x&&actual->position.y==g->player.y&&actual->position.z==g->player.z;
                for(size_t i=0;i<actual->count;i++){
                    const PeActorBlendSample *sample=&actual->samples[i];
                    if(sample->clip>=33){match=false;continue;}
                    const PeActorClip *definition=&clips[actual->view][sample->clip];
                    match=match&&!strcmp(definition->name,before.blend[i].clip)&&definition->duration==before.blend[i].duration&&definition->loop==before.blend[i].loop&&sample->seconds==before.blend[i].seconds&&sample->weight==before.blend[i].weight&&sample->weight>=0;
                    total+=sample->weight;
                }
                match=match&&fabs(total-1)<1e-6;
                /* Keep earlier direction-selection checks as diagnostics; the
                 * loop above independently checks every rendered contributor. */
                for(size_t i=0;i<33;i++)if(!strcmp(clips[actual->view][i].name,before.clip)){actual->clip=i;actual->seconds=before.seconds;break;}
            }
        }else{
            const PeActorClip *visible=&clips[draws[0].view][draws[0].clip];
            match=match&&!draws[0].blended&&!strcmp(before.clip,visible->name)&&before.seconds==draws[0].seconds&&before.duration==visible->duration&&before.loop==visible->loop;
            for(int i=0;i<draw_count;i++)match=match&&!draws[i].blended;
        }
        if(!draw_failure)match=match&&pe_sandbox_actor_body_pose(a,g,&after)&&same_pose(&before,&after);
        if(!match){fprintf(stderr,"body/attachment snapshot differs from rendered contributors or query mutates state\n");pose_failures++;}
    }
}
static void init_clips(void) {
    for(int v=0;v<DIG_BODY;v++)for(int i=0;i<PE_LOCOMOTION_COUNT;i++) {
        snprintf(clips[v][i].name,sizeof clips[v][i].name,"%s",pe_player_locomotion_name(i));
        clips[v][i].fps=30;clips[v][i].duration=2;clips[v][i].frames=61;
        clips[v][i].loop=i<3||i==PE_LOCOMOTION_FALL||i>=PE_LOCOMOTION_WALK_LEFT;clips[v][i].terminal_at_duration=true;
    }
    static const float melee_length[]={1.0666667222976685f,1.100000262260437f,1.2666668891906738f,.9666671752929688f};
    static const unsigned melee_frames[]={34,35,40,31};
    for(unsigned j=0;j<2;j++)for(unsigned i=0;i<4;i++){
        PeActorClip *c=&clips[j?KNIFE_HELD:BODY][PE_LOCOMOTION_COUNT+i];
        snprintf(c->name,sizeof c->name,"Attack%u",i+6);c->fps=30;c->duration=melee_length[i];c->frames=melee_frames[i];c->loop=false;c->terminal_at_duration=true;
    }
    const int running_views[]={BODY,KNIFE_HELD,KNIFE};
    for(unsigned j=0;j<3;j++){
        PeActorClip *c=&clips[running_views[j]][PE_LOCOMOTION_COUNT+(j==2?0:4)];
        snprintf(c->name,sizeof c->name,"RunSwordAttack");c->fps=30;c->duration=1.8333396911621094f;
        c->frames=57;c->loop=false;c->terminal_at_duration=true;
    }
    for(unsigned j=0;j<3;j++)if(sprint_in_view(running_views[j])){
        int view=running_views[j];size_t index=PE_LOCOMOTION_COUNT+(view==KNIFE?0:4)+(running_in_view(view)?1:0);
        clips[view][index]=(PeActorClip){.name="SprintSwordAttack",.fps=30,.frames=56,.duration=1.8000068664550781f,.terminal_at_duration=true};
    }
    if(wood_pair){
        const char *names[]={"AttackIdle","SwordWF","SwordRF","SwordSprint","SwordJump","SwordFall","SwordLand","SwordRunJump","SwordRunFall","SwordRunLand","SwordWB","SwordRB","NormalWalkTurnLeft","NormalWalkTurnRight","NormalRunTurnLeft","NormalRunTurnRight"};
        const float times[]={1.7333335876464844f,1.0999984741210938f,.6666679382324219f,.6000001430511475f,1.0333404541015625f,.6666717529296875f,1.066680908203125f,1.4000015258789062f,1.0000038146972656f,.23333740234375f,.8000030517578125f,.6666717529296875f,1.1000022888183594f,1.100006103515625f,.6666717529296875f,.6666717529296875f};
        const unsigned frames[]={54,34,22,20,33,22,34,44,32,9,26,22,35,35,22,22};
        for(unsigned i=0;i<16;i++){
            PeActorClip *c=&clips[BODY][17+i];snprintf(c->name,sizeof c->name,"%s",names[i]);c->duration=times[i];c->frames=frames[i];c->fps=30;c->terminal_at_duration=true;
            c->loop=i<4||i==8||i>=10;
        }
        for(int v=WOOD_SWORD_STOWED;v<REACTION_BODY;v++)memcpy(clips[v],clips[BODY],sizeof clips[v]);
    }
    {
        static const char *const names[25]={"NormalIdle","IdleBeHit","SwordBeHit","ForwardFast","FowardRightFast","RightFast","BackRightFast","BackFast","ForwardLeftFast","LeftFast","BackLeftFast",
            "Forward","ForwardRight","Right","BackRight","Back","ForwardLeft","Left","BackLeft","BlockRepulsed","GetUpBackMirror","GetUpFrontMirror","GetUpBack","GetUpFront","SprintSwordAttack"};
        static const float times[25]={0,.9333336353302002f,.9333419799804688f,1.0000009536743164f,1.0000009536743164f,1.0000019073486328f,1,1.0000009536743164f,1.0000009536743164f,1.0000019073486328f,1,
            1.9666669368743896f,1.9666671752929688f,.9666671752929688f,1.9666681289672852f,1.9666669368743896f,1.9666671752929688f,.9666671752929688f,1.9666681289672852f,1.0000152587890625f,
            1.4666709899902344f,1.0666694641113281f,1.4666709899902344f,1.0666694641113281f,1.8000068664550781f};
        static const unsigned counts[25]={0,30,30,32,32,32,31,32,32,32,31,61,61,31,61,61,61,31,61,32,46,34,46,34,56};
        for(int v=REACTION_BODY;v<VIEWS;v++){
            memset(clips[v],0,sizeof clips[v]);clips[v][0]=clips[BODY][PE_LOCOMOTION_IDLE];
            for(unsigned i=1;i<25;i++){
                PeActorClip *c=&clips[v][i];snprintf(c->name,sizeof c->name,"%s",names[i]);c->fps=30;c->duration=times[i];c->frames=counts[i];
                c->loop=i>=3&&i<20&&i!=4;c->terminal_at_duration=true;
            }
        }
    }
    static const float length[3][3]={{.6999969482421875f,1.3333358764648438f,.8333358764648438f},{.6000003814697266f,1.100006103515625f,.6000003814697266f},
        {.6999969482421875f,1.3333358764648438f,.8333358764648438f}};
    static const unsigned frames[3][3]={{22,42,27},{20,35,20},{22,42,27}};
    for(int v=DIG_BODY;v<LEGACY_VIEWS;v++)for(int i=0;i<3;i++) {
        int kind=v>=MINE_BODY?2:v>=FELL_BODY?1:0;snprintf(clips[v][i].name,sizeof clips[v][i].name,"%s%d",kind==2?"Mine":kind==1?"Felling":"Spade",i+1);
        clips[v][i].fps=30;clips[v][i].duration=length[kind][i];clips[v][i].frames=frames[kind][i];clips[v][i].loop=i==1;clips[v][i].terminal_at_duration=true;
    }
}
/* The separate reaction defence actor is a fake view-id handle here. */
PeActorStats pe_actor_stats(const PeActor *a){int id=(int)(uintptr_t)a-1;return (PeActorStats){.clips=id==wrong_count_view?4:id>=REACTION_BODY?25:0};}
const PeActorClip *pe_actor_clip(const PeActor *a,size_t i){int id=(int)(uintptr_t)a-1;return id>=REACTION_BODY&&id<VIEWS&&i<25?&clips[id][i]:NULL;}
static int reaction_actor_tests(void){
    int failures=0;char error[256];uploads=0;
    PeActor *defence=(PeActor*)(uintptr_t)(REACTION_SHIELD+1);
    PeSandboxActor *a=pe_sandbox_actor_load("assets",error,sizeof error);
    CHECK(a&&!pe_sandbox_actor_reaction_ready(a)&&!pe_sandbox_actor_reaction_ready(NULL));
    CHECK(!pe_sandbox_actor_reaction_defence_ready(a,defence));
    pe_sandbox_actor_unload(a);CHECK(allocated==0);uploads=0;
    reaction_group=true;init_clips();
    for(missing_view=REACTION_BODY;missing_view<VIEWS;missing_view++){
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
        CHECK(strstr(error,"complete body/sword/shield")!=NULL);
    }
    missing_view=-1;
    for(wrong_count_view=REACTION_BODY;wrong_count_view<VIEWS;wrong_count_view++){
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
    }
    wrong_count_view=-1;
    for(bad_source_view=REACTION_BODY;bad_source_view<VIEWS;bad_source_view++){
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
    }
    bad_source_view=-1;
    for(unsigned field=0;field<7;field++){
        if(field==0)clips[REACTION_BODY][0].duration+=.1f;                 /* anchor differs from body */
        if(field==1)clips[REACTION_SHIELD][7].name[0]='X';                 /* name */
        if(field==2){PeActorClip t=clips[REACTION_SWORD_HELD][11];clips[REACTION_SWORD_HELD][11]=clips[REACTION_SWORD_HELD][12];clips[REACTION_SWORD_HELD][12]=t;} /* order */
        if(field==3)for(int v=REACTION_BODY;v<VIEWS;v++)clips[v][24].frames++;   /* coherent but not the source clock */
        if(field==4)for(int v=REACTION_BODY;v<VIEWS;v++)clips[v][4].loop=true;  /* source FowardRightFast is unlooped */
        if(field==5)clips[REACTION_SWORD_STOWED][20].duration+=.001f;      /* attachment clock */
        if(field==6)for(int v=REACTION_BODY;v<VIEWS;v++)clips[v][1].duration=1; /* invented stun length */
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
        CHECK(strstr(error,"Reaction")!=NULL);init_clips();
    }
    a=pe_sandbox_actor_load("assets",error,sizeof error);
    CHECK(a&&pe_sandbox_actor_reaction_ready(a)&&allocated==LEGACY_VIEWS+4&&uploads==LEGACY_VIEWS+4);
    /* Loading the group changes no existing capability and presents nothing. */
    CHECK(pe_sandbox_actor_hold_ready(a,33)&&!pe_sandbox_actor_hold_ready(a,1)&&!pe_sandbox_actor_shield_ready(a));
    PeGame *g=calloc(1,sizeof *g);if(!g)return failures+1;
    g->mode=PE_GAME_ADVENTURE;g->hp=500;g->grounded=true;pe_sandbox_actor_reset_motion(a,g);
    PeSandboxBodyPose pose={0};CHECK(pe_sandbox_actor_body_pose(a,g,&pose)&&!strcmp(pose.clip,"NormalIdle"));
    draw(a,g);CHECK(draw_count>0);for(int i=0;i<draw_count&&i<16;i++)CHECK(draws[i].view<REACTION_BODY);
    PeSandboxActionPose reaction={.item_id=33,.instance_id=1,.held=true,.clip="IdleBeHit",.seconds=.1};
    CHECK(!pe_sandbox_actor_set_action(a,&reaction));
    CHECK(pe_sandbox_actor_reaction_defence_ready(a,defence));
    CHECK(!pe_sandbox_actor_reaction_defence_ready(a,NULL));
    CHECK(!pe_sandbox_actor_reaction_defence_ready(a,(PeActor*)(uintptr_t)(WOOD_SHIELD+1)));
    clips[REACTION_SHIELD][9].frames++;CHECK(!pe_sandbox_actor_reaction_defence_ready(a,defence));clips[REACTION_SHIELD][9].frames--;
    wrong_count_view=REACTION_SHIELD;CHECK(!pe_sandbox_actor_reaction_defence_ready(a,defence));wrong_count_view=-1;
    free(g);pe_sandbox_actor_unload(a);CHECK(allocated==0);
    reaction_group=false;uploads=0;init_clips();failures+=pose_failures;pose_failures=0;
    return failures;
}
static int sprint_actor_tests(void){
    int failures=0;running_mask=7;
    CHECK(!pe_sandbox_actor_sprint_ready(NULL));
    for(sprint_mask=1;sprint_mask<7;sprint_mask++){
        init_clips();char error[128];PeSandboxActor *a=pe_sandbox_actor_load("assets",error,sizeof error);
        CHECK(!a&&strstr(error,"SprintSwordAttack"));pe_sandbox_actor_unload(a);CHECK(allocated==0);
    }
    sprint_mask=7;
    for(unsigned mutation=0;mutation<5;mutation++){
        init_clips();PeActorClip *clip=&clips[KNIFE_HELD][PE_LOCOMOTION_COUNT+5];
        if(mutation==0)bad_clock_view=KNIFE_HELD;
        if(mutation==1)clip->frames++;
        if(mutation==2)clip->duration+=.01f;
        if(mutation==3)clip->loop=true;
        if(mutation==4)bad_source_view=KNIFE_HELD;
        char error[128];PeSandboxActor *a=pe_sandbox_actor_load("assets",error,sizeof error);CHECK(!a);pe_sandbox_actor_unload(a);CHECK(allocated==0);
        bad_clock_view=bad_source_view=-1;
    }
    init_clips();char error[128];PeSandboxActor *a=pe_sandbox_actor_load("assets",error,sizeof error);
    CHECK(a&&pe_sandbox_actor_sprint_ready(a));pe_sandbox_actor_unload(a);CHECK(allocated==0);
    sprint_mask=running_mask=0;init_clips();return failures;
}
static int running_actor_tests(void){
    int failures=0;char error[256];uploads=0;
    CHECK(!pe_sandbox_actor_running_ready(NULL));
    for(running_mask=1;running_mask<7;running_mask++){
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
        CHECK(strstr(error,"RunSwordAttack")!=NULL);
    }
    for(unsigned field=0;field<4;field++){
        if(field==0)clips[KNIFE][PE_LOCOMOTION_COUNT].duration+=.1f;
        if(field==1)clips[KNIFE_HELD][PE_LOCOMOTION_COUNT+4].loop=true;
        if(field==2)for(int v=0;v<VIEWS;v++)if(running_in_view(v))clips[v][PE_LOCOMOTION_COUNT+(v==KNIFE?0:4)].frames++;
        if(field==3)clips[BODY][PE_LOCOMOTION_COUNT+4].name[0]='X';
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);init_clips();
    }
    missing_view=KNIFE_HELD;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);missing_view=-1;
    PeSandboxActor *a=pe_sandbox_actor_load("assets",error,sizeof error);
    CHECK(a&&pe_sandbox_actor_running_ready(a)&&pe_sandbox_actor_hold_ready(a,33));
    PeGame *g=calloc(1,sizeof *g);if(!a||!g){pe_sandbox_actor_unload(a);free(g);running_mask=0;return failures+1;}
    g->player=(PeVec3){20,10,20};g->yaw=.4f;g->grounded=true;g->hp=500;
    g->inventory.slots[0]=(PeSlot){.item_id=33,.instance_id=77,.count=1};g->equipped_instance_id=77;
    pe_sandbox_actor_reset_motion(a,g);pe_sandbox_actor_facing_mode(a,true);
    g->player.x+=.5f;pe_sandbox_actor_update(a,.1f,g);
    PeCombat combat={.active=true,.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,.variant=PE_COMBAT_RUNNING,.clip_time=.5};
    CHECK(pe_sandbox_actor_set_melee(a,77,true,&combat));draw(a,g);
    CHECK(draw_count==2&&!draws[0].blended&&!draws[1].blended&&draws[0].clip==PE_LOCOMOTION_COUNT+4&&draws[1].clip==PE_LOCOMOTION_COUNT+4);
    CHECK(draws[0].seconds==.5&&draws[1].seconds==.5&&draws[1].view==KNIFE_HELD);
    PeSandboxBodyPose before,after;CHECK(pe_sandbox_actor_body_pose(a,g,&before));
    CHECK(!strcmp(before.clip,"RunSwordAttack")&&before.blend_count==0&&before.duration==1.8333396911621094f&&!before.loop);
    float locked_yaw=before.yaw;g->yaw=-1;g->player.z+=.25f;
    pe_sandbox_actor_update(a,.1f,g);draw(a,g);
    CHECK(draws[0].seconds==.5&&draws[1].seconds==.5&&same_angle(draws[0].yaw,locked_yaw));
    CHECK(pe_sandbox_actor_body_pose(a,g,&after)&&same_pose(&before,&after));
    draw_count=0;pe_sandbox_actor_preview(a,g);CHECK(!draws[0].blended&&draws[0].clip==PE_LOCOMOTION_IDLE&&draws[1].clip==PE_LOCOMOTION_IDLE);
    CHECK(pe_sandbox_actor_body_pose(a,g,&after)&&same_pose(&before,&after));
    for(unsigned invalid=0;invalid<5;invalid++){
        PeCombat bad=combat;
        if(invalid==0)bad.clip_time=NAN;
        if(invalid==1)bad.clip_time=1.8333396911621094+.01;
        if(invalid==2)bad.variant=(PeCombatVariant)9;
        if(invalid==3)bad.gender=PE_COMBAT_MALE;
        if(invalid==4)bad.view=PE_COMBAT_FIRST_PERSON;
        CHECK(!pe_sandbox_actor_set_melee(a,77,true,&bad));
        CHECK(pe_sandbox_actor_body_pose(a,g,&after)&&same_pose(&before,&after));
    }
    for(unsigned stage=1;stage<4;stage++){
        combat.stage=stage;combat.clip_time=.125;CHECK(pe_sandbox_actor_set_melee(a,77,true,&combat));draw(a,g);
        CHECK(draws[0].clip==PE_LOCOMOTION_COUNT+stage&&draws[1].clip==PE_LOCOMOTION_COUNT+stage&&!draws[0].blended);
        CHECK(pe_sandbox_actor_body_pose(a,g,&after)&&!strcmp(after.clip,pe_combat_pose_name(&combat))&&after.blend_count==0);
    }
    combat.stage=0;combat.clip_time=.5;CHECK(pe_sandbox_actor_set_melee(a,77,true,&combat));
    g->inventory.slots[0].instance_id=78;g->equipped_instance_id=78;draw(a,g);CHECK(draws[0].blended&&draws[1].view==KNIFE);
    g->inventory.slots[0].instance_id=77;g->equipped_instance_id=77;g->hp=0;draw(a,g);CHECK(draws[0].blended&&draws[1].view==KNIFE);g->hp=500;
    pe_sandbox_actor_reset_motion(a,g);draw(a,g);CHECK(draws[0].blended&&draws[0].clip==PE_LOCOMOTION_IDLE&&draws[1].view==KNIFE);
    combat.active=false;CHECK(pe_sandbox_actor_set_melee(a,77,true,&combat));draw(a,g);CHECK(draws[0].blended&&draws[1].view==KNIFE_HELD);
    pe_sandbox_actor_unload(a);free(g);CHECK(allocated==0);uploads=0;running_mask=0;init_clips();
    return failures;
}
static int sword_actor_tests(void){
    int failures=0;char error[256];wood_pair=true;running_mask=7;init_clips();uploads=0;
    for(missing_view=WOOD_SWORD_STOWED;missing_view<REACTION_BODY;missing_view++){
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
    }
    missing_view=-1;
    for(unsigned invalid=0;invalid<4;invalid++){
        if(invalid==0)clips[WOOD_SWORD_HELD][27].duration+=.1f;
        if(invalid==1)for(int v=0;v<VIEWS;v++)if(v==BODY||v>=WOOD_SWORD_STOWED)clips[v][27].name[0]='X';
        if(invalid==2)for(int v=0;v<VIEWS;v++)if(v==BODY||v>=WOOD_SWORD_STOWED)clips[v][29].frames++;
        if(invalid==3)wrong_count_view=WOOD_SHIELD;
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);wrong_count_view=-1;init_clips();
    }
    PeSandboxActor *a=pe_sandbox_actor_load("assets",error,sizeof error);CHECK(a&&allocated==REACTION_BODY);
    PeGame *g=calloc(1,sizeof *g);if(!a||!g){free(g);pe_sandbox_actor_unload(a);return failures+1;}
    g->player=(PeVec3){20,10,20};g->yaw=PI;g->grounded=true;g->hp=500;
    g->inventory.slots[0]=(PeSlot){.item_id=1,.instance_id=77,.count=1};g->equipped_instance_id=77;
    g->inventory.slots[1]=(PeSlot){.item_id=34,.instance_id=88,.count=1};g->shield_instance_id=88;
    pe_sandbox_actor_reset_motion(a,g);
    const PeActor *body=NULL,*weapon=NULL;
    CHECK(pe_sandbox_actor_melee_assets_for_item(a,1,&body,&weapon));CHECK(body==(PeActor*)(uintptr_t)(BODY+1)&&weapon==(PeActor*)(uintptr_t)(WOOD_SWORD_HELD+1));
    const PeActor *old_body=body,*old_weapon=weapon;CHECK(!pe_sandbox_actor_melee_assets_for_item(a,34,&body,&weapon)&&body==old_body&&weapon==old_weapon);
    CHECK(pe_sandbox_actor_running_ready_for_item(a,1)&&pe_sandbox_actor_running_ready(a));
    CHECK(pe_sandbox_actor_shield_ready(a)&&pe_sandbox_actor_shield_ready_for_item(a,1)&&!pe_sandbox_actor_shield_ready_for_item(a,12)&&!pe_sandbox_actor_shield_ready_for_item(a,33));
    CHECK(pe_sandbox_actor_set_melee_item(a,1,77,true,NULL));CHECK(pe_sandbox_actor_set_shield(a,88));draw(a,g);
    CHECK(draw_count==3&&draws[1].view==WOOD_SWORD_HELD&&draws[2].view==WOOD_SHIELD&&draws[0].clip==17);
    CHECK(!pe_sandbox_actor_action_ready(a,12)&&!pe_sandbox_actor_action_ready(a,33)&&pe_sandbox_actor_action_ready(a,1));
    CHECK(!pe_sandbox_actor_set_melee(a,77,true,NULL));CHECK(!pe_sandbox_actor_set_melee_item(a,34,77,true,NULL));
    CHECK(pe_sandbox_actor_sword_ground_ready(a,g));
    g->player.z-=.2f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==27&&draws[0].blended);
    PeSandboxBodyPose pose;CHECK(pe_sandbox_actor_body_pose(a,g,&pose)&&!strcmp(pose.clip,"SwordWB"));
    for(size_t i=0;i<pose.blend_count;i++)CHECK(strcmp(pose.blend[i].clip,"NormalWB")!=0);
    g->player.z-=.5f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==28);
    g->player.x-=.2f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==29);
    g->player.x+=.5f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==32);
    PeSandboxBodyPose before=pose,after;CHECK(pe_sandbox_actor_body_pose(a,g,&before));
    draw_count=0;pe_sandbox_actor_preview(a,g);CHECK(draw_count==3&&!draws[0].blended&&draws[0].clip==17&&draws[2].clip==17);
    CHECK(pe_sandbox_actor_body_pose(a,g,&after)&&same_pose(&before,&after));
    PeCombat c={.active=true,.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,.variant=PE_COMBAT_STATIONARY,.clip_time=.125};
    for(unsigned i=0;i<4;i++){c.stage=i;CHECK(pe_sandbox_actor_set_melee_item(a,1,77,true,&c));draw(a,g);
        CHECK(draw_count==3&&!draws[0].blended&&draws[0].clip==12+i&&draws[1].seconds==.125&&draws[2].seconds==.125&&draws[2].clip==12+i);}
    c.variant=PE_COMBAT_RUNNING;c.stage=0;CHECK(pe_sandbox_actor_set_melee_item(a,1,77,true,&c));draw(a,g);CHECK(draws[0].clip==16&&draws[2].clip==16);
    CHECK(pe_sandbox_actor_body_pose(a,g,&before));c.clip_time=NAN;CHECK(!pe_sandbox_actor_set_melee_item(a,1,77,true,&c));CHECK(pe_sandbox_actor_body_pose(a,g,&after)&&same_pose(&before,&after));
    CHECK(pe_sandbox_actor_set_melee_item(a,1,77,true,NULL));
    g->inventory.slots[0].instance_id=78;g->equipped_instance_id=78;draw(a,g);CHECK(draws[1].view==WOOD_SWORD_STOWED&&draws[0].clip<12);
    g->inventory.slots[0].instance_id=77;g->equipped_instance_id=77;
    g->inventory.slots[1].instance_id=89;g->shield_instance_id=89;draw(a,g);CHECK(draw_count==2);
    g->inventory.slots[1].instance_id=88;g->shield_instance_id=88;g->inventory.slots[1].count=2;draw(a,g);CHECK(draw_count==2);g->inventory.slots[1].count=1;
    g->hp=0;draw(a,g);CHECK(draw_count==3&&draws[1].view==WOOD_SWORD_STOWED&&draws[0].clip<12);g->hp=500;
    g->grounded=false;CHECK(!pe_sandbox_actor_sword_ground_ready(a,g));before=after;CHECK(!pe_sandbox_actor_body_pose(a,g,&after)&&same_pose(&before,&after));
    g->grounded=true;g->sprinting=true;CHECK(!pe_sandbox_actor_sword_ground_ready(a,g)&&!pe_sandbox_actor_body_pose(a,g,&after));g->sprinting=false;
    pe_sandbox_actor_reset_motion(a,g);draw(a,g);CHECK(draw_count==2&&draws[1].view==WOOD_SWORD_STOWED&&draws[0].clip==0);
    CHECK(pe_sandbox_actor_set_shield(a,88));CHECK(pe_sandbox_actor_set_action(a,NULL));CHECK(pe_sandbox_actor_set_shield(a,0));
    CHECK(pe_sandbox_actor_set_melee(a,77,true,NULL));CHECK(!pe_sandbox_actor_set_shield(a,88));
    pe_sandbox_actor_unload(a);free(g);CHECK(allocated==0);uploads=0;wood_pair=false;running_mask=0;init_clips();
    return failures;
}
/* The always-on solve is presentation only: every drawn scene of a live pose
 * carries the same bone motions, or none does. */
static int ik_actor_tests(void){
    int failures=0;char error[256];
    ik_staged=true;ik_load_failure=true;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0&&ik_loaded==0);
    ik_load_failure=false;PeSandboxActor *a=pe_sandbox_actor_load("assets",error,sizeof error);CHECK(a&&ik_loaded==1);
    PeGame *g=calloc(1,sizeof *g);if(!a||!g){pe_sandbox_actor_unload(a);free(g);ik_staged=false;return failures+1;}
    g->player=(PeVec3){20,10,20};g->yaw=.4f;g->grounded=true;g->hp=500;
    pe_sandbox_actor_reset_motion(a,g);
    /* Unarmed idle: one driven body draw, solved from the locomotion clip by name with the arms mapped. */
    ik_solves=0;CHECK(pe_sandbox_actor_ik_draws(a)==0);draw(a,g);CHECK(draw_count==1&&draws[0].driven&&ik_solves==1&&ik_arms_mapped);
    CHECK(pe_sandbox_actor_ik_draws(a)==1&&pe_sandbox_actor_ik_draws(NULL)==0);
    CHECK(ik_count==1&&!strcmp(ik_samples[0].clip,pe_player_locomotion_name(PE_LOCOMOTION_IDLE))&&ik_samples[0].weight==1);
    /* A moving blend passes every contributor with the drawn clocks and weights. */
    pe_sandbox_actor_facing_mode(a,true);g->player.x+=.5f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);
    CHECK(draws[0].blended&&draws[0].driven&&ik_count==draws[0].count&&ik_count>1);
    for(size_t i=0;i<ik_count&&i<draws[0].count;i++)
        CHECK(!strcmp(ik_samples[i].clip,clips[BODY][draws[0].samples[i].clip].name)&&ik_samples[i].seconds==draws[0].samples[i].seconds&&ik_samples[i].weight==draws[0].samples[i].weight);
    /* The inventory preview and a dead player stay baked, and nothing stays active after a draw. */
    ik_solves=0;draw_count=0;pe_sandbox_actor_preview(a,g);CHECK(draw_count==1&&!draws[0].driven&&ik_solves==0);
    g->hp=0;draw(a,g);CHECK(!draws[0].driven&&ik_solves==0);g->hp=500;
    /* A stowed knife keeps the arm mapping; a drawn one switches it off. Both scenes are driven together. */
    g->inventory.slots[0]=(PeSlot){.item_id=33,.instance_id=77,.count=1};g->equipped_instance_id=77;
    CHECK(pe_sandbox_actor_set_melee(a,77,false,NULL));draw(a,g);CHECK(draw_count==2&&draws[0].driven&&draws[1].driven&&draws[1].view==KNIFE&&ik_arms_mapped);
    CHECK(pe_sandbox_actor_set_melee(a,77,true,NULL));draw(a,g);CHECK(draw_count==2&&draws[0].driven&&draws[1].driven&&draws[1].view==KNIFE_HELD&&!ik_arms_mapped);
    /* An attack is solved from its own single clip and clock. */
    PeCombat combat={.active=true,.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,.variant=PE_COMBAT_STATIONARY,.stage=1,.clip_time=.25};
    CHECK(pe_sandbox_actor_set_melee(a,77,true,&combat));draw(a,g);
    CHECK(draws[0].driven&&draws[1].driven&&ik_count==1&&!strcmp(ik_samples[0].clip,"Attack7")&&ik_samples[0].seconds==.25&&!ik_arms_mapped);
    CHECK(pe_sandbox_actor_set_melee(a,77,true,NULL));
    /* A clip without a staged pose, or a failed solve, leaves the whole pose baked. */
    ik_solve_failure=true;draw(a,g);CHECK(draw_count==2&&!draws[0].driven&&!draws[1].driven&&pe_sandbox_actor_ok(a));ik_solve_failure=false;
    /* The held torch's emitter moves with the same solve as its mesh. */
    g->inventory.slots[0]=(PeSlot){.item_id=10,.instance_id=5,.count=1};g->equipped_instance_id=5;
    PeSandboxActionPose pose={.item_id=10,.instance_id=5,.held=true};CHECK(pe_sandbox_actor_set_action(a,&pose));
    PeVec3 light,baked;ik_light_moves=0;CHECK(pe_sandbox_actor_torch_light(a,g,&light)&&ik_light_moves==1);
    ik_solve_failure=true;CHECK(pe_sandbox_actor_torch_light(a,g,&baked)&&ik_light_moves==1&&fabsf(light.y-baked.y-.25f)<1e-5f);ik_solve_failure=false;
    pe_sandbox_actor_unload(a);CHECK(allocated==0&&ik_loaded==0);
    /* One drawn scene without drivers for its exact geometry keeps the whole pose baked. */
    ik_unbound_view=KNIFE_HELD;uploads=0;a=pe_sandbox_actor_load("assets",error,sizeof error);CHECK(a);
    if(a){
        g->inventory.slots[0]=(PeSlot){.item_id=33,.instance_id=77,.count=1};g->equipped_instance_id=77;pe_sandbox_actor_reset_motion(a,g);
        CHECK(pe_sandbox_actor_set_melee(a,77,true,NULL));ik_solves=0;draw(a,g);CHECK(draw_count==2&&!draws[0].driven&&!draws[1].driven&&ik_solves==0);
        CHECK(pe_sandbox_actor_set_melee(a,77,false,NULL));draw(a,g);CHECK(draw_count==2&&draws[0].driven&&draws[1].driven);
        pe_sandbox_actor_unload(a);
    }
    CHECK(allocated==0&&ik_loaded==0);free(g);ik_unbound_view=-1;ik_staged=false;uploads=0;init_clips();failures+=pose_failures;pose_failures=0;
    return failures;
}
int main(void) {
    int failures=0;char error[256];init_clips();failures+=ik_actor_tests();failures+=reaction_actor_tests();failures+=running_actor_tests();failures+=sprint_actor_tests();failures+=sword_actor_tests();
    window=false;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0);window=true;
    missing_clip=true;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);missing_clip=false;
    for(missing_direction_view=0;missing_direction_view<DIG_BODY;missing_direction_view++){
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
        CHECK(strstr(error,"NormalWL")!=NULL);
    }
    missing_direction_view=-1;
    bad_clock_view=KNIFE;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);bad_clock_view=-1;
    bad_clock_view=DIG_TOOL;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);bad_clock_view=-1;
    bad_source_view=FELL_BODY;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);bad_source_view=-1;
    missing_view=WOOD_FELL_TOOL;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);missing_view=-1;
    missing_view=DIG_TOOL;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);missing_view=-1;
    missing_view=TORCH_STOWED;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);missing_view=-1;
    missing_view=TORCH_HELD;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);missing_view=-1;
    bad_clock_view=TORCH_HELD;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);bad_clock_view=-1;
    missing_view=KNIFE;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);missing_view=-1;
    for(unsigned field=0;field<4;field++){
        if(field==0)clips[BODY][PE_LOCOMOTION_COUNT].name[0]='X';
        if(field==1)clips[KNIFE_HELD][PE_LOCOMOTION_COUNT+1].duration+=.1f;
        if(field==2)clips[KNIFE_HELD][PE_LOCOMOTION_COUNT+2].loop=true;
        if(field==3){clips[BODY][PE_LOCOMOTION_COUNT+3].frames++;clips[KNIFE_HELD][PE_LOCOMOTION_COUNT+3].frames++;}
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);init_clips();
    }
    /* Every nonempty incomplete pickaxe group must fail before GPU upload,
     * including a lone stowed scene. An entirely absent group stays optional. */
    for(pickaxe_mask=1;pickaxe_mask<15;pickaxe_mask++){
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
    }
    for(int v=PICKAXE_STOWED;v<VIEWS;v++)if(v==PICKAXE_STOWED||v==PICKAXE_HELD||v==MINE_BODY||v==MINE_TOOL){
        bad_clock_view=v;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
    }
    bad_clock_view=-1;bad_source_view=MINE_TOOL;
    CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);bad_source_view=-1;
    wrong_count_view=MINE_BODY;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);wrong_count_view=-1;
    for(int field=0;field<4;field++){
        if(field==0)clips[MINE_BODY][1].frames=50; /* Reject male Mine2. */
        if(field==1)clips[MINE_TOOL][1].duration=1.6000022888183594f;
        if(field==2)clips[MINE_TOOL][0].loop=true;
        if(field==3)snprintf(clips[MINE_BODY][1].name,sizeof clips[MINE_BODY][1].name,"Spade2");
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);init_clips();
    }
    fail_upload=7;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0);fail_upload=0;uploads=0;
    fail_upload=LEGACY_VIEWS;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0);fail_upload=0;uploads=0;
    PeSandboxActor *a=pe_sandbox_actor_load("assets",error,sizeof error);CHECK(a&&allocated==LEGACY_VIEWS&&uploads==LEGACY_VIEWS&&pe_sandbox_actor_ok(a));
    CHECK(pe_sandbox_actor_action_ready(a,12)&&pe_sandbox_actor_action_ready(a,1527));
    CHECK(pe_sandbox_actor_action_ready(a,19)&&pe_sandbox_actor_action_ready(a,26));
    CHECK(pe_sandbox_actor_hold_ready(a,10)&&!pe_sandbox_actor_action_ready(a,10));
    CHECK(pe_sandbox_actor_hold_ready(a,33)&&pe_sandbox_actor_action_ready(a,33)&&!pe_sandbox_actor_running_ready(a));
    const PeActor *borrowed_body=NULL,*borrowed_knife=NULL;
    CHECK(pe_sandbox_actor_melee_assets(a,&borrowed_body,&borrowed_knife));
    CHECK((uintptr_t)borrowed_body==BODY+1&&(uintptr_t)borrowed_knife==KNIFE_HELD+1);
    PeGame *g=calloc(1,sizeof *g);if(!a||!g){pe_sandbox_actor_unload(a);free(g);return failures+1;}
    PeSandboxBodyPose snapshot={.clip="sentinel",.seconds=99,.duration=42,.loop=true},unchanged=snapshot;
    CHECK(!pe_sandbox_actor_body_pose(NULL,g,&snapshot)&&!memcmp(&snapshot,&unchanged,sizeof snapshot));
    CHECK(!pe_sandbox_actor_body_pose(a,NULL,&snapshot)&&!memcmp(&snapshot,&unchanged,sizeof snapshot));
    CHECK(!pe_sandbox_actor_body_pose(a,g,NULL));
    g->yaw=NAN;CHECK(!pe_sandbox_actor_body_pose(a,g,&snapshot)&&!memcmp(&snapshot,&unchanged,sizeof snapshot));g->yaw=0;
    g->player=(PeVec3){20,10,20};g->grounded=true;g->yaw=.5f;
    pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draw_count==1&&draws[0].clip==PE_LOCOMOTION_IDLE);
    CHECK(fabsf(draws[0].yaw-(180-.5f*RAD2DEG))<.001f&&draws[0].position.x==20);
    g->velocity.x=5;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_IDLE);
    g->player.x+=.2f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_WALK_LEFT);
    g->player.x+=.5f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_RUN_LEFT);
    g->player.x-=.5f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_RUN_RIGHT);
    g->yaw=0;g->player.z+=.5f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_RUN_BACK);
    g->player.z-=.5f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_RUN);
    g->yaw=PI/2;g->player.z+=.2f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_WALK_LEFT);
    g->inventory.slots[0]=(PeSlot){.item_id=33,.count=1,.instance_id=1};g->equipped_instance_id=1;draw(a,g);
    CHECK(draw_count==2&&draws[1].view==KNIFE&&draws[1].clip==draws[0].clip&&draws[1].seconds==draws[0].seconds&&draws[1].yaw==draws[0].yaw);
    PeSandboxBodyPose lateral_before,lateral_after;
    CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_before));
    draw_count=0;pe_sandbox_actor_preview(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_IDLE);
    CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_after));
    CHECK(!strcmp(lateral_before.clip,"NormalWL")&&same_pose(&lateral_before,&lateral_after));
    /* Blended clocks are independent of the diagnostic selected clock. A
     * reversal must retain both directions and use identical weighted clocks
     * for the body, stowed attachment and read-only defence-facing snapshot. */
    PeVec3 prior_position=g->player,prior_velocity=g->velocity;float prior_yaw=g->yaw;
    g->yaw=0;g->velocity=(PeVec3){0};pe_sandbox_actor_reset_motion(a,g);
    g->player.x+=.25f;pe_sandbox_actor_update(a,.05f,g);
    g->player.x+=.25f;pe_sandbox_actor_update(a,.05f,g);
    g->player.x-=.25f;pe_sandbox_actor_update(a,.05f,g);draw(a,g);
    CHECK(draws[0].blended&&draws[1].blended&&draws[0].count>=3&&draws[0].count==draws[1].count);
    CHECK(same_samples(draws[0].samples,draws[1].samples,draws[0].count));
    CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_before));
    bool left_contributes=false,right_contributes=false,separate_clock=false;
    for(size_t i=0;i<lateral_before.blend_count;i++){
        if(!strcmp(lateral_before.blend[i].clip,"NormalRL"))left_contributes=lateral_before.blend[i].weight>0;
        if(!strcmp(lateral_before.blend[i].clip,"NormalRR")){
            right_contributes=lateral_before.blend[i].weight>0;
            separate_clock=lateral_before.blend[i].seconds!=lateral_before.seconds;
        }
    }
    CHECK(left_contributes&&right_contributes&&separate_clock);
    draw_count=0;pe_sandbox_actor_preview(a,g);
    CHECK(draw_count==2&&!draws[0].blended&&!draws[1].blended&&draws[0].clip==PE_LOCOMOTION_IDLE&&draws[0].yaw==0);
    CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_after)&&same_pose(&lateral_before,&lateral_after));
    /* Free-pointer camera orbit cannot drag an idle body. Accepted movement
     * turns that body; stopping retains heading despite velocity hints. */
    pe_sandbox_actor_reset_motion(a,g);pe_sandbox_actor_facing_mode(a,true);
    float body_yaw=pe_sandbox_actor_yaw(a,g);g->yaw=1.1f;
    pe_sandbox_actor_update(a,.05f,g);CHECK(same_angle(pe_sandbox_actor_yaw(a,g),body_yaw));
    g->player.x+=.25f;pe_sandbox_actor_update(a,.05f,g);
    float turned=pe_sandbox_actor_yaw(a,g);
    CHECK(!same_angle(turned,body_yaw)&&fabsf(remainderf(turned-90,360))<fabsf(remainderf(body_yaw-90,360)));
    CHECK(!same_angle(turned,180-g->yaw*RAD2DEG));draw(a,g);
    CHECK(same_angle(draws[0].yaw,turned)&&same_angle(draws[1].yaw,turned));
    CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_before)&&same_angle(lateral_before.yaw,turned));
    g->yaw=-.7f;g->velocity.x=99;pe_sandbox_actor_update(a,.05f,g);
    CHECK(same_angle(pe_sandbox_actor_yaw(a,g),turned));
    CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_before));
    pe_sandbox_actor_update(a,0,g);CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_after)&&same_pose(&lateral_before,&lateral_after));
    /* Explicit action lock freezes body heading while the world position and
     * camera change. It is independent of the free/captured mode preference. */
    pe_sandbox_actor_facing_lock(a,true);g->player.z+=.25f;g->yaw=2;
    pe_sandbox_actor_update(a,.05f,g);CHECK(same_angle(pe_sandbox_actor_yaw(a,g),turned));
    pe_sandbox_actor_facing_mode(a,false);pe_sandbox_actor_update(a,.05f,g);
    CHECK(same_angle(pe_sandbox_actor_yaw(a,g),turned));
    pe_sandbox_actor_facing_lock(a,false);pe_sandbox_actor_update(a,.05f,g);
    CHECK(same_angle(pe_sandbox_actor_yaw(a,g),180-g->yaw*RAD2DEG));
    /* Melee heading is shared by visible body/attachment/defence snapshot;
     * it cannot rotate the camera or escape the active action lock. */
    float camera_before=g->yaw;
    CHECK(pe_sandbox_actor_face_yaw(a,407));
    pe_sandbox_actor_facing_lock(a,true);pe_sandbox_actor_update(a,.05f,g);draw(a,g);
    CHECK(same_angle(draws[0].yaw,47)&&same_angle(draws[1].yaw,47)&&g->yaw==camera_before);
    CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_before)&&same_angle(lateral_before.yaw,47));
    CHECK(!pe_sandbox_actor_face_yaw(a,NAN)&&same_angle(pe_sandbox_actor_yaw(a,g),47));
    CHECK(!pe_sandbox_actor_face_yaw(NULL,0));pe_sandbox_actor_facing_lock(a,false);
    /* Explicit face-camera is available to align an authoritative action. */
    pe_sandbox_actor_facing_mode(a,true);g->yaw=.4f;pe_sandbox_actor_face_camera(a,g);
    CHECK(same_angle(pe_sandbox_actor_yaw(a,g),180-g->yaw*RAD2DEG));
    /* Load/reset snaps all transient weights to idle, seeds the new position,
     * restores camera heading and clears a stale action-facing lock. */
    pe_sandbox_actor_facing_lock(a,true);g->player=(PeVec3){200,30,200};g->yaw=0;g->velocity=(PeVec3){0};
    pe_sandbox_actor_reset_motion(a,g);CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_before));
    CHECK(lateral_before.blend_count==1&&!strcmp(lateral_before.blend[0].clip,"NormalIdle")&&lateral_before.blend[0].weight==1&&lateral_before.blend[0].seconds==0);
    CHECK(same_angle(lateral_before.yaw,180));g->player.x+=.25f;pe_sandbox_actor_update(a,.05f,g);
    CHECK(!same_angle(pe_sandbox_actor_yaw(a,g),180));
    /* Restore the captured-camera behavior and the original fixture fields;
     * subsequent action/instance assertions remain independent of this test. */
    g->player=prior_position;g->velocity=prior_velocity;g->yaw=prior_yaw;
    pe_sandbox_actor_facing_mode(a,false);pe_sandbox_actor_reset_motion(a,g);
    CHECK(same_angle(pe_sandbox_actor_yaw(a,g),180-prior_yaw*RAD2DEG));
    /* Knife uses precisely the simulation snapshot, even when preview,
     * locomotion updates and pause redraws interleave with active combat. */
    g->hp=100;
    CHECK(pe_sandbox_actor_set_melee(a,1,true,NULL));draw(a,g);
    CHECK(draws[0].view==BODY&&draws[1].view==KNIFE_HELD&&draws[0].seconds==draws[1].seconds);
    PeCombat combat={.active=true,.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,.clip_time=.123456789};
    for(unsigned stage=0;stage<4;stage++){
        combat.stage=stage;CHECK(pe_sandbox_actor_set_melee(a,1,true,&combat));draw(a,g);
        CHECK(draws[0].clip==PE_LOCOMOTION_COUNT+stage&&draws[1].clip==PE_LOCOMOTION_COUNT+stage);
        CHECK(draws[0].seconds==combat.clip_time&&draws[1].seconds==combat.clip_time&&draws[1].view==KNIFE_HELD);
        pe_sandbox_actor_update(a,.2f,g);draw(a,g);CHECK(draws[0].seconds==combat.clip_time);
        pe_sandbox_actor_update(a,0,g);draw(a,g);CHECK(draws[0].seconds==combat.clip_time);
    }
    combat.variant=PE_COMBAT_RUNNING;CHECK(!pe_sandbox_actor_set_melee(a,1,true,&combat));
    draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_COUNT+3&&draws[0].seconds==combat.clip_time);
    combat.variant=PE_COMBAT_STATIONARY;
    draw_count=0;pe_sandbox_actor_preview(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_IDLE&&draws[1].view==KNIFE_HELD&&draws[0].seconds==draws[1].seconds);
    draw(a,g);CHECK(draws[0].seconds==combat.clip_time&&draws[0].clip==PE_LOCOMOTION_COUNT+3);
    /* Reset/load also clears a bound active action; the host must explicitly
     * bind its new authoritative equipment/action snapshot after loading. */
    pe_sandbox_actor_reset_motion(a,g);draw(a,g);
    CHECK(draws[0].blended&&draws[0].clip==PE_LOCOMOTION_IDLE&&draws[1].view==KNIFE);
    CHECK(pe_sandbox_actor_set_melee(a,1,true,&combat));draw(a,g);
    CHECK(!draws[0].blended&&draws[0].seconds==combat.clip_time&&draws[1].view==KNIFE_HELD);
    combat.clip_time=NAN;CHECK(!pe_sandbox_actor_set_melee(a,1,true,&combat));draw(a,g);CHECK(draws[0].seconds==.123456789);
    combat.clip_time=2;CHECK(!pe_sandbox_actor_set_melee(a,1,true,&combat));combat.clip_time=.123456789;
    combat.gender=PE_COMBAT_MALE;CHECK(!pe_sandbox_actor_set_melee(a,1,true,&combat));combat.gender=PE_COMBAT_FEMALE;
    CHECK(!pe_sandbox_actor_set_melee(a,0,true,&combat));CHECK(!pe_sandbox_actor_set_melee(a,1,false,&combat));
    g->inventory.slots[0].instance_id=2;g->equipped_instance_id=2;draw(a,g);CHECK(draws[0].clip<PE_LOCOMOTION_COUNT&&draws[1].view==KNIFE);
    g->inventory.slots[0].instance_id=1;g->equipped_instance_id=1;g->hp=0;draw(a,g);CHECK(draws[0].clip<PE_LOCOMOTION_COUNT&&draws[1].view==KNIFE);g->hp=100;
    combat.active=false;CHECK(pe_sandbox_actor_set_melee(a,1,true,&combat));draw(a,g);CHECK(draws[0].clip<PE_LOCOMOTION_COUNT&&draws[1].view==KNIFE_HELD);
    /* Natural completion carries a display-only tail into the SAME body,
     * held attachment and defence-facing snapshot, including while walking. */
    g->grounded=true;combat.active=true;combat.stage=0;combat.event_index=4;combat.clip_time=.69;
    CHECK(pe_sandbox_actor_set_melee(a,1,true,&combat));
    combat.active=false;combat.event_index=5;combat.clip_time=.71;
    PeCombat mechanical=combat;
    CHECK(pe_sandbox_actor_set_melee(a,1,true,&combat));draw(a,g);
    CHECK(draws[0].blended&&draws[1].blended&&same_samples(draws[0].samples,draws[1].samples,draws[0].count));
    CHECK(draws[0].samples[draws[0].count-1].weight==1);
    CHECK(draws[0].samples[draws[0].count-1].clip==PE_LOCOMOTION_COUNT);
    PeSandboxBodyPose recovery_before,recovery_after;
    CHECK(pe_sandbox_actor_body_pose(a,g,&recovery_before));
    pe_sandbox_actor_update(a,0,g);CHECK(pe_sandbox_actor_body_pose(a,g,&recovery_after)&&same_pose(&recovery_before,&recovery_after));
    g->player.z+=.15f;pe_sandbox_actor_update(a,.05f,g);
    CHECK(pe_sandbox_actor_set_melee(a,1,true,&combat));draw(a,g);
    CHECK(draws[0].samples[draws[0].count-1].weight>0&&draws[0].samples[draws[0].count-1].weight<1);
    CHECK(fabs(draws[0].samples[draws[0].count-1].seconds-.76)<1e-6);
    CHECK(pe_sandbox_actor_body_pose(a,g,&recovery_after)&&recovery_after.blend_count==draws[0].count);
    CHECK(!memcmp(&combat,&mechanical,sizeof combat));
    g->inventory.slots[0].instance_id=2;g->equipped_instance_id=2;draw(a,g);
    for(size_t i=0;i<draws[0].count;i++)CHECK(draws[0].samples[i].clip<PE_LOCOMOTION_COUNT);
    g->inventory.slots[0].instance_id=1;g->equipped_instance_id=1;
    pe_sandbox_actor_update(a,.1f,g);draw(a,g);
    for(size_t i=0;i<draws[0].count;i++)CHECK(draws[0].samples[i].clip<PE_LOCOMOTION_COUNT);
    combat.active=true;combat.event_index=4;CHECK(pe_sandbox_actor_set_melee(a,1,true,&combat));
    combat.active=false;combat.event_index=5;CHECK(pe_sandbox_actor_set_melee(a,1,true,&combat));
    CHECK(pe_sandbox_actor_set_action(a,NULL));draw(a,g);
    for(size_t i=0;i<draws[0].count;i++)CHECK(draws[0].samples[i].clip<PE_LOCOMOTION_COUNT);
    CHECK(pe_sandbox_actor_set_melee(a,1,false,NULL));draw(a,g);CHECK(draws[0].clip<PE_LOCOMOTION_COUNT&&draws[1].view==KNIFE);
    combat.active=true;CHECK(pe_sandbox_actor_set_melee(a,1,true,&combat));CHECK(pe_sandbox_actor_set_action(a,NULL));draw(a,g);CHECK(draws[0].clip<PE_LOCOMOTION_COUNT&&draws[1].view==KNIFE);
    g->inventory.slots[0].item_id=12;draw(a,g);CHECK(draw_count==2&&draws[1].view==SHOVEL_STOWED);
    PeSandboxActionPose pose={.item_id=12,.instance_id=1,.held=true};
    CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].view==BODY&&draws[1].view==SHOVEL_HELD&&draws[0].seconds==draws[1].seconds);
    for(int phase=0;phase<3;phase++) {
        pose.clip=phase==0?"Spade1":phase==1?"Spade2":"Spade3";pose.seconds=.123456789;
        CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
        CHECK(draw_count==2&&draws[0].view==DIG_BODY&&draws[1].view==DIG_TOOL);
        CHECK(draws[0].clip==(size_t)phase&&draws[1].clip==(size_t)phase&&draws[0].seconds==pose.seconds&&draws[1].seconds==pose.seconds);
        CHECK(draws[0].position.x==draws[1].position.x&&draws[0].yaw==draws[1].yaw);
    }
    /* Neither locomotion time, redraws, nor pause can advance gameplay's clock. */
    pe_sandbox_actor_update(a,.2f,g);draw(a,g);CHECK(draws[0].seconds==pose.seconds);
    pe_sandbox_actor_update(a,0,g);draw(a,g);CHECK(draws[0].seconds==pose.seconds);
    pose.seconds=NAN;CHECK(!pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].seconds==.123456789);
    pose.seconds=5;CHECK(!pe_sandbox_actor_set_action(a,&pose));pose.seconds=.2;pose.clip="Pickaxe";CHECK(!pe_sandbox_actor_set_action(a,&pose));
    pose.clip=NULL;CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].view==BODY&&draws[1].view==SHOVEL_HELD);
    pose.held=false;CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[1].view==SHOVEL_STOWED);
    pose.held=true;pose.clip="Spade2";pose.seconds=.6035147905349731;CHECK(pe_sandbox_actor_set_action(a,&pose));
    g->inventory.slots[0].instance_id=2;g->equipped_instance_id=2;draw(a,g);
    CHECK(draws[0].view==BODY&&draws[1].view==SHOVEL_STOWED); /* no stale bound-instance action */
    pose=(PeSandboxActionPose){.item_id=1527,.instance_id=2,.held=true,.clip="Felling2",.seconds=.612334668636322};
    g->inventory.slots[0].item_id=1527;CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
    CHECK(draws[0].view==FELL_BODY&&draws[1].view==FELL_TOOL&&draws[0].clip==1&&draws[1].clip==1);
    CHECK(draws[0].seconds==pose.seconds&&draws[1].seconds==pose.seconds);
    for(int phase=0;phase<3;phase++){
        const char *names[]={"Felling1","Felling2","Felling3"};pose.clip=names[phase];pose.seconds=.125;
        CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].view==FELL_BODY&&draws[0].clip==(size_t)phase);
    }
    pose.clip="Felling2";pose.seconds=.612334668636322;CHECK(pe_sandbox_actor_set_action(a,&pose));
    draw_count=0;pe_sandbox_actor_preview(a,g);CHECK(mode_depth==0&&draws[0].view==BODY&&draws[1].view==AXE_HELD&&draws[0].clip==PE_LOCOMOTION_IDLE&&draws[0].position.x==0&&draws[0].yaw==0);
    draw(a,g);CHECK(draws[0].view==FELL_BODY&&draws[0].seconds==pose.seconds);
    CHECK(pe_sandbox_actor_set_action(a,NULL));draw(a,g);CHECK(draw_count==1&&draws[0].view==BODY); /* no invented axe stow */
    g->inventory.slots[0].item_id=26;pose.item_id=26;
    CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
    CHECK(draw_count==2&&draws[0].view==FELL_BODY&&draws[1].view==WOOD_FELL_TOOL);
    CHECK(draws[0].seconds==pose.seconds&&draws[1].seconds==pose.seconds);
    pose.clip=NULL;CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
    CHECK(draws[0].view==BODY&&draws[1].view==WOOD_AXE_HELD);
    CHECK(pe_sandbox_actor_set_action(a,NULL));draw(a,g);CHECK(draw_count==1);
    g->inventory.slots[0].item_id=19;draw(a,g);CHECK(draw_count==2&&draws[1].view==PICKAXE_STOWED);
    pose=(PeSandboxActionPose){.item_id=19,.instance_id=2,.held=true};
    CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
    CHECK(draw_count==2&&draws[0].view==BODY&&draws[1].view==PICKAXE_HELD&&draws[0].seconds==draws[1].seconds);
    const char *mine_names[]={"Mine1","Mine2","Mine3"};
    for(int phase=0;phase<3;phase++){
        pose.clip=mine_names[phase];pose.seconds=phase==1?5.123456789:.123456789;
        CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
        CHECK(draw_count==2&&draws[0].view==MINE_BODY&&draws[1].view==MINE_TOOL);
        CHECK(draws[0].clip==(size_t)phase&&draws[1].clip==(size_t)phase&&draws[0].seconds==pose.seconds&&draws[1].seconds==pose.seconds);
        CHECK(draws[0].yaw==draws[1].yaw&&draws[0].position.x==draws[1].position.x);
        pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].seconds==pose.seconds);
        pe_sandbox_actor_update(a,0,g);draw(a,g);CHECK(draws[0].seconds==pose.seconds);
    }
    pose.clip="Spade3";CHECK(!pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].view==MINE_BODY&&draws[0].seconds==.123456789);
    pose.clip="Mine3";pose.seconds=.8333358764648438;
    CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].seconds==pose.seconds);
    pose.seconds+=.01;CHECK(!pe_sandbox_actor_set_action(a,&pose));
    draw_count=0;pe_sandbox_actor_preview(a,g);
    CHECK(mode_depth==0&&draws[0].view==BODY&&draws[1].view==PICKAXE_HELD&&draws[0].clip==PE_LOCOMOTION_IDLE&&draws[0].seconds==draws[1].seconds);
    draw(a,g);CHECK(draws[0].view==MINE_BODY&&draws[0].seconds==.8333358764648438);
    g->inventory.slots[0].instance_id=3;g->equipped_instance_id=3;draw(a,g);
    CHECK(draws[0].view==BODY&&draws[1].view==PICKAXE_STOWED); /* Replaced item19 instance is not held. */
    g->equipped_instance_id=0;draw(a,g);CHECK(draw_count==1&&draws[0].view==BODY);
    g->equipped_instance_id=3;CHECK(pe_sandbox_actor_set_action(a,NULL));draw(a,g);CHECK(draws[1].view==PICKAXE_STOWED);
    double before=draws[0].seconds;pe_sandbox_actor_update(a,0,g);draw(a,g);CHECK(draws[0].seconds==before);
    g->grounded=false;g->jumping=true;g->velocity.y=7;g->player.y+=.1f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_JUMP&&draws[0].seconds==0);
    pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].seconds>.09);
    pe_sandbox_actor_update(a,.9f,g);pe_sandbox_actor_update(a,.9f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_FALL);
    g->grounded=true;g->jumping=false;g->velocity=(PeVec3){0};pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_LAND);
    g->player=(PeVec3){40,15,40};g->grounded=true;g->jumping=false;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_IDLE);
    before=draws[0].seconds;pe_sandbox_actor_update(a,NAN,g);draw(a,g);CHECK(draws[0].seconds==before);
    /* Torch holding selects only rigid source attachments, never an attack.
     * Its emitter follows the same selected clip/time and root yaw as its mesh. */
    g->hp=500;g->inventory.slots[0].item_id=10;
    pose=(PeSandboxActionPose){.item_id=10,.instance_id=3,.held=false};
    CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[1].view==TORCH_STOWED);
    PeVec3 light={9,8,7},sentinel=light;CHECK(!pe_sandbox_actor_torch_light(a,g,&light));CHECK(!memcmp(&light,&sentinel,sizeof light));
    pose.held=true;CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[1].view==TORCH_HELD);
    CHECK(draws[1].clip==draws[0].clip&&draws[1].seconds==draws[0].seconds);
    CHECK(pe_sandbox_actor_torch_light(a,g,&light));CHECK(light_clip==draws[1].clip&&light_seconds==draws[1].seconds);
    float c=cosf(PI-g->yaw),sn=sinf(PI-g->yaw);
    CHECK(fabsf(light.x-(g->player.x+c*.5f+sn*PE_TORCH_LIGHT_POSITION_Z))<.00001f&&light.y==g->player.y+1);
    CHECK(fabsf(light.z-(g->player.z-sn*.5f+c*PE_TORCH_LIGHT_POSITION_Z))<.00001f);
    PeVec3 flame;CHECK(pe_sandbox_actor_torch_flame(a,g,&flame));
    CHECK(fabsf(flame.x-(g->player.x+c*.5f+sn*PE_TORCH_FLAME_POSITION_Z))<.00001f);
    CHECK(fabsf(flame.z-(g->player.z-sn*.5f+c*PE_TORCH_FLAME_POSITION_Z))<.00001f&&flame.y==light.y);
    /* Torch joint queries use all actual blended contributors, including
     * outgoing directions, and the independently turned body root. */
    pe_sandbox_actor_facing_mode(a,true);g->yaw=0;pe_sandbox_actor_reset_motion(a,g);
    draw(a,g);CHECK(draws[1].view==TORCH_STOWED); /* Load/reset clears stale held instance. */
    CHECK(!pe_sandbox_actor_torch_light(a,g,&light));CHECK(pe_sandbox_actor_set_action(a,&pose));
    g->player.x+=.5f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);
    CHECK(draws[1].blended&&draws[1].count>1&&same_samples(draws[0].samples,draws[1].samples,draws[1].count));
    CHECK(pe_sandbox_actor_torch_light(a,g,&light));
    CHECK(light_count==draws[1].count&&same_samples(light_samples,draws[1].samples,light_count));
    c=cosf(pe_sandbox_actor_yaw(a,g)*DEG2RAD);sn=sinf(pe_sandbox_actor_yaw(a,g)*DEG2RAD);
    CHECK(!same_angle(pe_sandbox_actor_yaw(a,g),180-g->yaw*RAD2DEG));
    CHECK(fabsf(light.x-(g->player.x+c*.5f+sn*PE_TORCH_LIGHT_POSITION_Z))<.00001f);
    CHECK(fabsf(light.z-(g->player.z-sn*.5f+c*PE_TORCH_LIGHT_POSITION_Z))<.00001f);
    CHECK(pe_sandbox_actor_torch_flame(a,g,&flame));
    CHECK(light_count==draws[1].count&&same_samples(light_samples,draws[1].samples,light_count));
    CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_before));
    draw_count=0;pe_sandbox_actor_preview(a,g);CHECK(!draws[0].blended&&!draws[1].blended);
    CHECK(pe_sandbox_actor_body_pose(a,g,&lateral_after)&&same_pose(&lateral_before,&lateral_after));
    pe_sandbox_actor_facing_mode(a,false);pe_sandbox_actor_face_camera(a,g);
    pose.clip="Attack6";CHECK(!pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].view==BODY&&draws[1].view==TORCH_HELD);
    light_pose_failure=true;sentinel=light;CHECK(!pe_sandbox_actor_torch_light(a,g,&light)&&!memcmp(&light,&sentinel,sizeof light));light_pose_failure=false;
    g->hp=0;CHECK(!pe_sandbox_actor_torch_light(a,g,&light));g->hp=500;
    g->inventory.slots[0].instance_id=4;g->equipped_instance_id=4;CHECK(!pe_sandbox_actor_torch_light(a,g,&light));draw(a,g);CHECK(draws[1].view==TORCH_STOWED);
    g->inventory.slots[0].item_id=19;g->inventory.slots[0].instance_id=3;g->equipped_instance_id=3;CHECK(pe_sandbox_actor_set_action(a,NULL));
    draw_failure=true;draw(a,g);CHECK(!pe_sandbox_actor_ok(a));draw_failure=false;draw(a,g);CHECK(draw_count==0);
    CHECK(!pe_sandbox_actor_body_pose(a,g,&snapshot)&&!memcmp(&snapshot,&unchanged,sizeof snapshot));
    pe_sandbox_actor_unload(a);CHECK(allocated==0);
    pickaxe_mask=0;uploads=0;a=pe_sandbox_actor_load("assets",error,sizeof error);
    CHECK(a&&allocated==LEGACY_VIEWS-4&&!pe_sandbox_actor_action_ready(a,19)&&pe_sandbox_actor_action_ready(a,12));
    pose=(PeSandboxActionPose){.item_id=19,.instance_id=3,.held=true,.clip="Mine2",.seconds=.2};
    CHECK(!pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draw_count==1&&draws[0].view==BODY);
    pe_sandbox_actor_unload(a);CHECK(allocated==0);pickaxe_mask=15;
    missing_view=KNIFE_HELD;uploads=0;a=pe_sandbox_actor_load("assets",error,sizeof error);
    CHECK(a&&allocated==LEGACY_VIEWS-1&&!pe_sandbox_actor_hold_ready(a,33));
    borrowed_body=(PeActor*)(uintptr_t)123;borrowed_knife=(PeActor*)(uintptr_t)456;
    CHECK(!pe_sandbox_actor_melee_assets(a,&borrowed_body,&borrowed_knife));CHECK((uintptr_t)borrowed_body==123&&(uintptr_t)borrowed_knife==456);
    CHECK(!pe_sandbox_actor_set_melee(a,3,true,NULL));
    pe_sandbox_actor_unload(a);CHECK(allocated==0);missing_view=-1;
    optional=false;a=pe_sandbox_actor_load("assets",error,sizeof error);CHECK(a&&allocated==1);
    CHECK(!pe_sandbox_actor_action_ready(a,12)&&!pe_sandbox_actor_action_ready(a,1527)&&!pe_sandbox_actor_action_ready(a,19));
    CHECK(!pe_sandbox_actor_hold_ready(a,10)&&!pe_sandbox_actor_hold_ready(a,33));
    pe_sandbox_actor_unload(a);CHECK(allocated==0);free(g);
    failures+=pose_failures;
    if(!failures)puts("sandbox actor: shared locomotion blend/query, body facing, exact action clocks, instance/reset binding and cleanup passed");
    return failures;
}
