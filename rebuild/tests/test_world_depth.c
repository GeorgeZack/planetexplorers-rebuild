#define _POSIX_C_SOURCE 200809L
#include "pe_game.h"
#include "pe_settlement.h"
#include "pe_sandbox_actions.h"
#include "adventure_test_helpers.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define CHECK(e) do {if(!(e)){fprintf(stderr,"world-depth:%d: %s\n",__LINE__,#e);failures++;}}while(0)
static void put32(unsigned char *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
/* Patch footer or remove/add a byte while preserving the envelope checksum. */
static bool corrupt(const char *path,int mode){
    FILE *f=fopen(path,"rb");if(!f)return false;
    if(fseek(f,0,SEEK_END)){fclose(f);return false;}long n=ftell(f);rewind(f);
    if(n<20){fclose(f);return false;}unsigned char *p=malloc((size_t)n+1);if(!p){fclose(f);return false;}
    bool ok=fread(p,1,(size_t)n,f)==(size_t)n;fclose(f);
    if(mode==0)put32(p+n-PE_SAVE_V16_APPEND-PE_SAVE_V15_APPEND-PE_SAVE_V14_APPEND-PE_SAVE_V13_APPEND-PE_SAVE_V11_APPEND-PE_SAVE_V10_APPEND-(8+16*(PE_TOWN_STOCK_CAPACITY-PE_TOWN_V8_STOCK_CAPACITY))-(12+4+16*PE_TOWN_V8_STOCK_CAPACITY)-4,96);else if(mode==1)n--;else p[n++]=0;
    put32(p+8,(uint32_t)n-16);uint32_t sum=2166136261u;
    for(long i=16;i<n;i++){sum^=p[i];sum*=16777619u;}put32(p+12,sum);
    f=fopen(path,"wb");if(!f)ok=false;else{ok=ok&&fwrite(p,1,(size_t)n,f)==(size_t)n;if(fclose(f))ok=false;}
    free(p);return ok;
}
static int static_geometry_tests(void){
    int failures=0;char error[200];PeWorld *w=calloc(1,sizeof *w);
    PeSandboxCollision *cache=pe_sandbox_collision_create();
    if(!w||!cache){free(w);pe_sandbox_collision_free(cache);return 1;}w->depth=128;
    /* One vertical wall and a supported floor, independently of terrain. */
    PeVertex vertices[]={
        {.position={10,8,85}},{.position={10,14,85}},{.position={10,14,105}},
        {.position={10,8,85}},{.position={10,14,105}},{.position={10,8,105}},
        {.position={10,9.1f,90}},{.position={16,9.1f,90}},{.position={16,9.1f,105}},
        {.position={10,9.1f,90}},{.position={16,9.1f,105}},{.position={10,9.1f,105}}
    };
    PeMeshData mesh={vertices,sizeof vertices/sizeof *vertices};
    PeSceneCollision *scene=pe_scene_from_mesh(&mesh,(PeVec3){0},error,sizeof error);CHECK(scene);
    CHECK(pe_scene_overlaps_aabb(scene,(PeVec3){9.9f,10,95},(PeVec3){10.1f,11,96}));
    CHECK(!pe_scene_overlaps_aabb(scene,(PeVec3){9,10,95},(PeVec3){10,11,96}));
    CHECK(!pe_scene_overlaps_aabb(scene,(PeVec3){11,10,95},(PeVec3){12,11,96}));
    CHECK(pe_sandbox_collision_update_with_static(cache,w,NULL,0,NULL,0,NULL,0,scene,error,sizeof error));
    CHECK(pe_sandbox_collision_stats(cache).triangles==4);
    CHECK(pe_sandbox_collision_ray_geometry(cache,(PeVec3){8,11,95},(PeVec3){1,0,0},4).hit);
    CHECK(pe_sandbox_collision_overlap(cache,(PeVec3){9.9f,9,95},.3f,1.8f));
    CHECK(!pe_sandbox_collision_overlap(cache,(PeVec3){8,9,95},.3f,1.8f));
    PeSceneHit hit=pe_sandbox_collision_sweep_geometry(cache,(PeVec3){8,9,95},.3f,1.8f,(PeVec3){4,0,0});
    CHECK(hit.hit&&fabsf(hit.position.x-10)<.001f&&hit.fraction<.5f);
    CHECK(pe_sandbox_collision_support(cache,(PeVec3){12,10,95},.3f,1.8f,2,.65f).hit);
    PeVec3 settled={0};CHECK(pe_sandbox_collision_settle(cache,(PeVec3){12,10,95},.3f,1.8f,0,2,&settled));
    CHECK(fabsf(settled.y-9.1f)<.01f);
    PeSandboxMotion motion={0};CHECK(pe_sandbox_collision_move(cache,(PeVec3){8,9,95},(PeVec3){20,0,0},.2f,.3f,1.8f,false,false,&motion));
    CHECK(motion.position.x<9.701f&&motion.position.x>9.6f);
    PeSandboxBox box={{9.9f,10,95},{10.4f,11,96}};
    uint64_t generation=pe_sandbox_collision_stats(cache).generation;
    CHECK(!pe_sandbox_collision_update_with_static(cache,w,NULL,0,&box,1,NULL,0,scene,error,sizeof error));
    CHECK(pe_sandbox_collision_stats(cache).generation==generation);
    /* A static surface crossing construction rejects the entire new snapshot. */
    PeB45Cell cell={23,18,190,(PE_B45_CUBE<<2),0};
    CHECK(!pe_sandbox_collision_update_with_static(cache,w,&cell,1,NULL,0,NULL,0,scene,error,sizeof error));
    CHECK(pe_sandbox_collision_stats(cache).generation==generation);
    CHECK(pe_sandbox_collision_update(cache,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_sandbox_collision_stats(cache).generation>generation);
    CHECK(!pe_sandbox_collision_ray_geometry(cache,(PeVec3){8,11,95},(PeVec3){1,0,0},4).hit);
    CHECK(!pe_sandbox_collision_overlap(cache,(PeVec3){9.9f,9,95},.3f,1.8f));
    CHECK(!pe_sandbox_collision_support(cache,(PeVec3){12,10,95},.3f,1.8f,2,.65f).hit);
    pe_sandbox_collision_free(cache);pe_scene_free(scene);free(w);return failures;
}
static int static_game_child(void){
    int failures=0;char directory[]="/tmp/pe-static-game-XXXXXX",folder[128],path[160],save[160],error[200];
    if(!mkdtemp(directory))return 1;
    snprintf(folder,sizeof folder,"%s/adventure-settlement",directory);
    snprintf(path,sizeof path,"%s/settlement-collision.obj",folder);
    snprintf(save,sizeof save,"%s/game.pecs",directory);
    CHECK(mkdir(folder,0700)==0);
    CHECK(pe_settlement_configure(directory,error,sizeof error));
    PeGame *g=calloc(1,sizeof *g),*before=calloc(1,sizeof *before);
    PeSandboxCollision *cache=pe_sandbox_collision_create();
    if(!g||!before||!cache){free(g);free(before);pe_sandbox_collision_free(cache);rmdir(folder);rmdir(directory);return 1;}
    CHECK(pe_game_init_mode(g,666,PE_GAME_ADVENTURE));
    memset(&g->world,0,sizeof g->world);g->world.depth=128;g->world.source=1;
    g->world.origin_x=-627;g->world.origin_y=16;g->world.origin_z=323;
    g->player=(PeVec3){9.5f,9,95};g->tree_count=0;
    CHECK(!pe_game_collision_update(g,cache,error,sizeof error));
    CHECK(!pe_game_save(g,save,error,sizeof error));
    CHECK(!pe_game_static_segment_clear(g,(PeVec3){9,10,95},(PeVec3){11,10,95}));
    FILE *f=fopen(path,"wb");CHECK(f);
    if(f){fputs("v 10.25 8 94\nv 10.25 14 94\nv 10.25 14 98\nv 10.25 8 98\nf 1 2 3\nf 1 3 4\n",f);CHECK(fclose(f)==0);}
    CHECK(pe_game_collision_update(g,cache,error,sizeof error));
    CHECK(!pe_game_static_segment_clear(g,(PeVec3){9,10,95},(PeVec3){11,10,95}));
    CHECK(pe_game_static_segment_clear(g,(PeVec3){9,10,95},(PeVec3){10,10,95}));
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};
    CHECK(pe_inventory_add(&g->inventory,12,1));CHECK(pe_game_equip_slot(g,0));
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,10));
    PeB45Cell cell={20,20,190,(PE_B45_CUBE<<2),0};
    CHECK(!pe_game_can_build_cells(g,&cell,1));*before=*g;
    CHECK(!pe_game_build_cells(g,&cell,1));CHECK(g->cell_count==before->cell_count);
    CHECK(!memcmp(&g->inventory,&before->inventory,sizeof g->inventory));
    CHECK(pe_world_set(&g->world,12,9,95,(PeVoxel){255,2}));
    PeResourceEdit edit={0};*before=*g;
    CHECK(!pe_game_dig_sample(g,12,9,95,&edit));CHECK(!memcmp(g,before,sizeof *g));
    PeSandboxActions action;pe_sandbox_actions_init(&action,false);
    PeSandboxActionInput in={.enabled=true,.held=true,.tool_drawn=true,
        .ray_origin={9.5f,9.1f,95.1f},.ray_direction={1,0,0},.camera_tree=-1,.forward_tree=-1};
    CHECK(pe_world_dig_target(&g->world,in.ray_origin,in.ray_direction,g->player,3).hit);
    PeSandboxActionUpdate update={0};float stamina=g->stamina;
    CHECK(pe_sandbox_actions_step(&action,g,in,&update));
    CHECK(!update.started&&action.stage==PE_SANDBOX_ACTION_IDLE&&g->stamina==stamina);
    CHECK(pe_game_save(g,save,error,sizeof error));
    g->player.x=10.25f;CHECK(!pe_game_save(g,save,error,sizeof error));
    CHECK(pe_game_load(g,save,error,sizeof error));CHECK(g->player.x==9.5f);
    g->world.source=0;g->world.origin_x=0;
    CHECK(pe_game_collision_update(g,cache,error,sizeof error));
    CHECK(pe_game_static_segment_clear(g,(PeVec3){9,10,95},(PeVec3){11,10,95}));
    CHECK(!pe_sandbox_collision_ray_geometry(cache,(PeVec3){9,11,95},(PeVec3){1,0,0},2).hit);
    pe_sandbox_collision_free(cache);free(g);free(before);unlink(save);unlink(path);rmdir(folder);rmdir(directory);return failures;
}
static int static_game_tests(void){
    /* Isolate the immutable process-lifetime asset configuration from tests
     * that may inspect actual recovered assets later in this executable. */
    pid_t pid=fork();if(pid<0)return 1;
    if(!pid)_exit(static_game_child()?1:0);
    int status=0;if(waitpid(pid,&status,0)!=pid)return 1;
    return !WIFEXITED(status)||WEXITSTATUS(status)!=0;
}
static int extended_dimensions_tests(void){
    int failures=0;char error[200],path[]="/tmp/pe-dimensions-XXXXXX";
    PeWorld *w=calloc(1,sizeof *w),*copy=calloc(1,sizeof *copy);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*before=calloc(1,sizeof *before);
    PeSandboxCollision *collision=pe_sandbox_collision_create();
    if(!w||!copy||!g||!loaded||!before||!collision){failures++;goto done;}
    CHECK(pe_world_resize(w,96,64,160));CHECK(w->extended);
    CHECK(pe_world_width(w)==96&&pe_world_size_y(w)==64&&pe_world_depth(w)==160);
    CHECK(pe_world_voxel_count(w)==96u*64u*160u&&pe_world_region_count(w)==30);
    for(int z=0;z<160;z++)for(int y=0;y<=40;y++)for(int x=0;x<96;x++)
        pe_world_set(w,x,y,z,(PeVoxel){255,PE_MAT_DIRT});
    CHECK(pe_world_clone(copy,w));CHECK(copy->extended!=w->extended);
    uint64_t hash=pe_world_hash(w);CHECK(hash==pe_world_hash(copy));
    CHECK(!pe_world_resize(w,513,64,160)&&hash==pe_world_hash(w));
    CHECK(!pe_world_set(w,96,40,140,(PeVoxel){0}));
    CHECK(pe_world_set(copy,80,40,140,(PeVoxel){0}));CHECK(pe_world_get(w,80,40,140).volume==255);
    CHECK(pe_world_mesh_region_changed(w,copy,64,32,128,31,31,31));
    PeHit hit=pe_world_raycast(w,(PeVec3){80,60,140},(PeVec3){0,-1,0},60);
    CHECK(hit.hit&&fabsf(hit.position.y-40.49804f)<.001f);
    CHECK(pe_sandbox_collision_update(collision,w,NULL,0,NULL,0,error,sizeof error));
    PeSceneHit physical=pe_sandbox_collision_ray_geometry(collision,(PeVec3){80,60,140},(PeVec3){0,-1,0},60);
    CHECK(physical.hit&&fabsf(physical.position.y-hit.position.y)<.001f);
    CHECK(!pe_sandbox_collision_overlap(collision,(PeVec3){80,41,140},.25f,1.8f));
    CHECK(pe_sandbox_collision_overlap(collision,(PeVec3){80,39,140},.25f,1.8f));
    PeSandboxCollisionStats stats=pe_sandbox_collision_stats(collision);
    CHECK(pe_sandbox_collision_update(collision,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_sandbox_collision_stats(collision).terrain_rebuilds==stats.terrain_rebuilds);
    CHECK(pe_game_init_mode(g,91,PE_GAME_ADVENTURE));CHECK(pe_game_set_world(g,w,(PeVec3){80,42,140}));
    CHECK(g->world.extended!=w->extended&&g->player.x==80&&g->player.z==140&&g->player.y>40);
    CHECK(pe_game_clone(before,g));CHECK(before->world.extended!=g->world.extended);
    PeWorld *empty=calloc(1,sizeof *empty);CHECK(empty);
    if(empty){CHECK(pe_world_resize(empty,96,64,160));CHECK(!pe_game_set_world(g,empty,(PeVec3){80,42,140}));CHECK(pe_world_hash(&g->world)==pe_world_hash(&before->world));pe_world_destroy(empty);free(empty);}
    int fd=mkstemp(path);CHECK(fd>=0);
    if(fd>=0){close(fd);
        CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));
        CHECK(pe_world_width(&loaded->world)==96&&pe_world_size_y(&loaded->world)==64&&pe_world_depth(&loaded->world)==160);
        CHECK(pe_world_hash(&loaded->world)==pe_world_hash(&g->world));
        CHECK(loaded->world.extended!=g->world.extended&&loaded->player.z==140);
        CHECK(corrupt(path,1));hash=pe_world_hash(&loaded->world);
        CHECK(!pe_game_load(loaded,path,error,sizeof error)&&hash==pe_world_hash(&loaded->world));
        unlink(path);
    }
    pe_world_destroy(copy);CHECK(pe_world_resize(copy,512,128,512));CHECK(pe_world_region_count(copy)==PE_WORLD_MAX_REGIONS);
    CHECK(pe_world_set(copy,511,127,511,(PeVoxel){255,17}));CHECK(pe_world_get(copy,511,127,511).material==17);
    pe_world_destroy(copy);CHECK(pe_world_clone(copy,w));
    CHECK(pe_world_set(copy,80,40,140,(PeVoxel){0}));
    CHECK(pe_sandbox_collision_update(collision,copy,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_sandbox_collision_stats(collision).terrain_rebuilds>stats.terrain_rebuilds);
    CHECK(pe_sandbox_collision_stats(collision).terrain_rebuilds-stats.terrain_rebuilds<30);
    /* A new independently owned world can share the same revision and metadata;
     * replacement buffer identity must prevent an unchanged-world fast path. */
    pe_world_data(copy)[((size_t)140*64+40)*96+80]=(PeVoxel){255,PE_MAT_DIRT};
    copy->revision=w->revision;
    CHECK(pe_sandbox_collision_update(collision,w,NULL,0,NULL,0,error,sizeof error));
    stats=pe_sandbox_collision_stats(collision);
    pe_world_data(copy)[((size_t)140*64+40)*96+80]=(PeVoxel){0};
    CHECK(pe_sandbox_collision_update(collision,copy,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_sandbox_collision_stats(collision).terrain_rebuilds>stats.terrain_rebuilds);
 done:
    pe_world_destroy(w);pe_world_destroy(copy);free(w);free(copy);
    pe_game_destroy(g);pe_game_destroy(loaded);pe_game_destroy(before);free(g);free(loaded);free(before);
    pe_sandbox_collision_free(collision);return failures;
}
int pe_test_world_depth(void){
    int failures=static_geometry_tests()+static_game_tests()+extended_dimensions_tests();char error[200],path[]="/tmp/pe-depth-XXXXXX";
    PeWorld *w=calloc(1,sizeof *w),*old=calloc(1,sizeof *old);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*before=calloc(1,sizeof *before);
    PeSandboxCollision *collision=pe_sandbox_collision_create();
    if(!w||!old||!g||!loaded||!before||!collision){free(w);free(old);free(g);free(loaded);free(before);pe_sandbox_collision_free(collision);return 1;}
    CHECK(pe_world_depth(w)==64&&pe_world_voxel_count(w)==PE_VOXEL_COUNT);
    pe_world_init(w,666);CHECK(pe_world_hash(w)==UINT64_C(14854765168810625299));
    for(size_t i=PE_VOXEL_COUNT;i<PE_MAX_VOXEL_COUNT;i++)CHECK(!w->voxels[i].volume&&!w->voxels[i].material);
    CHECK(!pe_world_set(w,20,8,95,(PeVoxel){255,PE_MAT_STONE}));
    memset(w,0,sizeof *w);w->depth=128;
    for(int z=0;z<128;z++)for(int y=0;y<=8;y++)for(int x=0;x<64;x++)
        CHECK(pe_world_set(w,x,y,z,(PeVoxel){255,PE_MAT_STONE}));
    *old=*w;CHECK(pe_world_depth(w)==128&&pe_world_voxel_count(w)==PE_MAX_VOXEL_COUNT);
    PeMeshData mesh={0};CHECK(pe_world_mesh(w,&mesh));bool extended=false;
    for(size_t i=0;i<mesh.count;i++)if(mesh.vertices[i].position.z>95)extended=true;
    CHECK(extended);pe_mesh_free(&mesh);
    PeHit ray=pe_world_raycast(w,(PeVec3){20,15,95},(PeVec3){0,-1,0},20);
    CHECK(ray.hit&&fabsf(ray.position.y-(8+127.0f/255.0f))<.001f);
    CHECK(pe_world_dig_target(w,(PeVec3){20,10,95},(PeVec3){0,-1,0},(PeVec3){20,9,95},3).hit);
    CHECK(pe_sandbox_collision_update(collision,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_sandbox_collision_stats(collision).terrain_rebuilds==8);
    CHECK(!pe_sandbox_collision_overlap(collision,(PeVec3){20,9,95},.3f,1.8f));
    CHECK(pe_sandbox_collision_overlap(collision,(PeVec3){20,7,95},.3f,1.8f));
    CHECK(pe_sandbox_collision_support(collision,(PeVec3){20,9,95},.3f,1.8f,2,.65f).hit);
    PeSceneHit boundary=pe_sandbox_collision_sweep(collision,(PeVec3){20,15,95},.3f,1.8f,(PeVec3){0,0,50});
    CHECK(boundary.hit&&fabsf(boundary.position.z-126.7f)<.001f);
    CHECK(pe_world_dig(w,(PeVec3){20,8,95},1)>0);
    CHECK(pe_world_mesh_region_changed(w,old,0,0,64,32,31,32));
    CHECK(pe_sandbox_collision_update(collision,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_sandbox_collision_stats(collision).terrain_rebuilds>8);
    PeSandboxBox box={{20,10,95},{21,12,96}};
    CHECK(pe_sandbox_collision_update(collision,w,NULL,0,&box,1,error,sizeof error));
    CHECK(pe_sandbox_collision_overlap(collision,(PeVec3){20.5f,10,95.5f},.3f,1.8f));
    w->depth=64;CHECK(pe_sandbox_collision_update(collision,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_sandbox_collision_overlap(collision,(PeVec3){20,15,95},.3f,1.8f));
    boundary=pe_sandbox_collision_sweep(collision,(PeVec3){20,15,60},.3f,1.8f,(PeVec3){0,0,10});
    CHECK(boundary.hit&&fabsf(boundary.position.z-62.7f)<.001f);
    CHECK(!pe_sandbox_collision_ray_geometry(collision,(PeVec3){20,15,95},(PeVec3){0,-1,0},20).hit);
    w->depth=96;CHECK(!pe_sandbox_collision_update(collision,w,NULL,0,NULL,0,error,sizeof error));
    CHECK(pe_world_depth(w)==0);w->depth=128;
    pe_game_init_mode(g,666,PE_GAME_ADVENTURE);
    CHECK(pe_game_set_world(g,w,(PeVec3){25,9,95}));CHECK(g->player.z==95);
    memset(g->inventory.slots,0,sizeof g->inventory.slots);
    CHECK(pe_inventory_add(&g->inventory,12,1));CHECK(pe_game_equip_slot(g,0));
    PeResourceEdit edit={0};CHECK(pe_game_dig_sample(g,22,8,95,&edit)&&edit.changed);
    /* A stored extended-Z action must survive both pose and subsequent-clock
     * validation, not merely select a target on its initial frame. */
    PeSandboxActions action;pe_sandbox_actions_init(&action,false);
    PeSandboxActionInput in={.enabled=true,.held=true,.tool_drawn=true,
        .ray_origin={27,10,95},.ray_direction={0,-1,0},.camera_tree=-1,.forward_tree=-1};
    PeSandboxActionUpdate update={0};PeSandboxActionPose pose;
    CHECK(pe_sandbox_actions_step(&action,g,in,&update));CHECK(update.started&&action.target_z==95);
    CHECK(pe_sandbox_actions_pose(&action,g,&pose));
    g->world.depth=64;CHECK(!pe_sandbox_actions_pose(&action,g,&pose));
    CHECK(!pe_sandbox_actions_step(&action,g,in,&update));g->world.depth=128;
    bool struck=false;
    for(int frame=0;frame<100&&!struck;frame++){
        CHECK(pe_sandbox_actions_step(&action,g,in,&update));
        CHECK(pe_sandbox_actions_pose(&action,g,&pose));
        if(update.swung){CHECK(update.changed&&update.z==95);struck=true;}
    }
    CHECK(struck&&action.swings==1);
    int fd=mkstemp(path);CHECK(fd>=0);if(fd>=0){close(fd);
        CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));
        CHECK(pe_world_depth(&loaded->world)==128&&loaded->player.z==95);
        CHECK(pe_world_hash(&loaded->world)==pe_world_hash(&g->world));
        CHECK(!memcmp(loaded->world.voxels,g->world.voxels,sizeof g->world.voxels));
        CHECK(pe_test_save_pre_welcome(path,7));CHECK(pe_game_load(loaded,path,error,sizeof error));
        CHECK(pe_world_depth(&loaded->world)==128&&pe_world_hash(&loaded->world)==pe_world_hash(&g->world));
        for(int mode=1;mode<=2;mode++){
            CHECK(pe_game_save(g,path,error,sizeof error));CHECK(corrupt(path,mode));*before=*loaded;
            CHECK(!pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(before,loaded,sizeof *loaded));
        }
        pe_game_init_mode(g,666,PE_GAME_ADVENTURE);CHECK(pe_game_welcome_advance(g,0));
        for(unsigned v=4;v<=7;v++){
            pe_game_init_mode(g,666,PE_GAME_ADVENTURE);
            if(v<6)CHECK(pe_test_ack_welcome(g));else CHECK(pe_game_welcome_advance(g,0));
            CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_test_save_pre_welcome(path,v));
            CHECK(pe_game_load(loaded,path,error,sizeof error));CHECK(pe_world_depth(&loaded->world)==64);
            CHECK(loaded->welcome_page==(v>=6?1:PE_ADVENTURE_WELCOME_PAGES));
            for(size_t i=PE_VOXEL_COUNT;i<PE_MAX_VOXEL_COUNT;i++)CHECK(!loaded->world.voxels[i].volume&&!loaded->world.voxels[i].material);
        }
        for(int mode=0;mode<3;mode++){
            CHECK(pe_game_save(g,path,error,sizeof error));CHECK(corrupt(path,mode));*before=*loaded;
            CHECK(!pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(before,loaded,sizeof *loaded));
        }
        unlink(path);
    }
    pe_sandbox_collision_free(collision);free(w);free(old);free(g);free(loaded);free(before);return failures;
}
