#define _POSIX_C_SOURCE 200809L
#include "pe_camera_query.h"
#include "pe_sandbox_camera.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"camera query:%d: %s\n",__LINE__,#c);exit(1);}}while(0)
static PeCameraGeometryQuery query(PeCameraGeometryQueryKind kind,PeVec3 origin,PeVec3 dir,float radius,float distance){
    return (PeCameraGeometryQuery){.kind=kind,.origin=origin,.direction=dir,.radius=radius,.distance=distance,.mask=PE_SANDBOX_CAMERA_OBSTACLE_MASK,.ignore_triggers=true};
}
static PeSceneCollision *objects(bool unknown){
    char path[]="build/camera-query-obj-XXXXXX",error[256];int fd=mkstemp(path);CHECK(fd>=0);FILE *f=fdopen(fd,"w");CHECK(f);
    /* Two submeshes of the same original collider and a later obstacle. */
    fprintf(f,"o %s\nv 0 -2 -2\nv 0 2 -2\nv 0 2 2\nf 1 2 3\n",unknown?"unknown":"entity_20001_component_1573717156708898775_sub_0");
    fprintf(f,"o entity_20001_component_1573717156708898775_sub_1\nv 0 -2 2\nf 1 3 4\n");
    fprintf(f,"o entity_20004_component_7538488328409834502_sub_0\nv 4 -2 -2\nv 4 2 -2\nv 4 2 2\nv 4 -2 2\nf 5 6 7\nf 5 7 8\n");
    CHECK(!fclose(f));PeSceneCollision *s=pe_scene_load_obj(path,error,sizeof error);unlink(path);CHECK(s);return s;
}
static void scene_queries(void){
    PeSceneCollision *s=objects(false);CHECK(pe_scene_group_count(s)==2);PeSceneGroup group;
    CHECK(pe_scene_group(s,0,&group)&&group.source_identity&&group.entity_id==20001&&group.component_id==INT64_C(1573717156708898775));
    CHECK(group.minimum.y==-2&&group.maximum.z==2);CHECK(pe_scene_stats(s).objects==3);
    unsigned char flags[]={1,1};PeCameraGeometryHit hit;
    PeCameraGeometryQuery q=query(PE_CAMERA_GEOMETRY_OVERLAP,(PeVec3){.25f,0,0},(PeVec3){0},.25f,0);
    CHECK(pe_scene_camera_query(s,&q,flags,2,&hit)&&hit.hit); /* touching is included */
    q.origin.x=.26f;CHECK(pe_scene_camera_query(s,&q,flags,2,&hit)&&!hit.hit);
    q=query(PE_CAMERA_GEOMETRY_SPHERE_CAST,(PeVec3){.1f,0,0},(PeVec3){1,0,0},.25f,8);
    CHECK(pe_scene_camera_query(s,&q,flags,2,&hit)&&hit.hit&&fabsf(hit.distance-3.65f)<.0001f);
    q.direction.x=-1;CHECK(pe_scene_camera_query(s,&q,flags,2,&hit)&&!hit.hit); /* initially overlapped group skipped */
    PeSceneHit movement=pe_scene_capsule_sweep(s,(PeVec3){.1f,-.25f,0},.25f,.5f,(PeVec3){-1,0,0});CHECK(movement.hit&&movement.fraction==0);
    q=query(PE_CAMERA_GEOMETRY_RAY,(PeVec3){-1,0,0},(PeVec3){1,0,0},0,8);CHECK(pe_scene_camera_query(s,&q,flags,2,&hit)&&hit.hit&&hit.distance==1);
    flags[0]=0;CHECK(pe_scene_camera_query(s,&q,flags,2,&hit)&&hit.hit&&hit.distance==5);flags[0]=1;
    q.kind=PE_CAMERA_GEOMETRY_LINE;q.end=(PeVec3){2,0,0};q.distance=3;
    CHECK(pe_scene_camera_query(s,&q,flags,2,&hit)&&hit.hit&&hit.distance==1);
    q.end=q.origin;q.distance=0;CHECK(pe_scene_camera_query(s,&q,flags,2,&hit)&&!hit.hit);
    PeCameraGeometryHit old={true,37};hit=old;q.radius=NAN;CHECK(!pe_scene_camera_query(s,&q,flags,2,&hit)&&!memcmp(&old,&hit,sizeof hit));
    pe_scene_free(s);
}
static void snapshot_queries(void){
    PeWorld *w=calloc(1,sizeof *w);PeSandboxCollision *c=pe_sandbox_collision_create();CHECK(w&&c);char error[256];
    PeCameraQueryContext context={c};PeCameraGeometryHit hit={true,37},old=hit;
    PeCameraGeometryQuery q=query(PE_CAMERA_GEOMETRY_OVERLAP,(PeVec3){20,10,20},(PeVec3){0},.1f,0);
    CHECK(!pe_camera_query(&context,&q,&hit)&&!memcmp(&old,&hit,sizeof hit));
    PeSandboxBox boxes[]={{{19,9,19},{21,11,21}},{{24,9,19},{25,11,21}}};
    CHECK(pe_sandbox_collision_update(c,w,NULL,0,boxes,2,error,sizeof error));PeSandboxCollisionStats stats=pe_sandbox_collision_stats(c);
    CHECK(pe_camera_query(&context,&q,&hit)&&hit.hit); /* deep inside first solid */
    q=query(PE_CAMERA_GEOMETRY_SPHERE_CAST,(PeVec3){20,10,20},(PeVec3){1,0,0},.1f,10);
    CHECK(pe_camera_query(&context,&q,&hit)&&hit.hit&&fabsf(hit.distance-3.9f)<.0001f);
    q.kind=PE_CAMERA_GEOMETRY_RAY;q.radius=0;CHECK(pe_camera_query(&context,&q,&hit)&&hit.hit&&hit.distance==4);
    PeSandboxCollisionStats unchanged=pe_sandbox_collision_stats(c);CHECK(!memcmp(&stats,&unchanged,sizeof stats));
    PeTreeCapsule tree=pe_tree_collision_source((PeVec3){20,9,20});
    CHECK(pe_sandbox_collision_update_with_trees(c,w,NULL,0,NULL,0,&tree,1,error,sizeof error));
    q=query(PE_CAMERA_GEOMETRY_RAY,(PeVec3){20,10,15},(PeVec3){0,0,1},0,10);
    CHECK(pe_sandbox_collision_ray_perception(c,q.origin,q.direction,q.distance).hit);
    CHECK(pe_camera_query(&context,&q,&hit)&&!hit.hit);
    /* Crop-boundary planes are motor-only; camera sees actual surfaces only. */
    q=query(PE_CAMERA_GEOMETRY_SPHERE_CAST,(PeVec3){1,10,10},(PeVec3){-1,0,0},.2f,8);
    CHECK(pe_sandbox_collision_sweep_geometry(c,(PeVec3){1,9.8f,10},.2f,.4f,(PeVec3){-8,0,0}).hit);
    CHECK(pe_camera_query(&context,&q,&hit)&&!hit.hit);
    q.origin.x=-2;CHECK(pe_camera_query(&context,&q,&hit)&&!hit.hit);
    q.mask=1;hit=old;CHECK(!pe_camera_query(&context,&q,&hit)&&!memcmp(&old,&hit,sizeof hit));q.mask=PE_SANDBOX_CAMERA_OBSTACLE_MASK;
    q.ignore_triggers=false;CHECK(!pe_camera_query(&context,&q,&hit));q.ignore_triggers=true;
    /* A valid earlier construction overlap cannot conceal a later unknown
     * static identity. Failure keeps the caller's previous result intact. */
    PeSceneCollision *s=objects(true);CHECK(pe_sandbox_collision_update_with_static(c,w,NULL,0,boxes,2,NULL,0,s,error,sizeof error));
    q=query(PE_CAMERA_GEOMETRY_OVERLAP,(PeVec3){20,10,20},(PeVec3){0},.1f,0);
    hit=old;CHECK(!pe_camera_query(&context,&q,&hit)&&!memcmp(&old,&hit,sizeof hit));
    CHECK(pe_sandbox_collision_update(c,w,NULL,0,NULL,0,error,sizeof error));pe_scene_free(s);
    /* Real evaluator with this backend: previews do not commit clocks/state. */
    PeCameraGeometryState state={0};PeCameraGeometryInput input={.position={20,10,26},.forward={0,0,-1},.right={-1,0,0},.up={0,1,0},.distance=6,.near_clip=.8f,.fov_degrees=60,.aspect=16.0f/9,.activity=50};
    PeCameraGeometryResult result,again;CHECK(pe_camera_geometry_evaluate(&state,&input,pe_camera_query,&context,&result));
    CHECK(pe_camera_geometry_evaluate(&state,&input,pe_camera_query,&context,&again)&&state.rise==0);
    CHECK(result.position.x==again.position.x&&result.position.y==again.position.y&&result.position.z==again.position.z&&result.near_clip==again.near_clip&&result.distance==again.distance&&result.rise==again.rise&&result.ncr==again.ncr&&result.clamped==again.clamped);
    CHECK(!result.clamped&&fabsf(result.distance-6)<.0001f);
    q=query(PE_CAMERA_GEOMETRY_LINE,(PeVec3){20,10,20},(PeVec3){0},0,1);
    q.end=q.origin;hit=old;CHECK(!pe_camera_query(&context,&q,&hit)&&!memcmp(&old,&hit,sizeof hit));
    pe_sandbox_collision_free(c);pe_world_destroy(w);free(w);
}
static void installed(const char *path){
    char error[256];PeSceneCollision *s=pe_scene_load_obj(path,error,sizeof error);CHECK(s);
    PeWorld *w=calloc(1,sizeof *w);PeSandboxCollision *c=pe_sandbox_collision_create();CHECK(w&&c);
    CHECK(pe_sandbox_collision_update_with_static(c,w,NULL,0,NULL,0,NULL,0,s,error,sizeof error));
    PeCameraQueryContext context={c};PeCameraGeometryHit hit;
    for(size_t i=0;i<pe_scene_group_count(s);i++){
        PeSceneGroup g;CHECK(pe_scene_group(s,i,&g)&&g.source_identity);
        PeVec3 p={(g.minimum.x+g.maximum.x)*.5f,g.maximum.y+2,(g.minimum.z+g.maximum.z)*.5f};
        PeCameraGeometryQuery q=query(PE_CAMERA_GEOMETRY_RAY,p,(PeVec3){0,-1,0},0,20);CHECK(pe_camera_query(&context,&q,&hit));
        q.kind=PE_CAMERA_GEOMETRY_SPHERE_CAST;q.radius=.2f;CHECK(pe_camera_query(&context,&q,&hit));
        q.kind=PE_CAMERA_GEOMETRY_OVERLAP;q.origin=g.minimum;q.distance=0;CHECK(pe_camera_query(&context,&q,&hit));
    }
    printf("camera query installed static: %zu collider groups, %zu triangles\n",pe_scene_group_count(s),pe_scene_stats(s).triangles);
    pe_sandbox_collision_free(c);pe_scene_free(s);pe_world_destroy(w);free(w);
}
int main(int argc,char **argv){scene_queries();snapshot_queries();for(int i=1;i<argc;i++)installed(argv[i]);puts("camera query: grouped surfaces, touching, initial overlap, later hits, trees/crop, rollback and evaluator pass");return 0;}
