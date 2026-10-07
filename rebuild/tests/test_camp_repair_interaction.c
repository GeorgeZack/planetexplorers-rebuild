#include "pe_camp_repair_interaction.h"
#include "pe_settlement.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PeVec3 plus(PeVec3 a,PeVec3 b){return(PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 minus(PeVec3 a,PeVec3 b){return(PeVec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static void prepare(PeGame *g,bool route){
    memset(g,0,sizeof *g);g->mode=PE_GAME_ADVENTURE;g->hp=100;
    assert(pe_world_resize(&g->world,route?416:64,route?64:32,route?416:128));
    g->world.source=1;g->world.origin_x=route?-640:-627;g->world.origin_y=route?0:16;g->world.origin_z=route?64:323;
}
static void update(PeSandboxCollision *c,PeGame *g,const PeSandboxBox *boxes,size_t count,const PeSceneCollision *static_scene){
    char error[256];
    if(!pe_sandbox_collision_update_with_static(c,&g->world,NULL,0,boxes,count,NULL,0,static_scene,error,sizeof error)){
        fprintf(stderr,"snapshot: %s\n",error);abort();
    }
}
static void cases(bool route){
    PeGame *g=calloc(1,sizeof *g),*before=malloc(sizeof *g);assert(g&&before);prepare(g,route);
    PeSandboxCollision *c=pe_sandbox_collision_create();assert(c);
    PeVec3 root={999,999,999};
    assert(!pe_camp_repair_present(g,c,true,&root)&&root.x==999);
    PeVec3 expected={route?26.5f:13.5f,route?29.5f:13.5f,route?340:81};
    PeSandboxBox box={plus(expected,(PeVec3){-.5f,0,-.5f}),plus(expected,(PeVec3){.5f,1,.5f})};
    update(c,g,&box,1,NULL);
    assert(pe_camp_repair_present(g,c,true,&root));assert(!memcmp(&root,&expected,sizeof root));
    assert(!pe_camp_repair_present(g,NULL,true,NULL));assert(!pe_camp_repair_present(NULL,c,true,NULL));
    assert(!pe_camp_repair_present(g,c,false,NULL));
    g->mode=PE_GAME_LEGACY;assert(!pe_camp_repair_present(g,c,true,NULL));g->mode=PE_GAME_ADVENTURE;
    g->world.origin_x++;assert(!pe_camp_repair_present(g,c,true,NULL));g->world.origin_x--;
    g->world.source=0;assert(!pe_camp_repair_present(g,c,true,NULL));g->world.source=1;
    g->player=plus(root,(PeVec3){8,0,0});assert(pe_camp_repair_in_range(g,c,true));
    g->player.x=nextafterf(g->player.x,INFINITY);assert(!pe_camp_repair_in_range(g,c,true));
    g->player=plus(root,(PeVec3){0,8,0});assert(pe_camp_repair_in_range(g,c,true));
    g->player.y=nextafterf(g->player.y,INFINITY);assert(!pe_camp_repair_in_range(g,c,true));
    g->player=plus(root,(PeVec3){4,0,0});g->hp=0;assert(!pe_camp_repair_in_range(g,c,true));
    g->hp=NAN;assert(!pe_camp_repair_in_range(g,c,true));g->hp=100;
    g->revival.phase=PE_REVIVAL_GETTING_UP;assert(!pe_camp_repair_in_range(g,c,true));g->revival.phase=PE_REVIVAL_IDLE;
    g->player.x=NAN;assert(!pe_camp_repair_in_range(g,c,true));g->player=plus(root,(PeVec3){4,0,0});
    const PeVec3 sides[]={{4,.5f,0},{-4,.5f,0},{0,.5f,4},{0,.5f,-4},{0,4,0},{0,-4,0}};
    for(unsigned i=0;i<sizeof sides/sizeof *sides;i++){
        PeVec3 from=plus(root,sides[i]),to=plus(root,(PeVec3){0,.5f,0}),dir=minus(to,from);
        PeSceneHit h=pe_camp_repair_pick(g,c,true,from,dir),actual=pe_sandbox_collision_ray_geometry(c,from,dir,100);
        assert(h.hit&&actual.hit&&fabsf(h.distance-actual.distance)<.0001f&&isfinite(h.normal.x));
        assert(h.triangle==UINT32_MAX);
    }
    PeVec3 from=plus(root,(PeVec3){4,.5f,0}),dir={-2,0,0};
    memcpy(before,g,sizeof *g);uint64_t generation=pe_sandbox_collision_stats(c).generation;
    for(int i=0;i<10;i++)assert(pe_camp_repair_pick(g,c,true,from,dir).hit);
    assert(!memcmp(before,g,sizeof *g)&&pe_sandbox_collision_stats(c).generation==generation);
    assert(!pe_camp_repair_pick(g,c,false,from,dir).hit);
    assert(!pe_camp_repair_pick(g,c,true,from,(PeVec3){0}).hit);
    assert(!pe_camp_repair_pick(g,c,true,from,(PeVec3){NAN,0,0}).hit);
    assert(!pe_camp_repair_pick(g,c,true,(PeVec3){INFINITY,0,0},dir).hit);
    assert(!pe_camp_repair_pick(g,c,true,from,(PeVec3){1,0,0}).hit);
    assert(!pe_camp_repair_pick(g,c,true,plus(root,(PeVec3){0,.5f,0}),dir).hit);
    assert(!pe_camp_repair_pick(g,c,true,plus(root,(PeVec3){101,.5f,0}),dir).hit);
    assert(pe_camp_repair_pick(g,c,true,plus(root,(PeVec3){100.5f,.5f,0}),dir).hit);
    g->player=plus(root,(PeVec3){8.001f,0,0});assert(!pe_camp_repair_pick(g,c,true,from,dir).hit);g->player=plus(root,(PeVec3){4,0,0});
    PeSandboxBox blocked[]={box,{plus(root,(PeVec3){1,0,-1}),plus(root,(PeVec3){1.01f,2,1})}};
    update(c,g,blocked,2,NULL);assert(!pe_camp_repair_pick(g,c,true,from,dir).hit);
    /* A submillimeter obstruction is not swallowed by a broad self-hit exemption. */
    blocked[1]=(PeSandboxBox){plus(root,(PeVec3){.5002f,0,-1}),plus(root,(PeVec3){.5003f,2,1})};
    update(c,g,blocked,2,NULL);assert(!pe_camp_repair_pick(g,c,true,from,dir).hit);
    pe_sandbox_collision_free(c);pe_world_destroy(&g->world);free(before);free(g);
}
static void installed(bool route){
    PeGame *g=calloc(1,sizeof *g);assert(g);prepare(g,route);
    char error[256],path[256];snprintf(path,sizeof path,"rebuild/build/reference-assets/%s/terrain.pevx",route?"adventure-route":"adventure-settlement");
    pe_world_destroy(&g->world);
    if(!pe_settlement_load_world(path,&g->world,error,sizeof error)){fprintf(stderr,"Required installed terrain: %s\n",error);abort();}
    const PeSceneCollision *scene=pe_settlement_collision_for_world(&g->world,error,sizeof error);
    if(!scene){fprintf(stderr,"Required installed geometry: %s\n",error);abort();}
    PeSandboxCollision *c=pe_sandbox_collision_create();assert(c);update(c,g,NULL,0,scene);
    PeVec3 root;assert(pe_camp_repair_present(g,c,true,&root));g->player=plus(root,(PeVec3){4,0,0});
    const PeVec3 sides[]={{4,.5f,0},{-4,.5f,0},{0,.5f,4},{0,.5f,-4},{0,4,0}};
    unsigned visible=0;
    for(unsigned i=0;i<sizeof sides/sizeof *sides;i++){
        PeVec3 from=plus(root,sides[i]),dir=minus(plus(root,(PeVec3){0,.5f,0}),from);
        PeSceneHit obstacle=pe_sandbox_collision_ray_geometry(c,from,dir,100),pick=pe_camp_repair_pick(g,c,true,from,dir);
        printf("%s side%u: scene %.7f pick %d distance %.7f\n",route?"route":"starting",i,obstacle.hit?obstacle.distance:-1,pick.hit,pick.distance);
        if(pick.hit){visible++;assert(obstacle.hit&&fabsf(obstacle.distance-pick.distance)<.0001f);}
    }
    assert(visible>=3);pe_sandbox_collision_free(c);pe_world_destroy(&g->world);free(g);
}
int main(int argc,char **argv){
    cases(false);cases(true);
    if(argc==2&&!strcmp(argv[1],"--installed")){installed(false);installed(true);}
    else assert(argc==1);
    puts("Camp repair interaction: PASS");return 0;
}
