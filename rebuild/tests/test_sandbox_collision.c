#include "pe_sandbox_collision.h"
#include "pe_hare_perception.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do{if(!(e)){fprintf(stderr,"sandbox collision:%d: %s\n",__LINE__,#e);failures++;}}while(0)
static void plane(PeWorld *w,int y){memset(w,0,sizeof *w);w->revision=1;for(int z=0;z<PE_WORLD_Z;z++)for(int x=0;x<PE_WORLD_X;x++)for(int j=0;j<=y;j++)w->voxels[((z*PE_WORLD_Y)+j)*PE_WORLD_X+x]=(PeVoxel){255,1};}
static bool update(PeSandboxCollision *c,PeWorld *w,const PeB45Cell *cells,size_t count,const PeSandboxBox *boxes,size_t n){char error[256];bool ok=pe_sandbox_collision_update(c,w,cells,count,boxes,n,error,sizeof error);if(!ok)fprintf(stderr,"sandbox update: %s\n",error);return ok;}
static int lifecycle(PeSandboxCollision *c,PeWorld *w){
    int failures=0;char error[128];plane(w,8);CHECK(update(c,w,NULL,0,NULL,0));
    PeSandboxCollisionStats first=pe_sandbox_collision_stats(c);CHECK(first.generation==1&&first.terrain_rebuilds==4&&first.construction_rebuilds==1&&first.triangles>0);
    CHECK(update(c,w,NULL,0,NULL,0));CHECK(pe_sandbox_collision_stats(c).generation==first.generation);
    float y=8+127.0f/255;PeVec3 floor={20,y+.003f,20};
    CHECK(!pe_sandbox_collision_overlap(c,floor,.3f,1.8f));CHECK(pe_sandbox_collision_overlap(c,(PeVec3){20,y-.01f,20},.3f,1.8f));
    CHECK(pe_sandbox_collision_overlap(c,(PeVec3){20,3,20},.3f,1.8f));
    PeVec3 settled={99,99,99};
    CHECK(pe_sandbox_collision_settle(c,(PeVec3){20,y-.05f,20},.3f,1.8f,4,5,&settled));
    CHECK(settled.x==20&&settled.z==20&&fabsf(settled.y-y)<.005f);
    CHECK(pe_sandbox_collision_settle(c,(PeVec3){20,y+3,20},.3f,1.8f,0,5,&settled));
    CHECK(fabsf(settled.y-y)<.005f);
    settled=(PeVec3){99,99,99};CHECK(!pe_sandbox_collision_settle(c,(PeVec3){20,2,20},.3f,1.8f,4,5,&settled));CHECK(settled.y==99);
    CHECK(!pe_sandbox_collision_settle(c,(PeVec3){20,y+6,20},.3f,1.8f,4,5,&settled));CHECK(settled.y==99);
    CHECK(!pe_sandbox_collision_settle(c,(PeVec3){-.1f,y,20},.3f,1.8f,4,5,&settled));
    PeSceneHit hit=pe_sandbox_collision_support(c,(PeVec3){20,20,20},.3f,1.8f,20,.65f);CHECK(hit.hit&&fabsf(hit.distance-(20-y))<.0001f&&hit.normal.y>.99f);
    /* Caller changes bytes without changing revision: reload must still update. */
    memset(w,0,sizeof *w);w->revision=1;CHECK(update(c,w,NULL,0,NULL,0));CHECK(!pe_sandbox_collision_support(c,(PeVec3){20,20,20},.3f,1.8f,15,.65f).hit);
    CHECK(pe_sandbox_collision_stats(c).generation==first.generation+1);
    PeB45Cell bad={20,20,20,255,0};uint64_t before=pe_sandbox_collision_stats(c).generation;
    CHECK(!pe_sandbox_collision_update(c,w,&bad,1,NULL,0,error,sizeof error));CHECK(pe_sandbox_collision_stats(c).generation==before);
    CHECK(!pe_sandbox_collision_update(c,w,NULL,1,NULL,0,error,sizeof error));
    PeSandboxBox invalid={{4,4,4},{NAN,5,5}};CHECK(!pe_sandbox_collision_update(c,w,NULL,0,&invalid,1,error,sizeof error));
    /* An invalid construction input must not publish otherwise valid new terrain. */
    plane(w,8);CHECK(!pe_sandbox_collision_update(c,w,&bad,1,NULL,0,error,sizeof error));CHECK(!pe_sandbox_collision_support(c,(PeVec3){20,20,20},.3f,1.8f,15,.65f).hit);
    CHECK(update(c,w,NULL,0,NULL,0));before=pe_sandbox_collision_stats(c).terrain_rebuilds;
    pe_world_set(w,10,8,10,(PeVoxel){0,0});CHECK(update(c,w,NULL,0,NULL,0));CHECK(pe_sandbox_collision_stats(c).terrain_rebuilds==before+1);
    return failures;
}
static int geometry(PeSandboxCollision *c,PeWorld *w){
    int failures=0;memset(w,0,sizeof *w);pe_world_set(w,10,10,10,(PeVoxel){255,1});CHECK(update(c,w,NULL,0,NULL,0));
    /* This point is inside the trilinear density surface but beyond the actual
       octahedral MC face x+y+z=30+127/255. Capsule contact follows triangles. */
    PeVec3 outside_mc={10.2f,10.199f,10.2f};CHECK(pe_world_collides(w,outside_mc,.001f,.002f));CHECK(!pe_sandbox_collision_overlap(c,outside_mc,.001f,.002f));
    CHECK(pe_sandbox_collision_overlap(c,(PeVec3){10.1f,10.099f,10.1f},.001f,.002f));
    for(int i=0;i<PE_VOXEL_COUNT;i++)w->voxels[i]=(PeVoxel){255,1};
    CHECK(update(c,w,NULL,0,NULL,0));CHECK(pe_sandbox_collision_overlap(c,(PeVec3){30,14,30},.3f,1.8f));
    memset(w,0,sizeof *w);
    PeSandboxBox large={{18,2,18},{25,12,25}};CHECK(update(c,w,NULL,0,&large,1));CHECK(pe_sandbox_collision_overlap(c,(PeVec3){20,4,20},.3f,1.8f));
    PeSceneHit hit=pe_sandbox_collision_sweep(c,(PeVec3){15,4,20},.3f,1.8f,(PeVec3){15,0,0});CHECK(hit.hit&&fabsf(hit.fraction-(2.7f/15))<.0001f&&hit.normal.x<-.99f);
    for(int rotation=0;rotation<4;rotation++){
        PeB45Cell ramp={40,10,40,(uint8_t)(8|rotation),0};CHECK(update(c,w,&ramp,1,NULL,0));
        PeB45Hit surface;CHECK(pe_b45_raycast(&ramp,(PeVec3){20.25f,8,20.25f},(PeVec3){0,-1,0},5,&surface));
        PeVec3 p={20.25f,surface.position.y+.02f,20.25f};CHECK(!pe_sandbox_collision_overlap(c,p,.005f,.05f));
        hit=pe_sandbox_collision_support(c,p,.005f,.05f,.1f,.65f);CHECK(hit.hit&&hit.normal.y>.70f&&hit.normal.y<.72f);
        CHECK(pe_sandbox_collision_overlap(c,(PeVec3){20.25f,5.10f,20.25f},.005f,.05f));
    }
    CHECK(update(c,w,NULL,0,NULL,0));CHECK(!pe_sandbox_collision_overlap(c,(PeVec3){.3f,0,.3f},.3f,1.8f));
    hit=pe_sandbox_collision_sweep(c,(PeVec3){.3f,2,20},.3f,1.8f,(PeVec3){-10,0,0});CHECK(hit.hit&&hit.fraction==0&&hit.normal.x==1);
    hit=pe_sandbox_collision_sweep(c,(PeVec3){20,28,20},.3f,1.8f,(PeVec3){0,5,0});CHECK(hit.hit&&fabsf(hit.fraction-.24f)<.00001f&&hit.normal.y==-1);
    CHECK(pe_sandbox_collision_overlap(c,(PeVec3){20,29.3f,20},.3f,1.8f));CHECK(pe_sandbox_collision_overlap(c,(PeVec3){NAN,2,20},.3f,1.8f));
    CHECK(!pe_sandbox_collision_sweep(c,(PeVec3){20,2,20},.3f,1.8f,(PeVec3){NAN,0,0}).hit);
    return failures;
}
static int movement(PeSandboxCollision *c,PeWorld *w){
    int failures=0;plane(w,8);CHECK(update(c,w,NULL,0,NULL,0));float floor=8+127.0f/255;
    PeSandboxMotion m={{10,floor+.003f,20},{0,0,0},true};
    for(int i=0;i<60;i++)CHECK(pe_sandbox_collision_move(c,m.position,(PeVec3){5,-20.0f/60,0},1.0f/60,.3f,1.8f,m.grounded,true,&m));
    CHECK(fabsf(m.position.x-15)<.001f&&fabsf(m.position.y-floor)<.006f&&m.grounded);
    PeSandboxBox wall={{20,floor,18},{20.03f,15,25}};CHECK(update(c,w,NULL,0,&wall,1));
    m=(PeSandboxMotion){{18,floor+.003f,20},{0,0,0},true};
    CHECK(pe_sandbox_collision_move(c,m.position,(PeVec3){20,-5,3},.25f,.3f,1.8f,true,true,&m));
    CHECK(m.position.x<19.701f&&m.position.x>19.69f&&m.position.z>20.70f);CHECK(!pe_sandbox_collision_overlap(c,m.position,.3f,1.8f));
    /* Jumping at a low ceiling clips vertical speed, not horizontal motion. */
    PeSandboxBox ceiling={{15,10.5f,15},{25,10.7f,25}};CHECK(update(c,w,NULL,0,&ceiling,1));
    CHECK(pe_sandbox_collision_move(c,(PeVec3){20,floor+.003f,20},(PeVec3){1,7,0},.1f,.3f,1.8f,false,false,&m));
    CHECK(m.position.y<=8.701f&&m.velocity.y==0&&m.position.x>20.09f);
    PeSandboxBox step={{12,floor,18},{14,floor+.25f,22}};CHECK(update(c,w,NULL,0,&step,1));
    m=(PeSandboxMotion){{11,floor+.003f,20},{0,0,0},true};
    for(int i=0;i<60;i++)CHECK(pe_sandbox_collision_move(c,m.position,(PeVec3){2,-20.0f/60,0},1.0f/60,.3f,1.8f,m.grounded,true,&m));
    CHECK(m.position.x>12.7f&&m.position.y>floor+.24f&&m.position.y<floor+.26f&&m.grounded);
    PeSandboxMotion sentinel={{42,42,42},{3,4,5},false},copy=sentinel;
    CHECK(!pe_sandbox_collision_move(c,(PeVec3){20,3,20},(PeVec3){0},.1f,.3f,1.8f,false,true,&sentinel));CHECK(memcmp(&sentinel,&copy,sizeof copy)==0);
    CHECK(!pe_sandbox_collision_move(c,(PeVec3){20,20,20},(PeVec3){0},NAN,.3f,1.8f,false,true,&sentinel));CHECK(memcmp(&sentinel,&copy,sizeof copy)==0);
    /* After support removal the next snapshot permits falling immediately. */
    memset(w,0,sizeof *w);CHECK(update(c,w,NULL,0,NULL,0));
    CHECK(pe_sandbox_collision_move(c,(PeVec3){20,floor+.003f,20},(PeVec3){0,-1,0},.1f,.3f,1.8f,true,true,&m));CHECK(!m.grounded&&m.position.y<floor-.09f);
    return failures;
}
static int tree_geometry(void){
    int failures=0;
    PeTreeCapsule source=pe_tree_collision_source((PeVec3){0});
    CHECK(source.center.x==-.1f&&source.center.y==3&&source.center.z==-.2f&&source.radius==1&&source.height==7);
    PeTreeCapsule tree={{20,12,20},1,7};float r=.25f,h=1.5f;
    PeVec3 touching={18.75f,11.25f,20};
    CHECK(!pe_tree_collision_overlap(&tree,touching,r,h));
    CHECK(pe_tree_collision_overlap(&tree,(PeVec3){18.751f,11.25f,20},r,h));
    CHECK(!pe_tree_collision_overlap(&tree,(PeVec3){18.749f,11.25f,20},r,h));
    PeSceneHit hit=pe_tree_collision_sweep(&tree,(PeVec3){10,11.25f,20},r,h,(PeVec3){20,0,0});
    CHECK(hit.hit&&fabsf(hit.fraction-.4375f)<1e-6f&&hit.normal.x==-1&&hit.position.x==19);
    CHECK(pe_tree_collision_sweep(&tree,touching,r,h,(PeVec3){1,0,0}).hit);
    CHECK(!pe_tree_collision_sweep(&tree,touching,r,h,(PeVec3){-1,0,0}).hit);
    CHECK(!pe_tree_collision_sweep(&tree,touching,r,h,(PeVec3){0,0,1}).hit);
    CHECK(!pe_tree_collision_sweep(&tree,(PeVec3){18.75f,11.25f,10},r,h,(PeVec3){0,0,20}).hit);
    CHECK(!pe_tree_collision_sweep(&tree,touching,r,h,(PeVec3){0,2,0}).hit);
    hit=pe_tree_collision_sweep(&tree,(PeVec3){20,17,20},r,h,(PeVec3){0,-3,0});
    CHECK(hit.hit&&fabsf(hit.distance-1.5f)<1e-6f&&hit.normal.y==1&&hit.position.y==15.5f);
    hit=pe_tree_collision_sweep(&tree,(PeVec3){20.75f,17,20},r,h,(PeVec3){0,-3,0});
    CHECK(hit.hit&&fabsf(hit.distance-1.75f)<1e-6f&&fabsf(hit.normal.y-.8f)<1e-6f&&fabsf(hit.normal.x-.6f)<1e-6f);
    CHECK(!pe_tree_collision_overlap(&tree,(PeVec3){20.75f,15.25f,20},r,h));
    CHECK(pe_tree_collision_overlap(&tree,(PeVec3){20.75f,15.249f,20},r,h));
    CHECK(!pe_tree_collision_sweep(&tree,(PeVec3){15,16,20},r,h,(PeVec3){10,0,0}).hit);
    /* Moving up from below meets the lower hemisphere, not the cylinder's
     * internal sphere seam. Touching end of the requested sweep is retained. */
    hit=pe_tree_collision_sweep(&tree,(PeVec3){20,5,20},r,h,(PeVec3){0,2,0});
    CHECK(hit.hit&&hit.distance==2&&hit.fraction==1&&hit.normal.y==-1&&hit.position.y==8.5f);
    PeTreeCapsule sphere={{20,12,20},1,2};
    hit=pe_tree_collision_sweep(&sphere,(PeVec3){15,11.75f,20},.25f,.5f,(PeVec3){10,0,0});
    CHECK(hit.hit&&fabsf(hit.distance-3.75f)<1e-6f); /* Degenerate capsule axes. */
    tree.center.y=NAN;CHECK(!pe_tree_collision_valid(&tree));CHECK(pe_tree_collision_overlap(&tree,touching,r,h));
    CHECK(!pe_tree_collision_sweep(&tree,touching,r,h,(PeVec3){1,0,0}).hit);
    CHECK(!pe_tree_collision_valid(NULL));CHECK(!pe_tree_collision_sweep(&source,touching,r,h,(PeVec3){NAN,0,0}).hit);
    return failures;
}
static int tree_rays(void){
    int failures=0;PeTreeCapsule tree={{20,12,20},1,7};
    PeSceneHit hit=pe_tree_collision_ray(&tree,(PeVec3){10,12,20},(PeVec3){2,0,0},10);
    CHECK(hit.hit&&hit.distance==9&&fabsf(hit.fraction-.9f)<1e-6f&&hit.position.x==19&&hit.normal.x==-1&&hit.triangle==UINT32_MAX);
    CHECK(pe_tree_collision_ray(&tree,(PeVec3){10,12,20},(PeVec3){1,0,0},9).hit);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){10,12,20},(PeVec3){1,0,0},nextafterf(9,0)).hit);
    hit=pe_tree_collision_ray(&tree,(PeVec3){20,20,20},(PeVec3){0,-3,0},10);
    CHECK(hit.hit&&hit.distance==4.5f&&hit.position.y==15.5f&&hit.normal.y==1);
    hit=pe_tree_collision_ray(&tree,(PeVec3){20.6f,20,20},(PeVec3){0,-1,0},10);
    CHECK(hit.hit&&fabsf(hit.distance-4.7f)<.00001f&&fabsf(hit.normal.x-.6f)<.00001f&&fabsf(hit.normal.y-.8f)<.00001f);
    hit=pe_tree_collision_ray(&tree,(PeVec3){20,0,20},(PeVec3){0,1,0},20);
    CHECK(hit.hit&&hit.distance==8.5f&&hit.normal.y==-1);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){10,16,20},(PeVec3){1,0,0},30).hit);
    /* Exact ray tangency is distinct from a moving finite capsule. */
    hit=pe_tree_collision_ray(&tree,(PeVec3){10,12,21},(PeVec3){1,0,0},20);
    CHECK(hit.hit&&hit.distance==10&&hit.normal.z==1);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){10,12,nextafterf(21,INFINITY)},(PeVec3){1,0,0},20).hit);
    hit=pe_tree_collision_ray(&tree,(PeVec3){10,15.5f,20},(PeVec3){1,0,0},20);
    CHECK(hit.hit&&hit.distance==10&&hit.normal.y==1);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){20,12,20},(PeVec3){1,0,0},20).hit);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){20,15,20},(PeVec3){0,1,0},20).hit);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){19,12,20},(PeVec3){-1,0,0},20).hit);
    hit=pe_tree_collision_ray(&tree,(PeVec3){19,12,20},(PeVec3){1,0,0},20);CHECK(hit.hit&&hit.distance==0);
    hit=pe_tree_collision_ray(&tree,(PeVec3){19,12,20},(PeVec3){0,0,1},20);CHECK(hit.hit&&hit.distance==0);
    PeTreeCapsule sphere={{20,12,20},1,2};hit=pe_tree_collision_ray(&sphere,(PeVec3){20,20,20},(PeVec3){0,-1,0},20);
    CHECK(hit.hit&&hit.distance==7&&hit.normal.y==1);
    CHECK(!pe_tree_collision_ray(NULL,(PeVec3){0},(PeVec3){1,0,0},20).hit);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){NAN,0,0},(PeVec3){1,0,0},20).hit);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){0},(PeVec3){INFINITY,0,0},20).hit);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){0},(PeVec3){0},20).hit);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){0},(PeVec3){1,0,0},INFINITY).hit);
    CHECK(!pe_tree_collision_ray(&tree,(PeVec3){0},(PeVec3){1,0,0},0).hit);
    return failures;
}
static bool perception_block(const void *context,PeVec3 origin,PeVec3 target,uint32_t mask,bool *blocked){
    if(mask!=PE_HARE_PERCEPTION_BLOCK_MASK)return false;
    PeVec3 direction={target.x-origin.x,target.y-origin.y,target.z-origin.z};
    float distance=sqrtf(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
    *blocked=pe_sandbox_collision_ray_perception(context,origin,direction,distance).hit;return true;
}
static int perception_snapshot(PeSandboxCollision *c,PeWorld *w){
    int failures=0;char error[128];plane(w,8);float floor=8+127.0f/255;
    PeTreeCapsule trees[]={{{20,floor+3,20},1,7},{{26,floor+3,20},1,7}};
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,trees,2,error,sizeof(error)));
    PeVec3 origin={17,floor+1.5f,20},direction={3,0,0};
    CHECK(!pe_sandbox_collision_ray_geometry(c,origin,direction,20).hit);
    PeHarePerception perception;CHECK(pe_hare_perception_init(&perception,100,1));
    PeHarePerceptionQueries queries={c,perception_block};
    PeHarePerceptionTarget target={true,true,true,false,{27,floor,20},{27,floor+1.5f,20},.3f};
    PeHareVisionPose sensor={origin,{1,0,0}};perception.vision_updates=19;perception.hearing_updates=15;
    CHECK(pe_hare_perception_vision(&perception,(PeHareVisionConfig){20,60},sensor,target,&queries));
    CHECK(pe_hare_perception_hearing(&perception,(PeHareHearingConfig){15,false},origin,target,&queries));
    CHECK(!perception.visible&&!perception.heard); /* Actual ray occlusion reaches both source sensor caches. */
    PeSceneHit hit=pe_sandbox_collision_ray_perception(c,origin,direction,20);
    CHECK(hit.hit&&hit.distance==2&&hit.position.x==19&&hit.normal.x==-1&&hit.triangle==UINT32_MAX);
    CHECK(!pe_sandbox_collision_ray_perception(c,origin,direction,nextafterf(2,0)).hit);
    CHECK(pe_sandbox_collision_ray_perception(c,origin,direction,2).hit);
    /* Interior first tree is ignored; a later tree still occludes the ray. */
    hit=pe_sandbox_collision_ray_perception(c,(PeVec3){20,floor+1.5f,20},direction,20);
    CHECK(hit.hit&&hit.distance==5&&hit.position.x==25);
    /* A real box closer than a tree wins; clearing it restores tree contact. */
    PeSandboxBox box={{18,floor,19},{18.1f,floor+4,21}};
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,&box,1,trees,2,error,sizeof(error)));
    hit=pe_sandbox_collision_ray_perception(c,origin,direction,20);CHECK(hit.hit&&hit.distance==1&&hit.triangle!=UINT32_MAX);
    CHECK(pe_sandbox_collision_ray_geometry(c,origin,direction,20).hit);
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,trees,2,error,sizeof(error)));
    trees[0].center.x=30;CHECK(pe_sandbox_collision_ray_perception(c,origin,direction,20).distance==2); /* Borrowed inputs copied. */
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,trees,2,error,sizeof(error)));
    CHECK(pe_sandbox_collision_ray_perception(c,origin,direction,20).distance==8);
    hit=pe_sandbox_collision_ray_perception(c,(PeVec3){10,floor+5,10},(PeVec3){0,-1,0},10);
    CHECK(hit.hit&&fabsf(hit.distance-5)<.00001f&&hit.normal.y>.99f);
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,NULL,0,error,sizeof(error)));
    CHECK(!pe_sandbox_collision_ray_perception(c,origin,direction,20).hit);
    perception.vision_updates=39;perception.hearing_updates=31;
    CHECK(pe_hare_perception_vision(&perception,(PeHareVisionConfig){20,60},sensor,target,&queries));
    CHECK(pe_hare_perception_hearing(&perception,(PeHareHearingConfig){15,false},origin,target,&queries));
    CHECK(perception.visible&&perception.heard);
    CHECK(!pe_sandbox_collision_ray_perception(c,(PeVec3){62,floor+10,20},(PeVec3){1,0,0},20).hit); /* No crop-wall fiction. */
    CHECK(!pe_sandbox_collision_ray_perception(c,origin,(PeVec3){0},20).hit);
    CHECK(!pe_sandbox_collision_ray_perception(c,origin,direction,NAN).hit);
    CHECK(!pe_sandbox_collision_ray_perception(NULL,origin,direction,20).hit);
    return failures;
}
static int tree_snapshot(PeSandboxCollision *c,PeWorld *w){
    int failures=0;char error[128];plane(w,8);float floor=8+127.0f/255;
    PeTreeCapsule trees[]={{{20,floor+3,20},1,7},{{26,floor+3,20},1,7}};
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,trees,2,error,sizeof error));
    PeSandboxCollisionStats stats=pe_sandbox_collision_stats(c);
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,trees,2,error,sizeof error));
    CHECK(pe_sandbox_collision_stats(c).generation==stats.generation);
    PeVec3 start={17,floor+.003f,20},embedded={20,floor+.003f,20};
    CHECK(pe_sandbox_collision_overlap(c,embedded,.3f,1.8f));
    CHECK(!pe_sandbox_collision_overlap_geometry(c,embedded,.3f,1.8f));
    PeSceneHit hit=pe_sandbox_collision_sweep(c,start,.3f,1.8f,(PeVec3){12,0,0});
    CHECK(hit.hit&&fabsf(hit.distance-1.7f)<.00001f&&hit.normal.x<-.99f);
    CHECK(!pe_sandbox_collision_sweep_geometry(c,start,.3f,1.8f,(PeVec3){12,0,0}).hit);
    PeSandboxMotion motion;
    CHECK(pe_sandbox_collision_move(c,start,(PeVec3){40,0,0},.25f,.3f,1.8f,true,true,&motion));
    CHECK(motion.position.x<18.7f&&motion.position.x>18.69f);
    CHECK(pe_sandbox_collision_move(c,(PeVec3){17,floor+.003f,19.5f},(PeVec3){20,0,4},.25f,.3f,1.8f,true,true,&motion));
    CHECK(motion.position.z>20&&motion.position.x>17.5f&&!pe_sandbox_collision_overlap(c,motion.position,.3f,1.8f));
    hit=pe_sandbox_collision_support(c,(PeVec3){20,floor+7.5f,20},.3f,1.8f,2,.65f);
    CHECK(hit.hit&&fabsf(hit.distance-1)<.00001f&&hit.normal.y>.999f);
    CHECK(pe_sandbox_collision_move(c,(PeVec3){17,floor+7,20},(PeVec3){40,1,0},.25f,.3f,1.8f,false,false,&motion));
    CHECK(motion.position.x>26.99f&&motion.position.y>floor+7.24f);
    /* Snapshot owns inputs. Moving a tree only publishes at update, without
     * rebuilding MC/B45 or incrementing the world revision. */
    trees[0].center.x=23;
    CHECK(pe_sandbox_collision_overlap(c,embedded,.3f,1.8f));
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,trees,2,error,sizeof error));
    CHECK(!pe_sandbox_collision_overlap(c,embedded,.3f,1.8f));
    PeSandboxCollisionStats moved=pe_sandbox_collision_stats(c);
    CHECK(moved.generation==stats.generation+1&&moved.terrain_rebuilds==stats.terrain_rebuilds&&moved.construction_rebuilds==stats.construction_rebuilds);
    /* Invalid tree rejects a simultaneous terrain replacement atomically. */
    plane(w,15);PeTreeCapsule invalid=trees[0];invalid.radius=NAN;
    CHECK(!pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,&invalid,1,error,sizeof error));
    CHECK(!pe_sandbox_collision_overlap_geometry(c,embedded,.3f,1.8f));
    CHECK(pe_sandbox_collision_stats(c).generation==moved.generation);
    plane(w,8);
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,NULL,0,error,sizeof error));
    CHECK(!pe_sandbox_collision_sweep(c,start,.3f,1.8f,(PeVec3){12,0,0}).hit);
    CHECK(!pe_sandbox_collision_support(c,(PeVec3){23,floor+6.503f,20},.3f,1.8f,.1f,.65f).hit);
    CHECK(pe_sandbox_collision_stats(c).generation==moved.generation+1);
    /* Old API deliberately empties live trees; a boundary tree may extend
     * outside the cropped world or below its floor without invalidation. */
    trees[0]=pe_tree_collision_source((PeVec3){.2f,0,.2f});
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,trees,1,error,sizeof error));
    CHECK(update(c,w,NULL,0,NULL,0));
    return failures;
}
int pe_test_sandbox_collision(void){
    int failures=0;PeWorld *world=calloc(1,sizeof *world);PeSandboxCollision *c=pe_sandbox_collision_create();if(!world||!c){free(world);pe_sandbox_collision_free(c);return 1;}
    CHECK(pe_sandbox_collision_overlap(c,(PeVec3){20,20,20},.3f,1.8f));
    failures+=lifecycle(c,world);failures+=geometry(c,world);failures+=movement(c,world);failures+=tree_geometry();failures+=tree_rays();failures+=tree_snapshot(c,world);failures+=perception_snapshot(c,world);
    free(world);pe_sandbox_collision_free(c);return failures;
}
