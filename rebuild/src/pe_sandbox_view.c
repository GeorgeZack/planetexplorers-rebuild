#define _POSIX_C_SOURCE 200809L
#include "../generated/pe_player_collider_center_source.h"
#include "pe_deployable_view.h"
#include "pe_adventure_hunt.h"
#include "../generated/pe_torch_source.h"
#include "pe_sandbox_view.h"
#include "pe_sandbox_ui.h"
#include "pe_sandbox_actor.h"
#include "pe_sandbox_actions.h"
#include "pe_sandbox_melee.h"
#include "pe_sword_variant.h"
#include "pe_running_attack_motion.h"
#include "pe_sword_root_motion.h"
#include "pe_hunt_combat.h"
#include "pe_actor_view.h"
#include "pe_sandbox_camera.h"
#include "pe_sandbox_world_view.h"
#include "pe_settlement.h"
#include "pe_town_interaction.h"
#include "pe_town_wallet.h"
#include "pe_camp_repair.h"
#include "pe_camp_repair_interaction.h"
#include "pe_build_history.h"
#include "pe_vegetation.h"
#include "pe_vegetation_view.h"
#include "../generated/pe_adventure_population.h"
#include "pe_reference.h"
#include "pe_terrain_material_view.h"
#include "pe_adventure_npc.h"
#include "pe_adventure_npc_view.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* Opt-in fixed-storage diagnostics. No timing feeds back into simulation.
 * First-frame breadcrumbs locate stalls before a frame can finish. Aggregate
 * wall/process-CPU totals are reported at 120 frames and on exit. Present time
 * includes the existing frame limiter, buffer swap and input polling. */
enum { PROFILE_SETUP,PROFILE_ENCOUNTERS,PROFILE_UI_ROUTE,PROFILE_HOST,
    PROFILE_QUERIES,PROFILE_SIM,PROFILE_REMESH,PROFILE_POSE,PROFILE_UI_PREPARE,
    PROFILE_WORLD_DRAW,PROFILE_ACTOR_DRAW,PROFILE_NPC_DRAW,PROFILE_UI_DRAW,
    PROFILE_PRESENT,PROFILE_COUNT };
typedef struct {
    bool enabled;
    int stage;
    unsigned frames,focused_frames,paused_frames;
    double wall_start,cpu_start,wall[PROFILE_COUNT],wall_max[PROFILE_COUNT];
    double cpu[PROFILE_COUNT],cpu_max[PROFILE_COUNT];
    unsigned calls[PROFILE_COUNT];
} SandboxProfile;
static const char *const profile_names[PROFILE_COUNT]={"setup","encounters","ui_route","host_input",
    "queries","sim_collision","terrain_remesh","actor_pose","ui_prepare",
    "world_draw","actor_draw","npc_draw","ui_draw","present"};
static double profile_wall(void){
    struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;
    return (double)t.tv_sec+(double)t.tv_nsec*1e-9;
}
static void profile_stage(SandboxProfile *p,int stage){
    if(!p->enabled)return;
    double wall=profile_wall(),cpu=(double)clock()/CLOCKS_PER_SEC;
    if(p->stage>=0){
        int i=p->stage;double w=fmax(0,wall-p->wall_start),c=fmax(0,cpu-p->cpu_start);
        p->wall[i]+=w;p->cpu[i]+=c;p->wall_max[i]=fmax(p->wall_max[i],w);p->cpu_max[i]=fmax(p->cpu_max[i],c);p->calls[i]++;
    }
    p->stage=stage;p->wall_start=wall;p->cpu_start=cpu;
    if(!p->frames&&stage>=0){fprintf(stderr,"Sandbox profile: enter %s (first frame/setup)\n",profile_names[stage]);fflush(stderr);}
}
static void profile_report(const SandboxProfile *p,const char *reason){
    if(!p->enabled)return;
    fprintf(stderr,"Sandbox profile %s: frames=%u focused=%u paused=%u; milliseconds wall total/max, process CPU total/max\n",
        reason,p->frames,p->focused_frames,p->paused_frames);
    for(int i=0;i<PROFILE_COUNT;i++)if(p->calls[i])fprintf(stderr,"  %-15s calls=%u wall=%.3f/%.3f cpu=%.3f/%.3f\n",
        profile_names[i],p->calls[i],p->wall[i]*1000,p->wall_max[i]*1000,p->cpu[i]*1000,p->cpu_max[i]*1000);
    fflush(stderr);
}
static Vector3 rv(PeVec3 p) { return (Vector3){p.x,p.y,p.z}; }
static PeVec3 pv(Vector3 p) { return (PeVec3){p.x,p.y,p.z}; }
/* Unity source coordinates are left-handed. Reflect clip X for presentation;
 * retain source XYZ throughout simulation/collision and invert screen queries. */
static Ray source_ray(Vector2 point,Camera3D camera){
    point.x=GetScreenWidth()-point.x;return GetScreenToWorldRay(point,camera);
}
static Vector2 source_screen(Vector3 point,Camera3D camera){
    Vector2 p=GetWorldToScreen(point,camera);p.x=GetScreenWidth()-p.x;return p;
}
static void source_begin_3d(Camera3D camera){
    BeginMode3D(camera);Matrix projection=rlGetMatrixProjection();
    projection.m0=-projection.m0;rlSetMatrixProjection(projection);
    rlSetCullFace(RL_CULL_FACE_FRONT);
}
static void source_end_3d(void){
    EndMode3D();rlSetCullFace(RL_CULL_FACE_BACK);
}

typedef struct {PeAdventureNpc npc;PeAdventureNpcView view;bool visible;} Resident;
static bool residents_refresh(Resident *residents,const PeGame *g,const PeSandboxCollision *collision,const char *assets,char *error,size_t cap){
    for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++){
        Resident *r=&residents[i];const PeAdventurePopulationSource *s=&pe_adventure_population_sources[i];r->visible=false;
        if(!pe_settlement_matches(&g->world)||g->mode!=PE_GAME_ADVENTURE||s->adventure_role==1||s->adventure_role==2)continue;
        if((s->adventure_role==3||s->adventure_role==4)&&!g->arrival_complete)continue;
        if(s->adventure_role==5&&g->welcome_page<3)continue;
        PeVec3 p={s->position[0]-g->world.origin_x,s->position[1]-g->world.origin_y,s->position[2]-g->world.origin_z};
        if(p.x<0||p.z<0||p.y<0||p.x>=pe_world_width(&g->world)||p.z>=pe_world_depth(&g->world)||p.y>=pe_world_size_y(&g->world))continue;
        if(!r->view.actor.loaded){
            char path[4096];int n=snprintf(path,sizeof path,"%s/%s",assets,s->scene);
            if(n<0||(size_t)n>=sizeof path||!pe_adventure_npc_view_load_scene(&r->view,path,error,cap))return false;
        }
        r->visible=pe_adventure_npc_refresh_source(&r->npc,&g->world,collision,p,s->yaw_degrees,error,cap);
    }
    return true;
}
static PeAdventureNpc *resident_entity(Resident *residents,uint32_t entity){
    for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)if(residents[i].visible&&(uint32_t)pe_adventure_population_sources[i].entity_id==entity)return &residents[i].npc;
    return NULL;
}
static void load_town_name(PeSandboxUI *ui,const PeGame *game,const char *assets){
    ui->start_town_name[0]=0;
    /* Town names are observed metadata of this bounded reference world. */
    if(pe_settlement_matches(&game->world)){
        char path[4096];int n=snprintf(path,sizeof path,"%s/adventure-route/town-name.txt",assets);
        FILE *f=n>0&&(size_t)n<sizeof path?fopen(path,"r"):NULL;
        if(f){if(fgets(ui->start_town_name,sizeof ui->start_town_name,f))ui->start_town_name[strcspn(ui->start_town_name,"\r\n")]=0;fclose(f);}
    }
}
static Color block_color(int item) {
    if (item == PE_ITEM_STONE) return (Color){143,156,160,255};
    if (item == PE_ITEM_DIRT) return (Color){145,111,81,255};
    return (Color){184,141,90,255};
}
static bool update_mesh(const PeWorld *world, Model *model, bool *loaded, size_t *triangles,int region,const PeTerrainMaterialView *surface) {
    PeMeshData data = {0};
    int o[3],n[3];
    if(!pe_world_region(world,region,o,n)||!pe_world_mesh_region(world,o[0],o[1],o[2],n[0],n[1],n[2],&data))return false;
    if (data.count > INT_MAX/3) {pe_mesh_free(&data);return false;}
    if (!data.count) {
        if (*loaded) UnloadModel(*model);
        *loaded=false; *triangles=0; pe_mesh_free(&data); return true;
    }
    Mesh mesh = {0};
    mesh.vertexCount=(int)data.count;
    mesh.triangleCount=(int)(data.count/3);
    mesh.vertices=MemAlloc((unsigned int)(data.count*3*sizeof(float)));
    mesh.normals=MemAlloc((unsigned int)(data.count*3*sizeof(float)));
    mesh.texcoords=MemAlloc((unsigned int)(data.count*2*sizeof(float)));
    mesh.texcoords2=MemAlloc((unsigned int)(data.count*2*sizeof(float)));
    mesh.colors=MemAlloc((unsigned int)(data.count*4));
    if (!mesh.vertices || !mesh.normals || !mesh.colors || !mesh.texcoords || !mesh.texcoords2) {
        MemFree(mesh.vertices); MemFree(mesh.normals); MemFree(mesh.colors);
        MemFree(mesh.texcoords);MemFree(mesh.texcoords2);
        pe_mesh_free(&data); return false;
    }
    for (size_t i=0;i<data.count;i++) {
        PeVertex v=data.vertices[i];
        mesh.vertices[i*3]=v.position.x; mesh.vertices[i*3+1]=v.position.y; mesh.vertices[i*3+2]=v.position.z;
        mesh.normals[i*3]=v.normal.x; mesh.normals[i*3+1]=v.normal.y; mesh.normals[i*3+2]=v.normal.z;
        size_t triangle=i-i%3;
        mesh.texcoords[i*2]=(float)pe_terrain_source_material(world->source,data.vertices[triangle].material);
        mesh.texcoords[i*2+1]=(float)pe_terrain_source_material(world->source,data.vertices[triangle+1].material);
        mesh.texcoords2[i*2]=(float)pe_terrain_source_material(world->source,data.vertices[triangle+2].material);
        mesh.texcoords2[i*2+1]=0;
        mesh.colors[i*4]=i%3==0?255:0;
        mesh.colors[i*4+1]=i%3==1?255:0;
        mesh.colors[i*4+2]=i%3==2?255:0;
        mesh.colors[i*4+3]=255;
    }
    UploadMesh(&mesh,false);
    /* Keep the previous render region until its replacement owns usable GPU
       buffers and a complete model. UploadMesh reports failure via the mesh. */
    if(!mesh.vboId||!mesh.vboId[0]||!mesh.vboId[2]||!mesh.vboId[3]||!mesh.vboId[1]||!mesh.vboId[5]){
        UnloadMesh(mesh);pe_mesh_free(&data);return false;
    }
    Model candidate=LoadModelFromMesh(mesh);
    if(candidate.meshCount!=1||!candidate.meshes||candidate.materialCount<1||
       !candidate.materials||!candidate.materials[0].maps||!candidate.meshMaterial){
        bool owns_mesh=candidate.meshes&&candidate.meshCount>0&&candidate.meshes[0].vertices==mesh.vertices;
        /* UnloadModel expects its arrays whenever the corresponding count is
           nonzero; tolerate a partially constructed allocation failure. */
        if(!candidate.meshes)candidate.meshCount=0;
        if(!candidate.materials)candidate.materialCount=0;
        UnloadModel(candidate);
        if(!owns_mesh)UnloadMesh(mesh);
        pe_mesh_free(&data);return false;
    }
    if(!pe_terrain_material_view_bind(surface,&candidate)){UnloadModel(candidate);pe_mesh_free(&data);return false;}
    if (*loaded) UnloadModel(*model);
    *model=candidate; *loaded=true;
    *triangles=data.count/3; pe_mesh_free(&data); return true;
}
typedef struct {
    Model model[PE_WORLD_MAX_REGIONS];
    bool loaded[PE_WORLD_MAX_REGIONS],ready;
    size_t triangles[PE_WORLD_MAX_REGIONS],uploads;
    PeWorld previous;
} TerrainCache;
static bool update_terrain(const PeWorld *world,TerrainCache *cache,size_t *triangles,const PeTerrainMaterialView *surface) {
    *triangles=0;
    for(int i=0;i<PE_WORLD_MAX_REGIONS;i++) {
        if(i>=pe_world_region_count(world)){
            if(cache->loaded[i])UnloadModel(cache->model[i]);
            cache->loaded[i]=false;cache->triangles[i]=0;continue;
        }
        int o[3],n[3];if(!pe_world_region(world,i,o,n))return false;
        if(!cache->ready||pe_world_mesh_region_changed(world,&cache->previous,o[0],o[1],o[2],n[0],n[1],n[2])) {
            if(!update_mesh(world,&cache->model[i],&cache->loaded[i],&cache->triangles[i],i,surface))return false;
            cache->uploads++;
        }
        *triangles+=cache->triangles[i];
    }
    if(!pe_world_clone(&cache->previous,world))return false;
    cache->ready=true;return true;
}
static void free_terrain(TerrainCache *cache) {
    if(!cache)return;
    for(int i=0;i<PE_WORLD_MAX_REGIONS;i++)if(cache->loaded[i])UnloadModel(cache->model[i]);
    pe_world_destroy(&cache->previous);free(cache);
}
static void draw_block(PeBlock b) {
    Vector3 c={(float)b.x+.5f,(float)b.y+.5f,(float)b.z+.5f};
    Color color=block_color(b.item_id);
    if (b.item_id==PE_ITEM_FENCE || b.item_id==PE_ITEM_LADDER) {
        DrawCube((Vector3){c.x-.38f,c.y,c.z},.13f,1,.15f,color);
        DrawCube((Vector3){c.x+.38f,c.y,c.z},.13f,1,.15f,color);
        int bars=b.item_id==PE_ITEM_FENCE?2:4;
        for(int i=0;i<bars;i++) DrawCube((Vector3){c.x,(float)b.y+.2f+(float)i*.6f/(float)(bars-1),c.z},.88f,.12f,.12f,color);
    } else {
        DrawCube(c,1,1,1,color);
        DrawCubeWires(c,1.003f,1.003f,1.003f,Fade(BLACK,.15f));
    }
}
static void draw_cell(const PeB45Cell *cell) {
    PeVertex vertices[PE_B45_MAX_VERTICES];
    size_t count=pe_b45_mesh(cell,vertices,PE_B45_MAX_VERTICES);
    Color base=block_color(pe_b45_material_item(cell->material));
    for(size_t i=0;i<count;i+=3) {
        PeVec3 n=vertices[i].normal;
        float light=.58f+.42f*fmaxf(0,n.x*-.35f+n.y*.85f+n.z*.4f);
        Color color={(unsigned char)(base.r*light),(unsigned char)(base.g*light),(unsigned char)(base.b*light),255};
        DrawTriangle3D(rv(vertices[i].position),rv(vertices[i+1].position),rv(vertices[i+2].position),color);
    }
}

