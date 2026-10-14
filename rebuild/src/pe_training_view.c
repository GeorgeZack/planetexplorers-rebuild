#include "pe_training_view.h"
#include "pe_reference.h"
#include "pe_training.h"
#include "pe_player_locomotion.h"
#include "pe_training_melee.h"
#include "pe_melee_facing.h"
#include "pe_ui_settings.h"
#include "pe_training_tree_hit.h"
#include "pe_training_terrain_view.h"
#include "pe_training_build.h"
#include "pe_training_npc_view.h"
#include "pe_training_ui.h"
#include "pe_ui_input.h"
#include "pe_training_text.h"
#include "pe_actor_view.h"
#include "pe_actor_scene_view.h"
#include "pe_hare.h"
#include "pe_scene_collision.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include "pe_ik_drive.h"
#include "pe_halt_host.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

_Static_assert(PE_LOCOMOTION_COUNT<=PE_ACTOR_BLEND_MAX,"locomotion blend capacity");

static const Color accent_color = {228, 191, 107, 255};
static Vector3 vector(PeVec3 p) { return (Vector3){p.x, p.y, p.z}; }
static PeVec3 point(Vector3 p) { return (PeVec3){p.x, p.y, p.z}; }

static bool draw_locomotion(PeActorSceneView *view,const PePlayerLocomotion *locomotion,Vector3 position,float yaw){
    PePlayerLocomotionPose pose;if(!pe_player_locomotion_snapshot(locomotion,&pose))return false;
    PeActorBlendSample samples[PE_ACTOR_BLEND_MAX];size_t count=0;
    for(int i=0;i<PE_LOCOMOTION_COUNT;i++)if(pose.weights[i]>0)
        samples[count++]=(PeActorBlendSample){pe_actor_scene_view_find_clip(view,pe_player_locomotion_name(i)),pose.seconds[i],(float)pose.weights[i]};
    return pe_actor_scene_view_draw_blend(view,samples,count,position,yaw,1,WHITE);
}
static float movement_heading(float current,float target,float dt){
    float delta=remainderf(target-current,360);
    if(fabsf(delta)>150)delta=90;
    return remainderf(current+delta*fminf(1,6*dt),360);
}

static void draw_marker(const PeTrainingUi *ui,Vector3 position,const char *label,Camera3D camera) {
    Vector3 delta=Vector3Subtract(position,camera.position);
    if(Vector3DotProduct(delta,Vector3Subtract(camera.target,camera.position))<=0)return;
    Vector2 at=GetWorldToScreen(position,camera);
    if(at.x<20||at.x>GetScreenWidth()-20||at.y<80||at.y>GetScreenHeight()-90)return;
    Font font=pe_ui_font(ui->assets,false);float size=18*ui->layout.scale;
    Vector2 measure=MeasureTextEx(font,label,size,0),origin={at.x-measure.x*.5f,at.y};
    DrawTextEx(font,label,(Vector2){origin.x+ui->layout.scale,origin.y+ui->layout.scale},size,0,(Color){0,0,0,160});
    DrawTextEx(font,label,origin,size,0,(Color){236,246,255,255});
}

/* Source OverHead.NameLabel: UIFont26, .01 Namlb scale, .125 local
   elevation, and distance-clamped world billboard scaling. Head positions
   use the recovered model bounds while runtime attachment bones are absent. */
static void draw_name(const PeTrainingUi *ui,Vector3 head,const char *name,Camera3D camera) {
    Vector3 forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    Vector3 delta=Vector3Subtract(head,camera.position);
    float distance=Clamp(Vector3Length(delta),4,30),scale=distance/9;
    Vector3 position=Vector3Add(head,(Vector3){0,.04f*distance,0});
    position=Vector3Add(position,Vector3Scale(camera.up,.125f*scale));
    float depth=Vector3DotProduct(Vector3Subtract(position,camera.position),forward);
    if(depth<=0)return;
    float size=26*.01f*scale*GetScreenHeight()/(2*tanf(camera.fovy*.5f*DEG2RAD)*depth);
    Font font=pe_ui_font_name(ui->assets);Vector2 at=GetWorldToScreen(position,camera);
    Vector2 extent=MeasureTextEx(font,name,size,0);
    if(at.x+extent.x<0||at.x-extent.x>GetScreenWidth()||at.y+extent.y<0||at.y-extent.y>GetScreenHeight())return;
    DrawTextEx(font,name,(Vector2){at.x-extent.x*.5f,at.y-extent.y*.5f},size,0,WHITE);
}

