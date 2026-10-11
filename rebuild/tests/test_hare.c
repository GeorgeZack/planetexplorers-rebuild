#define _POSIX_C_SOURCE 200809L
#include "pe_test_paths.h"
#include "pe_hare.h"
#include "pe_sandbox_collision.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hare:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static bool near(float a,float b){return fabsf(a-b)<.003f;}
static PeSceneCollision *scene_file(const char *path,bool wall,float maximum_z){
    FILE*f=fopen(path,"w");if(!f)return NULL;
    fprintf(f,"v -100 0 -100\nv 100 0 -100\nv 100 0 %.3f\nv -100 0 %.3f\nf 1 3 2\nf 1 4 3\n",maximum_z,maximum_z);
    if(wall)fprintf(f,"v -100 0 15\nv 100 0 15\nv 100 10 15\nv -100 10 15\nf 5 6 7\nf 5 7 8\n");
    fclose(f);char error[256];return pe_scene_load_obj(path,error,sizeof(error));
}
static PeHareTarget target(PeVec3 p){return(PeHareTarget){.accepted=true,.position=p,.radius=.3f,.height=1.8f,.approach=PE_HARE_MELEE};}
static void sandbox_plane(PeWorld *w,bool filled){
    memset(w,0,sizeof(*w));w->revision=filled?1:2;
    if(filled)for(int z=0;z<PE_WORLD_Z;z++)for(int x=0;x<PE_WORLD_X;x++)for(int y=0;y<=8;y++)
        w->voxels[((z*PE_WORLD_Y)+y)*PE_WORLD_X+x]=(PeVoxel){255,1};
}
static void adventure_queries(void){
    PeWorld *w=calloc(1,sizeof(*w));PeSandboxCollision *s=pe_sandbox_collision_create();
    CHECK(w&&s);if(!w||!s){free(w);pe_sandbox_collision_free(s);return;}
    sandbox_plane(w,true);char error[256];
    CHECK(pe_sandbox_collision_update(s,w,NULL,0,NULL,0,error,sizeof(error)));
    PeHareQueries q=pe_hare_queries_sandbox(s);PeHareConfig c=pe_hare_adventure_config((PeVec3){20,11,20},90);
    PeHare h={0};CHECK(pe_hare_init_query(&h,&q,&c,77,1,false));
    CHECK(near(h.position.y,8+127.0f/255+.003f)&&h.yaw_degrees==90&&h.grounded);
    CHECK(h.spawn.x==20&&h.spawn.y==11&&h.spawn.z==20&&!h.patrol.spawn_center);
    PeHare old=h;
    PeVec3 invalid[]={{-.1f,11,20},{20,30,20},{20,4,20},{20,20,20},{NAN,11,20}};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++){
        c.spawn=invalid[i];CHECK(!pe_hare_init_query(&h,&q,&c,1,1,false));CHECK(!memcmp(&h,&old,sizeof(h)));
    }
    c=pe_hare_adventure_config((PeVec3){20,11,20},0);c.patrol.minimum_radius=31;
    CHECK(!pe_hare_init_query(&h,&q,&c,1,1,false));CHECK(!memcmp(&h,&old,sizeof(h)));
    PeHareQueries bad=q;bad.ray=NULL;CHECK(!pe_hare_step_query(&h,&bad,NULL));CHECK(!memcmp(&h,&old,sizeof(h)));
    /* Newly embedded geometry is rejected transactionally from the current snapshot. */
    PeSandboxBox embedded={{19,8,19},{21,12,21}};
    CHECK(pe_sandbox_collision_update(s,w,NULL,0,&embedded,1,error,sizeof(error)));
    CHECK(!pe_hare_step_query(&h,&q,NULL));CHECK(!memcmp(&h,&old,sizeof(h)));
    /* Shared furniture geometry blocks movement and midpoint LOS. */
    PeSandboxBox wall={{10,8,24},{30,15,24.03f}};
    CHECK(pe_sandbox_collision_update(s,w,NULL,0,&wall,1,error,sizeof(error)));
    c=pe_hare_adventure_config((PeVec3){20,11,20},0);CHECK(pe_hare_init_query(&h,&q,&c,77,1,false));
    PeHareTarget t=target((PeVec3){20,h.position.y,28});
    CHECK(!pe_hare_engagement_query(&h,&q,&t).unobstructed);
    for(int i=0;i<120;i++){CHECK(pe_hare_step_query(&h,&q,&t));CHECK(!pe_sandbox_collision_overlap(s,h.position,h.radius,h.height));}
    CHECK(h.position.z>23&&h.position.z<23.405f);
    /* Tree capsule participates in movement, while the geometry-ray mask excludes it. */
    PeTreeCapsule tree={{20,12,24},1,7};
    CHECK(pe_sandbox_collision_update_with_trees(s,w,NULL,0,NULL,0,&tree,1,error,sizeof(error)));
    CHECK(pe_hare_init_query(&h,&q,&c,77,1,false));CHECK(pe_hare_engagement_query(&h,&q,&t).unobstructed);
    for(int i=0;i<120;i++){CHECK(pe_hare_step_query(&h,&q,&t));}
    CHECK(h.position.z>22&&h.position.z<22.405f);
    /* Accepted targets outside the crop never pull the capsule outside the bounds. */
    CHECK(pe_sandbox_collision_update(s,w,NULL,0,NULL,0,error,sizeof(error)));
    c.spawn=(PeVec3){20,11,61};CHECK(pe_hare_init_query(&h,&q,&c,77,1,false));t=target((PeVec3){20,h.position.y,80});
    for(int i=0;i<90;i++){CHECK(pe_hare_step_query(&h,&q,&t));CHECK(!pe_sandbox_collision_overlap(s,h.position,h.radius,h.height));}
    CHECK(h.position.z<=63-h.radius&&h.grounded);
    /* Removing support causes falling, never synthetic crop-bottom grounding. */
    c.spawn=(PeVec3){20,11,20};CHECK(pe_hare_init_query(&h,&q,&c,77,1,false));old=h;
    sandbox_plane(w,false);CHECK(pe_sandbox_collision_update(s,w,NULL,0,NULL,0,error,sizeof(error)));
    for(int i=0;i<120;i++){CHECK(pe_hare_step_query(&h,&q,NULL));CHECK(!h.grounded);}
    CHECK(h.position.y<old.position.y-1&&h.position.y>=0);
    old=h;c.spawn=(PeVec3){20,2,20};CHECK(!pe_hare_init_query(&h,&q,&c,77,1,false));CHECK(!memcmp(&h,&old,sizeof(h)));
    /* Tiny real island has no supported 15..30m destination: no raw fallback. */
    for(int z=18;z<=22;z++)for(int x=18;x<=22;x++)for(int y=0;y<=8;y++)
        w->voxels[((z*PE_WORLD_Y)+y)*PE_WORLD_X+x]=(PeVoxel){255,1};
    w->revision++;CHECK(pe_sandbox_collision_update(s,w,NULL,0,NULL,0,error,sizeof(error)));
    c.spawn=(PeVec3){20,11,20};CHECK(pe_hare_init_query(&h,&q,&c,77,1,false));
    h.mode=PE_HARE_PATROL;h.mode_duration=20;CHECK(pe_hare_step_query(&h,&q,NULL));CHECK(!h.goal_valid);
    pe_sandbox_collision_free(s);pe_world_destroy(w);free(w);
}
int pe_test_hare(void){
    failures=0;char path[]="/tmp/pe-hare-test-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return failures;close(fd);
    PeSceneCollision *floor=scene_file(path,false,100);CHECK(floor);if(!floor){unlink(path);return failures;}
    PeHare a,b;CHECK(pe_hare_init(&a,floor,1234,0,false));CHECK(pe_hare_init(&b,floor,1234,0,false));CHECK(!memcmp(&a,&b,sizeof(a)));
    CHECK(a.scale>=.8f&&a.scale<1.3f&&a.spawn.x==15&&a.spawn.y==2&&a.spawn.z==12);
    CHECK(near(a.max_hp,212*a.scale)&&near(a.attack,64*a.scale)&&near(a.defense,29*a.scale));
    CHECK(pe_hare_init(&b,floor,1234,1,true));CHECK(b.max_hp==106&&b.attack==32&&b.defense==14.5f);
    CHECK(pe_hare_init(&a,floor,5,1,false));CHECK(near(a.position.y,.003f)&&a.grounded);
    /* Source walk/run/turn constants are independent of scale and stats. */
    a.mode=PE_HARE_PATROL;a.mode_duration=15;a.goal_valid=true;a.patrol_goal=(PeVec3){15,0,42};
    for(int i=0;i<60;i++){CHECK(pe_hare_step(&a,floor,NULL));}
    CHECK(near(a.position.z,14.5f));CHECK(near(a.yaw_degrees,0));
    CHECK(pe_hare_init(&a,floor,5,1,false));PeHareTarget t=target((PeVec3){15,.003f,22});
    CHECK(pe_hare_step(&a,floor,&t));CHECK(near(a.position.z,12+6.5f/60)&&a.mode==PE_HARE_APPROACH);
    CHECK(pe_hare_init(&a,floor,5,1,false));t=target((PeVec3){25,.003f,12});CHECK(pe_hare_step(&a,floor,&t));CHECK(near(a.yaw_degrees,2.5f));
    /* A body-bounds gap .5 means center separation1.4, not .5m. */
    CHECK(pe_hare_init(&a,floor,5,1,false));t=target((PeVec3){15,.003f,13.4f});PeHareEngagement e=pe_hare_engagement(&a,floor,&t);
    CHECK(e.present&&e.unobstructed&&near(e.gap,.5f)&&e.melee_geometry);
    t.position.z=15.9f;t.approach=PE_HARE_POUNCE;e=pe_hare_engagement(&a,floor,&t);CHECK(e.pounce_geometry&&!e.melee_geometry&&near(e.gap,3));
    CHECK(pe_hare_step(&a,floor,&t));CHECK(a.mode==PE_HARE_HOLD&&near(a.position.z,12));
    t.position.z=13.4f;CHECK(pe_hare_step(&a,floor,&t));CHECK(a.mode==PE_HARE_RETREAT&&near(a.position.z,12-2.0f/60));
    CHECK(pe_hare_init(&a,floor,5,1,false));a.yaw_degrees=30;e=pe_hare_engagement(&a,floor,&t);CHECK(!e.melee_geometry&&!e.pounce_geometry);
    t.position=(PeVec3){15,10,12};e=pe_hare_engagement(&a,floor,&t);CHECK(e.gap>8&&!e.melee_geometry);
    /* Full deterministic leisure trajectory, including radius30 patrol goals. */
    CHECK(pe_hare_init(&a,floor,77,0,false));CHECK(pe_hare_init(&b,floor,77,0,false));bool patrol=false;
    for(int i=0;i<1800;i++){CHECK(pe_hare_step(&a,floor,NULL));CHECK(pe_hare_step(&b,floor,NULL));CHECK(!memcmp(&a,&b,sizeof(a)));if(a.goal_valid){patrol=true;CHECK(near(hypotf(a.patrol_goal.x-a.spawn.x,a.patrol_goal.z-a.spawn.z),30));}CHECK(!pe_scene_capsule_overlap(floor,a.position,a.radius,a.height));}
    CHECK(patrol&&a.decisions>0);
    /* The explicit API is byte-identical to the tutorial wrapper trajectory. */
    PeHareQueries q=pe_hare_queries_scene(floor);PeHareConfig c=pe_hare_tutorial_config();
    CHECK(pe_hare_init(&a,floor,77,0,false));CHECK(pe_hare_init_query(&b,&q,&c,77,0,false));
    for(int i=0;i<1800;i++){CHECK(pe_hare_step(&a,floor,NULL));CHECK(pe_hare_step_query(&b,&q,NULL));CHECK(!memcmp(&a,&b,sizeof(a)));}
    /* Adventure goals use the current center and vary in radius, not spawn30. */
    float minimum=30,maximum=15;
    for(uint64_t seed=1;seed<=32;seed++){
        c=pe_hare_adventure_config((PeVec3){-50,2,-50},0);CHECK(pe_hare_init_query(&b,&q,&c,seed,1,false));
        b.position=(PeVec3){20,.003f,20};b.mode=PE_HARE_PATROL;b.mode_duration=20;
        CHECK(pe_hare_step_query(&b,&q,NULL));CHECK(b.goal_valid);
        float r=hypotf(b.patrol_goal.x-20,b.patrol_goal.z-20);CHECK(r>=15&&r<=30);
        minimum=fminf(minimum,r);maximum=fmaxf(maximum,r);
    }
    CHECK(minimum<20&&maximum>25);

    PeHare old=a;CHECK(!pe_hare_init(&a,floor,1,NAN,false));CHECK(!memcmp(&a,&old,sizeof(a)));CHECK(!pe_hare_init(&a,floor,1,.5f,false));CHECK(!memcmp(&a,&old,sizeof(a)));
    t=target((PeVec3){NAN,0,0});CHECK(!pe_hare_step(&a,floor,&t));CHECK(!memcmp(&a,&old,sizeof(a)));CHECK(!pe_hare_step(&a,NULL,NULL));CHECK(!memcmp(&a,&old,sizeof(a)));
    CHECK(!pe_hare_engagement(&a,floor,&t).present);
    /* Approach cannot walk through a wall; obstacle also blocks eligibility. */
    PeSceneCollision *wall=scene_file(path,true,100);CHECK(wall);
    if(wall){CHECK(pe_hare_init(&a,wall,1,1,false));t=target((PeVec3){15,.003f,17});e=pe_hare_engagement(&a,wall,&t);CHECK(!e.unobstructed);
        for(int i=0;i<600;i++){CHECK(pe_hare_step(&a,wall,&t));CHECK(!pe_scene_capsule_overlap(wall,a.position,a.radius,a.height));}
        CHECK(a.position.z<14.405f&&a.position.z>14.2f);pe_scene_free(wall);}
    /* A radius30 goal beyond the small recovered deck must not create a fall. */
    PeSceneCollision *edge=scene_file(path,false,15);CHECK(edge);
    if(edge){CHECK(pe_hare_init(&a,edge,1,1,false));a.mode=PE_HARE_PATROL;a.mode_duration=20;a.goal_valid=true;a.patrol_goal=(PeVec3){15,0,42};
        for(int i=0;i<300;i++){CHECK(pe_hare_step(&a,edge,NULL));CHECK(a.position.y>-.02f);CHECK(!pe_scene_capsule_overlap(edge,a.position,a.radius,a.height));}CHECK(a.position.z<15.2f);pe_scene_free(edge);}
    pe_scene_free(floor);unlink(path);
    const char *actual=pe_test_path("rebuild/build/reference-assets/room-collision.obj");FILE*f=fopen(actual,"rb");if(f){fclose(f);char error[256];PeSceneCollision *room=pe_scene_load_obj(actual,error,sizeof(error));CHECK(room);if(room){CHECK(pe_hare_init(&a,room,1234,1,false));CHECK(near(a.position.x,15)&&near(a.position.z,12)&&a.position.y>1&&a.position.y<2);
        for(int i=0;i<900;i++){CHECK(pe_hare_step(&a,room,NULL));CHECK(!pe_scene_capsule_overlap(room,a.position,a.radius,a.height));}pe_scene_free(room);}}
    adventure_queries();
    return failures;
}