static void draw_build_preview(const PeB45Cell *cells,size_t count,bool allowed){
    Color fill=allowed?(Color){60,220,145,70}:(Color){230,80,75,70};
    Color edge=allowed?(Color){60,220,145,255}:(Color){230,80,75,255};
    rlDrawRenderBatchActive();rlDisableDepthMask();
    for(size_t c=0;c<count;c++){
        PeVertex vertices[PE_B45_MAX_VERTICES];
        size_t n=pe_b45_mesh(&cells[c],vertices,PE_B45_MAX_VERTICES);
        for(size_t i=0;i<n;i+=3){
            Vector3 a=rv(vertices[i].position),b=rv(vertices[i+1].position),d=rv(vertices[i+2].position);
            DrawTriangle3D(a,b,d,fill);
            DrawLine3D(a,b,edge);DrawLine3D(b,d,edge);DrawLine3D(d,a,edge);
        }
    }
    rlDrawRenderBatchActive();rlEnableDepthMask();
}

static void interact(PeGame *g) {
    const PeSlot *tool=pe_game_equipped_tool(g);
    if(g->mode==PE_GAME_ADVENTURE&&tool&&pe_sandbox_actions_supported(tool->item_id))
        snprintf(g->message,sizeof g->message,"Face the target and hold left mouse to use your tool.");
    else pe_game_gather(g);
}

/* Activity uses the same tree-excluding immutable snapshot as camera geometry.
 * The eye anchor and small capsule/rise remain native framing adapters. */
static bool sandbox_camera_ray(void *context,PeVec3 origin,PeVec3 direction,float maximum,uint32_t mask,float *distance){
    const PeSandboxCollision *collision=context;
    if(!distance||mask!=PE_SANDBOX_CAMERA_OBSTACLE_MASK||!pe_sandbox_collision_stats(collision).generation)return false;
    PeSceneHit hit=pe_sandbox_collision_ray_geometry(collision,origin,direction,maximum);
    *distance=hit.hit?hit.distance:maximum;return true;
}
static bool sandbox_camera_frame(PeSandboxCamera *orbit,PeSandboxCameraRecovery *recovery,
    const PeGame *game,const PeSandboxCollision *collision,PeSandboxCameraInput input){
    if(game->mode!=PE_GAME_ADVENTURE)return pe_sandbox_camera_step(orbit,input);
    float activity=50;
    if(input.third_person&&!input.paused&&input.dt>0&&
       !pe_sandbox_camera_activity((PeVec3){game->player.x,game->player.y+1.58f,game->player.z},
           sandbox_camera_ray,(void *)collision,&activity))return false;
    return pe_sandbox_camera_follow_step(orbit,recovery,input,activity);
}
static bool sandbox_camera_feedback_frame(PeSandboxCamera *orbit,PeSandboxCameraRecovery *recovery,
    PeGameMode mode,PeSandboxCameraInput input,bool overview,float clamped_distance){
    if(mode!=PE_GAME_ADVENTURE||!input.third_person||overview||input.paused||input.dt<=0||clamped_distance<0)return true;
    return pe_sandbox_camera_feedback(orbit,recovery,clamped_distance,true);
}
static Camera3D game_camera(const PeGame *g,const PeSandboxCollision *collision,bool third,bool overview,float distance,float *clamped_distance) {
    if(clamped_distance)*clamped_distance=-1;
    Vector3 direction={sinf(g->yaw)*cosf(g->pitch),sinf(g->pitch),-cosf(g->yaw)*cosf(g->pitch)};
    Vector3 eye={g->player.x,g->player.y+1.58f,g->player.z};
    Camera3D camera={eye,Vector3Add(eye,direction),{0,1,0},60,CAMERA_PERSPECTIVE};
    if(third) {
        camera.target=Vector3Add(eye,Vector3Scale(direction,2));
        Vector3 delta=Vector3Scale(direction,-distance);delta.y+=.5f;
        PeSceneHit hit=pe_sandbox_collision_sweep_geometry(collision,pv((Vector3){eye.x,eye.y-.1f,eye.z}),.1f,.2f,pv(delta));
        float f=hit.hit?fmaxf(.05f,hit.fraction-.02f):1;
        camera.position=Vector3Add(eye,Vector3Scale(delta,f));
        if(hit.hit&&clamped_distance)*clamped_distance=distance*f;
    }
    if(overview){camera.target=rv(g->player);camera.position=Vector3Add(camera.target,(Vector3){9,6,pe_settlement_matches(&g->world)?-11:11});camera.fovy=62;}
    return camera;
}
/* Construction transactions retain their player-origin reach validation. Aim
 * through the pointer at the closest visible terrain/cell, then trace from the
 * player's eye to that point so a third-person camera cannot extend reach. */
static PeVec3 construction_direction(const PeGame *g,Ray ray,PeVec3 eye) {
    PeHit h=pe_world_raycast(&g->world,pv(ray.position),pv(ray.direction),100);
    float nearest=h.hit?h.distance:100;
    for(int i=0;i<g->cell_count;i++){
        PeB45Hit hit;
        if(pe_b45_raycast(&g->cells[i],pv(ray.position),pv(ray.direction),nearest,&hit))nearest=hit.distance;
    }
    for(int i=0;i<g->block_count;i++){
        const PeBlock *b=&g->blocks[i];
        BoundingBox bounds={{b->x,b->y,b->z},{b->x+1,b->y+1,b->z+1}};
        RayCollision hit=GetRayCollisionBox(ray,bounds);
        if(hit.hit&&hit.distance>=0&&hit.distance<nearest)nearest=hit.distance;
    }
    Vector3 target=Vector3Add(ray.position,Vector3Scale(ray.direction,nearest));
    return pv(Vector3Normalize(Vector3Subtract(target,rv(eye))));
}

static bool sandbox_action_pose(PeSandboxActor *actor,PeGame *game,PeSandboxActions *action,const PeSandboxMelee *melee,bool drawn){
    PeSandboxActionPose pose;if(!pe_sandbox_actions_pose(action,game,&pose))return false;
    const PeSlot *item=pe_game_equipped_tool(game);
    if(item&&item->item_id==PE_TORCH_ITEM&&pe_sandbox_actor_hold_ready(actor,PE_TORCH_ITEM))
        pose=(PeSandboxActionPose){.item_id=PE_TORCH_ITEM,.instance_id=item->instance_id,.held=drawn&&game->hp>0};
    if(!pe_sandbox_actor_set_action(actor,&pose))return false;
    if(item&&(item->item_id==1||item->item_id==33)&&pe_sandbox_actor_hold_ready(actor,item->item_id)){
        bool held=drawn&&game->hp>0;
        if(item->item_id==1&&!(melee&&melee->action.active)&&!pe_sandbox_actor_sword_ground_ready(actor,game))held=false;
        return pe_sandbox_actor_set_melee_item(actor,item->item_id,item->instance_id,held,melee?&melee->action:NULL);
    }
    return true;
}
static bool sandbox_blade_owned(const PeSlot *item,const PeSandboxMelee *melee){
    if(!item||!melee||!melee->ready||item->item_id!=melee->setup.weapon_item_id||
       item->count!=1||!item->instance_id||!isfinite(item->durability))return false;
    /* Source PeSword can still attack after its equipment buff is exhausted. */
    const PeItemDef *def=pe_item_def(item->item_id);
    return def&&item->durability<=def->durability_max&&
        (item->item_id==1?item->durability>=0:item->item_id==33&&item->durability>0);
}
/* One active-sandbox process clock survives loads and graphics/menu round trips.
 * Native pause/focus policy freezes scaled time; it is not the world/day clock. */
static PeAttributeClock sandbox_attribute_clock;
static bool sandbox_health_frame(PeAttributeClock *clock,PeGame *game,double dt,bool focused,bool paused){
    if(!clock||!game||!isfinite(dt)||dt<0)return false;
    PeAttributeClock next=*clock;PeAttributeFrame frame;
    bool active=focused&&!paused;
    if(!pe_attribute_clock_step(&next,active?dt:0,&frame))return false;
    PePassiveRecovery recovery=game->passive_recovery;float hp=game->hp;
    PeStaminaRecovery stamina=game->stamina_recovery;
    PeRevival revival=game->revival;
    PeStaminaVitals vitals={game->stamina,game->stamina_max,game->stamina_regen,game->stamina_recover_delay,game->comfort};
    if(active&&game->mode==PE_GAME_ADVENTURE&&
       (!pe_revival_frame(&revival,frame,hp>0)||
        !pe_passive_recovery_frame(&recovery,frame,&hp,game->max_hp)||
        !pe_stamina_recovery_frame(&stamina,&game->passive_recovery,&recovery,frame,&vitals)))return false;
    *clock=next;game->passive_recovery=recovery;game->hp=hp;
    game->revival=revival;
    game->stamina_recovery=stamina;game->stamina=vitals.stamina;game->comfort=vitals.comfort;return true;
}
/* One observed world-clock update per active display frame, independent of
 * player life, shop visibility and fixed-step catch-up. Native pause/focus
 * policy applies; original entity activation/order is not reproduced. */
static bool sandbox_wallet_frame(PeGame *game,bool focused,bool paused){
    if(!game)return false;
    return !focused||paused||pe_town_wallet_step(game);
}
/* One source UTimer step per display, never per movement catch-up tick.
 * Native pause/focus/menu/load policy suspends it; closing the UI or death
 * does not. Successful-load frames continue before reaching this adapter. */
static bool sandbox_repair_frame(PeGame *game,float dt,bool focused,bool paused){
    bool completed=false;
    if(!pe_game_repair_frame(game,dt,focused&&!paused,&completed))return false;
    if(completed)snprintf(game->message,sizeof game->message,"Repair complete. Collect your item at the Repair Machine.");
    return true;
}
/* Closing the panel never releases the machine's item/job ownership. The
 * caller retains modal input ownership for this entire cancellation frame. */
static bool sandbox_repair_close_invalid(PeSandboxUI *ui,const PeGame *game,
    const PeSandboxCollision *collision,bool model_ready){
    if(!ui||!ui->repair_open||pe_camp_repair_in_range(game,collision,model_ready))return false;
    pe_sandbox_ui_cancel_repair(ui);return true;
}
/* The selected machine competes with the same NPC/Torch/herb ray distances.
 * Opening owns the original press until release, just like other town modals. */