static Camera3D camera_for(const PeTraining *training, const PeSceneCollision *scene,
                           float yaw, float pitch) {
    Vector3 direction = {sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch)};
    Vector3 feet = vector(training->position);
    if (training->camera_mode == 2) {
        Vector3 eye = Vector3Add(feet, (Vector3){0, 1.58f, 0});
        return (Camera3D){eye, Vector3Add(eye, direction), {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
    }
    Vector3 target = Vector3Add(feet, (Vector3){0, 1.25f, 0});
    Vector3 behind = Vector3Scale(direction, -1);
    float distance = 6.0f; /* PECamera_Start: Dist/DistWanted/DistLevel. */
    PeSceneHit obstruction = pe_scene_raycast(scene, point(target), point(behind), distance);
    if (obstruction.hit) distance = fmaxf(.15f, obstruction.distance - .15f);
    return (Camera3D){Vector3Add(target, Vector3Scale(behind, distance)), target,
                      {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
}

static void draw_avatar(const PeTraining *training) {
    if (training->camera_mode == 2) return;
    Vector3 feet = vector(training->position);
    /* Deliberate temporary avatar, not recovered character art or animation. */
    DrawCapsule(Vector3Add(feet, (Vector3){0, PE_TRAINING_RADIUS, 0}),
                Vector3Add(feet, (Vector3){0, PE_TRAINING_HEIGHT - PE_TRAINING_RADIUS, 0}), PE_TRAINING_RADIUS, 8, 6,
                (Color){89, 151, 172, 255});
}

static bool capture_training_frame(const char *path,const char *suffix) {
    char output[4096];int n=snprintf(output,sizeof output,"%s%s",path,suffix?suffix:"");
    if(n<0||(size_t)n>=sizeof output)return false;
    rlDrawRenderBatchActive();
    Image image=LoadImageFromScreen();bool saved=image.data&&ExportImage(image,output);
    UnloadImage(image);return saved;
}
typedef struct {
    PeTrainingBuild blocks;
    PeMeshData mesh;
    PeSceneCollision *collision;
    uint32_t rendered_revision;
    bool active,collision_receivers;
    PeTrainingReceiverResult result;
} TrainingBuildView;
typedef struct { const PeWorld *world;const PeTrainingBuild *blocks; } ReceiverContext;
/* Grounder ray adapter: the nearest of the room, the dug terrain, the built
 * blocks and, while enabled, the lesson tree: what the tutorial controller
 * stands on. Actors (AIPlayer) are not included. */
typedef struct { const PeSceneCollision *scene[3]; bool tree; } GroundScenes;
static bool training_ground_ray(void *context,PeVec3 origin,PeVec3 direction,float maximum,PeGrounderHit *out){
    const GroundScenes *ground=context;PeSceneHit best={0};
    if(!out)return false;
    for(int i=0;i<3;i++)if(ground->scene[i]){
        PeSceneHit hit=pe_scene_raycast(ground->scene[i],origin,direction,maximum);
        if(hit.hit&&(!best.hit||hit.distance<best.distance))best=hit;
    }
    /* The HoloTree root capsule is on Default, inside the grounder's mask, while its collider is enabled. */
    if(ground->tree){
        PeSceneHit hit=pe_training_tree_ray(origin,direction,maximum);
        if(hit.hit&&(!best.hit||hit.distance<best.distance))best=hit;
    }
    if(!best.hit)return false;
    if(best.normal.y<0)best.normal=(PeVec3){-best.normal.x,-best.normal.y,-best.normal.z};
    *out=(PeGrounderHit){best.position,best.normal,best.distance};return true;
}
static bool receiver_query(void *opaque,const PeTrainingReceiverRay *ray,PeTrainingReceiverHit *hit) {
    ReceiverContext *context=opaque;
    if(!pe_training_receiver_boxes(ray,hit))return false;
    PeHit terrain=pe_training_terrain_raycast(context->world,ray->origin,ray->direction,ray->range);
    PeB45Hit block={0};
    for(size_t i=0;i<context->blocks->count;i++) {
        const PeB45Cell *cell=&context->blocks->cells[i];
        if(pe_b45_contains(cell,ray->origin))continue; /* Unity containing-collider suppression. */
        PeB45Hit next;
        if(pe_b45_raycast(cell,ray->origin,ray->direction,ray->range,&next)&&(!block.hit||next.distance<block.distance))block=next;
    }
    /* Conservative exact-distance ties retain the receiver; PhysX tie order is unverified. */
    if(terrain.hit&&(!hit->hit||terrain.distance<hit->distance))*hit=(PeTrainingReceiverHit){true,0,terrain.distance};
    if(block.hit&&(!hit->hit||block.distance<hit->distance))*hit=(PeTrainingReceiverHit){true,0,block.distance};
    return true;
}
static bool update_build_view(TrainingBuildView *view) {
    if(view->collision&&view->rendered_revision==view->blocks.revision&&view->collision_receivers==view->active)return true;
    PeMeshData cells={0},receivers={0},combined={0};
    if(!pe_training_build_mesh(&view->blocks,&cells))return false;
    if(view->active&&!pe_training_receiver_mesh(&receivers)){pe_mesh_free(&cells);return false;}
    combined.count=cells.count+receivers.count;
    if(combined.count) {
        combined.vertices=malloc(combined.count*sizeof *combined.vertices);
        if(!combined.vertices){pe_mesh_free(&cells);pe_mesh_free(&receivers);return false;}
        if(cells.count)memcpy(combined.vertices,cells.vertices,cells.count*sizeof *cells.vertices);
        if(receivers.count)memcpy(combined.vertices+cells.count,receivers.vertices,receivers.count*sizeof *receivers.vertices);
    }
    char error[256];PeSceneCollision *collision=pe_scene_from_mesh(&combined,(PeVec3){0},error,sizeof error);
    pe_mesh_free(&combined);pe_mesh_free(&receivers);
    if(!collision){fprintf(stderr,"Construction collision: %s\n",error);pe_mesh_free(&cells);return false;}
    pe_scene_free(view->collision);pe_mesh_free(&view->mesh);view->mesh=cells;view->collision=collision;
    view->rendered_revision=view->blocks.revision;view->collision_receivers=view->active;return true;
}
static void draw_build_view(const TrainingBuildView *view) {
    for(size_t i=0;i<view->mesh.count;i+=3) {
        PeVec3 n=view->mesh.vertices[i].normal;
        float light=.59f+.41f*fmaxf(0,n.x*-.35f+n.y*.85f+n.z*.4f);
        Color color={(unsigned char)(145*light),(unsigned char)(111*light),(unsigned char)(81*light),255};
        DrawTriangle3D(vector(view->mesh.vertices[i].position),vector(view->mesh.vertices[i+1].position),vector(view->mesh.vertices[i+2].position),color);
    }
}
static bool matching_clocks(const PeActorSceneView *a,const PeActorSceneView *b,bool allow_running_suffix) {
    size_t count=pe_actor_scene_view_clip_count(a),other=pe_actor_scene_view_clip_count(b);
    /* Shared sandbox recovery appends exact weapon-only leaves. Tutorial
     * keeps every used prefix clock and never selects these extra poses. */
    static const char *suffix[]={"RunSwordAttack","AttackIdle","SwordWF","SwordRF","SwordSprint",
        "SwordJump","SwordFall","SwordLand","SwordRunJump","SwordRunFall","SwordRunLand","SwordWB","SwordRB"};
    /* The installed sprint extension is the 21-clip body plus SprintSwordAttack. */
    static const char *sprint_suffix[]={"RunSwordAttack","SprintSwordAttack"};
    if(allow_running_suffix&&count>other&&(other==20||other==21)&&
       (count==21||count==22||count==31||count==33)){
        for(size_t i=20;i<count;i++){
            const PeActorClip *extra=pe_actor_scene_view_clip_at(a,i);
            if(!extra||strcmp(extra->name,(count==22?sprint_suffix:suffix)[i-20]))return false;
        }
        count=other;
    }
    if(!count||count!=other)return false;
    for(size_t i=0;i<count;i++) {
        const PeActorClip *x=pe_actor_scene_view_clip_at(a,i),*y=pe_actor_scene_view_clip_at(b,i);
        if(!x||!y||strcmp(x->name,y->name)||x->fps!=y->fps||x->duration!=y->duration||
           x->frames!=y->frames||x->loop!=y->loop||x->terminal_at_duration!=y->terminal_at_duration)return false;
    }
    return true;
}
static void foliage_shader(PeReferenceModel *model,Shader shader) {
    for(int i=0;model->loaded&&i<model->model.materialCount;i++)model->model.materials[i].shader=shader;
}
static void draw_foliage(const PeReferenceModel *model,Shader shader,int location,float cutoff) {
    SetShaderValue(shader,location,&cutoff,SHADER_UNIFORM_FLOAT);
    pe_reference_draw(model);
}
static bool load_axe_views(const char *dir,PeActorSceneView *body,PeActorSceneView *axe,
                           PeActorSceneView *cut_body,PeActorSceneView *cut_axe,size_t cuts[3]) {
    const char *names[]={"player-axe","player-cut","player-axe-cut"};
    PeActorSceneView *views[]={axe,cut_body,cut_axe};bool exists[3];char paths[3][4096],error[256];
    for(unsigned i=0;i<3;i++) {
        int n=snprintf(paths[i],sizeof paths[i],"%s/%s/recording.peas",dir,names[i]);
        if(n<0||(size_t)n>=sizeof paths[i])return false;
        exists[i]=FileExists(paths[i]);
    }
    if((exists[1]!=exists[2])||((exists[1]||exists[2])&&!exists[0]))return false;
    for(unsigned i=0;i<3;i++)if(exists[i]&&!pe_actor_scene_view_load(views[i],paths[i],error,sizeof error))goto fail;
    if(exists[0]&&(!body->loaded||!matching_clocks(body,axe,true)))goto fail;
    if(exists[1]) {
        if(!matching_clocks(cut_body,cut_axe,false)||pe_actor_scene_view_clip_count(cut_body)!=3)goto fail;
        const char *clips[]={"Felling1","Felling2","Felling3"};
        for(unsigned i=0;i<3;i++) {
            cuts[i]=pe_actor_scene_view_find_clip(cut_body,clips[i]);
            if(!pe_actor_scene_view_clip_at(cut_body,cuts[i]))goto fail;
        }
    }
    return true;
fail:
    for(unsigned i=0;i<3;i++)pe_actor_scene_view_unload(views[i]);
    return false;
}
static bool load_shovel_views(const char *dir,PeActorSceneView *body,PeActorSceneView views[4]) {
    const char *names[]={"player-shovel","player-shovel-stowed","player-dig","player-shovel-dig"};
    char paths[4][4096],error[256];unsigned count=0;
    for(unsigned i=0;i<4;i++) {
        int n=snprintf(paths[i],sizeof paths[i],"%s/%s/recording.peas",dir,names[i]);
        if(n<0||(size_t)n>=sizeof paths[i])return false;
        count+=FileExists(paths[i]);
    }
    if(!count)return true;
    if(count!=4)return false;
    for(unsigned i=0;i<4;i++)if(!pe_actor_scene_view_load(&views[i],paths[i],error,sizeof error))goto fail;
    if(!matching_clocks(body,&views[0],true)||!matching_clocks(body,&views[1],true)||!matching_clocks(&views[2],&views[3],false)||
       pe_actor_scene_view_clip_count(&views[2])!=3)goto fail;
    for(unsigned i=0;i<3;i++) {
        const char *clips[]={"Spade1","Spade2","Spade3"};
        const double durations[]={.6999969482421875,1.3333358764648438,.8333358764648438};
        const unsigned frames[]={22,42,27};
        const PeActorClip *clip=pe_actor_scene_view_clip_at(&views[2],pe_actor_scene_view_find_clip(&views[2],clips[i]));
        if(!clip||fabs(clip->duration-durations[i])>1e-6||clip->fps!=30||clip->frames!=frames[i]||
           clip->loop!=(i==1)||!clip->terminal_at_duration)goto fail;
    }
    return true;
fail:
    for(unsigned i=0;i<4;i++)pe_actor_scene_view_unload(&views[i]);
    return false;
}

int pe_training_view(const char *asset_dir, int smoke_frames, const char *screenshot) {
    PeUiSettings settings;pe_ui_settings_init(&settings);
    char model_path[4096], collision_path[4096], kit_path[4096], knife_path[4096], actor_path[4096], actor_texture[4096], player_path[4096], idle_path[4096], stowed_path[4096], error[256];
    if (!asset_dir || !*asset_dir || smoke_frames < 0 || (screenshot && !smoke_frames)) {
        fprintf(stderr, "Invalid training view arguments\n");
        return 1;
    }
    int n = snprintf(model_path, sizeof(model_path), "%s/room.obj", asset_dir);
    int c = snprintf(collision_path, sizeof(collision_path), "%s/room-collision.obj", asset_dir);
    int p = snprintf(player_path, sizeof(player_path), "%s/player/recording.peas", asset_dir);
    int i = snprintf(idle_path, sizeof(idle_path), "%s/player-idle/recording.peas", asset_dir);
    int s = snprintf(stowed_path, sizeof(stowed_path), "%s/player-knife-stowed/knife-recording.peas", asset_dir);
    if (n < 0 || (size_t)n >= sizeof(model_path) || c < 0 || (size_t)c >= sizeof(collision_path) ||
        p < 0 || (size_t)p >= sizeof(player_path) || i < 0 || (size_t)i >= sizeof(idle_path) || s < 0 || (size_t)s >= sizeof(stowed_path)) {
        fprintf(stderr, "Training asset path too long\n");
        return 1;
    }
    PeSceneCollision *scene = pe_scene_load_obj(collision_path, error, sizeof(error));
    if (!scene) { fprintf(stderr, "Training collision load failed: %s\n", error); return 1; }
    PeTraining training;
    if (!pe_training_init(&training, scene)) {
        fprintf(stderr, "Training source spawn has no safe room floor\n");
        pe_scene_free(scene);
        return 1;
    }
    size_t triangles = 0;
    if (!pe_reference_validate(model_path, &triangles, error, sizeof(error))) {
        fprintf(stderr, "Training model validation failed: %s\n", error);
        pe_scene_free(scene);
        return 1;
    }
    snprintf(kit_path,sizeof kit_path,"%s/training-kit.obj",asset_dir);
    snprintf(knife_path,sizeof knife_path,"%s/training-knife.obj",asset_dir);
    snprintf(actor_path,sizeof actor_path,"%s/hare.peac",asset_dir);
    snprintf(actor_texture,sizeof actor_texture,"%s/hare-diffuse.png",asset_dir);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 800, "Planet Explorers | Training Room");
    if (!IsWindowReady()) { pe_scene_free(scene); return 1; }
    SetWindowMinSize(1000, 700);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    PeReferenceModel reference = {0}, kit = {0}, knife = {0};
    if (!pe_reference_load(&reference, model_path, error, sizeof(error))) {
        fprintf(stderr, "Training model load failed: %s\n", error);
        pe_scene_free(scene); CloseWindow(); return 1;
    }
    if(FileExists(kit_path)&&!pe_reference_load(&kit,kit_path,error,sizeof error)) {
        fprintf(stderr,"Training kit load failed: %s\n",error);
        pe_reference_unload(&reference);pe_scene_free(scene);CloseWindow();return 1;
    }
    if(FileExists(knife_path)&&!pe_reference_load(&knife,knife_path,error,sizeof error)) {
        fprintf(stderr,"Training knife load failed: %s\n",error);
        pe_reference_unload(&kit);pe_reference_unload(&reference);pe_scene_free(scene);CloseWindow();return 1;
    }
    PeActorView hare={0};
    size_t hare_idle=PE_ACTOR_BIND_POSE,hare_walk=PE_ACTOR_BIND_POSE,hare_clip=PE_ACTOR_BIND_POSE;
    PeHare hare_motion={0};
    if(FileExists(actor_path)) {
        if(!pe_actor_view_load(&hare,actor_path,actor_texture,error,sizeof error)) {
            fprintf(stderr,"Training hare load failed: %s\n",error);
            pe_reference_unload(&knife);pe_reference_unload(&kit);pe_reference_unload(&reference);
            pe_scene_free(scene);CloseWindow();return 1;
        }
        hare_idle=pe_actor_find_clip(hare.actor,"idle");
        hare_walk=pe_actor_find_clip(hare.actor,"walk");hare_clip=hare_idle;
        if(hare_idle==PE_ACTOR_BIND_POSE||!pe_hare_init(&hare_motion,scene,261,1,false)){fprintf(stderr,"Training hare idle clip missing or spawn obstructed\n");pe_actor_view_unload(&hare);
            pe_reference_unload(&knife);pe_reference_unload(&kit);pe_reference_unload(&reference);pe_scene_free(scene);CloseWindow();return 1;}
    }
    PeActorSceneView player={0},stowed={0};
    bool animated_player=FileExists(player_path);
    if(!animated_player)memcpy(player_path,idle_path,sizeof player_path);
    if(FileExists(player_path)&&!pe_actor_scene_view_load(&player,player_path,error,sizeof error)) {
        fprintf(stderr,"Training player load failed: %s\n",error);
        pe_actor_view_unload(&hare);pe_reference_unload(&knife);pe_reference_unload(&kit);
        pe_reference_unload(&reference);pe_scene_free(scene);CloseWindow();return 1;
    }
    char animated_stowed_path[4096];
    int ap=snprintf(animated_stowed_path,sizeof animated_stowed_path,"%s/player-knife-stowed/recording.peas",asset_dir);
    bool animated_stowed=animated_player&&ap>=0&&(size_t)ap<sizeof animated_stowed_path&&FileExists(animated_stowed_path);
    if(animated_stowed)memcpy(stowed_path,animated_stowed_path,sizeof stowed_path);
    if(player.loaded&&FileExists(stowed_path)&&!pe_actor_scene_view_load(&stowed,stowed_path,error,sizeof error)) {
        fprintf(stderr,"Training stowed knife load failed: %s\n",error);
        pe_actor_scene_view_unload(&player);pe_actor_view_unload(&hare);pe_reference_unload(&knife);
        pe_reference_unload(&kit);pe_reference_unload(&reference);pe_scene_free(scene);CloseWindow();return 1;
    }
    PePlayerLocomotion locomotion={0};
    size_t player_clips[PE_LOCOMOTION_COUNT]={0},stowed_clips[PE_LOCOMOTION_COUNT]={0};
    unsigned locomotion_seen=0;
    if(animated_player) {
        double durations[PE_LOCOMOTION_COUNT]={0};
        for(int i=0;i<PE_LOCOMOTION_COUNT;i++) {
            player_clips[i]=pe_actor_scene_view_find_clip(&player,pe_player_locomotion_name(i));
            const PeActorClip *clip=pe_actor_scene_view_clip_at(&player,player_clips[i]);
            if(clip)durations[i]=clip->duration;
            if(animated_stowed) {
                stowed_clips[i]=pe_actor_scene_view_find_clip(&stowed,pe_player_locomotion_name(i));
                const PeActorClip *mount=pe_actor_scene_view_clip_at(&stowed,stowed_clips[i]);
                if(!clip||!mount||clip->fps!=mount->fps||clip->duration!=mount->duration||
                   clip->frames!=mount->frames||clip->loop!=mount->loop||
                   clip->terminal_at_duration!=mount->terminal_at_duration)durations[i]=0;
            }
        }
        if(!pe_player_locomotion_init(&locomotion,durations)) {
            fprintf(stderr,"Training player needs all twelve directional locomotion clips\n");
            pe_actor_scene_view_unload(&stowed);pe_actor_scene_view_unload(&player);
            pe_actor_view_unload(&hare);pe_reference_unload(&knife);pe_reference_unload(&kit);
            pe_reference_unload(&reference);pe_scene_free(scene);CloseWindow();return 1;
        }
    }
    PeActorSceneView drawn_view={0};
    PeHitboxes *drawn_boxes=NULL,*hare_boxes=NULL;
    PeTrainingMelee melee={0};
    char drawn_path[4096],drawn_box_path[4096],hare_box_path[4096];
    int dp=snprintf(drawn_path,sizeof drawn_path,"%s/player-knife-drawn/recording.peas",asset_dir);
    int db=snprintf(drawn_box_path,sizeof drawn_box_path,"%s/player-knife-drawn/knife-drawn.pehb",asset_dir);
    int hb=snprintf(hare_box_path,sizeof hare_box_path,"%s/hare.pehb",asset_dir);
    bool combat_assets=dp>=0&&(size_t)dp<sizeof drawn_path&&FileExists(drawn_path);
    if(combat_assets) {
        bool loaded=animated_player&&hare.loaded&&db>=0&&(size_t)db<sizeof drawn_box_path&&hb>=0&&(size_t)hb<sizeof hare_box_path&&
            pe_actor_scene_view_load(&drawn_view,drawn_path,error,sizeof error);
        if(loaded) {
            drawn_boxes=pe_hitboxes_load(drawn_box_path,drawn_view.parts[0].actor,error,sizeof error);
            hare_boxes=pe_hitboxes_load(hare_box_path,hare.actor,error,sizeof error);
            PeTrainingMeleeSetup setup={.body=player.parts[0].actor,.drawn_knife=drawn_view.parts[0].actor,
                .target_actor=hare.actor,.drawn_capsule=drawn_boxes,.target_defence=hare_boxes,
                .gender=PE_COMBAT_FEMALE,.player_id=1,.target_id=PE_HARE_SPAWN_ID,
                .attack=19,.stamina_reduce_percent=1,.target_hp=hare_motion.max_hp,
                .target_max_hp=hare_motion.max_hp,.target_defense=hare_motion.defense};
            loaded=drawn_boxes&&hare_boxes&&pe_training_melee_init(&melee,&setup);
            loaded=loaded&&matching_clocks(&player,&drawn_view,true);
        }
        if(!loaded) {
            fprintf(stderr,"Training melee needs matching baked body/drawn knife and hare hitboxes\n");
            pe_hitboxes_free(drawn_boxes);pe_hitboxes_free(hare_boxes);pe_actor_scene_view_unload(&drawn_view);
            pe_actor_scene_view_unload(&stowed);pe_actor_scene_view_unload(&player);pe_actor_view_unload(&hare);
            pe_reference_unload(&knife);pe_reference_unload(&kit);pe_reference_unload(&reference);
            pe_scene_free(scene);CloseWindow();return 1;
        }
    }
    PeReferenceModel herbs[PE_GATHERING_HERBS]={0},tree={0};
    bool gathering_assets=false,gathering_requested=false;
    for(unsigned i=0;i<PE_GATHERING_HERBS;i++) {
        char path[4096];int n=snprintf(path,sizeof path,"%s/gathering/herb-%u.obj",asset_dir,i);
        if(n>=0&&(size_t)n<sizeof path&&FileExists(path))gathering_requested=true;
    }
    for(unsigned i=0;i<PE_GATHERING_HERBS;i++) {
        char path[4096];int n=snprintf(path,sizeof path,"%s/gathering/herb-%u.obj",asset_dir,i);
        bool valid_path=n>=0&&(size_t)n<sizeof path;
        if(i==0)gathering_assets=valid_path&&FileExists(path);
        if(!gathering_assets)break;
        if(!valid_path||!pe_reference_load(&herbs[i],path,error,sizeof error)) {
            fprintf(stderr,"Training herb asset load failed\n");gathering_assets=false;break;
        }
    }
    char tree_path[4096];int tp=snprintf(tree_path,sizeof tree_path,"%s/gathering/tree.obj",asset_dir);
    if(tp>=0&&(size_t)tp<sizeof tree_path&&FileExists(tree_path))gathering_requested=true;
    bool tree_ok=!gathering_assets||(tp>=0&&(size_t)tp<sizeof tree_path&&pe_reference_load(&tree,tree_path,error,sizeof error));
    if((gathering_requested&&!gathering_assets)||!tree_ok) {
        for(unsigned i=0;i<PE_GATHERING_HERBS;i++){pe_reference_unload(&herbs[i]);}
        pe_reference_unload(&tree);
        pe_hitboxes_free(drawn_boxes);pe_hitboxes_free(hare_boxes);pe_actor_scene_view_unload(&drawn_view);
        pe_actor_scene_view_unload(&stowed);pe_actor_scene_view_unload(&player);pe_actor_view_unload(&hare);
        pe_reference_unload(&knife);pe_reference_unload(&kit);pe_reference_unload(&reference);
        pe_scene_free(scene);CloseWindow();return 1;
    }
    PeActorSceneView axe_view={0},cut_body_view={0},cut_axe_view={0};size_t cut_clips[3]={0};
    if(!load_axe_views(asset_dir,&player,&axe_view,&cut_body_view,&cut_axe_view,cut_clips)) {
        fprintf(stderr,"Training axe scenes need matching body clocks and a complete cutting pair\n");
        for(unsigned i=0;i<PE_GATHERING_HERBS;i++){pe_reference_unload(&herbs[i]);}
        pe_reference_unload(&tree);pe_hitboxes_free(drawn_boxes);pe_hitboxes_free(hare_boxes);pe_actor_scene_view_unload(&drawn_view);
        pe_actor_scene_view_unload(&stowed);pe_actor_scene_view_unload(&player);pe_actor_view_unload(&hare);
        pe_reference_unload(&knife);pe_reference_unload(&kit);pe_reference_unload(&reference);
        pe_scene_free(scene);CloseWindow();return 1;
    }
    bool knife_drawn=false,attack_pending=false;
    double right_pressed=0;
    unsigned melee_hits=0,melee_swings=0;
    bool contact_screenshot_written=false;
    unsigned contact_capture_delay=0,tree_capture_delay=0;
    bool herb_screenshot_written=false,tree_screenshot_written=false,craft_screenshot_written=false;
    bool storage_ui_written=false,inventory_ui_written=false,quickbar_ui_written=false;
    PeVec3 measured_velocity={0};
    double actor_seconds=0;
    float player_yaw=30.0f;
    bool inventory=false,log=false,dragging=false,equipment_tab=false,menu_open=false;
    PeUiPointerGate pointer_gate={0};
    int inventory_tab=0;
    float preview_yaw=0;
    bool extended_smoke=smoke_frames>=1200;
    bool npc_smoke=smoke_frames==1750;
    bool combat_smoke=smoke_frames>=3600;
    bool gathering_smoke=smoke_frames>=6000;
    bool crafting_smoke=smoke_frames>=9000;
    bool mining_smoke=smoke_frames>=12000;
    bool building_smoke=smoke_frames>=15000;
    unsigned build_added=0,build_removed=0,build_rotation=0;
    bool build_screenshot_written=false,smoke_removed=false;
    bool build_pending=false,build_pending_subtract=false;
    Ray build_pending_ray={0};
    unsigned dig_swings=0,dig_removals=0;
    unsigned dig_capture_delay=0;
    bool dig_screenshot_written=false;
    bool paused = false, free_cursor = false, captured = false, autorun = false, walk = false;
    bool failed = false, quit = false, return_main = false, jump_pending = false;
    PeTrainingTerrainView *terrain=calloc(1,sizeof *terrain);
    TrainingBuildView *building=calloc(1,sizeof *building);
    if(building)pe_training_build_init(&building->blocks);
    PeActorSceneView shovel_views[4]={0};
    PeTrainingNpcView npcs={0};
    PeTrainingUi ui={0};
    if(!pe_training_ui_load(&ui,asset_dir,error,sizeof error)){fprintf(stderr,"Training UI load failed: %s\n",error);failed=true;quit=true;}
    if(!pe_training_npc_view_load(&npcs,asset_dir,scene,error,sizeof error)) {
        fprintf(stderr,"Training NPC load failed: %s\n",error);failed=true;quit=true;
    }
    if(!terrain||!building||!load_shovel_views(asset_dir,&player,shovel_views)) {
        fprintf(stderr,"Unable to initialize training terrain or matching shovel scenes\n");failed=true;quit=true;
    }
    /* Optional always-on FinalIK solve shared with the sandbox (pe_ik_drive.h). A present staged pair must
     * load; every loaded animated player scene must bind to its exact geometry or the pose stays baked. */
    PeIkDrive *ik=NULL;bool ik_ready=false;unsigned long long ik_draws=0;
    PeHaltHost halt={0};unsigned long long halt_starts=0,halt_refusals=0; /* Transient. */
    PeGrounderHost grounder={0};double grounder_time=0;unsigned long long grounder_updates=0,grounder_planted=0;float grounder_reach=0; /* Transient. */
    struct {PeActorSceneView *view;const char *group;} ik_views[]={{&player,"player"},{&stowed,"player-knife-stowed"},
        {&drawn_view,"player-knife-drawn"},{&axe_view,"player-axe"},{&cut_body_view,"player-cut"},{&cut_axe_view,"player-axe-cut"},
        {&shovel_views[0],"player-shovel"},{&shovel_views[1],"player-shovel-stowed"},{&shovel_views[2],"player-dig"},{&shovel_views[3],"player-shovel-dig"}};
    const size_t ik_view_count=sizeof ik_views/sizeof ik_views[0];
    if(pe_ik_drive_present(asset_dir)&&!(ik=pe_ik_drive_load(asset_dir,error,sizeof error))) {
        fprintf(stderr,"Training IK drive load failed: %s\n",error);failed=true;quit=true;
    }
    if(ik&&animated_player&&(animated_stowed||!stowed.loaded)) {
        ik_ready=true;
        for(size_t i=0;i<ik_view_count;i++)if(ik_views[i].view->loaded&&!pe_actor_scene_view_bind_ik(ik_views[i].view,ik,ik_views[i].group))ik_ready=false;
    }
    PeReferenceModel receiver_models[2]={0},marker_models[2]={0};
    const char *receiver_names[]={"receiver-upright.obj","receiver-flat.obj"};
    for(unsigned i=0;i<2;i++) {
        char path[4096];int n=snprintf(path,sizeof path,"%s/building/%s",asset_dir,receiver_names[i]);
        if(n<0||(size_t)n>=sizeof path||(FileExists(path)&&!pe_reference_load(&receiver_models[i],path,error,sizeof error))) {
            fprintf(stderr,"Unable to load receiver model\n");failed=true;quit=true;
        }
    }
    for(unsigned i=0;i<2;i++) {
        char path[4096];int n=snprintf(path,sizeof path,"%s/building/effect-%s.obj",asset_dir,i?"false":"true");
        if(n<0||(size_t)n>=sizeof path||(FileExists(path)&&!pe_reference_load(&marker_models[i],path,error,sizeof error))) {
            fprintf(stderr,"Unable to load receiver indicator\n");failed=true;quit=true;
        }
        for(int j=0;marker_models[i].loaded&&j<marker_models[i].model.meshCount;j++) {
            int material=marker_models[i].model.meshMaterial[j];
            if(material<0||material>=marker_models[i].model.materialCount||
               !marker_models[i].model.materials[material].maps[MATERIAL_MAP_DIFFUSE].texture.id||
               marker_models[i].model.materials[material].maps[MATERIAL_MAP_DIFFUSE].texture.id==rlGetTextureIdDefault()) {
                fprintf(stderr,"Receiver indicator requires its recovered texture\n");failed=true;quit=true;
            }
        }
    }
    Shader foliage={0};int cutoff_location=-1;
    if(gathering_assets) {
        foliage=LoadShaderFromMemory(NULL,
            "#version 330\nin vec2 fragTexCoord;in vec4 fragColor;\n"
            "uniform sampler2D texture0;uniform vec4 colDiffuse;uniform float cutoff;\n"
            "out vec4 finalColor;void main(){vec4 texel=texture(texture0,fragTexCoord);"
            "if(texel.a<cutoff)discard;finalColor=texel*colDiffuse*fragColor;}\n");
        cutoff_location=GetShaderLocation(foliage,"cutoff");
        if(!foliage.id||foliage.id==rlGetShaderIdDefault()||cutoff_location<0) {
            fprintf(stderr,"Unable to initialize source foliage alpha cutoffs\n");failed=true;quit=true;
        }else {
            for(unsigned i=0;i<PE_GATHERING_HERBS;i++)foliage_shader(&herbs[i],foliage);
            foliage_shader(&tree,foliage);
        }
    }
    Shader marker_shader={0};
    if(marker_models[0].loaded||marker_models[1].loaded) {
        marker_shader=LoadShaderFromMemory(NULL,
            "#version 330\nin vec2 fragTexCoord;uniform sampler2D texture0;"
            "out vec4 finalColor;void main(){finalColor=texture(texture0,fragTexCoord)*vec4(.5);}\n");
        if(!marker_shader.id||marker_shader.id==rlGetShaderIdDefault()) {
            fprintf(stderr,"Unable to initialize original additive marker shader\n");failed=true;quit=true;
        }else for(unsigned i=0;i<2;i++)foliage_shader(&marker_models[i],marker_shader);
    }
    /* Source pose Euler(-pitch, yaw, 0) rotates Unity +Z forward.
     * Our forward has -cos(yaw) Z, so yaw_C = 180deg - yaw_Unity.
     * Source Yaw=30, Pitch=-20 therefore maps to (150deg,-20deg). */
    float yaw = 150.0f * DEG2RAD, pitch = -20.0f * DEG2RAD, accumulator = 0;
    int frame = 0;
    while (!WindowShouldClose() && !quit) {
        pe_training_ui_resize(&ui);
        bool dialogue_input_consumed=false;
        bool pointer_release_pending=pointer_gate.waiting_for_release;
        const bool pause_was_active=paused;
        float dt = smoke_frames ? PE_TRAINING_DT : fminf(GetFrameTime(), .1f);
        bool focused=smoke_frames||IsWindowFocused();
        bool settings_consumed=pe_ui_settings_update(&settings,focused&&!smoke_frames);
        if(!focused){ui.history.scroll_dragging=false;dragging=false;autorun=false;jump_pending=false;attack_pending=false;pe_training_melee_cancel(&melee);pe_training_dig_cancel(&training.dig);}
        if(focused&&!paused&&training.tutorial.mission_id==PE_TUTORIAL_INVENTORY&&!training.inventory_lesson_accepted)
            pe_training_accept_inventory_lesson(&training);
        if(focused&&!paused&&training.tutorial.mission_id==PE_TUTORIAL_EQUIPMENT&&!training.equipment_lesson_accepted)
            pe_training_accept_equipment_lesson(&training);
        inventory=training.inventory_open;
        pe_training_ui_record_dialogue(&ui,&training);
        int talk=pe_training_dialogue(&training);
        bool history_was_open=ui.history.open;
        bool ui_was_modal=ui.history.open||talk||training.kit_open||inventory||log||training.replicator_open||training.character_open||menu_open;
        if(focused&&!smoke_frames&&!settings_consumed&&IsKeyPressed(KEY_ESCAPE)) {
            if(paused)paused=false;
            else if(ui.history.open){ui.history.open=false;ui.history.scroll_dragging=false;}
            else if(menu_open)menu_open=false;
            else if(training.character_open)pe_training_close_character(&training);
            else if(training.replicator_open)pe_training_close_replicator(&training);
            else if(training.kit_open)pe_training_close_kit(&training);
            else if(inventory||log){pe_training_close_inventory(&training);inventory=false;log=false;dragging=false;}
            else paused=!paused;
        }
        if(focused&&paused&&!smoke_frames&&!settings_consumed) {
            Vector2 pointer=GetMousePosition();
            int command=pe_training_pause_command(&ui.layout,pointer.x,pointer.y,
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT),IsKeyPressed(KEY_M),IsKeyPressed(KEY_Q));
            if(command==1)paused=false;
            if(command==2){return_main=true;quit=true;}
            if(command==3)quit=true;
            if(command==4)pe_ui_settings_open(&settings);
            if(quit)break;
        }
        unsigned visible_windows=((inventory||training.kit_open)?1u<<PE_UI_WINDOW_INVENTORY:0)|
            (training.kit_open?1u<<PE_UI_WINDOW_STORAGE:0)|(training.character_open?1u<<PE_UI_WINDOW_CHARACTER:0)|
            (training.replicator_open?1u<<PE_UI_WINDOW_REPLICATOR:0)|(log?1u<<PE_UI_WINDOW_MISSION:0)|(ui.history.open?1u<<PE_UI_WINDOW_HISTORY:0);
        Vector2 window_pointer=GetMousePosition();
        int pointer_window=pe_training_ui_window_at(&ui.layout,visible_windows,window_pointer.x,window_pointer.y);
        bool cancel_window_drag=!focused||paused||pause_was_active||talk||menu_open||dragging||smoke_frames||IsKeyPressed(KEY_ESCAPE)||
            (ui.windows.dragging&&!(visible_windows&(1u<<(ui.windows.dragging-1))));
        bool window_input_consumed=pe_training_ui_window_drag(&ui.windows,&ui.layout,pointer_window,
            window_pointer.x,window_pointer.y,IsMouseButtonPressed(MOUSE_BUTTON_LEFT),IsMouseButtonReleased(MOUSE_BUTTON_LEFT),cancel_window_drag);
        dialogue_input_consumed=window_input_consumed;
        /* A Resume click/key belongs to the pause screen for its whole frame. */
        if(focused&&!paused&&!pause_was_active&&!smoke_frames&&!window_input_consumed) {
            Vector2 pointer=GetMousePosition();
            unsigned response=0;
            for(int k=0;k<9;k++)if(IsKeyPressed(KEY_ONE+k)||IsKeyPressed(KEY_KP_1+k)){response=(unsigned)k+1;break;}
            if(pe_training_dialogue_response(&training,response,IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)||
               (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&pe_training_ui_contains(ui.layout.dialogue,pointer.x,pointer.y)))) {
                dialogue_input_consumed=true;
            }
            else if(ui.history.open||history_was_open){
                dialogue_input_consumed=true;
                if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                    if(pe_training_ui_contains(ui.layout.history_close,pointer.x,pointer.y)||pe_training_ui_contains(ui.layout.history_button,pointer.x,pointer.y)){ui.history.open=false;ui.history.scroll_dragging=false;}
                    else if(pe_training_ui_contains(ui.layout.history_clear,pointer.x,pointer.y)){
                        pe_conversation_history_clear(&ui.conversation_history);ui.history.scroll=-1;ui.history.scroll_dragging=false;
                    }
                }
                pe_ui_history_pointer(&ui.history,&ui.layout,pointer.x,pointer.y,GetMouseWheelMove(),
                    IsMouseButtonPressed(MOUSE_BUTTON_LEFT),IsMouseButtonReleased(MOUSE_BUTTON_LEFT),!ui.history.open);
            }
            else if(!talk) {
                const bool menu_was_open=menu_open;
                const bool left_pressed=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
                const bool right_pressed_now=IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
                bool left_available=left_pressed;
                PeUiChromeResult chrome=pe_ui_chrome_click(&ui.layout,pointer.x,pointer.y,
                    left_pressed,menu_open,!ui_was_modal&&!pointer_release_pending);
                int menu_action=chrome.action;
                menu_open=chrome.menu_open;
                if(chrome.consume_press)left_available=false;
                if(left_available&&log&&pe_training_ui_contains(ui.layout.mission_close,pointer.x,pointer.y)){
                    log=false;left_available=false;
                }
                if(menu_action==4)paused=true;
                if(menu_action==5){
                    pe_training_close_character(&training);pe_training_close_replicator(&training);pe_training_close_kit(&training);pe_training_close_inventory(&training);
                    inventory=log=dragging=false;ui.history.open=true;ui.history.scroll=-1;ui.history.scroll_dragging=false;dialogue_input_consumed=true;
                }
                bool character_action=IsKeyPressed(KEY_C)||menu_action==1;
                bool replicator_action=IsKeyPressed(KEY_Y)||menu_action==2;
                bool inventory_action=IsKeyPressed(KEY_I)||menu_action==0;
                bool mission_action=IsKeyPressed(KEY_Q)||menu_action==3;
                bool panel_action=character_action||replicator_action||inventory_action||mission_action||menu_action>=0;
                if(!paused&&character_action){
                    if(training.character_open)pe_training_close_character(&training);
                    else {log=false;pe_training_close_replicator(&training);pe_training_close_kit(&training);pe_training_open_character(&training);}
                    dragging=false;
                }
                if(!paused&&replicator_action){
                    if(training.replicator_open)pe_training_close_replicator(&training);
                    else {pe_training_close_character(&training);pe_training_close_inventory(&training);pe_training_close_kit(&training);inventory=false;pe_training_open_replicator(&training);}
                    log=false;dragging=false;
                }
                if(!paused&&inventory_action){
                    pe_training_close_replicator(&training);
                    if(inventory)pe_training_close_inventory(&training);else pe_training_open_inventory(&training);
                    inventory=training.inventory_open;log=false;dragging=false;pe_training_close_kit(&training);
                }
                if(!paused&&mission_action){
                    pe_training_close_character(&training);pe_training_close_replicator(&training);log=!log;
                    pe_training_close_inventory(&training);inventory=false;dragging=false;pe_training_close_kit(&training);
                }
                /* A panel opened by this click cannot receive that same click.
                   Character+Inventory intentionally coexist; Mission and
                   Replicator exclusively cover their underlying controls. */
                if(panel_action||paused)left_available=false;
                bool pointer_available=!menu_was_open&&!menu_open&&!panel_action&&!paused;
                bool right_available=pointer_available&&right_pressed_now;
                if(pointer_available&&pointer_window==PE_UI_WINDOW_CHARACTER&&!log&&!training.replicator_open&&training.character_open&&
                   IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&pe_training_ui_contains(ui.layout.equipment_preview,pointer.x,pointer.y))preview_yaw+=GetMouseDelta().x;
                if(!paused&&!training.character_open&&!menu_open&&!training.replicator_open&&!inventory&&!log&&!training.kit_open){
                    if(IsKeyPressed(KEY_B))pe_training_set_build_mode(&training,!training.build_mode);
                    if(training.build_mode&&IsKeyPressed(KEY_T))build_rotation=(build_rotation+1)%4;
                }
                if(left_available){
                    if(pointer_window==PE_UI_WINDOW_CHARACTER&&training.character_open&&!log&&!training.replicator_open&&pe_training_ui_contains(ui.layout.equipment_close,pointer.x,pointer.y)){
                        pe_training_close_character(&training);left_available=false;
                    }else if(pointer_window==PE_UI_WINDOW_INVENTORY&&(inventory||training.kit_open)&&!log&&!training.replicator_open&&pe_training_ui_contains(ui.layout.inventory_close,pointer.x,pointer.y)){
                        pe_training_close_inventory(&training);pe_training_close_kit(&training);inventory=false;dragging=false;left_available=false;
                    }else if(pointer_window==PE_UI_WINDOW_STORAGE&&training.kit_open&&pe_training_ui_contains(ui.layout.storage_close,pointer.x,pointer.y)){
                        pe_training_close_kit(&training);left_available=false;
                    }else if(training.replicator_open&&pe_training_ui_contains(ui.layout.replicator_close,pointer.x,pointer.y)){
                        pe_training_close_replicator(&training);left_available=false;
                    }
                }
                if(left_available&&training.replicator_open&&pe_training_ui_contains(ui.layout.replicator_craft,pointer.x,pointer.y)){
                    pe_training_start_shovel_craft(&training);left_available=false;
                }
                if(left_available&&pointer_window==PE_UI_WINDOW_INVENTORY&&(inventory||training.kit_open)&&!log&&!training.replicator_open){
                    int tab=pe_training_ui_hit(ui.layout.inventory_tabs,PE_UI_INVENTORY_TABS,pointer.x,pointer.y);
                    if(tab>=0){inventory_tab=tab;equipment_tab=tab==1;dragging=false;left_available=false;}
                }
                if(left_available&&!training.character_open&&!menu_open&&!inventory&&!training.kit_open&&!training.replicator_open&&!log&&
                   !ui_was_modal&&pe_training_ui_contains(ui.layout.build_button,pointer.x,pointer.y))pe_training_set_build_mode(&training,!training.build_mode);
                if(right_available&&pointer_window==PE_UI_WINDOW_STORAGE&&training.kit_open&&!log&&!training.replicator_open&&pe_training_ui_contains(ui.layout.storage_slots[0],pointer.x,pointer.y))pe_training_transfer_juice(&training);
                if(right_available&&pointer_window==PE_UI_WINDOW_INVENTORY&&inventory&&!log&&!training.replicator_open){
                    if(equipment_tab&&pe_training_ui_contains(ui.layout.inventory_slots[0],pointer.x,pointer.y)){
                        if(!training.knife_equipped)pe_training_equip_knife(&training);
                    }
                    if(pe_training_ui_contains(ui.layout.inventory_slots[1],pointer.x,pointer.y)){
                        if(equipment_tab){if(!training.axe_equipped)pe_training_equip_axe(&training);}
                        else if(inventory_tab==0)pe_training_learn_script(&training);
                    }
                    if(equipment_tab)for(unsigned i=0;i<2;i++)if(pe_training_ui_contains(ui.layout.inventory_slots[2+i],pointer.x,pointer.y)){
                        if(training.equipped_shovel!=(int)i)pe_training_equip_shovel(&training,i);
                    }
                }
                if(left_available&&pointer_window==PE_UI_WINDOW_INVENTORY&&inventory&&!log&&!training.replicator_open&&inventory_tab==0&&training.juice_count>0&&pe_training_ui_contains(ui.layout.inventory_slots[0],pointer.x,pointer.y))dragging=true;
                bool quickslot_visible=!(training.character_open&&pe_training_ui_contains(ui.layout.equipment,pointer.x,pointer.y))&&
                    !(log&&pe_training_ui_contains(ui.layout.mission,pointer.x,pointer.y))&&
                    !(training.replicator_open&&pe_training_ui_contains(ui.layout.replicator,pointer.x,pointer.y))&&
                    !((inventory||training.kit_open)&&pe_training_ui_contains(ui.layout.inventory,pointer.x,pointer.y))&&
                    !(training.kit_open&&pe_training_ui_contains(ui.layout.storage,pointer.x,pointer.y));
                if(dragging&&IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
                    if(pointer_available&&quickslot_visible&&pe_training_ui_contains(ui.layout.quickslots[0],pointer.x,pointer.y))pe_training_assign_quickslot(&training,916);
                    dragging=false;
                }
                if(right_available&&pointer_window==PE_UI_WINDOW_CHARACTER&&training.character_open&&!log&&!training.replicator_open&&pe_training_ui_contains(ui.layout.equipment_weapon,pointer.x,pointer.y)){
                    if(training.knife_equipped)pe_training_unequip_knife(&training);
                    else if(training.axe_equipped)pe_training_unequip_axe(&training);
                    else if(training.equipped_shovel>=0)pe_training_unequip_shovel(&training);
                }
                if(!paused&&!ui.history.open&&!menu_open&&(IsKeyPressed(KEY_ONE)||(right_available&&quickslot_visible&&pe_training_ui_contains(ui.layout.quickslots[0],pointer.x,pointer.y))))pe_training_use_quickslot(&training);
                bool interact=!ui.history.open&&!pointer_release_pending&&!paused&&!training.character_open&&!menu_open&&!training.replicator_open&&!inventory&&!log&&!training.kit_open&&IsKeyPressed(KEY_E);
                if(!ui.history.open&&!pointer_release_pending&&!paused&&right_available&&!training.character_open&&!training.replicator_open&&!training.kit_open&&!inventory&&!log&&!ui_was_modal&&!pe_training_ui_pointer_over(&ui,pointer)) {
                    Vector2 cursor=captured?(Vector2){GetScreenWidth()/2.0f,GetScreenHeight()/2.0f}:GetMousePosition();
                    Ray ray=GetScreenToWorldRay(cursor,camera_for(&training,scene,yaw,pitch));
                    BoundingBox box=training.tutorial.mission_id==PE_TUTORIAL_STORAGE?
                        (BoundingBox){{6.4f,1.45f,11.35f},{7.6f,2.15f,12.65f}}:
                        (BoundingBox){{31.6f,2,11.6f},{32.4f,3.8f,12.4f}};
                    PeVec3 andy;
                    if(training.tutorial.mission_id==PE_TUTORIAL_RETURN&&pe_training_npc_view_position(&npcs,9041,&andy)) {
                        const PeTrainingNpc *npc=pe_training_npc_find(9041);
                        box=(BoundingBox){{andy.x-npc->radius,andy.y+npc->center_y-npc->height*.5f,andy.z-npc->radius},
                            {andy.x+npc->radius,andy.y+npc->center_y+npc->height*.5f,andy.z+npc->radius}};
                    }
                    interact=interact||GetRayCollisionBox(ray,box).hit;
                }
                if(interact) {
                    pe_training_close_inventory(&training);inventory=false;log=false;dragging=false;
                    if(training.tutorial.mission_id==PE_TUTORIAL_STORAGE){if(pe_training_interact_kit(&training)){pe_training_open_inventory(&training);inventory=true;inventory_tab=0;equipment_tab=false;}}
                    else if(training.tutorial.mission_id==PE_TUTORIAL_RETURN)pe_training_interact_andy(&training);
                    else if(training.tutorial.mission_id==PE_TUTORIAL_GATHER&&gathering_assets) {
                        int nearest=-1;float distance=7;
                        for(unsigned i=0;i<PE_GATHERING_HERBS;i++) {
                            if(training.gathering.herb_dead[i]||training.gathering.herb_active[i])continue;
                            Vector3 delta=Vector3Subtract(vector(pe_training_gathering_herb_position(i)),vector(training.position));
                            float d=Vector3LengthSqr(delta);if(d<distance){nearest=(int)i;distance=d;}
                        }
                        if(nearest>=0&&!pe_halt_host_masked(&halt))pe_training_begin_herb(&training,(unsigned)nearest); /* the Halt mask refuses Gather */
                    }
                }
                if(!ui.history.open&&!pointer_release_pending&&!paused&&!training.character_open&&!menu_open&&!training.replicator_open&&!training.kit_open&&!inventory&&!log) {
                    if(IsKeyPressed(KEY_F5)){pe_training_camera(&training,(training.camera_mode+1)%3);free_cursor=false;}
                    if(IsKeyPressed(KEY_TAB))free_cursor=!free_cursor;
                    if(IsKeyPressed(KEY_Z))walk=!walk;
                    if(melee.ready&&training.knife_equipped) {
                        Vector2 mouse=GetMousePosition();
                        bool play_area=!ui_was_modal&&!pe_training_ui_pointer_over(&ui,mouse);
                        if(training.tutorial.mission_id==PE_TUTORIAL_COMBAT&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&(captured||play_area)) {
                            if(!knife_drawn)knife_drawn=true;else attack_pending=true;
                        }
                        if(!ui_was_modal&&(captured||!pe_training_ui_pointer_over(&ui,pointer))&&IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))right_pressed=GetTime();
                        if(IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)&&GetTime()-right_pressed<.25){knife_drawn=false;attack_pending=false;}
                    }
                    if(training.axe_equipped&&!training.build_mode) {
                        if(!ui_was_modal&&(captured||!pe_training_ui_pointer_over(&ui,pointer))&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))pe_training_set_axe_held(&training,true);
                        if(!ui_was_modal&&(captured||!pe_training_ui_pointer_over(&ui,pointer))&&IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))right_pressed=GetTime();
                        if(IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)&&GetTime()-right_pressed<.25)pe_training_set_axe_held(&training,false);
                    }
                    if(training.equipped_shovel>=0&&!training.build_mode) {
                        if(!ui_was_modal&&(captured||!pe_training_ui_pointer_over(&ui,pointer))&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))pe_training_set_shovel_held(&training,true);
                        if(!ui_was_modal&&(captured||!pe_training_ui_pointer_over(&ui,pointer))&&IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))right_pressed=GetTime();
                        if(IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)&&GetTime()-right_pressed<.25)pe_training_set_shovel_held(&training,false);
                    }
                    if(IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))autorun=!autorun;
                }
            }
        }
        if(!IsWindowFocused()||!inventory||pe_training_dialogue(&training))dragging=false;
        bool modal=ui.history.open||pe_training_dialogue(&training)||training.kit_open||inventory||log||training.replicator_open||training.character_open||menu_open;
        if(!focused||paused||modal||!training.build_mode)build_pending=false;
        if(modal){autorun=false;jump_pending=false;attack_pending=false;pe_training_melee_cancel(&melee);pe_training_dig_cancel(&training.dig);}
        if(!training.knife_equipped){knife_drawn=false;pe_training_melee_cancel(&melee);}
        unsigned pointer_buttons=0;
        for(int button=0;button<7;button++)if(IsMouseButtonDown(button))pointer_buttons|=1u<<button;
        bool pointer_world=!focused||paused||pause_was_active||modal||ui_was_modal||window_input_consumed||dialogue_input_consumed;
        bool pointer_actions=!captured&&pe_training_ui_pointer_over(&ui,GetMousePosition());
        bool pointer_blocked=pe_ui_pointer_route(&pointer_gate,&pointer_world,&pointer_actions,pointer_buttons);
        if(pointer_world){autorun=false;jump_pending=false;attack_pending=false;}
        bool want_capture=!pointer_blocked&&focused&&!smoke_frames&&!paused&&!pause_was_active&&!modal&&!ui_was_modal&&
            (captured||!pe_training_ui_pointer_over(&ui,GetMousePosition()))&&
            ((training.camera_mode==0&&IsMouseButtonDown(MOUSE_BUTTON_RIGHT))||(training.camera_mode!=0&&!free_cursor));
        bool was_captured = captured;
        if (want_capture != captured) {
            if (want_capture) DisableCursor(); else EnableCursor();
            captured = want_capture;
        }
        if(ui.assets&&!captured){if(focused)HideCursor();else ShowCursor();}
        if (captured && was_captured) {
            Vector2 delta = GetMouseDelta();
            yaw += delta.x * .0025f;
            float minimum_pitch = training.camera_mode == 2 ? -75.0f : -70.0f;
            float maximum_pitch = training.camera_mode == 2 ? 75.0f : 55.0f;
            pitch = Clamp(pitch - delta.y * .0025f, minimum_pitch * DEG2RAD, maximum_pitch * DEG2RAD);
        }
        if (focused&&!paused&&!pause_was_active) {
            float forward = 0, side = 0;
            bool sprint = false;
            if (smoke_frames) {
                if(frame==0||frame==1)pe_training_advance_dialogue(&training,pe_training_dialogue(&training));
                if (frame == 5) pe_training_camera(&training, 1);
                if (frame == 10) pe_training_camera(&training, 2);
                if (frame >= 15 && frame < 20) {forward = 1; walk=true;}
                if(frame==20)walk=false;
                if (frame >= 20 && frame < 36) { forward = 1; sprint = true; }
                if (frame == 25) jump_pending = true;
                if (frame == 45) autorun = true;
                if (frame == 60) pe_training_camera(&training, 0);
            } else if(!modal&&!pointer_world) {
                forward = (float)(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
                side = (float)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
                sprint = IsKeyDown(KEY_LEFT_SHIFT);
                jump_pending = jump_pending || (!dialogue_input_consumed&&IsKeyPressed(KEY_SPACE));
                if (forward < 0) autorun = false;
            }
            if (autorun && !forward && !side) forward = 1;
            PeTrainingInput input = {0};
            bool world_pointer=!pointer_world&&!pointer_actions&&!pointer_blocked&&!ui_was_modal&&!dialogue_input_consumed&&(captured||!pe_training_ui_pointer_over(&ui,GetMousePosition()));
            bool dig_held=world_pointer&&!smoke_frames&&!modal&&!talk&&!training.build_mode&&training.shovel_held&&IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            bool build_request=world_pointer&&!smoke_frames&&!modal&&!talk&&training.build_mode&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            bool build_subtract=IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT);
            Camera3D dig_camera=camera_for(&training,scene,yaw,pitch);
            Vector2 dig_cursor=captured?(Vector2){GetScreenWidth()/2.0f,GetScreenHeight()/2.0f}:GetMousePosition();
            Ray dig_ray=GetScreenToWorldRay(dig_cursor,dig_camera);
            Ray build_ray=dig_ray;
            input.move_x = side * cosf(yaw) + forward * sinf(yaw);
            input.move_z = side * sinf(yaw) - forward * cosf(yaw);
            input.sprint = sprint;
            input.walk = walk;
            input.autorun = autorun;
            input.cut_held=world_pointer&&!smoke_frames&&!modal&&!talk&&!training.build_mode&&training.axe_held&&IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            if(input.cut_held&&training.gathering.tree_stage==PE_TREE_IDLE)
                player_yaw=atan2f(sinf(yaw),-cosf(yaw))*RAD2DEG;
            if(extended_smoke&&frame>=90) {
                input=(PeTrainingInput){0};autorun=false;
                int current=pe_training_dialogue(&training);
                /* Dialogue now correctly owns F5. The frame60 camera request
                   may occur during Tips, so restore third-person once clear. */
                if(!current&&training.camera_mode!=0)pe_training_camera(&training,0);
                if(current) {if(frame%12==0)pe_training_advance_dialogue(&training,current);}
                else if(training.tutorial.mission_id==PE_TUTORIAL_STORAGE) {
                    if(!training.kit_open)pe_training_interact_kit(&training);
                    else pe_training_transfer_juice(&training);
                } else if(training.tutorial.mission_id==PE_TUTORIAL_INVENTORY) {
                    if(training.inventory_open)pe_training_close_inventory(&training);
                    pe_training_open_inventory(&training);
                } else if(training.tutorial.mission_id==PE_TUTORIAL_QUICKBAR) {
                    pe_training_assign_quickslot(&training,916);
                } else if(training.tutorial.mission_id==PE_TUTORIAL_CONSUME) {
                    pe_training_close_inventory(&training);pe_training_use_quickslot(&training);
                } else if(training.tutorial.mission_id==PE_TUTORIAL_EQUIPMENT) {
                    pe_training_open_inventory(&training);pe_training_equip_knife(&training);pe_training_close_inventory(&training);
                } else if(crafting_smoke&&training.tutorial.mission_id==PE_TUTORIAL_TREE) {
                    if(!training.axe_equipped) {
                        pe_training_open_inventory(&training);pe_training_equip_axe(&training);pe_training_close_inventory(&training);
                    } else {
                        pe_training_set_axe_held(&training,true);
                        Vector3 delta=Vector3Subtract(vector(pe_training_tree_center()),vector(training.position));
                        float distance=hypotf(delta.x,delta.z);
                        player_yaw=atan2f(delta.x,delta.z)*RAD2DEG;
                        if(distance>2.2f){input.move_x=delta.x/distance;input.move_z=delta.z/distance;}
                        else input.cut_held=true;
                        yaw=atan2f(delta.x,-delta.z);pitch=-20*DEG2RAD;
                    }
                } else if(crafting_smoke&&training.tutorial.mission_id==PE_TUTORIAL_SCRIPT) {
                    pe_training_open_inventory(&training);pe_training_learn_script(&training);pe_training_close_inventory(&training);
                } else if(crafting_smoke&&training.tutorial.mission_id==PE_TUTORIAL_REPLICATE) {
                    if(!training.replicator_open)pe_training_open_replicator(&training);
                    else if(!training.gathering.crafting)pe_training_start_shovel_craft(&training);
                } else if(crafting_smoke&&training.tutorial.mission_id==PE_TUTORIAL_MINE) {
                    pe_training_close_replicator(&training);
                    if(mining_smoke) {
                        if(training.equipped_shovel<0) {
                            pe_training_open_inventory(&training);pe_training_equip_shovel(&training,0);pe_training_close_inventory(&training);
                        }else {
                            pe_training_set_shovel_held(&training,true);
                            Vector3 target={8,2,12+(float)dig_removals};
                            Vector3 approach={9.8f,training.position.y,12.5f};
                            Vector3 delta=Vector3Subtract(approach,vector(training.position));
                            float distance=hypotf(delta.x,delta.z);
                            if(distance>.15f&&training.dig.stage==PE_DIG_IDLE){input.move_x=delta.x/distance;input.move_z=delta.z/distance;}
                            else dig_held=true;
                            dig_ray.position=Vector3Add(vector(training.position),(Vector3){0,1,0});
                            dig_ray.direction=Vector3Normalize(Vector3Subtract(target,dig_ray.position));
                            player_yaw=atan2f(dig_ray.direction.x,dig_ray.direction.z)*RAD2DEG;
                            yaw=atan2f(dig_ray.direction.x,-dig_ray.direction.z);pitch=-20*DEG2RAD;
                        }
                    }
                } else if(building_smoke&&training.tutorial.mission_id==PE_TUTORIAL_BUILD) {
                    pe_training_set_build_mode(&training,true);
                } else if(building_smoke&&training.tutorial.mission_id==PE_TUTORIAL_RECEIVERS&&building->active&&frame%12==0) {
                    if(!training.build_mode)pe_training_set_build_mode(&training,true);
                    unsigned ray_index=0;
                    while(ray_index<50&&!(building->result.receiver_mask&(UINT64_C(1)<<ray_index)))ray_index++;
                    if(ray_index<50) {
                        PeTrainingReceiverRay ray;pe_training_receiver_ray(ray_index,&ray);
                        build_ray=(Ray){vector(ray.origin),vector(ray.direction)};build_request=true;build_subtract=false;
                        if(build_added==1&&!smoke_removed) {
                            pe_training_receiver_ray(0,&ray);build_ray=(Ray){vector(ray.origin),vector(ray.direction)};build_subtract=true;
                        }
                        Vector3 delta=Vector3Subtract((Vector3){14,3,11},vector(training.position));
                        yaw=atan2f(delta.x,-delta.z);pitch=-10*DEG2RAD;
                    }
                } else if(gathering_smoke&&training.tutorial.mission_id==PE_TUTORIAL_GATHER&&gathering_assets) {
                    knife_drawn=false;
                    for(unsigned i=0;i<PE_GATHERING_HERBS;i++) {
                        if(training.gathering.herb_dead[i])continue;
                        Vector3 delta=Vector3Subtract(vector(pe_training_gathering_herb_position(i)),vector(training.position));
                        float distance=hypotf(delta.x,delta.z);
                        if(!pe_training_gathering_movement_locked(&training.gathering)) {
                            if(distance>1.5f){input.move_x=delta.x/distance;input.move_z=delta.z/distance;}
                            else if(!pe_halt_host_masked(&halt))pe_training_begin_herb(&training,i);
                        }
                        yaw=atan2f(delta.x,-delta.z);pitch=-20*DEG2RAD;break;
                    }
                } else if(combat_smoke&&training.tutorial.mission_id==PE_TUTORIAL_COMBAT&&melee.ready&&melee.target_hp>0) {
                    knife_drawn=true;
                    Vector3 delta=Vector3Subtract(vector(hare_motion.position),vector(training.position));
                    float distance=hypotf(delta.x,delta.z);
                    if(!melee.action.active) {
                        if(distance>1.05f){input.move_x=delta.x/distance;input.move_z=delta.z/distance;}
                        else {player_yaw=atan2f(delta.x,delta.z)*RAD2DEG;attack_pending=true;}
                    } else if(melee.action.combo_window)attack_pending=true;
                    yaw=atan2f(delta.x,-delta.z);pitch=-20*DEG2RAD;
                } else if(training.tutorial.mission_id==PE_TUTORIAL_LOCATION||training.tutorial.mission_id==PE_TUTORIAL_RETURN) {
                    Vector3 target=training.tutorial.mission_id==PE_TUTORIAL_LOCATION?(Vector3){7,1.5f,12}:(Vector3){32,4,12};
                    Vector3 delta=Vector3Subtract(target,vector(training.position));
                    float length=hypotf(delta.x,delta.z);
                    if(training.tutorial.mission_id==PE_TUTORIAL_RETURN&&training.andy_reached)pe_training_interact_andy(&training);
                    else if(length>.2f){input.move_x=delta.x/length;input.move_z=delta.z/length;}
                    if(frame%90==0&&training.grounded)jump_pending=true;
                    yaw=atan2f(delta.x,-delta.z);pitch=-20*DEG2RAD;
                }
            }
            if(modal){input=(PeTrainingInput){0};jump_pending=false;}
            if(build_request){build_pending=true;build_pending_ray=build_ray;build_pending_subtract=build_subtract;}
            accumulator += dt;
            int steps = 0;
            while (accumulator >= PE_TRAINING_DT && steps++ < 6) {
                input.jump = jump_pending;
                if(training.tutorial.mission_id==PE_TUTORIAL_MINE&&!terrain->initialized) {
                    pe_training_terrain_init(&terrain->world);terrain->initialized=true;
                    if(!pe_training_terrain_view_update(terrain)) {failed=true;quit=true;break;}
                    if(!pe_training_resolve_terrain(&training,scene,terrain->collision)) {
                        fprintf(stderr,"No clear capsule placement above new training mound\n");failed=true;quit=true;break;
                    }
                }
                if(terrain->initialized) {
                    PeTrainingDigUpdate update={0};
                    if(modal||!shovel_views[0].loaded)pe_training_dig_cancel(&training.dig);
                    else if(!pe_training_mine_step(&training,&terrain->world,point(dig_ray.position),point(dig_ray.direction),dig_held,
                             hypotf(input.move_x,input.move_z)>.0001f||input.jump,&update)) {
                        fprintf(stderr,"Training dig input rejected\n");failed=true;quit=true;break;
                    }
                    if(update.swung) {
                        dig_swings++;if(update.removed)dig_removals++;
                        printf("Training dig: frame %d sample %d,%d,%d removed %d dirt %u mission %u\n",frame,
                            update.x,update.y,update.z,update.removed,training.gathering.dirt,training.tutorial.mission_id);fflush(stdout);
                    }
                    if(training.dig.stage!=PE_DIG_IDLE)player_yaw=atan2f(dig_ray.direction.x,dig_ray.direction.z)*RAD2DEG;
                    if(!pe_training_terrain_view_update(terrain)) {failed=true;quit=true;break;}
                    input.terrain_scene=terrain->collision;
                }
                if(training.tutorial.mission_id==PE_TUTORIAL_RECEIVERS&&!building->active) {
                    if(!terrain->initialized||!pe_training_terrain_build_anchors(&terrain->world,false)) {failed=true;quit=true;break;}
                    building->active=true;
                    if(!pe_training_terrain_view_update(terrain)||!update_build_view(building)||
                       !pe_training_resolve_obstacles(&training,scene,terrain->collision,building->collision)) {failed=true;quit=true;break;}
                    input.terrain_scene=terrain->collision;
                }
                if(building->active) {
                    if(build_pending&&training.build_mode&&!pe_training_dialogue(&training)) {
                        PeTrainingBuildTarget target=pe_training_build_target(&terrain->world,&building->blocks,point(build_pending_ray.position),point(build_pending_ray.direction),build_pending_subtract);
                        if(target.hit) {
                            if(build_pending_subtract&&target.index!=SIZE_MAX) {
                                if(pe_training_build_remove(&building->blocks,&training.gathering.dirt,&target.index,1)){build_removed++;smoke_removed=true;}
                            }else if(!build_pending_subtract) {
                                target.cell.type=(uint8_t)((PE_B45_CUBE<<2)|build_rotation);
                                if(pe_training_build_add(&building->blocks,&training.gathering.dirt,&target.cell,1,training.position,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT))build_added++;
                            }
                        }
                        build_pending=false;
                    }
                    ReceiverContext context={&terrain->world,&building->blocks};
                    if(!pe_training_dialogue(&training)&&!pe_training_check_receivers(&training,receiver_query,&context,&building->result)) {failed=true;quit=true;break;}
                    if(training.tutorial.mission_id!=PE_TUTORIAL_RECEIVERS) {
                        if(!pe_training_build_clear(&building->blocks)||!pe_training_terrain_build_anchors(&terrain->world,true)) {failed=true;quit=true;break;}
                        building->active=false;
                        printf("Training building complete: %u added, %u removed, dirt %u, score %d\n",build_added,build_removed,training.gathering.dirt,building->result.score);fflush(stdout);
                    }
                    if(!update_build_view(building)||!pe_training_terrain_view_update(terrain)) {failed=true;quit=true;break;}
                    input.terrain_scene=terrain->collision;input.build_scene=building->collision;
                }
                input.tree_raycast_hit=tree.loaded&&pe_training_tree_hit(training.position,
                    (PeVec3){sinf(player_yaw*DEG2RAD),0,cosf(player_yaw*DEG2RAD)},NULL);
                input.animator_do_nothing=training.grounded&&!melee.action.active&&
                    hypotf(measured_velocity.x,measured_velocity.z)<.05f;
                if(training.replicator_open)input.cut_held=false;
                PeTrainingMeleeGate gate={.alive=training.hp>0,.knife_owned=training.knife_owned,
                    .knife_equipped=training.knife_equipped,.knife_drawn=knife_drawn,.grounded=training.grounded,
                    .third_person=training.camera_mode!=2,.action_allowed=!modal&&training.tutorial.mission_id==PE_TUTORIAL_COMBAT,
                    .move_requested=hypotf(input.move_x,input.move_z)>.0001f||input.jump,
                    .velocity=measured_velocity};
                if(attack_pending&&melee.ready) {
                    if(melee.action.active)pe_training_melee_queue(&melee);
                    else {
                        PeVec3 request,direction;
                        /* Stationary lesson attacks share the free-camera
                         * preference; captured modes retain camera facing. */
                        PeVec3 base=training.camera_mode==0?(PeVec3){input.move_x,0,input.move_z}:
                            (PeVec3){sinf(yaw),0,-cosf(yaw)};
                        if(!pe_melee_facing_request(base,settings.preferences.camera_facing_attacks,training.camera_mode==0,yaw,&request)||
                           !pe_melee_facing_direction(request,player_yaw,&direction)){
                            failed=true;quit=true;break;
                        }
                        if(pe_training_melee_begin(&melee,gate,&training.stamina)){
                            player_yaw=atan2f(direction.x,direction.z)*RAD2DEG;melee_swings++;
                        }
                    }
                }
                attack_pending=false;
                input.stamina_action=melee.ready&&melee.action.active;
                /* Source Action_Halt, as in the sandbox: releasing movement on the ground with empty
                 * hands refuses Move and Jump while the foot-stop effect runs (0.2-0.9 s). */
                PeHaltHostInput halt_input={.delta_time=PE_TRAINING_DT,.move_requested=hypotf(input.move_x,input.move_z)>.0001f,
                    .jump_requested=input.jump,.move_admissible=training.grounded,.grounded=training.grounded,.alive=training.hp>0,
                    .equipment_held=(training.axe_equipped&&training.axe_held)||(training.equipped_shovel>=0&&training.shovel_held)||(training.knife_equipped&&knife_drawn),
                    .velocity=training.velocity,.left_foot=training.position,.right_foot=training.position};
                PeHaltHostOutput halt_output;
                if(ik_ready){
                    PeIkDriveSample foot_samples[PE_ACTOR_BLEND_MAX];size_t foot_count=0;PePlayerLocomotionPose pose;PeVec3 left,right;
                    if(pe_player_locomotion_snapshot(&locomotion,&pose))for(int i=0;i<PE_LOCOMOTION_COUNT;i++)if(pose.weights[i]>0)
                        foot_samples[foot_count++]=(PeIkDriveSample){pe_player_locomotion_name(i),pose.seconds[i],(float)pose.weights[i]};
                    if(foot_count&&pe_ik_drive_feet(ik,foot_samples,foot_count,&left,&right)){
                        halt_input.left_foot=pe_ik_drive_world_vector(left,player_yaw);halt_input.right_foot=pe_ik_drive_world_vector(right,player_yaw);
                    }
                }
                if(!pe_halt_host_step(&halt,halt_input,&halt_output)){fprintf(stderr,"Invalid Halt state\n");failed=true;quit=true;break;}
                if(halt_output.started)halt_starts++;
                if(halt_output.block_move){input.move_x=input.move_z=0;input.sprint=false;halt_refusals++;}
                if(halt_output.block_jump)input.jump=jump_pending=false;
                bool was_jumping=training.jumping;
                PeVec3 previous_position=training.position;
                pe_training_step(&training, scene, input);
                npcs.seconds+=PE_TRAINING_DT;
                ui.seconds+=PE_TRAINING_DT;
                PeVec3 motion_velocity=training.velocity;
                motion_velocity.x=(training.position.x-previous_position.x)/PE_TRAINING_DT;
                motion_velocity.z=(training.position.z-previous_position.z)/PE_TRAINING_DT;
                /* Free third person turns into travel. Captured camera modes
                 * retain camera-facing body orientation so A/D can strafe. */
                bool action_facing=(melee.ready&&melee.action.active)||
                    training.dig.stage!=PE_DIG_IDLE||training.gathering.tree_stage!=PE_TREE_IDLE;
                if(!action_facing&&training.camera_mode!=0)
                    player_yaw=atan2f(sinf(yaw),-cosf(yaw))*RAD2DEG;
                else if(!action_facing&&hypotf(motion_velocity.x,motion_velocity.z)>.05f)
                    player_yaw=movement_heading(player_yaw,atan2f(motion_velocity.x,motion_velocity.z)*RAD2DEG,PE_TRAINING_DT);
                if(animated_player) {
                    PePlayerLocomotionInput motion={.velocity=motion_velocity,
                        .grounded=training.grounded,.walk=input.walk,.sprint=training.sprinting,
                        .jump_started=!was_jumping&&training.jumping,.directional=true,
                        .local_right=motion_velocity.x*cosf(player_yaw*DEG2RAD)-motion_velocity.z*sinf(player_yaw*DEG2RAD),
                        .local_forward=motion_velocity.x*sinf(player_yaw*DEG2RAD)+motion_velocity.z*cosf(player_yaw*DEG2RAD)};
                    if(!pe_player_locomotion_step(&locomotion,motion,PE_TRAINING_DT)){failed=true;quit=true;}
                    locomotion_seen|=1u<<locomotion.clip;
                }
                if(hare.loaded&&training.tutorial.mission_id==PE_TUTORIAL_COMBAT&&(!melee.ready||melee.target_hp>0)) {
                    /* Leisure only: target acquisition and contact are not yet ported. */
                    if(!pe_hare_step(&hare_motion,scene,NULL)){failed=true;quit=true;}
                    size_t next=hypotf(hare_motion.velocity.x,hare_motion.velocity.z)>.05f&&hare_walk!=PE_ACTOR_BIND_POSE?hare_walk:hare_idle;
                    if(next!=hare_clip){hare_clip=next;actor_seconds=0;}
                    actor_seconds+=PE_TRAINING_DT;
                }
                measured_velocity=motion_velocity;
                if(melee.ready) {
                    gate.grounded=training.grounded;gate.velocity=motion_velocity;
                    PeTrainingMeleeFrame combat_frame={.gate=gate,.player_position=training.position,
                        .target_position=hare_motion.position,.player_yaw=player_yaw,.player_scale=1,
                        .target_yaw=hare_motion.yaw_degrees,.target_scale=hare_motion.scale,
                        .target_clip=hare_clip,.target_seconds=actor_seconds,
                        /* Provisional fixed tutorial player/hare whitelist, not the general faction system. */
                        .attacker_eligible=training.tutorial.mission_id==PE_TUTORIAL_COMBAT,
                        .target_eligible=training.tutorial.mission_id==PE_TUTORIAL_COMBAT&&melee.target_hp>0};
                    PeTrainingMeleeUpdate update;
                    if(!pe_training_melee_step(&melee,&combat_frame,&training.stamina,&update)){failed=true;quit=true;}
                    else if(update.contact_status==PE_KNIFE_CONTACT_HIT) {
                        melee_hits++;
                        printf("Training contact: frame %d stage %u clip %.6f damage %.6f HP %.6f -> %.6f died %d\n",
                            frame,melee.action.stage,melee.action.clip_time,update.contact.applied_damage,
                            update.contact.hp_before,update.contact.hp_after,update.contact.died);
                        if(update.contact.died)pe_training_monster_died(&training,PE_HARE_PROTOTYPE,PE_HARE_SPAWN_ID);
                    }
                }
                jump_pending = false;
                accumulator -= PE_TRAINING_DT;
            }
            if (steps >= 6) accumulator = 0;
        } else { accumulator = 0; jump_pending = false; }
        pitch=Clamp(pitch,(training.camera_mode==2?-75.0f:-70.0f)*DEG2RAD,(training.camera_mode==2?75.0f:55.0f)*DEG2RAD);
        Camera3D camera = camera_for(&training, scene, yaw, pitch);
        int gallery_npc=npc_smoke&&frame>=1500?(frame-1500)/25:-1;
        if(gallery_npc>=0&&(size_t)gallery_npc<npcs.count) {
            const PeTrainingNpc *npc=pe_training_npc_at((size_t)gallery_npc);
            camera.target=Vector3Add(vector(npcs.positions[gallery_npc]),(Vector3){0,1,0});
            Vector3 direction={sinf(npc->yaw*DEG2RAD),.18f,cosf(npc->yaw*DEG2RAD)};
            direction=Vector3Normalize(direction);
            PeSceneHit wall=pe_scene_raycast(scene,point(camera.target),point(direction),3.5f);
            float distance=wall.hit?fmaxf(.4f,wall.distance-.2f):3.5f;
            camera.position=Vector3Add(camera.target,Vector3Scale(direction,distance));
        }
        if(training.character_open&&ui.preview.id&&player.loaded){
            BeginTextureMode(ui.preview);ClearBackground(BLANK);
            Camera3D preview_camera={(Vector3){0,1,3.8f},(Vector3){0,.94f,0},(Vector3){0,1,0},30,CAMERA_PERSPECTIVE};
            BeginMode3D(preview_camera);
            size_t idle=pe_actor_scene_view_find_clip(&player,"NormalIdle");
            if(!pe_actor_scene_view_draw_clip(&player,idle,ui.seconds,(Vector3){0},preview_yaw,1,WHITE))failed=true;
            EndMode3D();EndTextureMode();
        }
        BeginDrawing();
        ClearBackground((Color){18, 25, 31, 255});
        BeginMode3D(camera);
        pe_reference_draw(&reference);
        if(!pe_training_npc_view_draw(&npcs))failed=true;
        if(terrain->initialized)pe_training_terrain_view_draw(terrain);
        draw_build_view(building);
        if(building->active) {
            for(unsigned i=0;i<2;i++) {
                if(receiver_models[i].loaded)pe_reference_draw(&receiver_models[i]);
                else if(!i)DrawCube((Vector3){11.5f,3.5f,9.5f},3,3,.2f,WHITE);
                else DrawCube((Vector3){18.5f,1.5f,11.5f},3,.2f,3,WHITE);
            }
            ReceiverContext context={&terrain->world,&building->blocks};
            for(unsigned i=0;i<50;i++) {
                PeTrainingReceiverRay ray;PeTrainingReceiverHit hit;pe_training_receiver_ray(i,&ray);
                if(!receiver_query(&context,&ray,&hit)){failed=true;break;}
                Vector3 start=Vector3Add(vector(ray.origin),Vector3Scale(vector(ray.direction),15));
                Vector3 hit_point=hit.hit?Vector3Add(vector(ray.origin),Vector3Scale(vector(ray.direction),hit.distance)):(Vector3){0};
                /* Source uses absolute distance, including pre-emitter hits and default miss point. */
                Vector3 end=Vector3Add(start,Vector3Scale(vector(ray.direction),Vector3Distance(hit_point,start)-.01f));
                bool rejected=hit.hit&&hit.receiver;
                DrawLine3D(start,end,rejected?(Color){215,65,65,255}:(Color){60,220,145,255});
                PeReferenceModel *marker=&marker_models[rejected?1:0];
                if(marker->loaded) {
                    rlDrawRenderBatchActive();
                    rlSetBlendFactors(RL_ONE,RL_ONE,RL_FUNC_ADD);BeginBlendMode(BLEND_CUSTOM);
                    rlDisableBackfaceCulling();
                    rlDisableDepthMask();
                    DrawModelEx(marker->model,end,i<25?(Vector3){0,1,0}:(Vector3){1,0,0},i<25?180:90,(Vector3){1,1,1},WHITE);
                    rlEnableDepthMask();
                    rlEnableBackfaceCulling();
                    EndBlendMode();
                }
            }
            if(training.build_mode&&!modal) {
                Ray ray=GetScreenToWorldRay(captured?(Vector2){GetScreenWidth()/2.0f,GetScreenHeight()/2.0f}:GetMousePosition(),camera);
                bool subtract=IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT);
                PeTrainingBuildTarget target=pe_training_build_target(&terrain->world,&building->blocks,point(ray.position),point(ray.direction),subtract);
                if(target.hit)DrawCubeWires((Vector3){target.cell.x*.5f+.25f,target.cell.y*.5f+.25f,target.cell.z*.5f+.25f},.502f,.502f,.502f,subtract?RED:accent_color);
            }
        }
        if(gathering_assets&&training.tutorial.mission_id==PE_TUTORIAL_GATHER)
            for(unsigned i=0;i<PE_GATHERING_HERBS;i++)if(!training.gathering.herb_dead[i])draw_foliage(&herbs[i],foliage,cutoff_location,i==4?.649999976f:.5f);
        if(tree.loaded&&training.tutorial.mission_id==PE_TUTORIAL_TREE&&training.gathering.tree_hp)draw_foliage(&tree,foliage,cutoff_location,.25f);
        if(training.tutorial.mission_id==PE_TUTORIAL_LOCATION||training.tutorial.mission_id==PE_TUTORIAL_STORAGE) {
            if(kit.loaded)pe_reference_draw(&kit);
            else {DrawCube((Vector3){7,1.78f,12},1.0f,.56f,.7f,(Color){76,105,105,255});}
            DrawCircle3D((Vector3){7,1.52f,12},1,(Vector3){1,0,0},90,accent_color);
        }
        if(training.tutorial.mission_id==PE_TUTORIAL_RETURN&&!npcs.count) {
            DrawCapsule((Vector3){32,2.3f,12},(Vector3){32,3.5f,12},.3f,8,6,accent_color);
        }
        if(training.tutorial.mission_id==PE_TUTORIAL_COMBAT&&hare.loaded&&(!melee.ready||melee.target_hp>0))
            if(!pe_actor_view_draw(&hare,hare_clip,actor_seconds,vector(hare_motion.position),hare_motion.yaw_degrees,hare_motion.scale,(Color){232,232,232,255}))failed=true;
        int cut_phase=training.gathering.tree_stage>=PE_TREE_ENTRY&&training.gathering.tree_stage<=PE_TREE_EXIT?
            (int)training.gathering.tree_stage-PE_TREE_ENTRY:-1;
        PeIkDriveFrame ik_solved;
        if(player.loaded&&training.camera_mode!=2) {
            PeActorSceneView *body_view=&player;
            size_t clip=animated_player?player_clips[locomotion.clip]:0;
            double seconds=animated_player?locomotion.seconds:0;
            if(melee.ready&&melee.action.active){clip=melee.body_clip[melee.action.stage];seconds=melee.action.clip_time;}
            if(cut_phase>=0&&cut_body_view.loaded){body_view=&cut_body_view;clip=cut_clips[cut_phase];seconds=training.gathering.tree_time;}
            if(training.dig.stage!=PE_DIG_IDLE&&shovel_views[2].loaded) {
                body_view=&shovel_views[2];clip=pe_actor_scene_view_find_clip(body_view,pe_training_dig_clip(&training.dig));
                seconds=pe_training_dig_clip_seconds(&training.dig);
            }
            bool blending=animated_player&&!(melee.ready&&melee.action.active)&&cut_phase<0&&training.dig.stage==PE_DIG_IDLE;
            if(training.hp<=0)pe_grounder_host_reset(&grounder);
            if(ik_ready&&training.hp>0) {
                PeIkDriveSample ik_samples[PE_ACTOR_BLEND_MAX];size_t ik_count=0;PePlayerLocomotionPose pose;
                if(!blending){const PeActorClip *c=pe_actor_scene_view_clip_at(body_view,clip);if(c)ik_samples[ik_count++]=(PeIkDriveSample){c->name,seconds,1};}
                else if(pe_player_locomotion_snapshot(&locomotion,&pose))for(int i=0;i<PE_LOCOMOTION_COUNT;i++)if(pose.weights[i]>0)
                    ik_samples[ik_count++]=(PeIkDriveSample){pe_player_locomotion_name(i),pose.seconds[i],(float)pose.weights[i]};
                /* Held hand equipment switches the original's arm mapping off. */
                bool held=(training.axe_equipped&&training.axe_held)||(training.equipped_shovel>=0&&training.shovel_held)||(training.knife_equipped&&knife_drawn);
                /* The stop effect's world offsets, turned into the drawn model's frame. */
                PeVec3 ik_offsets[PE_FBBIK_EFFECTORS]={{0}};
                for(unsigned i=0;i<halt.offset_count;i++)ik_offsets[halt.offsets[i].effector]=pe_ik_drive_model_vector(halt.offsets[i].offset,player_yaw);
                /* Source GrounderFBBIK, once per drawn frame before the solve, as in the sandbox. */
                /* The tree collider is enabled for movement under the same condition (pe_training_step). */
                GroundScenes ground_scenes={{scene,terrain?terrain->collision:NULL,building?building->collision:NULL},
                    training.tutorial.mission_id==PE_TUTORIAL_TREE&&training.gathering.tree_hp>0};PeVec3 left,right;
                PeGrounderHostInput ground_input={.time=(float)(grounder_time+dt),.delta_time=dt,.root=training.position,.yaw_degrees=player_yaw,
                    .ground_contact=training.ground_contact,.ground_normal=training.ground_normal,.spine_effect=!held,.ray=training_ground_ray,.context=&ground_scenes};
                if(dt>0&&ik_count&&pe_ik_drive_feet(ik,ik_samples,ik_count,&left,&right)){
                    left=pe_ik_drive_world_vector(left,player_yaw);right=pe_ik_drive_world_vector(right,player_yaw);
                    ground_input.foot[0]=(PeVec3){training.position.x+left.x,training.position.y+left.y,training.position.z+left.z};
                    ground_input.foot[1]=(PeVec3){training.position.x+right.x,training.position.y+right.y,training.position.z+right.z};
                    if(pe_grounder_host_update(&grounder,&ground_input)){
                        grounder_time+=dt;grounder_updates++;grounder_planted+=grounder.grounded;
                        for(unsigned e=7;e<=8;e++)grounder_reach=fmaxf(grounder_reach,fabsf(grounder.effector_offset[e].y));
                    }
                }
                if(grounder.active)for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)
                    ik_offsets[e]=(PeVec3){ik_offsets[e].x+grounder.effector_offset[e].x,ik_offsets[e].y+grounder.effector_offset[e].y,ik_offsets[e].z+grounder.effector_offset[e].z};
                if(ik_count&&pe_ik_drive_solve_grounded(ik,ik_samples,ik_count,!held,ik_offsets,grounder.active?&grounder.pose:NULL,&ik_solved)) {
                    ik_draws++;
                    for(size_t i=0;i<ik_view_count;i++)pe_actor_scene_view_set_ik(ik_views[i].view,&ik_solved);
                }
            }
            if(!(blending?draw_locomotion(body_view,&locomotion,vector(training.position),player_yaw):
                pe_actor_scene_view_draw_clip(body_view,clip,seconds,vector(training.position),player_yaw,1,WHITE)))failed=true;
        } else if(!player.loaded)draw_avatar(&training);
        if(training.axe_equipped&&training.axe_held&&axe_view.loaded&&training.camera_mode!=2) {
            PeActorSceneView *tool_view=&axe_view;
            size_t clip=pe_actor_scene_view_find_clip(&axe_view,pe_player_locomotion_name(locomotion.clip));
            double seconds=locomotion.seconds;
            if(cut_phase>=0&&cut_axe_view.loaded){tool_view=&cut_axe_view;clip=cut_clips[cut_phase];seconds=training.gathering.tree_time;}
            if(!(animated_player&&cut_phase<0?draw_locomotion(tool_view,&locomotion,vector(training.position),player_yaw):
                pe_actor_scene_view_draw_clip(tool_view,clip,seconds,vector(training.position),player_yaw,1,WHITE)))failed=true;
        }
        if(training.equipped_shovel>=0&&shovel_views[0].loaded&&training.camera_mode!=2) {
            PeActorSceneView *tool_view=&shovel_views[training.shovel_held?0:1];
            const char *clip_name=pe_player_locomotion_name(locomotion.clip);double seconds=locomotion.seconds;
            if(training.dig.stage!=PE_DIG_IDLE) {
                tool_view=&shovel_views[3];clip_name=pe_training_dig_clip(&training.dig);seconds=pe_training_dig_clip_seconds(&training.dig);
            }
            size_t clip=pe_actor_scene_view_find_clip(tool_view,clip_name);
            if(!(animated_player&&training.dig.stage==PE_DIG_IDLE?draw_locomotion(tool_view,&locomotion,vector(training.position),player_yaw):
                pe_actor_scene_view_draw_clip(tool_view,clip,seconds,vector(training.position),player_yaw,1,WHITE)))failed=true;
        }
        if(training.knife_equipped&&knife_drawn&&drawn_view.loaded&&training.camera_mode!=2) {
            size_t clip=melee.action.active?melee.knife_clip[melee.action.stage]:
                pe_actor_scene_view_find_clip(&drawn_view,pe_player_locomotion_name(locomotion.clip));
            double seconds=melee.action.active?melee.action.clip_time:locomotion.seconds;
            if(!(!animated_player||melee.action.active?pe_actor_scene_view_draw_clip(&drawn_view,clip,seconds,vector(training.position),player_yaw,1,WHITE):
                draw_locomotion(&drawn_view,&locomotion,vector(training.position),player_yaw)))failed=true;
        } else if(training.knife_equipped&&stowed.loaded&&training.camera_mode!=2) {
            if(!(animated_stowed?draw_locomotion(&stowed,&locomotion,vector(training.position),player_yaw):
                pe_actor_scene_view_draw_clip(&stowed,0,0,vector(training.position),player_yaw,1,WHITE)))failed=true;
        } else if(training.knife_equipped&&knife.loaded&&training.camera_mode!=2) {
            /* Waist proxy only: the source hand/waist skeleton is not loaded. */
            Vector3 waist=Vector3Add(vector(training.position),(Vector3){.35f*cosf(yaw),.95f,.35f*sinf(yaw)});
            DrawModelEx(knife.model,waist,(Vector3){0,1,0},180-yaw*RAD2DEG,(Vector3){1,1,1},WHITE);
        }
        for(size_t i=0;i<ik_view_count;i++)pe_actor_scene_view_set_ik(ik_views[i].view,NULL);
        EndMode3D();
        if(training.tutorial.mission_id==PE_TUTORIAL_LOCATION||training.tutorial.mission_id==PE_TUTORIAL_STORAGE)
            draw_marker(&ui,(Vector3){7,3,12},"Emergency Kit  [E]",camera);
        if(training.tutorial.mission_id==PE_TUTORIAL_RETURN&&!npcs.count)draw_marker(&ui,(Vector3){32,4.4f,12},"Andy  [E]",camera);
        for(size_t i=0;i<npcs.count;i++) {
            const PeTrainingNpc *npc=pe_training_npc_at(i);
            Vector3 label=Vector3Add(vector(npcs.positions[i]),(Vector3){0,npcs.label_heights[i],0});
            Vector3 delta=Vector3Subtract(label,camera.position);float distance=Vector3Length(delta);
            if(distance>35)continue;
            PeSceneHit wall=pe_scene_raycast(scene,point(camera.position),point(delta),distance);
            if(wall.hit&&wall.distance<distance-.2f)continue;
            draw_name(&ui,Vector3Subtract(label,(Vector3){0,.25f,0}),npc->name,camera);
        }
        if(training.camera_mode!=2)draw_name(&ui,Vector3Add(vector(training.position),(Vector3){0,1.8f,0}),"Tutorial",camera);
        if(tree.loaded&&training.tutorial.mission_id==PE_TUTORIAL_TREE)
            draw_marker(&ui,Vector3Add(vector(pe_training_tree_center()),(Vector3){0,4,0}),"Tree",camera);
        if(terrain->initialized&&training.tutorial.mission_id==PE_TUTORIAL_MINE) {
            draw_marker(&ui,(Vector3){6,4,12},"Dirt mound",camera);
            if(training.shovel_held&&!modal) {
                Ray ray=GetScreenToWorldRay(captured?(Vector2){GetScreenWidth()/2.0f,GetScreenHeight()/2.0f}:GetMousePosition(),camera);
                PeHit hit=pe_training_terrain_dig_target(&terrain->world,point(ray.position),point(ray.direction),training.position,3);
                if(hit.hit) {
                    PeVec3 selected=pe_training_terrain_to_world(&terrain->world,(PeVec3){hit.x,hit.y,hit.z});
                    BeginMode3D(camera);DrawCubeWires(vector(selected),1,1,1,accent_color);EndMode3D();
                }
            }
        }
        if(gathering_assets&&training.tutorial.mission_id==PE_TUTORIAL_GATHER)
            for(unsigned i=0;i<PE_GATHERING_HERBS;i++)if(!training.gathering.herb_dead[i])
                draw_marker(&ui,Vector3Add(vector(pe_training_gathering_herb_position(i)),(Vector3){0,1,0}),
                    training.gathering.herb_active[i]?"Gathering...":"Herb  [E]",camera);
        if(melee.ready&&training.tutorial.mission_id==PE_TUTORIAL_COMBAT&&melee.target_hp>0)
            draw_marker(&ui,Vector3Add(vector(hare_motion.position),(Vector3){0,2.2f,0}),TextFormat("Lepus Hare %.0f / %.0f",melee.target_hp,melee.setup.target_max_hp),camera);
        pe_training_ui_record_dialogue(&ui,&training);
        pe_training_ui_draw(&ui,&training,(PeTrainingUiState){.paused=paused,.free_cursor=free_cursor,
            .autorun=autorun,.walk=walk,.log=log,.dragging=dragging,.inventory_tab=inventory_tab,
            .melee_ready=melee.ready,.knife_drawn=knife_drawn,.hare_hp=melee.target_hp,.heading=player_yaw,
            .menu=menu_open,.cursor_visible=focused&&!captured&&IsCursorOnScreen(),
            .receivers_blocked=50u-(unsigned)__builtin_popcountll(building->result.receiver_mask)});
        pe_ui_settings_draw(&settings,ui.assets);
        if(extended_smoke&&screenshot&&!storage_ui_written&&training.kit_open&&!training.kit_transferred) {
            storage_ui_written=true;if(!capture_training_frame(screenshot,"-storage-ui.png"))failed=true;
        }
        if(extended_smoke&&screenshot&&!inventory_ui_written&&training.inventory_open&&training.juice_count>0) {
            inventory_ui_written=true;if(!capture_training_frame(screenshot,"-inventory-ui.png"))failed=true;
        }
        if(extended_smoke&&screenshot&&!quickbar_ui_written&&training.quickslot_item_id==916&&training.juice_count==10) {
            quickbar_ui_written=true;if(!capture_training_frame(screenshot,"-quickbar-ui.png"))failed=true;
        }
        if(screenshot&&gallery_npc>=0&&(size_t)gallery_npc<npcs.count&&(frame-1500)%25==20) {
            char suffix[40];snprintf(suffix,sizeof suffix,"-npc-%d.png",pe_training_npc_at((size_t)gallery_npc)->id);
            if(!capture_training_frame(screenshot,suffix))failed=true;
        }
        if(building_smoke&&screenshot&&!build_screenshot_written&&build_added>=3&&building->active) {
            build_screenshot_written=true;if(!capture_training_frame(screenshot,"-build.png"))failed=true;
        }
        if(dig_swings&&dig_capture_delay<3)dig_capture_delay++;
        if(mining_smoke&&screenshot&&!dig_screenshot_written&&dig_capture_delay==3) {
            dig_screenshot_written=true;if(!capture_training_frame(screenshot,"-dig.png"))failed=true;
        }
        if(melee_hits&&contact_capture_delay<3)contact_capture_delay++;
        if(combat_smoke&&screenshot&&contact_capture_delay==3&&!contact_screenshot_written) {
            contact_screenshot_written=true;
            if(!capture_training_frame(screenshot,"-contact.png")){fprintf(stderr,"Training contact screenshot failed\n");failed=true;}
        }
        if(gathering_smoke&&screenshot&&!herb_screenshot_written&&training.tutorial.mission_id==PE_TUTORIAL_GATHER) {
            bool gathering=false;for(unsigned i=0;i<PE_GATHERING_HERBS;i++)gathering|=training.gathering.herb_active[i];
            if(gathering){herb_screenshot_written=true;if(!capture_training_frame(screenshot,"-herb.png"))failed=true;}
        }
        if(training.tutorial.mission_id==PE_TUTORIAL_TREE&&training.gathering.tree_hp<7&&tree_capture_delay<3)tree_capture_delay++;
        if(crafting_smoke&&screenshot&&tree_capture_delay==3&&!tree_screenshot_written) {
            tree_screenshot_written=true;if(!capture_training_frame(screenshot,"-tree.png"))failed=true;
        }
        if(crafting_smoke&&screenshot&&!craft_screenshot_written&&training.gathering.crafting&&training.craft_elapsed_ticks>30) {
            craft_screenshot_written=true;if(!capture_training_frame(screenshot,"-craft.png"))failed=true;
        }
        /* Read the rendered back buffer before swapping, including transient
           one-frame UI states in automated playthroughs. */
        if(screenshot&&smoke_frames&&frame+1>=smoke_frames&&!capture_training_frame(screenshot,NULL)){
            fprintf(stderr,"Training screenshot export failed: %s\n",screenshot);failed=true;
        }
        EndDrawing();
        frame++;
        if (smoke_frames && frame >= smoke_frames) {
            if(npc_smoke&&npcs.count!=PE_TRAINING_NPC_COUNT) {
                fprintf(stderr,"NPC smoke requires all nine recovered characters\n");failed=true;
            }
            unsigned expected=building_smoke?PE_TUTORIAL_CREATION:(mining_smoke?PE_TUTORIAL_BUILD:(crafting_smoke?PE_TUTORIAL_MINE:(gathering_smoke?PE_TUTORIAL_TREE:(combat_smoke?PE_TUTORIAL_GATHER:(extended_smoke?PE_TUTORIAL_COMBAT:PE_TUTORIAL_MISSION_PANEL)))));
            if (smoke_frames >= 90 && (training.tutorial.mission_id != expected || (extended_smoke&&(training.juice_count!=9||!training.knife_owned||(!crafting_smoke&&!training.knife_equipped)||pe_training_dialogue(&training))))) {
                fprintf(stderr, "Training smoke did not reach mission %u with expected inventory\n",expected);
                failed = true;
            }
            if(crafting_smoke&&(training.gathering.wood!=11||training.gathering.shovel!=2||training.gathering.script!=0||
                !training.gathering.formula1||(!mining_smoke&&!training.axe_equipped)||training.knife_equipped)) {
                fprintf(stderr,"Training crafting smoke failed source item conservation\n");failed=true;
            }
            if(mining_smoke&&(training.gathering.dirt!=9006-build_added||dig_removals!=3||training.equipped_shovel!=0||
                training.axe_equipped||terrain->triangles||!shovel_views[3].loaded||training.shovels[1].durability!=3553)) {
                fprintf(stderr,"Training mining smoke failed real edit / resource / shovel conservation\n");failed=true;
            }
            if(building_smoke&&(!building->result.complete||building->result.score!=50||!build_added||build_removed!=1||
                building->blocks.count||building->mesh.count||building->active)) {
                fprintf(stderr,"Training building smoke failed actual receiver / cleanup / edit checks\n");failed=true;
            }
            if(gathering_smoke&&(!gathering_assets||training.gathering.herbs_depleted!=5||training.gathering.axe!=1)) {
                fprintf(stderr,"Training gathering smoke requires all five actual herbs and one axe\n");failed=true;
            }
            if(combat_smoke&&(!melee.ready||melee.target_hp>0||melee_hits==0)) {
                fprintf(stderr,"Training combat smoke requires actual blade contacts and hare death\n");failed=true;
            }
            quit = true;
        }
    }
    if(smoke_frames&&frame<smoke_frames){fprintf(stderr,"Training smoke interrupted before requested frame count\n");failed=true;}
    if (captured) EnableCursor();
    printf("Training room: %d frames, mission %u, camera mask %u, movement mask %u, position %.3f %.3f %.3f\n",
           frame, training.tutorial.mission_id, training.tutorial.camera_objectives,
           training.tutorial.movement_objectives, training.position.x, training.position.y, training.position.z);
    printf("Training visuals: recovered diffuse room textures; %s.\n",
           animated_player?"recovered starter character with twelve directional locomotion clips":
           (player.loaded?"recovered starter character at recorded idle pose":"temporary capsule avatar"));
    if(animated_player)printf("Training locomotion: clip %s, seconds %.3f, exercised mask %u; smoothed pose weights, sprint uses run fallback.\n",
        pe_player_locomotion_name(locomotion.clip),locomotion.seconds,locomotion_seen);
    printf("Training items: Herbal Juice %d; kit transferred %d; Andy reached %d\n",training.juice_count,training.kit_transferred,training.andy_reached);
    printf("Training NPCs: %zu of %u recovered characters, idle clock %.3f\n",npcs.count,PE_TRAINING_NPC_COUNT,npcs.seconds);
    pe_training_npc_view_unload(&npcs);
    pe_training_ui_unload(&ui);
    if(hare.loaded)printf("Training hare: mode %d, position %.3f %.3f %.3f, decisions %u\n",hare_motion.mode,hare_motion.position.x,hare_motion.position.y,hare_motion.position.z,hare_motion.decisions);
    printf("Training equipment: knife owned %d, equipped %d; HP %.0f / %.0f\n",training.knife_owned,training.knife_equipped,training.hp,training.max_hp);
    printf("Training gathering: %u herbs depleted, axe %d, ledger mission %d\n",training.gathering.herbs_depleted,
        pe_training_inventory_count(&training,1527),training.gathering.mission);
    for(unsigned i=0;i<PE_GATHERING_HERBS;i++)pe_reference_unload(&herbs[i]);
    pe_reference_unload(&tree);
    printf("Training crafting: wood %u, script %u, shovel %u, formula %d, axe equipped %d\n",training.gathering.wood,
        training.gathering.script,training.gathering.shovel,training.gathering.formula1,training.axe_equipped);
    printf("Training mining: %u swings, %u removals, dirt %u, shovel %d, durability %.0f/%.0f, triangles %zu\n",dig_swings,
        dig_removals,training.gathering.dirt,training.equipped_shovel,training.shovels[0].durability,training.shovels[1].durability,terrain?terrain->triangles:0);
    printf("Training building: %u added, %u removed, score %d, cells %zu\n",build_added,build_removed,
        building?building->result.score:0,building?building->blocks.count:0);
    if(building){pe_mesh_free(&building->mesh);pe_scene_free(building->collision);free(building);}
    for(unsigned i=0;i<2;i++){pe_reference_unload(&receiver_models[i]);pe_reference_unload(&marker_models[i]);}
    if(foliage.id&&foliage.id!=rlGetShaderIdDefault())UnloadShader(foliage);
    if(marker_shader.id&&marker_shader.id!=rlGetShaderIdDefault())UnloadShader(marker_shader);
    for(unsigned i=0;i<4;i++)pe_actor_scene_view_unload(&shovel_views[i]);
    pe_training_terrain_view_unload(terrain);free(terrain);
    printf("Training IK: %llu poses drawn through the solve\n",ik_draws);
    printf("Training grounder: %llu updates, %llu grounded, largest foot request %.3f m\n",grounder_updates,grounder_planted,grounder_reach);
    printf("Training halt: %llu stops, %llu steps with the mask held\n",halt_starts,halt_refusals);
    if(melee.ready)printf("Training melee: %u starts, %u contacts, hare HP %.3f / %.3f\n",melee_swings,melee_hits,melee.target_hp,melee.setup.target_max_hp);
    pe_actor_scene_view_unload(&axe_view);pe_actor_scene_view_unload(&cut_body_view);pe_actor_scene_view_unload(&cut_axe_view);
    pe_hitboxes_free(drawn_boxes);pe_hitboxes_free(hare_boxes);pe_actor_scene_view_unload(&drawn_view);
    pe_actor_view_unload(&hare);
    pe_actor_scene_view_unload(&player);
    pe_actor_scene_view_unload(&stowed);
    pe_ik_drive_free(ik);
    pe_reference_unload(&knife);
    pe_reference_unload(&kit);
    pe_reference_unload(&reference);
    pe_scene_free(scene);
    CloseWindow();
    if(smoke_frames)printf("Training result: %s\n",failed?"FAIL":"PASS");
    return failed ? 1 : (return_main ? 2 : 0);
}
