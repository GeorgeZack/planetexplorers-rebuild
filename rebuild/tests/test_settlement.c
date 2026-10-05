#define _POSIX_C_SOURCE 200809L
#include "pe_settlement.h"
#include "pe_adventure_npc.h"
#include "pe_adventure_dialogue.h"
#include "adventure_test_helpers.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
#define CHECK(e) do{if(!(e)){fprintf(stderr,"settlement:%d: %s (%s)\n",__LINE__,#e,error);failures++;}}while(0)
static void put32(unsigned char *p,uint32_t value){for(int i=0;i<4;i++)p[i]=(unsigned char)(value>>(i*8));}
static bool bytes_file(const char *path,const unsigned char *bytes,size_t count){
    FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(bytes,1,count,f)==count;return fclose(f)==0&&ok;
}
static int loader_tests(void){
    int failures=0;char error[256]={0},path[]=PE_SOURCE_ROOT "/rebuild/build/settlement-loader-XXXXXX";
    int fd=mkstemp(path);if(fd<0)return 1;close(fd);
    const size_t length=32+2*64*32*128;
    unsigned char *bytes=calloc(length+1,1);PeWorld *w=calloc(1,sizeof *w),*before=calloc(1,sizeof *before);
    if(!bytes||!w||!before){free(bytes);free(w);free(before);unlink(path);return 1;}
    memcpy(bytes,"PEVX",4);put32(bytes+4,1);put32(bytes+8,(uint32_t)-627);put32(bytes+12,16);
    put32(bytes+16,323);put32(bytes+20,64);put32(bytes+24,32);put32(bytes+28,128);
    bytes[32]=128;bytes[33]=45;bytes[length-2]=201;bytes[length-1]=4;
    CHECK(bytes_file(path,bytes,length));CHECK(pe_settlement_load_world(path,w,error,sizeof error));
    CHECK(fabsf(fabsf(pe_settlement_npc_yaw(1))-180)<.001f);
    CHECK(fabsf(pe_settlement_npc_yaw(2))<.001f);
    CHECK(pe_settlement_matches(w));CHECK(pe_world_voxel_count(w)==64*32*128);
    CHECK(w->voxels[0].volume==128&&w->voxels[0].material==45);
    CHECK(w->voxels[64*32*128-1].volume==201&&w->voxels[64*32*128-1].material==4);
    *before=*w;
    const size_t lengths[]={0,31,32,length-1,length+1};
    for(size_t i=0;i<sizeof lengths/sizeof lengths[0];i++){
        CHECK(bytes_file(path,bytes,lengths[i]));CHECK(!pe_settlement_load_world(path,w,error,sizeof error));
        CHECK(!memcmp(w,before,sizeof *w));
    }
    for(int offset=0;offset<32;offset+=4){
        bytes[offset]^=1;CHECK(bytes_file(path,bytes,length));
        CHECK(!pe_settlement_load_world(path,w,error,sizeof error));CHECK(!memcmp(w,before,sizeof *w));
        bytes[offset]^=1;
    }
    CHECK(!pe_settlement_load_world(NULL,w,error,sizeof error));CHECK(!memcmp(w,before,sizeof *w));
    CHECK(!pe_settlement_load_world(path,NULL,error,sizeof error));
    unlink(path);free(bytes);free(w);free(before);return failures;
}
static int world_arrival_tests(void){
    int failures=0;char error[128]={0};PeGame *game=calloc(1,sizeof *game);if(!game)return 1;
    CHECK(pe_game_init_mode(game,666,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(game));
    game->world.source=1;game->world.origin_x=-627;game->world.origin_z=323;
    PeVec3 anchor=pe_settlement_npc_anchor(2);
    /* World supplier -598.52 truncates to -598 => crop X29, not local X28. */
    game->player=(PeVec3){34,10,18};
    CHECK(!pe_adventure_arrival_step(game,anchor));CHECK(!game->arrival_complete);
    game->player.x=24;CHECK(!pe_adventure_arrival_step(game,anchor));CHECK(!game->arrival_complete);
    game->player.x=33.999f;CHECK(pe_adventure_arrival_step(game,anchor));
    CHECK(game->arrival_complete&&!game->supplies_claimed);
    free(game);return failures;
}
static int actual_asset_child(void){
    int failures=0;char error[256]={0};
    const char *terrain=PE_SOURCE_ROOT "/rebuild/build/reference-assets/adventure-settlement/terrain.pevx";
    const char *collision=PE_SOURCE_ROOT "/rebuild/build/reference-assets/adventure-settlement/settlement-collision.obj";
    if(access(terrain,R_OK)||access(collision,R_OK)){
        fprintf(stderr,"SKIP settlement actual-asset start/NPC/save test: recovered terrain/collision unavailable\n");return 0;
    }
    PeGame *game=calloc(1,sizeof *game),*loaded=calloc(1,sizeof *loaded);
    PeSandboxCollision *cache=pe_sandbox_collision_create();
    if(!game||!loaded||!cache){free(game);free(loaded);pe_sandbox_collision_free(cache);return 1;}
    CHECK(pe_settlement_configure(PE_SOURCE_ROOT "/rebuild/build/reference-assets",error,sizeof error));
    CHECK(pe_game_init_mode(game,666,PE_GAME_ADVENTURE));
    if(!pe_settlement_start(game,error,sizeof error)){CHECK(false);goto done;}
    CHECK(pe_settlement_matches(&game->world));CHECK(game->world.seed==666);
    CHECK(fabsf(game->yaw-3.14159265358979323846f)<.000001f);
    CHECK(game->welcome_page==0);CHECK(game->player.x>=0&&game->player.x<pe_world_width(&game->world)&&game->player.z>=0&&game->player.z<pe_world_depth(&game->world));
    if(!pe_game_collision_update(game,cache,error,sizeof error)){CHECK(false);goto done;}
    CHECK(!pe_sandbox_collision_overlap(cache,game->player,.3f,1.8f));
    CHECK(pe_sandbox_collision_support(cache,game->player,.3f,1.8f,.1f,.65f).hit);
    for(int role=1;role<=2;role++){
        PeVec3 anchor=pe_settlement_npc_anchor_for_world(&game->world,role);PeAdventureNpc npc={0};
        CHECK(pe_adventure_npc_refresh_source(&npc,&game->world,cache,anchor,pe_settlement_npc_yaw(role),error,sizeof error));
        CHECK(npc.placed&&npc.position.x==anchor.x&&npc.position.z==anchor.z);
        CHECK(fabsf(npc.position.y-anchor.y)<.5f);
    }
    char save[]=PE_SOURCE_ROOT "/rebuild/build/settlement-save-XXXXXX";
    int fd=mkstemp(save);CHECK(fd>=0);
    if(fd>=0){
        close(fd);CHECK(pe_game_save(game,save,error,sizeof error));CHECK(pe_game_load(loaded,save,error,sizeof error));
        CHECK(pe_settlement_matches(&loaded->world));CHECK(pe_world_depth(&loaded->world)==pe_world_depth(&game->world));
        CHECK(!memcmp(pe_world_data_const(&game->world),pe_world_data_const(&loaded->world),pe_world_voxel_count(&game->world)*sizeof(PeVoxel)));
        CHECK(!memcmp(&game->player,&loaded->player,sizeof game->player));CHECK(loaded->welcome_page==0);CHECK(loaded->yaw==game->yaw);
        unlink(save);
    }
done:
    pe_sandbox_collision_free(cache);pe_game_destroy(game);pe_game_destroy(loaded);free(game);free(loaded);return failures;
}
int pe_test_settlement(void){
    int failures=loader_tests()+world_arrival_tests();
    /* Preserve the parent's global immutable geometry/root for other fixtures. */
    pid_t pid=fork();if(pid==0){int result=actual_asset_child();fflush(NULL);_exit(result?1:0);}
    if(pid<0)return failures+1;
    int status=0;if(waitpid(pid,&status,0)!=pid||!WIFEXITED(status)||WEXITSTATUS(status)!=0)failures++;
    return failures;
}