static bool sandbox_repair_interact(PeSandboxUI *ui,PeGame *game,
    const PeSandboxCollision *collision,bool model_ready,bool enabled,bool requested,
    bool modal_priority,PeSceneHit picked,float other_distance,PeSandboxUIResult *action,
    PeSandboxPointerGate *pointer,unsigned buttons){
    if(!ui||!game||!action||!pointer||!enabled||!requested||modal_priority||!picked.hit||
       !isfinite(picked.distance)||picked.distance<=0||isnan(other_distance)||picked.distance>=other_distance||
       action->consume_world||action->consume_actions||action->paused||!action->npc_interaction||
       !pe_camp_repair_in_range(game,collision,model_ready)||!pe_sandbox_ui_open_repair(ui,game,true))return false;
    action->consume_world=true;action->cursor_visible=true;action->npc_interaction=false;action->camera_cycle_allowed=false;
    pe_sandbox_pointer_release_gate(pointer,true,buttons);return true;
}
/* One admitted display-frame command, before any camera/pick queries. The
 * renderer reads cells directly; collision needs an immediate refresh even
 * when this display frame has no fixed simulation step. */
static bool sandbox_build_history_apply(PeGame *game,const PeSandboxUI *ui,
    PeSandboxUIResult action,bool enabled,PeSandboxCollision *collision,char *error,size_t capacity){
    if(!game||!ui||!collision)return false;
    if(!enabled||!ui->building||action.paused||ui->paused||ui->town_open||ui->repair_open||ui->revival_open||
       ui->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||ui->inventory_open||ui->character_open||
       ui->missions_open||ui->replicator_open||ui->menu_open||ui->dragging||game->hp<=0)return true;
    bool changed=action.undo_build?pe_build_history_undo(game):action.redo_build?pe_build_history_redo(game):false;
    return !changed||pe_game_collision_update(game,collision,error,capacity);
}
typedef struct {
    unsigned index;
    float distance,other_distance;
    bool enabled,available,requested,modal_priority;
    const PeSandboxMelee *melee;
    const PeSandboxActions *tool;
} SandboxVegetationInput;
/* A gathering attempt owns this action press, including rejection. It does
 * not borrow tutorial gather clocks or pause movement/other creatures. */
