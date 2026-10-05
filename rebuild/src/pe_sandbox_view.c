#include "pe_sandbox_view.h"
#include "pe_sandbox_ui.h"
#include "pe_sandbox_actor.h"
#include "pe_sandbox_actions.h"
#include "pe_sandbox_camera.h"
#include "pe_sandbox_world_view.h"
#include "pe_settlement.h"
#include "pe_town_interaction.h"
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

static Camera3D game_camera(const PeGame *g,const PeSandboxCollision *collision,bool third,bool overview,float distance) {
    Vector3 direction={sinf(g->yaw)*cosf(g->pitch),sinf(g->pitch),-cosf(g->yaw)*cosf(g->pitch)};
    Vector3 eye={g->player.x,g->player.y+1.58f,g->player.z};
    Camera3D camera={eye,Vector3Add(eye,direction),{0,1,0},60,CAMERA_PERSPECTIVE};
    if(third) {
        camera.target=Vector3Add(eye,Vector3Scale(direction,2));
        Vector3 delta=Vector3Scale(direction,-distance);delta.y+=.5f;
        PeSceneHit hit=pe_sandbox_collision_sweep_geometry(collision,pv((Vector3){eye.x,eye.y-.1f,eye.z}),.1f,.2f,pv(delta));
        float f=hit.hit?fmaxf(.05f,hit.fraction-.02f):1;
        camera.position=Vector3Add(eye,Vector3Scale(delta,f));
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

int pe_sandbox_view(PeGame *game,const char *assets,const char *save_path,
                    int smoke_frames,const char *smoke_panel,const char *screenshot) {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_WINDOW_RESIZABLE);
    InitWindow(1280,720,"Planet Explorers | C rebuild");
    if(!IsWindowReady()){fprintf(stderr,"Unable to initialize graphics\n");return 1;}
    SetWindowMinSize(1000,700);SetTargetFPS(60);SetExitKey(KEY_NULL);
    char error[256]={0};int result=0,frame=0;
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
    bool tool_drawn=false;uint32_t tool_instance=0;uint64_t tool_edits=0;
    bool captured=false,walk=false,autorun=false,jump_pending=false,third_person=true,build_mode=false;
    if(!terrain||!collision||!pe_terrain_material_view_load(&surface,assets,error,sizeof error)||
       !update_terrain(&game->world,terrain,&triangles,&surface)||
       !pe_game_collision_update(game,collision,error,sizeof error)||
       !pe_sandbox_ui_load(&ui,assets,error,sizeof error)) {
        fprintf(stderr,"Sandbox initialization failed: %s\n",error);result=1;goto cleanup;
    }
    if(game->mode==PE_GAME_ADVENTURE&&pe_sandbox_collision_overlap(collision,game->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT)) {
        snprintf(error,sizeof error,"Player overlaps world geometry");result=1;goto cleanup;
    }
    actor=pe_sandbox_actor_load(assets,error,sizeof error);
    if(!actor){fprintf(stderr,"Sandbox actor load failed: %s\n",error);result=1;goto cleanup;}
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
    printf("Sandbox ready: mode %d\n",game->mode);fflush(stdout);
    while(!WindowShouldClose()) {
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
        bool host_cancelled_modal=false;
        size_t marker_count=0;
        for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)if(residents[i].visible)resident_markers[marker_count++]=(PeSandboxMapNpc){(uint32_t)pe_adventure_population_sources[i].entity_id,residents[i].npc.position};
        pe_sandbox_ui_set_resident_markers(&ui,resident_markers,marker_count);
        uint32_t talking_id=ui.dialogue.entity_id;
        const PeAdventureNpc *talking=talking_id==PE_ADVENTURE_WELCOME_ENTITY&&welcome_visible?&welcome:
            talking_id==PE_ADVENTURE_NPC_ENTITY&&supplier_visible?&supplier:resident_entity(residents,talking_id);
        if(ui.dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE&&(!talking||game->hp<=0||
           Vector3Distance(rv(game->player),rv(talking->position))>9)){pe_adventure_dialogue_cancel(&ui.dialogue);host_cancelled_modal=true;}
        const PeAdventureNpc *trader=resident_entity(residents,ui.town_entity);
        if(ui.town_open&&(!trader||game->hp<=0||Vector3Distance(rv(game->player),rv(trader->position))>9)){pe_sandbox_ui_cancel_town(&ui);host_cancelled_modal=true;}
        if(!ui.paused&&(smoke_frames||IsWindowFocused())){
            if(supplier_visible)pe_adventure_arrival_step(game,supplier.spawn_position);
            pe_adventure_annot_step(game);
        }
        ui.building=build_mode;ui.build_item=selected_item;ui.build_shape=build_shape;
        ui.build_rotation=build_rotation;ui.build_brush=build_brush;
        ui.discard_input=host_cancelled_modal;
        PeSandboxUIResult action=pe_sandbox_ui_update(&ui,game,dt);
        if(host_cancelled_modal){action.consume_world=true;action.npc_interaction=false;}
        if(action.return_main){result=2;break;}
        if(action.selected_item)selected_item=action.selected_item;
        bool toggle_build=action.toggle_build;
        if(IsKeyPressed(KEY_B)&&ui.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&!action.consume_world&&!ui.inventory_open&&!ui.character_open&&!ui.missions_open&&!ui.replicator_open&&!ui.paused&&!ui.menu_open&&!ui.dragging&&IsWindowFocused())toggle_build=true;
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
            if(loaded){autorun=false;jump_pending=false;accumulator=0;ui.map_valid=false;tool_drawn=false;pe_sandbox_actions_cancel(&tool_action);
                pe_adventure_dialogue_cancel(&ui.dialogue);supplier=(PeAdventureNpc){0};supplier_visible=false;ui.supplier_present=false;
                welcome=(PeAdventureNpc){0};welcome_visible=false;welcome_prompted=false;ui.welcome_present=false;
                pe_sandbox_ui_cancel_town(&ui);for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++){residents[i].npc=(PeAdventureNpc){0};residents[i].visible=false;}}
            if(!loaded)snprintf(game->message,sizeof game->message,"%.159s",error);
            if(loaded&&(!pe_game_collision_update(game,collision,error,sizeof error)||
                (game->mode==PE_GAME_ADVENTURE&&pe_sandbox_collision_overlap(collision,game->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT)))){result=1;break;}
        }
        if(loaded){mesh_revision=game->world.revision-1;load_town_name(&ui,game,assets);pe_sandbox_ui_set_resident_markers(&ui,NULL,0);PollInputEvents();continue;}
        if(game->mode==PE_GAME_ADVENTURE&&!supplier_view.actor.loaded) {
            if(!pe_adventure_npc_view_load(&supplier_view,assets,error,sizeof error)){result=1;break;}
            ui.supplier_portrait=supplier_view.portrait.texture;
        }
        if(game->mode==PE_GAME_ADVENTURE&&!welcome_view.actor.loaded) {
            if(!pe_adventure_npc_view_load_role(&welcome_view,assets,1,error,sizeof error)){result=1;break;}
            ui.welcome_portrait=welcome_view.portrait.texture;
        }
        bool focused=smoke_frames||IsWindowFocused();
        bool resized=IsWindowResized();
        bool wanted_capture=focused&&!action.cursor_visible&&!action.paused&&!smoke_frames&&!resized;
        bool recaptured=wanted_capture&&!captured;
        if(wanted_capture!=captured){if(wanted_capture)DisableCursor();else EnableCursor();captured=wanted_capture;}
        if(recaptured||resized)look_settle=2;
        if(!captured){if(focused&&action.cursor_visible)HideCursor();else ShowCursor();}
        bool world_input=focused&&(captured||build_mode)&&!action.consume_world&&!action.paused&&!loaded;
        const PeSlot *equipped=pe_game_equipped_tool(game);
        uint32_t current_instance=equipped?equipped->instance_id:0;
        if(current_instance!=tool_instance){tool_instance=current_instance;tool_drawn=false;pe_sandbox_actions_cancel(&tool_action);}
        bool timed_tool=game->mode==PE_GAME_ADVENTURE&&equipped&&pe_sandbox_actions_supported(equipped->item_id);
        bool tool_ready=timed_tool&&pe_sandbox_actor_action_ready(actor,equipped->item_id);
        if(!world_input||build_mode||!third_person)pe_sandbox_actions_cancel(&tool_action);
        if(!world_input){autorun=false;jump_pending=false;}
        if(world_input) {
            if(IsKeyPressed(KEY_Z))walk=!walk;
            if(IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))autorun=!autorun;
            if(IsKeyPressed(KEY_F5))third_person=!third_person;

            if(IsKeyPressed(KEY_V)){
                build_shape=build_shape==PE_B45_CUBE?PE_B45_RAMP:PE_B45_CUBE;
                if(selected_item==PE_ITEM_FENCE||selected_item==PE_ITEM_LADDER)selected_item=PE_ITEM_WOOD;
            }
            if(IsKeyPressed(KEY_T))build_rotation=(build_rotation+1)%4;
            if(IsKeyPressed(KEY_G))build_brush=build_brush==4?1:4;
            Vector2 delta=(look_settle||(!captured&&!IsMouseButtonDown(MOUSE_BUTTON_RIGHT)))?(Vector2){0}:GetMouseDelta();
            game->yaw=remainderf(game->yaw-delta.x*.0024f,2*PI);
            game->pitch=Clamp(game->pitch-delta.y*.0024f,-1.4f,1.4f);
        }
        PeSandboxCameraInput camera_input={.dt=smoke_frames?PE_FIXED_DT:dt,
            .wheel_notches=GetMouseWheelMove(),.building=build_mode,.third_person=third_person,
            .input_allowed=world_input,.paused=action.paused||!focused};
        if(!pe_sandbox_camera_step(&orbit,camera_input)){
            snprintf(error,sizeof error,"Invalid sandbox camera state");result=1;break;
        }
        if(look_settle>0)look_settle--;
        Camera3D aim_camera=game_camera(game,collision,third_person,false,orbit.distance);
        Ray aim=source_ray(build_mode?GetMousePosition():(Vector2){GetScreenWidth()*.5f,GetScreenHeight()*.5f},aim_camera);
        int picked_role=0,picked_resident=-1;float nearest_npc=INFINITY;
        if(!build_mode) {
            Vector2 point=captured?(Vector2){GetScreenWidth()*.5f,GetScreenHeight()*.5f}:GetMousePosition();
            Ray pick=source_ray(point,aim_camera);
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
        bool auto_welcome=!welcome_prompted&&welcome_visible&&game->welcome_page<PE_ADVENTURE_WELCOME_PAGES&&
            Vector3Distance(rv(game->player),rv(welcome.position))<=9;
        int open_role=auto_welcome?1:((IsKeyPressed(KEY_E)||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))?picked_role:0);
        bool opened=false;PeAdventureNpc *selected=NULL;
        if(focused&&!loaded&&!build_mode&&action.npc_interaction){
            if(open_role){
                opened=open_role==1?pe_sandbox_ui_open_welcome(&ui,game,PE_ADVENTURE_WELCOME_ENTITY,true):pe_sandbox_ui_open_supplier(&ui,game,PE_ADVENTURE_NPC_ENTITY,true);
                selected=open_role==1?&welcome:&supplier;
                if(opened&&open_role==1)welcome_prompted=true;
            }else if(picked_resident>=0&&(IsKeyPressed(KEY_E)||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))){
                const PeAdventurePopulationSource *src=&pe_adventure_population_sources[picked_resident];Resident *r=&residents[picked_resident];selected=&r->npc;
                opened=src->adventure_role==3&&game->annot_complete?
                    pe_sandbox_ui_open_annot(&ui,game,(uint32_t)src->entity_id,r->view.portrait.texture,true):
                    pe_sandbox_ui_open_town(&ui,game,(uint32_t)src->entity_id,src->name,r->view.portrait.texture,true);
            }
        }
        if(opened){
            selected->yaw=atan2f(game->player.x-selected->position.x,game->player.z-selected->position.z)*RAD2DEG;
            world_input=false;autorun=false;jump_pending=false;tool_drawn=false;pe_sandbox_actions_cancel(&tool_action);
            action.consume_world=true;action.cursor_visible=true;
            if(captured){EnableCursor();captured=false;}HideCursor();
        }
        PeVec3 eye={game->player.x,game->player.y+1.58f,game->player.z};
        PeVec3 build_direction=construction_direction(game,aim,eye);
        if(focused&&!action.paused) {
            supplier_seconds+=smoke_frames?PE_FIXED_DT:dt;
            PeInput input={0};
            if(world_input) {
                float forward=(float)(IsKeyDown(KEY_W)-IsKeyDown(KEY_S));
                float side=(float)(IsKeyDown(KEY_A)-IsKeyDown(KEY_D));
                if(forward<0)autorun=false;
                if(autorun&&forward==0&&side==0)forward=1;
                input.move_x=side*cosf(game->yaw)+forward*sinf(game->yaw);
                input.move_z=side*sinf(game->yaw)-forward*cosf(game->yaw);
                input.sprint=IsKeyDown(KEY_LEFT_SHIFT);input.walk=walk;
                jump_pending=jump_pending||IsKeyPressed(KEY_SPACE);
                if(IsKeyPressed(KEY_E))interact(game);
                dig_cooldown=fmaxf(0,dig_cooldown-dt);
                if(!build_mode) {
                    if(IsKeyPressed(KEY_R)||IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)){tool_drawn=false;pe_sandbox_actions_cancel(&tool_action);}
                    else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                        tool_drawn=tool_ready;
                        if(timed_tool&&!tool_ready)snprintf(game->message,sizeof game->message,"Recover the equipped tool action assets before using it.");
                    }
                    if(!timed_tool&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&dig_cooldown<=0) {
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
            accumulator+=smoke_frames?PE_FIXED_DT:dt;int steps=0;
            while(accumulator>=PE_FIXED_DT&&steps++<6) {
                PeSandboxActionInput use={.enabled=world_input&&!build_mode&&third_person&&timed_tool&&
                    game->grounded&&!jump_pending&&fabsf(input.move_x)+fabsf(input.move_z)<.001f&&
                    tool_ready,
                    .held=world_input&&IsMouseButtonDown(MOUSE_BUTTON_LEFT),.tool_drawn=tool_drawn&&tool_ready,
                    .ray_origin=pv(aim.position),.ray_direction=pv(aim.direction),.camera_tree=-1,.forward_tree=-1};
                if(use.enabled&&(equipped->item_id==26||equipped->item_id==1527)){
                    float distance;
                    PeVec3 forward={sinf(game->yaw),0,-cosf(game->yaw)},origin={game->player.x,game->player.y+1,game->player.z};
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
                input.stamina_action=tool_action.stamina_lock;
                if(tool_action.stage!=PE_SANDBOX_ACTION_IDLE){input.move_x=input.move_z=0;input.sprint=false;}
                /* Refresh AFTER timed edits and before each motion step. */
                if(!pe_game_collision_update(game,collision,error,sizeof error)){result=1;break;}
                input.jump=jump_pending;
                /* Legacy saves retain their original controller/validator.
                 * Adventure uses the shared rendered-mesh capsule contract. */
                if(game->mode==PE_GAME_LEGACY)pe_game_step(game,input,PE_FIXED_DT);
                else if(!pe_game_step_collision(game,input,PE_FIXED_DT,collision)) {
                    snprintf(error,sizeof error,"Player capsule intersects world geometry");result=1;break;
                }
                jump_pending=false;accumulator-=PE_FIXED_DT;
            }
            if(result==1)break;
            if(steps>=6)accumulator=0;
            pe_sandbox_actor_update(actor,smoke_frames?PE_FIXED_DT:dt,game);
        } else {accumulator=0;jump_pending=false;}
        if(smoke_frames&&frame==30)pe_world_dig(&game->world,(PeVec3){27,pe_world_height(&game->world,27,27),27},1.7f);
        if(loaded||mesh_revision!=game->world.revision) {
            if(!update_terrain(&game->world,terrain,&triangles,&surface)){snprintf(error,sizeof error,"Terrain mesh allocation failed");result=1;break;}
            mesh_revision=game->world.revision;
        }
        PeSandboxActionPose pose;
        if(!pe_sandbox_actions_pose(&tool_action,game,&pose)||!pe_sandbox_actor_set_action(actor,&pose)){
            snprintf(error,sizeof error,"Tool action pose unavailable");result=1;break;
        }
        Camera3D camera=game_camera(game,collision,third_person,smoke_frames>0,orbit.distance);
        ui.building=build_mode;ui.build_item=selected_item;ui.build_shape=build_shape;
        ui.build_rotation=build_rotation;ui.build_brush=build_brush;
        double cycle=game->game_seconds/PE_DAY_SECONDS;
        float light=.6f+.4f*(float)(-.5*cos(cycle*2*PI)+.5);
        Color sky={(unsigned char)(127*light),(unsigned char)(176*light),(unsigned char)(188*light),255};
        pe_sandbox_ui_prepare(&ui,game);
        BeginDrawing();ClearBackground(sky);
        DrawRectangleGradientV(0,0,GetScreenWidth(),GetScreenHeight(),(Color){62,115,146,255},sky);
        source_begin_3d(camera);
        pe_terrain_material_view_frame(&surface,&game->world,camera.position);
        for(int i=0;i<PE_WORLD_MAX_REGIONS;i++)if(terrain->loaded[i])DrawModel(terrain->model[i],(Vector3){0},1,WHITE);
        if(pe_settlement_matches(&game->world))pe_reference_draw(&settlement);
        if(!pe_sandbox_world_view_draw_trees(world_view,game)){snprintf(error,sizeof error,"Tree rendering failed");result=1;}
        for(int i=0;i<game->block_count;i++)draw_block(game->blocks[i]);
        for(int i=0;i<game->cell_count;i++)draw_cell(&game->cells[i]);
        if(third_person||smoke_frames)pe_sandbox_actor_draw(actor,game);
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
            Ray indicator=source_ray((Vector2){GetScreenWidth()*.5f,GetScreenHeight()*.5f},camera);
            PeHit hit=pe_world_dig_target(&game->world,pv(indicator.position),pv(indicator.direction),game->player,3);
            if(hit.hit)DrawCubeWires((Vector3){hit.x,hit.y,hit.z},1,1,1,(Color){71,216,245,255});
        }
        if(welcome_visible&&!pe_adventure_npc_view_draw(&welcome_view,welcome.position,welcome.yaw,supplier_seconds)){
            snprintf(error,sizeof error,"Welcome NPC pose rendering failed");result=1;
        }
        if(supplier_visible&&!pe_adventure_npc_view_draw(&supplier_view,supplier.position,supplier.yaw,supplier_seconds)){
            snprintf(error,sizeof error,"Supplier pose rendering failed");result=1;
        }
        for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)if(residents[i].visible&&!pe_adventure_npc_view_draw(&residents[i].view,residents[i].npc.position,residents[i].npc.yaw,supplier_seconds)){snprintf(error,sizeof error,"Town NPC rendering failed");result=1;}
        if(!game->world.source)DrawCube((Vector3){31.5f,5.5f,31.5f},63,.06f,63,(Color){54,132,149,170});
        source_end_3d();
        if(supplier_visible&&ui.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE) {
            PeVec3 p=supplier.position;Vector2 pos=source_screen((Vector3){p.x,p.y+supplier_view.label_height,p.z},camera);
            const char *prompt=picked_role==2&&action.npc_interaction?"Eitai  [RMB / E]":"Eitai";
            if(Vector3DotProduct(Vector3Subtract(rv(p),camera.position),Vector3Subtract(camera.target,camera.position))>0)
                DrawTextEx(pe_ui_font(ui.assets,false),prompt,(Vector2){pos.x-45,pos.y},18,0,game->supplies_claimed?LIGHTGRAY:YELLOW);
        }
        if(welcome_visible&&ui.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE) {
            PeVec3 p=welcome.position;Vector2 pos=source_screen((Vector3){p.x,p.y+welcome_view.label_height,p.z},camera);
            const char *prompt=picked_role==1&&action.npc_interaction?"Westina  [RMB / E]":"Westina";
            if(Vector3DotProduct(Vector3Subtract(rv(p),camera.position),Vector3Subtract(camera.target,camera.position))>0)
                DrawTextEx(pe_ui_font(ui.assets,false),prompt,(Vector2){pos.x-45,pos.y},18,0,game->welcome_page<PE_ADVENTURE_WELCOME_PAGES?YELLOW:LIGHTGRAY);
        }
        if(ui.dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE&&!ui.town_open)for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)if(residents[i].visible){
            PeVec3 p=residents[i].npc.position;
            if(Vector3Distance(rv(p),rv(game->player))>40||Vector3DotProduct(Vector3Subtract(rv(p),camera.position),Vector3Subtract(camera.target,camera.position))<=0)continue;
            Vector2 screen=source_screen((Vector3){p.x,p.y+residents[i].view.label_height,p.z},camera);
            char label[180];snprintf(label,sizeof label,"%s%s",pe_adventure_population_sources[i].name,picked_resident==i&&action.npc_interaction?"  [RMB / E]":"");
            Font f=pe_ui_font(ui.assets,false);Vector2 size=MeasureTextEx(f,label,18,0);
            DrawTextEx(f,label,(Vector2){screen.x-size.x*.5f,screen.y},18,0,picked_resident==i?YELLOW:LIGHTGRAY);
        }
        pe_sandbox_ui_draw(&ui,game,build_mode);
        if(smoke_frames&&frame+1>=smoke_frames&&screenshot) {
            rlDrawRenderBatchActive();
            Image capture=LoadImageFromScreen();bool saved=capture.data&&ExportImage(capture,screenshot);UnloadImage(capture);
            if(!saved){snprintf(error,sizeof error,"Screenshot export failed");result=1;}
        }
        EndDrawing();frame++;
        if(!pe_sandbox_actor_ok(actor)){snprintf(error,sizeof error,"Character pose rendering failed");result=1;break;}
        if(result==1||(smoke_frames&&frame>=smoke_frames))break;
    }
    if(smoke_frames&&frame<smoke_frames)result=1;
cleanup:
    if(result==1)fprintf(stderr,"Sandbox failed: %s\n",error);
    if(captured)EnableCursor();
    ShowCursor();
    printf("Sandbox: %d frames, %zu triangles, mode %d, recipes %d, mission %d, result %d\n",
           frame,triangles,game->mode,game->known_recipe_count,game->adventure_mission,result);
    printf("Sandbox tools: %llu swings, %llu resource changes\n",(unsigned long long)tool_action.swings,(unsigned long long)tool_edits);
    printf("Sandbox camera: desired %.3fm, distance %.3fm\n",orbit.desired_distance,orbit.distance);
    pe_reference_unload(&settlement);
    pe_sandbox_world_view_unload(world_view);pe_sandbox_actor_unload(actor);pe_sandbox_ui_unload(&ui);
    for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)pe_adventure_npc_view_unload(&residents[i].view);
    pe_adventure_npc_view_unload(&supplier_view);pe_adventure_npc_view_unload(&welcome_view);
    pe_sandbox_collision_free(collision);free_terrain(terrain);pe_terrain_material_view_unload(&surface);CloseWindow();return result;
}