static bool sandbox_vegetation_interact(PeGame *game,const PeSandboxCollision *collision,
    SandboxVegetationInput input,PeSandboxUIResult *action,PeSandboxPointerGate *pointer,unsigned buttons){
    if(!game||!collision||!action||!pointer||!input.melee||!input.tool||!input.enabled||!input.available||!input.requested||input.modal_priority||
       !action->npc_interaction||action->consume_world||action->consume_actions||action->paused||
       input.index>=game->vegetation.count||!isfinite(input.distance)||input.distance<0||
       isnan(input.other_distance)||input.distance>=input.other_distance)return false;
    /* DBActionRelation Gather requires SwordAttack/Dig/Fell masks false.
     * Reject while their native owners run; do not cancel or freeze them. */
    if(!input.melee->action.active&&!input.melee->tick_pending&&input.tool->stage==PE_SANDBOX_ACTION_IDLE)
        pe_vegetation_harvest(game,input.index,collision);
    else snprintf(game->message,sizeof game->message,"Finish the current tool or weapon action before gathering.");
    action->consume_actions=true;action->npc_interaction=false;action->camera_cycle_allowed=false;
    pe_sandbox_pointer_route(pointer,action,buttons);
    return true;
}
static bool sandbox_revive_request(PeGame *game,const PeAttributeClock *clock){
    if(!game||!clock)return false;
    /* The native actor currently has no ragdoll, ride or Fall action owner.
     * This follows source model-present/no-ragdoll readiness; it does not
     * pretend to play the original death/get-up animation. */
    PeRevivalGate gate={.death_mask=game->hp==0,.getup_ready=true,.item_skill_ready=true};
    PeAttributeFrame frame={(float)clock->seconds,clock->frame,clock->frame==clock->attribute_frame};
    return pe_game_revive(game,frame,gate);
}
static bool sandbox_hunt_runs(const PeSandboxUI *ui,PeSandboxUIResult action){
    /* The get-up modal owns player input, not the other actors' clocks. */
    return !action.consume_world||ui->revival_open;
}
static bool sandbox_melee_motion(const PeRootMotion *running,const PeSwordRootMotion *ordinary,
    const PeCombatInterval *interval,PeVec3 velocity,double yaw,double dt,PeVec3 *request){
    if(!interval||!interval->valid||!request||!isfinite(interval->from)||!isfinite(interval->to)||
       interval->from<0||interval->from>interval->to||!isfinite(dt)||dt<=0||!isfinite(yaw)||
       !isfinite(velocity.x)||!isfinite(velocity.y)||!isfinite(velocity.z)||
       !pe_combat_variant_clip(interval->gender,interval->view,interval->variant,interval->stage))return false;
    if(interval->variant==PE_COMBAT_RUNNING&&interval->stage==0)
        return pe_running_attack_motion_request(running,interval,velocity,yaw,dt,request);
    if(ordinary)return pe_sword_root_motion_request(ordinary,interval,velocity,yaw,dt,request);
    /* Earlier asset groups retain their explicit stationary zero-root adapter. */
    *request=(PeVec3){0};return true;
}
static void sandbox_health_contacts(PeGame *game,uint64_t previous_hits,uint64_t committed_hits){
    if(game&&committed_hits!=previous_hits&&game->mode==PE_GAME_ADVENTURE)
        pe_passive_recovery_hurt(&game->passive_recovery,game->hp);
}
static bool sandbox_blade_prepare(PeSandboxActor *actor,const char *assets,int item_id,
        PeSandboxMelee *out,PeHitboxes **capsule,char *error,size_t capacity){
    if(!pe_sandbox_actor_hold_ready(actor,item_id))return true;
    const PeActor *body=NULL,*weapon=NULL;char path[4096];
    const char *folder=item_id==1?"player-wood-sword-drawn/wood-sword-drawn.pehb":"player-knife-drawn/knife-drawn.pehb";
    int n=snprintf(path,sizeof path,"%s/%s",assets,folder);const PeItemDef *item=pe_item_def(item_id);
    if(n<0||(size_t)n>=sizeof path||!item||!pe_sandbox_actor_melee_assets_for_item(actor,item_id,&body,&weapon)||
       !(*capsule=pe_hitboxes_load(path,weapon,error,capacity)))return false;
    PeSandboxMeleeSetup setup={.weapon_item_id=item_id,.body=body,.drawn_weapon=weapon,.drawn_capsule=*capsule,
        .gender=PE_COMBAT_FEMALE,.player_id=1,.attack=pe_player_defaults.attack+item->attack,
        .stamina_reduce_percent=pe_player_defaults.stamina_reduce_percent};
    setup.history_enabled=true;setup.playback_enabled=true;
    /* Source local bounds, distinct from native movement radius/height. */
    const float *center=pe_player_collider_center_source[setup.gender].local_center;
    setup.history_local_center=(PeVec3){center[0],center[1],center[2]};
    return pe_sandbox_melee_init(out,&setup);
}
static void sandbox_blade_rebind(PeSandboxMelee *blade,const PeSandboxMelee *model){
    /* Source reuses the character's Sword action when equipment changes. */
    PeAttackPlayback playback=blade->playback;
    pe_sandbox_melee_cancel(blade);*blade=*model;blade->playback=playback;
}
static bool hunt_player_pose(PeSandboxActor *actor,PeGame *game,const PeActor *defence,const PeSandboxMelee *melee,PeHuntCombatPlayer *out){
    if(!pe_outfit_state_valid(game))return false;
    PeSandboxBodyPose pose;if(!pe_sandbox_actor_body_pose(actor,game,&pose))return false;
    size_t clip=pe_actor_find_clip(defence,pose.clip);const PeActorClip *definition=pe_actor_clip(defence,clip);
    if(!definition||fabs(definition->duration-pose.duration)>1e-6||definition->loop!=pose.loop)return false;
    PeHuntCombatPlayer next=(PeHuntCombatPlayer){.position=game->player,.velocity=game->velocity,.yaw=pose.yaw,
        .radius=PE_PLAYER_RADIUS,.height=PE_PLAYER_HEIGHT,.defense=pe_player_defaults.defense,.outfit=&game->outfit,
        .max_hp=game->max_hp,.alive=game->hp>0,.grounded=game->grounded,.clip=clip,.seconds=pose.seconds,
        .defence_disabled=pe_revival_damage_disabled(&game->revival)};
    const PeSlot *weapon=pe_game_equipped_tool(game);
    if(weapon&&weapon->item_id==1&&melee&&melee->ready&&melee->setup.weapon_item_id==1){
        next.weapon=&game->inventory.slots[weapon-game->inventory.slots];
        next.base_attack=pe_player_defaults.attack;
    }
    next.blend_count=pose.blend_count;
    for(size_t i=0;i<pose.blend_count;i++){
        size_t c=pe_actor_find_clip(defence,pose.blend[i].clip);const PeActorClip *d=pe_actor_clip(defence,c);
        if(!d||fabs(d->duration-pose.blend[i].duration)>1e-6||d->loop!=pose.blend[i].loop)return false;
        next.blend[i]=(PeActorBlendSample){c,pose.blend[i].seconds,pose.blend[i].weight};
    }
    *out=next;return true;
}
int pe_sandbox_view(PeGame *game,const char *assets,const char *save_path,
                    int smoke_frames,const char *smoke_panel,const char *screenshot) {
    const char *profile_env=getenv("PE_SANDBOX_PROFILE");
    const char *combat_trace_env=getenv("PE_SANDBOX_COMBAT_TRACE");
    bool combat_trace=combat_trace_env&&!strcmp(combat_trace_env,"1");
    unsigned combat_trace_lines=0;
    SandboxProfile profile={.enabled=profile_env&&!strcmp(profile_env,"1"),.stage=-1};
    profile_stage(&profile,PROFILE_SETUP);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_WINDOW_RESIZABLE);
    InitWindow(1280,720,"Planet Explorers | C rebuild");
    if(!IsWindowReady()){fprintf(stderr,"Unable to initialize graphics\n");profile_stage(&profile,-1);profile_report(&profile,"initialization failed");return 1;}
    SetWindowMinSize(1000,700);SetTargetFPS(60);SetExitKey(KEY_NULL);
    char error[256]={0};int result=0,frame=0;
    PeDeployableView deploy_view={0};
    PeVegetationView *vegetation_view=NULL;
    PeActorView hunt_view={0};PeActor *player_defence=NULL;PeHitboxes *hare_boxes=NULL,*player_boxes=NULL;
    PeHuntCombat hunt={0};bool hunt_assets=false,hunt_attempted=false,return_prompted=false;
    PeTerrainMaterialView surface={0};PeReferenceModel settlement={0};bool settlement_route=false;
    TerrainCache *terrain=calloc(1,sizeof *terrain);
    PeSandboxCollision *collision=pe_sandbox_collision_create();
    PeSandboxUI ui={0};PeSandboxActor *actor=NULL;PeSandboxWorldView *world_view=NULL;size_t triangles=0;
    PeAdventureNpc supplier={0};PeAdventureNpcView supplier_view={0};bool supplier_visible=false;
    PeAdventureNpc welcome={0};PeAdventureNpcView welcome_view={0};bool welcome_visible=false,welcome_prompted=false;
    Resident residents[PE_ADVENTURE_POPULATION_COUNT]={0};
    PeSandboxMapNpc resident_markers[PE_ADVENTURE_POPULATION_COUNT];
    double supplier_seconds=0;
    PeSandboxActions tool_action;pe_sandbox_actions_init(&tool_action,false);
    PeSandboxCamera orbit;pe_sandbox_camera_init(&orbit);
    PeSandboxCameraRecovery camera_recovery;pe_sandbox_camera_recovery_init(&camera_recovery);
    PeSandboxCameraControls camera_controls;pe_sandbox_camera_controls_init(&camera_controls);
    Vector2 right_press_pointer={0};
    bool tool_drawn=false;uint32_t tool_instance=0;uint64_t tool_edits=0;
    bool placing_torch=false,attack_pending=false;
    PeRootMotion *running_motion=NULL;PeSwordRootMotion *ordinary_motion=NULL;
    PeSandboxMelee melee={0},blade_templates[2]={{0}};PeHitboxes *knife_capsule=NULL,*sword_capsule=NULL;uint64_t melee_swings=0,running_starts=0,walking_starts=0;
    bool captured=false,walk=false,autorun=false,jump_pending=false,third_person=true,build_mode=false;
    if(!terrain||!collision||!pe_terrain_material_view_load(&surface,assets,error,sizeof error)||
       !update_terrain(&game->world,terrain,&triangles,&surface)||
       !pe_game_collision_update(game,collision,error,sizeof error)||
       !pe_sandbox_ui_load(&ui,assets,error,sizeof error)||
       !pe_deployable_view_load(&deploy_view,assets,error,sizeof error)) {
       fprintf(stderr,"Sandbox initialization failed: %s\n",error);result=1;goto cleanup;
    }
    vegetation_view=pe_vegetation_view_load(assets,error,sizeof error);
    if(!vegetation_view||(game->vegetation.installed&&!pe_vegetation_view_available(vegetation_view))){
        if(vegetation_view)snprintf(error,sizeof error,"This vegetation patch requires recovered sandbox-vegetation assets.");
        fprintf(stderr,"Sandbox vegetation initialization failed: %s\n",error);result=1;goto cleanup;
    }
    if(game->mode==PE_GAME_ADVENTURE&&pe_sandbox_collision_overlap(collision,game->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT)) {
        snprintf(error,sizeof error,"Player overlaps world geometry");result=1;goto cleanup;
    }
    actor=pe_sandbox_actor_load(assets,error,sizeof error);
    pe_sandbox_actor_reset_motion(actor,game);
    if(!actor){fprintf(stderr,"Sandbox actor load failed: %s\n",error);result=1;goto cleanup;}
    if(!sandbox_blade_prepare(actor,assets,33,&blade_templates[0],&knife_capsule,error,sizeof error)||
       !sandbox_blade_prepare(actor,assets,1,&blade_templates[1],&sword_capsule,error,sizeof error)){
        if(!*error)snprintf(error,sizeof error,"Blade body, attachment and source collision clocks do not match");
        result=1;goto cleanup;
    }
    melee=blade_templates[0];
    /* Optional recovered combat group: absence keeps the explicit quest gate. */
    {char path[4096],texture[4096];
        snprintf(path,sizeof path,"%s/hare.peac",assets);snprintf(texture,sizeof texture,"%s/hare-diffuse.png",assets);
        if(pe_actor_view_load(&hunt_view,path,texture,error,sizeof error)){
            snprintf(path,sizeof path,"%s/hare.pehb",assets);hare_boxes=pe_hitboxes_load(path,hunt_view.actor,error,sizeof error);
            snprintf(path,sizeof path,"%s/player-defence/player-defence.peac",assets);player_defence=pe_actor_load(path,error,sizeof error);
            snprintf(path,sizeof path,"%s/player-defence/player-defence.pehb",assets);
            if(player_defence)player_boxes=pe_hitboxes_load(path,player_defence,error,sizeof error);
            hunt_assets=hare_boxes&&player_boxes&&melee.ready;
        }
        if(!hunt_assets)fprintf(stderr,"Sandbox hunt unavailable: %s\n",error);
        *error=0;
    }
    {char path[4096];
        int n=snprintf(path,sizeof path,"%s/player-running-motion/running-knife.perm",assets);
        if(n<0||(size_t)n>=sizeof path){snprintf(error,sizeof error,"Running motion path too long");result=1;goto cleanup;}
        if(FileExists(path)){
            running_motion=pe_root_motion_load(path,error,sizeof error);
            const PeActorClip *body=pe_actor_clip(melee.setup.body,pe_actor_find_clip(melee.setup.body,"RunSwordAttack"));
            const PeActorClip *defence=pe_actor_clip(player_defence,pe_actor_find_clip(player_defence,"RunSwordAttack"));
            if(!running_motion||!pe_sandbox_actor_running_ready(actor)||!body||!defence||
               body->duration!=defence->duration||body->fps!=defence->fps||body->frames!=defence->frames||
               body->loop!=defence->loop||body->terminal_at_duration!=defence->terminal_at_duration){
                snprintf(error,sizeof error,"Running knife requires qualified motion and matching body/knife/defence assets");result=1;goto cleanup;
            }
            for(unsigned k=0;k<2;k++)if(blade_templates[k].ready){
                PeSandboxMeleeSetup setup=blade_templates[k].setup;
                if(!pe_sandbox_actor_running_ready_for_item(actor,setup.weapon_item_id)){
                    snprintf(error,sizeof error,"Running blade source clocks unavailable");result=1;goto cleanup;
                }
                setup.running_motion_ready=true;
                if(!pe_sandbox_melee_init(&blade_templates[k],&setup)){
                    snprintf(error,sizeof error,"Running blade source clocks unavailable");result=1;goto cleanup;
                }
            }
            melee=blade_templates[0];
        }
    }
    {char path[4096];
        int n=snprintf(path,sizeof path,"%s/player-sword-motion/sword-root.pesr",assets);
        if(n<0||(size_t)n>=sizeof path){snprintf(error,sizeof error,"Ordinary motion path too long");result=1;goto cleanup;}
        if(FileExists(path)){
            ordinary_motion=pe_sword_root_motion_load(path,error,sizeof error);
            if(!ordinary_motion){result=1;goto cleanup;}
            for(unsigned k=0;k<2;k++)if(blade_templates[k].ready){
                PeSandboxMeleeSetup setup=blade_templates[k].setup;
                static const char *names[]={"Attack6","Attack7","Attack8","Attack9"};
                for(unsigned stage=0;stage<4;stage++){
                    const PeCombatClip *source=pe_combat_clip(setup.gender,PE_COMBAT_THIRD_PERSON,stage);
                    const PeActorClip *body=pe_actor_clip(setup.body,pe_actor_find_clip(setup.body,names[stage]));
                    const PeActorClip *defence=pe_actor_clip(player_defence,pe_actor_find_clip(player_defence,names[stage]));
                    if(!source||!body||!defence||body->duration!=pe_sword_root_motion_duration(ordinary_motion,(uint32_t)source->path_id)||
                       body->duration!=defence->duration||body->fps!=defence->fps||body->frames!=defence->frames||
                       body->loop||defence->loop||body->terminal_at_duration!=defence->terminal_at_duration){
                        snprintf(error,sizeof error,"Ordinary blade motion requires matching source body/blade/defence clocks");result=1;goto cleanup;
                    }
                }
                setup.ordinary_motion_ready=true;
                if(!pe_sandbox_melee_init(&blade_templates[k],&setup)){
                    snprintf(error,sizeof error,"Ordinary blade source clocks unavailable");result=1;goto cleanup;
                }
            }
            melee=blade_templates[0];
        }
    }
    world_view=pe_sandbox_world_view_load(assets,error,sizeof error);
    if(!world_view){result=1;goto cleanup;}
    if(game->mode==PE_GAME_ADVENTURE&&(!pe_adventure_npc_view_load(&supplier_view,assets,error,sizeof error)||
       !pe_adventure_npc_view_load_role(&welcome_view,assets,1,error,sizeof error))){result=1;goto cleanup;}
    ui.welcome_portrait=welcome_view.portrait.texture;
    ui.supplier_portrait=supplier_view.portrait.texture;
    ui.supplier_entity=PE_ADVENTURE_NPC_ENTITY;
    load_town_name(&ui,game,assets);
    pe_sandbox_ui_set_preview(&ui,pe_sandbox_actor_preview,actor);
    if(smoke_panel) {
        ui.inventory_open=!strcmp(smoke_panel,"inventory");
        ui.replicator_open=!strcmp(smoke_panel,"crafting");
        ui.character_open=!strcmp(smoke_panel,"character");
        ui.missions_open=!strcmp(smoke_panel,"missions");
        build_mode=!strcmp(smoke_panel,"building");
    }
    uint32_t mesh_revision=game->world.revision;
    int selected_item=PE_ITEM_WOOD,build_shape=PE_B45_CUBE,build_rotation=0,build_brush=4;
    double accumulator=0;float dig_cooldown=0;int look_settle=0;
    PeSandboxPointerGate pointer_gate={0};
    printf("Sandbox ready: mode %d\n",game->mode);fflush(stdout);
    profile_stage(&profile,-1);
    while(!WindowShouldClose()) {
        profile_stage(&profile,PROFILE_ENCOUNTERS);
        if(settlement.loaded&&settlement_route!=pe_settlement_is_route(&game->world))pe_reference_unload(&settlement);
        if(pe_settlement_matches(&game->world)&&!settlement.loaded){
            settlement_route=pe_settlement_is_route(&game->world);
            char path[4096];int n=snprintf(path,sizeof path,"%s/%s",assets,settlement_route?"adventure-route/route.obj":"adventure-settlement/settlement.obj");
            if(n<0||(size_t)n>=sizeof path||!pe_reference_load(&settlement,path,error,sizeof error)){result=1;break;}
        }
        float dt=fminf(GetFrameTime(),.1f);
        supplier_visible=game->mode==PE_GAME_ADVENTURE&&(!pe_settlement_matches(&game->world)||game->welcome_page>=3)&&
            (pe_settlement_matches(&game->world)?
                pe_adventure_npc_refresh_source(&supplier,&game->world,collision,pe_settlement_npc_anchor_for_world(&game->world,2),pe_settlement_npc_yaw(2),error,sizeof error):
                pe_adventure_npc_refresh(&supplier,&game->world,collision,error,sizeof error));
        welcome_visible=game->mode==PE_GAME_ADVENTURE&&
            (pe_settlement_matches(&game->world)?
                pe_adventure_npc_refresh_source(&welcome,&game->world,collision,pe_settlement_npc_anchor_for_world(&game->world,1),pe_settlement_npc_yaw(1),error,sizeof error):
                pe_adventure_npc_refresh_at(&welcome,&game->world,collision,(PeVec3){28,0,35},error,sizeof error));
        ui.welcome_present=welcome_visible;ui.welcome_position=welcome.position;
        ui.supplier_present=supplier_visible;ui.supplier_position=supplier.position;
        if(!residents_refresh(residents,game,collision,assets,error,sizeof error)){result=1;break;}
        bool repair_model_ready=settlement.loaded&&settlement_route==pe_settlement_is_route(&game->world);
        bool host_cancelled_modal=sandbox_repair_close_invalid(&ui,game,collision,repair_model_ready);
        size_t marker_count=0;
        for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)if(residents[i].visible)resident_markers[marker_count++]=(PeSandboxMapNpc){(uint32_t)pe_adventure_population_sources[i].entity_id,residents[i].npc.position};
        pe_sandbox_ui_set_resident_markers(&ui,resident_markers,marker_count);
        uint32_t talking_id=ui.dialogue.entity_id;
        const PeAdventureNpc *talking=talking_id==PE_ADVENTURE_WELCOME_ENTITY&&welcome_visible?&welcome:
            talking_id==PE_ADVENTURE_NPC_ENTITY&&supplier_visible?&supplier:resident_entity(residents,talking_id);
        if(ui.dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE&&(!talking||game->hp<=0||
           (ui.dialogue.mission_id!=9127&&Vector3Distance(rv(game->player),rv(talking->position))>9))){pe_adventure_dialogue_cancel(&ui.dialogue);host_cancelled_modal=true;}
        const PeAdventureNpc *trader=resident_entity(residents,ui.town_entity);
        if(ui.town_open&&(ui.deploy_entity?!pe_deployable_in_range(game,ui.deploy_entity):(!trader||game->hp<=0||Vector3Distance(rv(game->player),rv(trader->position))>9))){pe_sandbox_ui_cancel_town(&ui);host_cancelled_modal=true;}
        if(!ui.paused&&(smoke_frames||IsWindowFocused())){
            if(supplier_visible)pe_adventure_arrival_step(game,supplier.spawn_position);
            pe_adventure_annot_step(game);pe_adventure_hunt_step(game);pe_adventure_hunt_return_step(game);
        }
        if(game->mode==PE_GAME_ADVENTURE&&game->hunt_state==PE_HUNT_ENCOUNTER_PENDING&&!hunt_attempted){
            hunt_attempted=true;
            if(hunt_assets&&!pe_hunt_combat_prepare(&hunt,&game->world,collision,hunt_view.actor,hare_boxes,
                player_defence,player_boxes,game->hunt_kills,error,sizeof error))
                snprintf(game->message,sizeof game->message,"Hunt unavailable: %.130s",error);
        }
        /* Arrival queues source story replies independently of the NPC pick
         * distance. Other modal UI keeps ownership until it closes. Cancel may
         * be resumed by talking to Naoko; load restores pending presentation. */
        if(!return_prompted&&pe_adventure_hunt_return_talk(game)&&!build_mode&&!ui.paused&&!ui.town_open&&!ui.repair_open&&
           !ui.inventory_open&&!ui.character_open&&!ui.missions_open&&!ui.replicator_open&&!ui.menu_open&&!ui.dragging&&
           ui.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&(smoke_frames||IsWindowFocused())){
            for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)if(residents[i].visible&&pe_adventure_population_sources[i].adventure_role==3){
                if(pe_sandbox_ui_open_annot(&ui,game,(uint32_t)pe_adventure_population_sources[i].entity_id,residents[i].view.portrait.texture,true)){
                    return_prompted=true;host_cancelled_modal=true;
                }
                break;
            }
        }
        ui.hunt_encounter_available=hunt.encounter.ready&&hunt.encounter.active;
        ui.building=build_mode;ui.build_item=selected_item;ui.build_shape=build_shape;
        ui.build_rotation=build_rotation;ui.build_brush=build_brush;
        ui.camera_cursor=camera_controls.mode==PE_SANDBOX_CAMERA_FREE&&!captured;
        ui.discard_input=host_cancelled_modal;
        profile_stage(&profile,PROFILE_UI_ROUTE);
        PeSandboxUIResult action=pe_sandbox_ui_update(&ui,game,dt);
        profile_stage(&profile,PROFILE_HOST);
        bool focused=smoke_frames||IsWindowFocused(),resized=IsWindowResized();
        if(profile.enabled){profile.focused_frames+=focused;profile.paused_frames+=action.paused;}
        unsigned pointer_buttons=0;for(int button=0;button<7;button++)if(IsMouseButtonDown(button))pointer_buttons|=1u<<button;
        if(action.paused||!focused||resized)action.consume_world=true;
        bool pointer_was_owned=pointer_gate.waiting_for_release;
        bool pointer_blocked=pe_sandbox_pointer_route(&pointer_gate,&action,pointer_buttons);
        if(host_cancelled_modal){action.consume_world=true;action.npc_interaction=false;}
        if(action.return_main){result=2;break;}
        if(action.selected_item)selected_item=action.selected_item;
        bool toggle_build=action.toggle_build;
        if(IsKeyPressed(KEY_B)&&ui.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&!action.consume_world&&!action.consume_actions&&!ui.inventory_open&&!ui.character_open&&!ui.missions_open&&!ui.replicator_open&&!ui.paused&&!ui.menu_open&&!ui.dragging&&IsWindowFocused())toggle_build=true;
        if(toggle_build){build_mode=!build_mode;action.consume_world=true;ui.building=build_mode;}
        if(action.build_shape)build_shape=action.build_shape;
        if(action.build_brush)build_brush=action.build_brush;
        if(action.rotate_build)build_rotation=(build_rotation+1)%4;
        action.cursor_visible=pe_sandbox_ui_cursor(&ui);
        bool loaded=false;
        if(action.save) {
            bool ok=pe_game_save(game,save_path,error,sizeof error);
            snprintf(game->message,sizeof game->message,"%.159s",ok?"World saved.":error);
        }
        if(action.load) {
            loaded=pe_game_load(game,save_path,error,sizeof error);
            if(loaded){pe_sandbox_camera_init(&orbit);pe_sandbox_camera_recovery_init(&camera_recovery);melee.playback=(PeAttackPlayback){0};pe_sandbox_actor_reset_motion(actor,game);pe_sandbox_camera_controls_cancel(&camera_controls);hunt=(PeHuntCombat){0};hunt_attempted=false;return_prompted=false;ui.hunt_encounter_available=false;attack_pending=false;pe_sandbox_melee_cancel(&melee);placing_torch=false;autorun=false;jump_pending=false;accumulator=0;ui.map_valid=false;tool_drawn=false;tool_instance=0;pe_sandbox_actions_cancel(&tool_action);
                pe_adventure_dialogue_cancel(&ui.dialogue);supplier=(PeAdventureNpc){0};supplier_visible=false;ui.supplier_present=false;
                welcome=(PeAdventureNpc){0};welcome_visible=false;welcome_prompted=false;ui.welcome_present=false;
                pe_sandbox_ui_cancel_town(&ui);pe_sandbox_ui_cancel_repair(&ui);for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++){residents[i].npc=(PeAdventureNpc){0};residents[i].visible=false;}}
            if(!loaded)snprintf(game->message,sizeof game->message,"%.159s",error);
            if(loaded&&(!pe_game_collision_update(game,collision,error,sizeof error)||
                (game->mode==PE_GAME_ADVENTURE&&pe_sandbox_collision_overlap(collision,game->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT)))){result=1;break;}
            if(loaded&&game->vegetation.installed&&!pe_vegetation_view_available(vegetation_view)){
                snprintf(error,sizeof error,"Loaded vegetation requires recovered sandbox-vegetation assets.");result=1;break;
            }
        }
        if(loaded){mesh_revision=game->world.revision-1;load_town_name(&ui,game,assets);pe_sandbox_ui_set_resident_markers(&ui,NULL,0);PollInputEvents();continue;}
        if(!sandbox_build_history_apply(game,&ui,action,focused&&!resized&&!host_cancelled_modal&&!pointer_was_owned,
           collision,error,sizeof error)){result=1;break;}
        if(game->mode==PE_GAME_ADVENTURE&&!supplier_view.actor.loaded) {
            if(!pe_adventure_npc_view_load(&supplier_view,assets,error,sizeof error)){result=1;break;}
            ui.supplier_portrait=supplier_view.portrait.texture;
        }
        if(game->mode==PE_GAME_ADVENTURE&&!welcome_view.actor.loaded) {
            if(!pe_adventure_npc_view_load_role(&welcome_view,assets,1,error,sizeof error)){result=1;break;}
            ui.welcome_portrait=welcome_view.portrait.texture;
        }
        /* Do not warp/capture the pointer while a closing UI click is held.
         * Otherwise its remaining hold becomes a world tool action, and the
         * desktop's absolute pointer events become a large look delta. */
        Vector2 pointer_position=GetMousePosition(),mouse_delta=GetMouseDelta();
        if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))right_press_pointer=pointer_position;
        PeSandboxCameraControlResult camera_control;
        bool cycle_pressed=IsKeyPressed(KEY_F5);
        bool cycle_from_tab=cycle_pressed&&ui.free_cursor&&action.camera_cycle_allowed&&
            !pointer_blocked&&!host_cancelled_modal&&!toggle_build;
        PeSandboxCameraControlInput control_input={.dt=dt,
            .mouse_dx=look_settle?0:mouse_delta.x,.mouse_dy=look_settle?0:mouse_delta.y,
            .focused=focused,.input_allowed=(!action.consume_world||cycle_from_tab)&&!action.paused&&!loaded&&!resized&&game->hp>0,
            .pointer_over_ui=action.consume_world||(action.cursor_visible&&pe_training_ui_contains(ui.layout.hud,pointer_position.x,pointer_position.y)),.captured=captured,.free_cursor=ui.free_cursor,
            .cycle_pressed=cycle_pressed&&action.camera_cycle_allowed,.right_pressed=!action.consume_actions&&IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
            .right_down=IsMouseButtonDown(MOUSE_BUTTON_RIGHT),.right_released=!action.consume_actions&&IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)};
        if(!pe_sandbox_camera_controls_step(&camera_controls,control_input,&camera_control)){
            snprintf(error,sizeof error,"Invalid camera control state");result=1;break;
        }
        if(camera_control.cycle_accepted){
            ui.free_cursor=false;attack_pending=false;tool_drawn=false;placing_torch=false;
            action.consume_world=true;action.npc_interaction=false;
            camera_control.world_input=false;camera_control.capture=false;
            pe_sandbox_pointer_release_gate(&pointer_gate,true,pointer_buttons);
            pe_sandbox_melee_cancel(&melee);pe_sandbox_actions_cancel(&tool_action);
            snprintf(game->message,sizeof game->message,"Camera: %s",camera_controls.mode==PE_SANDBOX_CAMERA_FREE?
                "third person, free mouse (hold RMB to look)":camera_controls.mode==PE_SANDBOX_CAMERA_LOCKED?
                "third person, mouse look":"first person");
        }
        third_person=camera_control.third_person;
        pe_sandbox_actor_facing_mode(actor,camera_controls.mode==PE_SANDBOX_CAMERA_FREE);
        bool wanted_capture=camera_control.capture&&!smoke_frames&&!build_mode;
        bool recaptured=wanted_capture&&!captured;
        if(wanted_capture!=captured){if(wanted_capture)DisableCursor();else EnableCursor();captured=wanted_capture;}
        /* Ignore capture warps in both camera rotation and click/drag classification. */
        if(recaptured||resized)look_settle=3;
        ui.camera_cursor=camera_controls.mode==PE_SANDBOX_CAMERA_FREE&&!captured;
        action.cursor_visible=pe_sandbox_ui_cursor(&ui);
        if(!captured){if(focused&&action.cursor_visible)HideCursor();else ShowCursor();}
        bool world_input=camera_control.world_input;
        bool action_input=world_input&&!action.consume_actions;
        if(action.consume_actions)attack_pending=false;
        bool right_click=action_input&&camera_control.right_click;
        const PeSlot *equipped=pe_game_equipped_tool(game);
        uint32_t current_instance=equipped?equipped->instance_id:0;
        if(current_instance!=tool_instance){attack_pending=false;sandbox_blade_rebind(&melee,&blade_templates[equipped&&equipped->item_id==1&&blade_templates[1].ready?1:0]);placing_torch=false;tool_instance=current_instance;tool_drawn=false;pe_sandbox_actions_cancel(&tool_action);}
        bool torch_tool=equipped&&equipped->item_id==PE_TORCH_ITEM;
        bool knife_tool=equipped&&(equipped->item_id==33||equipped->item_id==1);
        bool knife_ready=knife_tool&&sandbox_blade_owned(equipped,&melee)&&game->mode==PE_GAME_ADVENTURE&&
            (equipped->item_id==33||ui.starter_pair_available)&&pe_sandbox_actor_hold_ready(actor,equipped->item_id);
        if(!world_input||build_mode||!torch_tool)placing_torch=false;
        bool timed_tool=game->mode==PE_GAME_ADVENTURE&&equipped&&pe_sandbox_actions_supported(equipped->item_id);
        bool tool_ready=timed_tool&&pe_sandbox_actor_action_ready(actor,equipped->item_id);
        if(!world_input||build_mode||!third_person)pe_sandbox_actions_cancel(&tool_action);
        if(!world_input){autorun=false;jump_pending=false;}
        if(action_input) {
            if(IsKeyPressed(KEY_P)&&!build_mode&&torch_tool&&game->mode==PE_GAME_ADVENTURE){
                if(deploy_view.available){placing_torch=!placing_torch;snprintf(game->message,sizeof game->message,placing_torch?"Place Torch: aim at a surface, left click to place; P or right click cancels.":"Torch placement canceled.");}
                else snprintf(game->message,sizeof game->message,"Recover the Torch deployment assets before placing it.");
            }
            if(IsKeyPressed(KEY_Z))walk=!walk;
            if(IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))autorun=!autorun;

            if(IsKeyPressed(KEY_V)){
                build_shape=build_shape==PE_B45_CUBE?PE_B45_RAMP:PE_B45_CUBE;
                if(selected_item==PE_ITEM_FENCE||selected_item==PE_ITEM_LADDER)selected_item=PE_ITEM_WOOD;
            }
            if(IsKeyPressed(KEY_T))build_rotation=(build_rotation+1)%4;
            if(IsKeyPressed(KEY_G))build_brush=build_brush==4?1:4;
        }
        if(world_input){
            Vector2 delta=(look_settle||(!camera_control.look&&!(build_mode&&action_input&&IsMouseButtonDown(MOUSE_BUTTON_RIGHT))))?(Vector2){0}:mouse_delta;
            game->yaw=remainderf(game->yaw-delta.x*.0024f,2*PI);
            game->pitch=Clamp(game->pitch-delta.y*.0024f,-1.4f,1.4f);
        }
        /* Clear immediately even on frames without a simulation tick. Modal
         * closure still owns its press through the shared pointer-release gate. */
        if(!world_input||build_mode||!third_person||game->hp<=0||!knife_ready){
            attack_pending=false;pe_sandbox_melee_cancel(&melee);if(knife_tool)tool_drawn=false;
        }
        profile_stage(&profile,PROFILE_QUERIES);
        PeSandboxCameraInput camera_input={.dt=smoke_frames?PE_FIXED_DT:dt,
            .wheel_notches=GetMouseWheelMove(),.building=build_mode,.third_person=third_person,
            .input_allowed=world_input,.paused=action.paused||!focused};
        if(!sandbox_camera_frame(&orbit,&camera_recovery,game,collision,camera_input)){
            snprintf(error,sizeof error,"Invalid sandbox camera state");result=1;break;
        }
        if(look_settle>0)look_settle--;
        Camera3D aim_camera=game_camera(game,collision,third_person,false,orbit.distance,NULL);
        Vector2 aim_point=captured?(Vector2){GetScreenWidth()*.5f,GetScreenHeight()*.5f}:pointer_position;
        Ray aim=source_ray(aim_point,aim_camera);
        Vector2 interaction_point=right_click&&camera_controls.mode==PE_SANDBOX_CAMERA_FREE?right_press_pointer:aim_point;
        int picked_role=0,picked_resident=-1;float nearest_npc=INFINITY;
        if(!build_mode) {
            Ray pick=source_ray(interaction_point,aim_camera);
            PeSceneHit obstacle=pe_sandbox_collision_ray_geometry(collision,pv(pick.position),pv(pick.direction),100);
            PeAdventureNpc *npcs[]={&welcome,&supplier};bool visible[]={welcome_visible,supplier_visible};
            for(int i=0;i<2;i++) {
                float distance;
                if(visible[i]&&pe_adventure_npc_pick(npcs[i],game->player,pv(pick.position),pv(pick.direction),
                   obstacle.hit?obstacle.distance:INFINITY,&distance)&&distance<nearest_npc){nearest_npc=distance;picked_role=i+1;}
            }
            for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++){
                float distance;Resident *r=&residents[i];
                if(r->visible&&pe_adventure_npc_pick_sex(&r->npc,pe_adventure_population_sources[i].male,game->player,pv(pick.position),pv(pick.direction),obstacle.hit?obstacle.distance:INFINITY,&distance)&&distance<nearest_npc){nearest_npc=distance;picked_resident=i;picked_role=0;}
            }
        }
        uint32_t picked_deploy=0;float deploy_distance=INFINITY;
        if(!build_mode&&!placing_torch&&deploy_view.available){
            Ray pick=source_ray(interaction_point,aim_camera);
            PeSceneHit obstruction=pe_sandbox_collision_ray_geometry(collision,pv(pick.position),pv(pick.direction),100);
            pe_deployable_pick(game,pv(pick.position),pv(pick.direction),obstruction.hit?obstruction.distance:INFINITY,&picked_deploy,&deploy_distance);
        }
        PeSceneHit picked_repair={0};float repair_distance=INFINITY;
        if(!build_mode&&!placing_torch){
            Ray pick=source_ray(interaction_point,aim_camera);
            picked_repair=pe_camp_repair_pick(game,collision,repair_model_ready,pv(pick.position),pv(pick.direction));
            if(picked_repair.hit)repair_distance=picked_repair.distance;
        }
        unsigned picked_plant=PE_MAX_PLANTS;float plant_distance=INFINITY;
        if(!build_mode&&!placing_torch&&pe_vegetation_view_available(vegetation_view)){
            Ray pick=source_ray(interaction_point,aim_camera);
            PeSceneHit obstruction=pe_sandbox_collision_ray_geometry(collision,pv(pick.position),pv(pick.direction),100);
            pe_vegetation_pick(game,pv(pick.position),pv(pick.direction),obstruction.hit?obstruction.distance:INFINITY,&picked_plant,&plant_distance);
        }
        bool auto_welcome=!welcome_prompted&&welcome_visible&&game->welcome_page<PE_ADVENTURE_WELCOME_PAGES&&
            Vector3Distance(rv(game->player),rv(welcome.position))<=9;
        bool npc_right=right_click||(ui.free_cursor&&IsMouseButtonPressed(MOUSE_BUTTON_RIGHT));
        SandboxVegetationInput plant_input={.index=picked_plant,.distance=plant_distance,.other_distance=fminf(repair_distance,fminf(nearest_npc,deploy_distance)),
            .enabled=focused&&!loaded&&!build_mode&&!placing_torch,.available=pe_vegetation_view_available(vegetation_view),
            .requested=IsKeyPressed(KEY_E)||npc_right,.modal_priority=auto_welcome,.melee=&melee,.tool=&tool_action};
        if(sandbox_vegetation_interact(game,collision,plant_input,&action,&pointer_gate,pointer_buttons)){
            action_input=false;right_click=false;attack_pending=false;
        }
        int open_role=auto_welcome?1:((IsKeyPressed(KEY_E)||npc_right)?picked_role:0);
        bool opened=sandbox_repair_interact(&ui,game,collision,repair_model_ready,
            focused&&!loaded&&!build_mode&&!placing_torch,IsKeyPressed(KEY_E)||npc_right,auto_welcome,
            picked_repair,fminf(plant_distance,fminf(nearest_npc,deploy_distance)),&action,&pointer_gate,pointer_buttons);
        PeAdventureNpc *selected=NULL;
        if(focused&&!loaded&&!build_mode&&!placing_torch&&action.npc_interaction){
            if(!auto_welcome&&picked_deploy&&deploy_distance<nearest_npc&&(IsKeyPressed(KEY_E)||npc_right))
                opened=pe_sandbox_ui_open_deployable(&ui,game,picked_deploy);
            else if(open_role){
                opened=open_role==1?pe_sandbox_ui_open_welcome(&ui,game,PE_ADVENTURE_WELCOME_ENTITY,true):pe_sandbox_ui_open_supplier(&ui,game,PE_ADVENTURE_NPC_ENTITY,true);
                selected=open_role==1?&welcome:&supplier;
                if(opened&&open_role==1)welcome_prompted=true;
            }else if(picked_resident>=0&&(IsKeyPressed(KEY_E)||npc_right)){
                const PeAdventurePopulationSource *src=&pe_adventure_population_sources[picked_resident];Resident *r=&residents[picked_resident];selected=&r->npc;
                opened=src->adventure_role==3&&game->annot_complete?
                    pe_sandbox_ui_open_annot(&ui,game,(uint32_t)src->entity_id,r->view.portrait.texture,true):
                    pe_sandbox_ui_open_town(&ui,game,(uint32_t)src->entity_id,src->name,r->view.portrait.texture,true);
            }
        }
        if(opened){
            if(selected)selected->yaw=atan2f(game->player.x-selected->position.x,game->player.z-selected->position.z)*RAD2DEG;
            world_input=false;action_input=false;autorun=false;jump_pending=false;tool_drawn=false;attack_pending=false;pe_sandbox_melee_cancel(&melee);pe_sandbox_actions_cancel(&tool_action);
            action.consume_world=true;action.cursor_visible=true;
            pe_sandbox_pointer_release_gate(&pointer_gate,true,pointer_buttons);
            if(captured){EnableCursor();captured=false;}HideCursor();
        }
        PeVec3 eye={game->player.x,game->player.y+1.58f,game->player.z};
        PeVec3 build_direction=construction_direction(game,aim,eye);
        profile_stage(&profile,PROFILE_SIM);
        /* Attribute Update is once per display, outside fixed-step catch-up.
         * Contacts below notify at this same validated frame timestamp. */
        if(!sandbox_health_frame(&sandbox_attribute_clock,game,smoke_frames?PE_FIXED_DT:dt,focused,action.paused)){
            snprintf(error,sizeof error,"Invalid player recovery clock");result=1;break;
        }
        if(!sandbox_wallet_frame(game,focused,action.paused)){
            snprintf(error,sizeof error,"Invalid or overflowing town wallet update");result=1;break;
        }
        if(!sandbox_repair_frame(game,smoke_frames?PE_FIXED_DT:dt,focused,action.paused)){
            snprintf(error,sizeof error,"Invalid camp repair state or clock");result=1;break;
        }
        if(action.revive&&focused&&!action.paused&&sandbox_revive_request(game,&sandbox_attribute_clock)){
            attack_pending=tool_drawn=placing_torch=autorun=jump_pending=false;
            build_mode=ui.building=false;accumulator=0;
            pe_sandbox_melee_cancel(&melee);pe_sandbox_actions_cancel(&tool_action);
            pe_sandbox_actor_reset_motion(actor,game);
            /* Preserve the actual cohort, injuries, kill count and encounter
             * clock. Revival must not reload or replace surviving creatures. */
            world_input=action_input=false;
            fprintf(stderr,"Player revived: hp=%.6f stamina=%.6f comfort=%.6f kills=%u\n",
                game->hp,game->stamina,game->comfort,game->hunt_kills);
        }
        if(focused&&!action.paused) {
            supplier_seconds+=smoke_frames?PE_FIXED_DT:dt;
            if(hunt.encounter.active&&sandbox_hunt_runs(&ui,action)){
                PeHuntCombatPlayer player;
                if(!sandbox_action_pose(actor,game,&tool_action,&melee,tool_drawn)||!hunt_player_pose(actor,game,player_defence,&melee,&player)||
                   !pe_hunt_combat_sense(&hunt,collision,player)){snprintf(error,sizeof error,"Hunt sensor pose unavailable");result=1;break;}
            }
            PeInput input={0};
            if(world_input) {
                float forward=(float)(IsKeyDown(KEY_W)-IsKeyDown(KEY_S));
                float side=(float)(IsKeyDown(KEY_A)-IsKeyDown(KEY_D));
                if(forward<0)autorun=false;
                if(autorun&&forward==0&&side==0)forward=1;
                input.move_x=side*cosf(game->yaw)+forward*sinf(game->yaw);
                input.move_z=side*sinf(game->yaw)-forward*cosf(game->yaw);
                input.sprint=IsKeyDown(KEY_LEFT_SHIFT);input.walk=walk;
                jump_pending=jump_pending||(action_input&&IsKeyPressed(KEY_SPACE));
            }
            if(action_input){
                if(IsKeyPressed(KEY_E))interact(game);
                dig_cooldown=fmaxf(0,dig_cooldown-dt);
                if(!build_mode) {
                    if(IsKeyPressed(KEY_R)||right_click){placing_torch=false;tool_drawn=false;attack_pending=false;pe_sandbox_melee_cancel(&melee);pe_sandbox_actions_cancel(&tool_action);}
                    else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                        if(placing_torch){
                            if(pe_deployable_place(game,collision,current_instance,pv(aim.position),pv(aim.direction))){
                                placing_torch=false;tool_drawn=false;
                                /* Placement consumes this press even after the equipped
                                 * instance leaves the bag. Wait for release before a new
                                 * world action can use the now-empty hands. */
                                pe_sandbox_pointer_release_gate(&pointer_gate,true,pointer_buttons);
                            }
                        }else if(knife_tool){
                            if(!knife_ready)snprintf(game->message,sizeof game->message,"This blade is not ready for combat in this build.");
                            else if(!third_person)snprintf(game->message,sizeof game->message,"Press F5 for third-person melee use.");
                            else if(tool_drawn)attack_pending=true;
                            else{tool_drawn=true;snprintf(game->message,sizeof game->message,"Weapon drawn: click to swing; click during a swing to chain.");}
                        }else tool_drawn=tool_ready||(torch_tool&&pe_sandbox_actor_hold_ready(actor,PE_TORCH_ITEM));
                        if(timed_tool&&!tool_ready)snprintf(game->message,sizeof game->message,"Recover the equipped tool action assets before using it.");
                        if(torch_tool&&!pe_sandbox_actor_hold_ready(actor,PE_TORCH_ITEM))snprintf(game->message,sizeof game->message,"Recover the Torch held and stowed assets before drawing it.");
                    }
                    if(!timed_tool&&!torch_tool&&!knife_tool&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&dig_cooldown<=0) {
                        pe_game_mine(game,eye,build_direction);dig_cooldown=.25f;
                    }
                    if(timed_tool&&!third_person&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                        snprintf(game->message,sizeof game->message,"Press F5 for third-person tool use.");
                } else {
                    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                        if(IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT))pe_game_remove_shape(game,eye,build_direction,build_brush);
                        else pe_game_place_shape(game,eye,build_direction,selected_item,build_shape,build_rotation,build_brush);
                    }
                    if(IsKeyPressed(KEY_X))pe_game_remove_shape(game,eye,build_direction,build_brush);
                }
            }
            /* Reject an unsupported press on this display frame, even if no
             * fixed tick runs. Otherwise a moving click could survive until
             * the next stationary tick and unexpectedly begin an attack. */
            PeSwordVariant attack_branch=PE_SWORD_VARIANT_ORDINARY;
            bool branch_valid=pe_sword_variant_select((PeSwordVariantInput){.velocity=game->velocity,
                .movement_present=true,.in_air=!game->grounded,.motor_present=true},&attack_branch);
            bool running_press=!melee.action.active&&melee.setup.running_motion_ready&&branch_valid&&attack_branch==PE_SWORD_VARIANT_RUNNING;
            bool running_active=melee.action.active&&melee.action.variant==PE_COMBAT_RUNNING;
            bool ordinary_press=!melee.action.active&&melee.setup.ordinary_motion_ready&&branch_valid&&attack_branch==PE_SWORD_VARIANT_ORDINARY;
            bool ordinary_active=melee.action.active&&melee.setup.ordinary_motion_ready&&melee.action.variant==PE_COMBAT_STATIONARY;
            if((attack_pending||melee.action.active)&&(!game->grounded||jump_pending||
               (!running_active&&!running_press&&!ordinary_press&&!ordinary_active&&(fabsf(input.move_x)+fabsf(input.move_z)>.001f||hypotf(game->velocity.x,game->velocity.z)>.05f)))){
                if(attack_pending&&!melee.action.active)snprintf(game->message,sizeof game->message,"This movement is not supported for blade attacks; use a grounded third-person stance.");
                attack_pending=false;pe_sandbox_melee_cancel(&melee);
            }
            accumulator+=smoke_frames?PE_FIXED_DT:dt;int steps=0;bool hunt_fixed_pose=false;
            while(accumulator>=PE_FIXED_DT&&steps++<6) {
                const PeSlot *knife_item=pe_game_equipped_tool(game);
                bool owns_knife=sandbox_blade_owned(knife_item,&melee)&&
                    (knife_item->item_id==33||ui.starter_pair_available);
                PeSandboxMeleeGate melee_gate={.alive=game->hp>0,.weapon_owned=owns_knife,
                    .grounded=game->grounded,.third_person=third_person,
                    .action_allowed=world_input&&!build_mode&&game->mode==PE_GAME_ADVENTURE,
                    .move_requested=fabsf(input.move_x)+fabsf(input.move_z)>.001f||jump_pending,
                    .equipped_item_id=knife_item?knife_item->item_id:0,
                    .equipped_instance_id=knife_item?knife_item->instance_id:0,
                    .drawn_instance_id=tool_drawn&&owns_knife?knife_item->instance_id:0,.velocity=game->velocity,
                    .scaled_time=(float)sandbox_attribute_clock.seconds};
                if(attack_pending&&melee.ready){
                    if(melee.action.active)pe_sandbox_melee_queue(&melee,melee_gate);
                    else if(pe_sandbox_melee_begin(&melee,melee_gate,&game->stamina)){
                        pe_sandbox_actor_face_camera(actor,game);melee_swings++;running_starts+=melee.action.variant==PE_COMBAT_RUNNING;
                        walking_starts+=melee.action.variant==PE_COMBAT_STATIONARY&&melee.setup.ordinary_motion_ready&&
                            (melee_gate.move_requested||hypotf(melee_gate.velocity.x,melee_gate.velocity.z)>.05f);
                    }
                    else snprintf(game->message,sizeof game->message,"This movement is not supported for blade attacks; use a grounded third-person stance.");
                }
                attack_pending=false;
                bool melee_stamina_lock=melee.ready&&melee.action.active;
                PeCombatInterval attack_interval={0};
                if(melee.ready&&(!pe_sandbox_melee_advance(&melee,melee_gate,&game->stamina)||
                   !pe_sandbox_melee_interval(&melee,&attack_interval))){
                    snprintf(error,sizeof error,"Invalid sandbox knife action clock");result=1;break;
                }
                bool action_motion=attack_interval.valid;
                PeVec3 motion_request={0};
                if(action_motion&&
                   !sandbox_melee_motion(running_motion,ordinary_motion,&attack_interval,game->velocity,
                      pe_sandbox_actor_yaw(actor,game),PE_SANDBOX_MELEE_DT,&motion_request)){
                    snprintf(error,sizeof error,"Invalid qualified blade motion");result=1;break;
                }
                PeSandboxActionInput use={.enabled=world_input&&!build_mode&&third_person&&timed_tool&&
                    game->grounded&&!jump_pending&&fabsf(input.move_x)+fabsf(input.move_z)<.001f&&
                    tool_ready,
                    .held=world_input&&(action_input||tool_action.stage!=PE_SANDBOX_ACTION_IDLE)&&IsMouseButtonDown(MOUSE_BUTTON_LEFT),.tool_drawn=tool_drawn&&tool_ready,
                    .ray_origin=pv(aim.position),.ray_direction=pv(aim.direction),.camera_tree=-1,.forward_tree=-1};
                if(use.enabled&&use.held&&tool_action.stage==PE_SANDBOX_ACTION_IDLE)
                    pe_sandbox_actor_face_camera(actor,game);
                if(use.enabled&&(equipped->item_id==26||equipped->item_id==1527)){
                    float distance;
                    float body_yaw=pe_sandbox_actor_yaw(actor,game)*DEG2RAD;
                    PeVec3 forward={sinf(body_yaw),0,cosf(body_yaw)},origin={game->player.x,game->player.y+1,game->player.z};
                    PeVec3 camera_origin=pv(Vector3Subtract(aim.position,Vector3Scale(aim.direction,.5f)));
                    if(!pe_sandbox_world_view_ray_trunk(world_view,game,camera_origin,use.ray_direction,100,&use.camera_tree,&distance)||
                       !pe_sandbox_world_view_ray_trunk(world_view,game,origin,forward,2,&use.forward_tree,&distance)){
                        snprintf(error,sizeof error,"Tree interaction query failed");result=1;break;
                    }
                }
                PeSandboxActionUpdate event;
                if(!pe_sandbox_actions_step(&tool_action,game,use,&event)){
                    snprintf(error,sizeof error,"Invalid sandbox tool action state");result=1;break;
                }
                if(event.changed)tool_edits++;
                input.stamina_action=tool_action.stamina_lock||melee_stamina_lock||melee.action.active;
                if(tool_action.stage!=PE_SANDBOX_ACTION_IDLE){input.move_x=input.move_z=0;input.sprint=false;}
                /* Refresh AFTER timed edits and before each motion step. */
                if(!pe_game_collision_update(game,collision,error,sizeof error)){result=1;break;}
                input.jump=jump_pending;
                /* Legacy saves retain their original controller/validator.
                 * Adventure uses the shared rendered-mesh capsule contract. */
                if(game->mode==PE_GAME_LEGACY)pe_game_step(game,input,PE_FIXED_DT);
                else if(!(action_motion?
                    pe_game_step_collision_motion(game,input,PE_FIXED_DT,collision,motion_request):
                    pe_game_step_collision(game,input,PE_FIXED_DT,collision))) {
                    snprintf(error,sizeof error,"Player capsule intersects world geometry");result=1;break;
                }
                melee_gate.alive=game->hp>0;melee_gate.grounded=game->grounded;melee_gate.velocity=game->velocity;
                if(melee.ready&&(!hunt.encounter.active||!sandbox_hunt_runs(&ui,action))){
                    PeSandboxMeleeFrame frame_pose={.gate=melee_gate,.player_position=game->player,
                        .player_yaw=pe_sandbox_actor_yaw(actor,game),.player_scale=1};
                    PeSandboxMeleeUpdate update;
                    if(!pe_sandbox_melee_resolve(&melee,&frame_pose,NULL,0,&update)){
                        snprintf(error,sizeof error,"Invalid sandbox knife contact state");result=1;break;
                    }
                    if(update.advance==PE_COMBAT_NEXT_STAGE)melee_swings++;
                }
                pe_sandbox_actor_facing_lock(actor,melee.action.active||tool_action.stage!=PE_SANDBOX_ACTION_IDLE);
                if(hunt.encounter.active){
                    pe_sandbox_actor_update(actor,PE_FIXED_DT,game);hunt_fixed_pose=true;
                    if(sandbox_hunt_runs(&ui,action)){
                        PeHuntCombatPlayer player;
                        if(!sandbox_action_pose(actor,game,&tool_action,&melee,tool_drawn)||!hunt_player_pose(actor,game,player_defence,&melee,&player)){
                            snprintf(error,sizeof error,"Hunt player defence pose unavailable");result=1;break;
                        }
                        melee_gate.alive=game->hp>0;melee_gate.grounded=game->grounded;melee_gate.velocity=game->velocity;
                        PeSandboxMeleeFrame combat_frame={.gate=melee_gate,.player_position=game->player,
                            .player_yaw=pe_sandbox_actor_yaw(actor,game),.player_scale=1,.attacker_eligible=game->hp>0};
                        PeSandboxMeleeUpdate combat_update;bool completed=false;unsigned before=game->hunt_kills;
                        uint64_t previous_hare_hits=hunt.hare_hits,previous_player_hits=hunt.player_hits;
                        if(!pe_hunt_combat_step(&hunt,&game->world,collision,player,&melee,&combat_frame,
                            &game->hp,&game->stamina,&game->hunt_kills,&combat_update,&completed)){
                            snprintf(error,sizeof error,"Hunt combat transaction failed");result=1;break;
                        }
                        sandbox_health_contacts(game,previous_player_hits,hunt.player_hits);
                        /* Opt-in, bounded observation only: no inputs or saved state.
                         * Keep per-target health available for reproducible playtests. */
                        if(combat_trace&&combat_trace_lines<4096&&
                           (hunt.hare_hits!=previous_hare_hits||hunt.player_hits!=previous_player_hits||game->hunt_kills!=before)){
                            fprintf(stderr,"Hunt event: frame=%d clock=%.6f hp=%.6f stamina=%.6f kills=%u hits=%llu/%llu player=%.6f,%.6f,%.6f yaw=%.6f variant=%d stage=%u clip=%.6f hares=",
                                frame,hunt.clock,game->hp,game->stamina,game->hunt_kills,
                                (unsigned long long)hunt.hare_hits,(unsigned long long)hunt.player_hits,
                                game->player.x,game->player.y,game->player.z,combat_frame.player_yaw,
                                (int)melee.action.variant,melee.action.stage,melee.action.clip_time);
                            for(unsigned h=0;h<PE_HUNT_ENCOUNTER_COUNT;h++){
                                const PeHuntEncounterActor *a=&hunt.encounter.actors[h];
                                fprintf(stderr,"%s%llu:%.6f:%d:%.6f,%.6f,%.6f",h?";":"",
                                    (unsigned long long)a->entity_id,a->hp,(int)a->present,
                                    a->motion.position.x,a->motion.position.y,a->motion.position.z);
                            }
                            fputc('\n',stderr);fflush(stderr);combat_trace_lines++;
                        }
                        if(combat_update.advance==PE_COMBAT_NEXT_STAGE)melee_swings++;
                        if(completed){game->hunt_state=PE_HUNT_RETURN_PENDING;snprintf(game->message,sizeof game->message,"Four hares defeated. Return to town and report in.");}
                        else if(game->hunt_kills!=before)snprintf(game->message,sizeof game->message,"Lepus hares defeated: %u/4",game->hunt_kills);
                        if(game->hp<=0){attack_pending=false;tool_drawn=false;pe_sandbox_melee_cancel(&melee);pe_sandbox_actions_cancel(&tool_action);
                            snprintf(game->message,sizeof game->message,"You have died. Use a Revival Shot or load a saved game.");}
                    }
                }
                jump_pending=false;accumulator-=PE_FIXED_DT;
            }
            if(result==1)break;
            if(steps>=6)accumulator=0;
            if(!hunt_fixed_pose&&!hunt.encounter.active)pe_sandbox_actor_update(actor,smoke_frames?PE_FIXED_DT:dt,game);
        } else {accumulator=0;jump_pending=false;}
        profile_stage(&profile,PROFILE_REMESH);
        if(smoke_frames&&frame==30)pe_world_dig(&game->world,(PeVec3){27,pe_world_height(&game->world,27,27),27},1.7f);
        if(loaded||mesh_revision!=game->world.revision) {
            if(!update_terrain(&game->world,terrain,&triangles,&surface)){snprintf(error,sizeof error,"Terrain mesh allocation failed");result=1;break;}
            mesh_revision=game->world.revision;
        }
        profile_stage(&profile,PROFILE_POSE);
        if(!sandbox_action_pose(actor,game,&tool_action,&melee,tool_drawn)){
            snprintf(error,sizeof error,"Equipped action pose unavailable");result=1;break;
        }
        float clamped_distance=-1;
        Camera3D camera=game_camera(game,collision,third_person,smoke_frames>0,orbit.distance,&clamped_distance);
        /* Feedback is once per final display pose; aim queries never mutate the
         * clock. Freeze it under pause/focus loss and ignore synthetic overview. */
        if(!sandbox_camera_feedback_frame(&orbit,&camera_recovery,game->mode,camera_input,smoke_frames>0,clamped_distance)){
            snprintf(error,sizeof error,"Invalid sandbox camera clamp");result=1;break;
        }
        ui.building=build_mode;ui.build_item=selected_item;ui.build_shape=build_shape;
        ui.build_rotation=build_rotation;ui.build_brush=build_brush;
        double cycle=game->game_seconds/PE_DAY_SECONDS;
        float light=.6f+.4f*(float)(-.5*cos(cycle*2*PI)+.5);
        Color sky={(unsigned char)(127*light),(unsigned char)(176*light),(unsigned char)(188*light),255};
        profile_stage(&profile,PROFILE_UI_PREPARE);
        pe_sandbox_ui_prepare(&ui,game);
        profile_stage(&profile,PROFILE_WORLD_DRAW);
        BeginDrawing();ClearBackground(sky);
        DrawRectangleGradientV(0,0,GetScreenWidth(),GetScreenHeight(),(Color){62,115,146,255},sky);
        source_begin_3d(camera);
        pe_terrain_material_view_frame(&surface,&game->world,camera.position);
        PeVec3 lights[PE_MAX_DEPLOYABLES+1];size_t light_count=0;
        if(deploy_view.available){
            for(unsigned k=0;k<game->deployed_count;k++)lights[light_count++]=pe_deployable_point(&game->deployed[k],(PeVec3){PE_TORCH_LIGHT_POSITION_X,PE_TORCH_LIGHT_POSITION_Y,PE_TORCH_LIGHT_POSITION_Z});
            PeVec3 held_light;if(pe_sandbox_actor_torch_light(actor,game,&held_light))lights[light_count++]=held_light;
        }
        pe_terrain_material_view_lights(&surface,lights,light_count,pv(camera.position));
        for(int i=0;i<PE_WORLD_MAX_REGIONS;i++)if(terrain->loaded[i])DrawModel(terrain->model[i],(Vector3){0},1,WHITE);
        if(pe_settlement_matches(&game->world))pe_reference_draw(&settlement);
        if(!pe_sandbox_world_view_draw_trees(world_view,game)){snprintf(error,sizeof error,"Tree rendering failed");result=1;}
        if(!pe_vegetation_view_draw(vegetation_view,game)){snprintf(error,sizeof error,"Vegetation rendering failed");result=1;}
        for(int i=0;i<game->block_count;i++)draw_block(game->blocks[i]);
        for(int i=0;i<game->cell_count;i++)draw_cell(&game->cells[i]);
        profile_stage(&profile,PROFILE_ACTOR_DRAW);
        if(third_person||smoke_frames)pe_sandbox_actor_draw(actor,game);
        if(hunt.encounter.ready)for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
            const PeHuntEncounterActor *hare=&hunt.encounter.actors[i];
            if(hare->present&&hare->hp>0&&!pe_actor_view_draw(&hunt_view,hare->clip,hare->seconds,rv(hare->motion.position),hare->motion.yaw_degrees,hare->motion.scale,WHITE)){
                snprintf(error,sizeof error,"Hunt actor rendering failed");result=1;}
        }
        if(!pe_deployable_view_draw(&deploy_view,game,camera,supplier_seconds)){snprintf(error,sizeof error,"Deployed item rendering failed");result=1;}
        PeVec3 held_flame;
        if((third_person||smoke_frames)&&pe_sandbox_actor_torch_flame(actor,game,&held_flame))pe_deployable_view_flame(&deploy_view,held_flame,camera,supplier_seconds,current_instance);
        if(world_input&&placing_torch){
            PeVec3 position;
            if(pe_deployable_preview(game,collision,current_instance,pv(aim.position),pv(aim.direction),&position))pe_deployable_view_preview(&deploy_view,position,camera,supplier_seconds,true);
        }
        if(world_input&&build_mode&&!IsKeyDown(KEY_LEFT_ALT)&&!IsKeyDown(KEY_RIGHT_ALT)){
            Ray pointer=source_ray(GetMousePosition(),camera);
            PeVec3 origin={game->player.x,game->player.y+1.58f,game->player.z};
            PeVec3 direction=construction_direction(game,pointer,origin);
            PeB45Cell cells[4];size_t count;
            if(pe_game_preview_shape(game,origin,direction,selected_item,build_shape,build_rotation,build_brush,cells,&count))
                draw_build_preview(cells,count,pe_game_can_build_cells(game,cells,(int)count));
        }
        const PeSlot *indicator_tool=pe_game_equipped_tool(game);
        if(world_input&&!build_mode&&tool_ready&&tool_drawn&&indicator_tool&&indicator_tool->durability>0&&
           (indicator_tool->item_id==12||indicator_tool->item_id==19)){
            Ray indicator=source_ray(captured?(Vector2){GetScreenWidth()*.5f,GetScreenHeight()*.5f}:GetMousePosition(),camera);
            PeHit hit=pe_world_dig_target(&game->world,pv(indicator.position),pv(indicator.direction),game->player,3);
            if(hit.hit)DrawCubeWires((Vector3){hit.x,hit.y,hit.z},1,1,1,(Color){71,216,245,255});
        }
        profile_stage(&profile,PROFILE_NPC_DRAW);
        if(welcome_visible&&!pe_adventure_npc_view_draw(&welcome_view,welcome.position,welcome.yaw,supplier_seconds)){
            snprintf(error,sizeof error,"Welcome NPC pose rendering failed");result=1;
        }
        if(supplier_visible&&!pe_adventure_npc_view_draw(&supplier_view,supplier.position,supplier.yaw,supplier_seconds)){
            snprintf(error,sizeof error,"Supplier pose rendering failed");result=1;
        }
        for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)if(residents[i].visible&&!pe_adventure_npc_view_draw(&residents[i].view,residents[i].npc.position,residents[i].npc.yaw,supplier_seconds)){snprintf(error,sizeof error,"Town NPC rendering failed");result=1;}
        if(!game->world.source)DrawCube((Vector3){31.5f,5.5f,31.5f},63,.06f,63,(Color){54,132,149,170});
        source_end_3d();
        profile_stage(&profile,PROFILE_UI_DRAW);
        if(picked_repair.hit&&repair_distance<fminf(plant_distance,fminf(nearest_npc,deploy_distance))&&
           action.npc_interaction&&!ui.town_open&&!ui.repair_open)
            DrawTextEx(pe_ui_font(ui.assets,false),"Repair Machine  [RMB / E]",(Vector2){GetScreenWidth()*.5f-120,GetScreenHeight()*.5f+30},18,0,YELLOW);
        bool npc_nearest=nearest_npc<=fminf(repair_distance,fminf(deploy_distance,plant_distance));
        if(supplier_visible&&ui.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE) {
            PeVec3 p=supplier.position;Vector2 pos=source_screen((Vector3){p.x,p.y+supplier_view.label_height,p.z},camera);
            const char *prompt=picked_role==2&&npc_nearest&&action.npc_interaction?"Eitai  [RMB / E]":"Eitai";
            if(Vector3DotProduct(Vector3Subtract(rv(p),camera.position),Vector3Subtract(camera.target,camera.position))>0)
                DrawTextEx(pe_ui_font(ui.assets,false),prompt,(Vector2){pos.x-45,pos.y},18,0,game->supplies_claimed?LIGHTGRAY:YELLOW);
        }
        if(welcome_visible&&ui.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE) {
            PeVec3 p=welcome.position;Vector2 pos=source_screen((Vector3){p.x,p.y+welcome_view.label_height,p.z},camera);
            const char *prompt=picked_role==1&&npc_nearest&&action.npc_interaction?"Westina  [RMB / E]":"Westina";
            if(Vector3DotProduct(Vector3Subtract(rv(p),camera.position),Vector3Subtract(camera.target,camera.position))>0)
                DrawTextEx(pe_ui_font(ui.assets,false),prompt,(Vector2){pos.x-45,pos.y},18,0,game->welcome_page<PE_ADVENTURE_WELCOME_PAGES?YELLOW:LIGHTGRAY);
        }
        if(ui.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&!ui.town_open&&!ui.repair_open)for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)if(residents[i].visible){
            PeVec3 p=residents[i].npc.position;
            if(Vector3Distance(rv(p),rv(game->player))>40||Vector3DotProduct(Vector3Subtract(rv(p),camera.position),Vector3Subtract(camera.target,camera.position))<=0)continue;
            Vector2 screen=source_screen((Vector3){p.x,p.y+residents[i].view.label_height,p.z},camera);
            char label[180];snprintf(label,sizeof label,"%s%s",pe_adventure_population_sources[i].name,picked_resident==i&&npc_nearest&&action.npc_interaction?"  [RMB / E]":"");
            Font f=pe_ui_font(ui.assets,false);Vector2 size=MeasureTextEx(f,label,18,0);
            DrawTextEx(f,label,(Vector2){screen.x-size.x*.5f,screen.y},18,0,picked_resident==i?YELLOW:LIGHTGRAY);
        }
        if(picked_deploy&&deploy_distance<=plant_distance&&deploy_distance<repair_distance&&deploy_distance<nearest_npc&&action.npc_interaction&&!ui.town_open&&!ui.repair_open)DrawTextEx(pe_ui_font(ui.assets,false),"Torch  [RMB / E]",(Vector2){GetScreenWidth()*.5f-70,GetScreenHeight()*.5f+30},18,0,YELLOW);
        if(picked_plant<game->vegetation.count&&!game->vegetation.plants[picked_plant].depleted&&
           plant_distance<fminf(repair_distance,fminf(nearest_npc,deploy_distance))&&action.npc_interaction&&!ui.town_open&&!ui.repair_open){
            const PePlantSource *source=pe_vegetation_source(game->vegetation.plants[picked_plant].prototype_id);
            const PeItemDef *item=source?pe_item_def(source->item_id):NULL;
            DrawTextEx(pe_ui_font(ui.assets,false),TextFormat("%s  [RMB / E]",item?item->name:"Herb"),
                (Vector2){GetScreenWidth()*.5f-90,GetScreenHeight()*.5f+30},18,0,YELLOW);
        }
        pe_sandbox_ui_draw(&ui,game,build_mode);
        if(smoke_frames&&frame+1>=smoke_frames&&screenshot) {
            rlDrawRenderBatchActive();
            Image capture=LoadImageFromScreen();bool saved=capture.data&&ExportImage(capture,screenshot);UnloadImage(capture);
            if(!saved){snprintf(error,sizeof error,"Screenshot export failed");result=1;}
        }
        profile_stage(&profile,PROFILE_PRESENT);
        EndDrawing();frame++;
        profile_stage(&profile,-1);
        if(profile.enabled&&++profile.frames==120)profile_report(&profile,"120 frames");
        if(!pe_sandbox_actor_ok(actor)){snprintf(error,sizeof error,"Character pose rendering failed");result=1;break;}
        if(result==1||(smoke_frames&&frame>=smoke_frames))break;
    }
    if(smoke_frames&&frame<smoke_frames)result=1;
cleanup:
    profile_stage(&profile,-1);profile_report(&profile,"exit");
    if(result==1)fprintf(stderr,"Sandbox failed: %s\n",error);
    if(captured)EnableCursor();
    ShowCursor();
    printf("Sandbox: %d frames, %zu triangles, mode %d, recipes %d, mission %d, result %d\n",
           frame,triangles,game->mode,game->known_recipe_count,game->adventure_mission,result);
    printf("Sandbox tools: %llu swings, %llu resource changes\n",(unsigned long long)tool_action.swings,(unsigned long long)tool_edits);
    printf("Sandbox camera: mode %d, desired %.3fm, distance %.3fm\n",camera_controls.mode,orbit.desired_distance,orbit.distance);
    printf("Sandbox locomotion: exercised mask %u\n",pe_sandbox_actor_locomotion_seen(actor));
    printf("Sandbox knife: %llu swing stages; hunt contacts %llu outgoing, %llu incoming; kills %u/4\n",(unsigned long long)melee_swings,
        (unsigned long long)hunt.hare_hits,(unsigned long long)hunt.player_hits,game->hunt_kills);
    printf("Sandbox running knife: %llu starts; qualified motion %s\n",(unsigned long long)running_starts,running_motion?"available":"unavailable");
    printf("Sandbox walking blade: %llu starts; source motion %s\n",(unsigned long long)walking_starts,ordinary_motion?"available":"unavailable");
    pe_hitboxes_free(hare_boxes);pe_hitboxes_free(player_boxes);pe_actor_free(player_defence);pe_actor_view_unload(&hunt_view);
    pe_hitboxes_free(knife_capsule);pe_hitboxes_free(sword_capsule);pe_root_motion_free(running_motion);pe_sword_root_motion_free(ordinary_motion);
    pe_deployable_view_unload(&deploy_view);
    pe_vegetation_view_unload(vegetation_view);
    pe_reference_unload(&settlement);
    pe_sandbox_world_view_unload(world_view);pe_sandbox_actor_unload(actor);pe_sandbox_ui_unload(&ui);
    for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)pe_adventure_npc_view_unload(&residents[i].view);
    pe_adventure_npc_view_unload(&supplier_view);pe_adventure_npc_view_unload(&welcome_view);
    pe_sandbox_collision_free(collision);free_terrain(terrain);pe_terrain_material_view_unload(&surface);CloseWindow();return result;
}
