#define _POSIX_C_SOURCE 200809L
#include "pe_test_paths.h"
#include "pe_scene_collision.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(e) do{if(!(e)){fprintf(stderr,"scene collision:%d: %s\n",__LINE__,#e);failures++;}}while(0)
static bool fixture(const char *path,const char *data,size_t size){FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(data,1,size,f)==size;if(fclose(f))ok=false;return ok;}
static int mesh_constructor_tests(void){
    int failures=0;char error[128];
    PeVertex vertices[6]={0};PeVec3 points[6]={{-4,0,-4},{4,0,4},{4,0,-4},{-4,0,-4},{-4,0,4},{4,0,4}};
    for(int i=0;i<6;i++)vertices[i].position=points[i];
    PeMeshData mesh={vertices,6};PeSceneCollision *scene=pe_scene_from_mesh(&mesh,(PeVec3){5,2,7},error,sizeof error);
    CHECK(scene!=NULL);
    if(scene){
        PeSceneStats stats=pe_scene_stats(scene);CHECK(stats.vertices==6&&stats.triangles==2&&stats.minimum.y==2);
        PeSceneHit hit=pe_scene_raycast(scene,(PeVec3){5,5,7},(PeVec3){0,-1,0},5);CHECK(hit.hit&&fabsf(hit.distance-3)<1e-6f);
        vertices[0].position.x=NAN; /* Scene owns copied positions. */
        CHECK(pe_scene_raycast(scene,(PeVec3){5,5,7},(PeVec3){0,-1,0},5).hit);
        CHECK(pe_scene_from_mesh(&mesh,(PeVec3){0},error,sizeof error)==NULL);
        pe_scene_free(scene);
    }
    vertices[0].position=points[0];mesh.count=5;CHECK(pe_scene_from_mesh(&mesh,(PeVec3){0},error,sizeof error)==NULL);
    mesh.count=1000002;CHECK(pe_scene_from_mesh(&mesh,(PeVec3){0},error,sizeof error)==NULL);
    mesh.count=6;CHECK(pe_scene_from_mesh(&mesh,(PeVec3){NAN,0,0},error,sizeof error)==NULL);
    vertices[0].position.x=1000000;CHECK(pe_scene_from_mesh(&mesh,(PeVec3){1,0,0},error,sizeof error)==NULL);
    memset(vertices,0,sizeof vertices);CHECK(pe_scene_from_mesh(&mesh,(PeVec3){0},error,sizeof error)==NULL);
    mesh=(PeMeshData){0};scene=pe_scene_from_mesh(&mesh,(PeVec3){0},error,sizeof error);CHECK(scene!=NULL);
    CHECK(pe_scene_stats(scene).triangles==0);CHECK(!pe_scene_raycast(scene,(PeVec3){0,5,0},(PeVec3){0,-1,0},10).hit);
    CHECK(!pe_scene_capsule_overlap(scene,(PeVec3){0},.3f,1.8f));CHECK(pe_scene_capsule_overlap(scene,(PeVec3){NAN,0,0},.3f,1.8f));
    CHECK(!pe_scene_capsule_sweep(scene,(PeVec3){0},.3f,1.8f,(PeVec3){0,-10,0}).hit);pe_scene_free(scene);
    return failures;
}
int pe_test_scene_collision(void){
    int failures=0;char path[]="/tmp/pe-scene-test-XXXXXX",error[256];int fd=mkstemp(path);if(fd<0)return 1;close(fd);
    const char room[]=
        "o floor\nv -20 0 -20\nv 20 0 -20\nv 20 0 20\nv -20 0 20\nf 1 3 2\nf 1 4 3\n"
        "o wall\nv 2 0 -4\nv 2 4 -4\nv 2 4 4\nv 2 0 4\nf 5 6 7\nf 5 7 8\n"
        "o ceiling\nv -4 3 -4\nv -4 3 4\nv 4 3 4\nv 4 3 -4\nf 9 10 11\nf 9 11 12\n";
    CHECK(fixture(path,room,sizeof room-1));PeSceneCollision *scene=pe_scene_load_obj(path,error,sizeof error);CHECK(scene!=NULL);
    if(scene){
        PeSceneStats stats=pe_scene_stats(scene);CHECK(stats.vertices==12&&stats.triangles==6&&stats.objects==3);
        PeSceneHit hit=pe_scene_raycast(scene,(PeVec3){0,2,0},(PeVec3){0,-7,0},5);
        CHECK(hit.hit&&fabsf(hit.distance-2)<1e-5f&&hit.normal.y>.99f);
        CHECK(!pe_scene_raycast(scene,(PeVec3){0,2,0},(PeVec3){0,-1,0},1).hit);
        CHECK(!pe_scene_raycast(scene,(PeVec3){0,2,0},(PeVec3){0,0,0},5).hit);
        CHECK(!pe_scene_capsule_overlap(scene,(PeVec3){0,0,0},.3f,1.8f));
        CHECK(pe_scene_capsule_overlap(scene,(PeVec3){0,-.01f,0},.3f,1.8f));
        CHECK(pe_scene_capsule_overlap(scene,(PeVec3){1.8f,0,0},.3f,1.8f));
        CHECK(pe_scene_capsule_overlap(scene,(PeVec3){0,1.3f,0},.3f,1.8f));
        hit=pe_scene_capsule_sweep(scene,(PeVec3){0,0,0},.3f,1.8f,(PeVec3){10,0,0});
        CHECK(hit.hit&&fabsf(hit.fraction-.17f)<1e-5f&&hit.normal.x<-.99f);
        hit=pe_scene_capsule_sweep(scene,(PeVec3){0,0,0},.3f,1.8f,(PeVec3){10000,0,0});
        CHECK(hit.hit&&fabsf(hit.distance-1.7f)<1e-4f);
        CHECK(!pe_scene_capsule_sweep(scene,(PeVec3){0,0,0},.3f,1.8f,(PeVec3){-10,0,0}).hit);
        hit=pe_scene_capsule_sweep(scene,(PeVec3){0,.8f,0},.3f,1.8f,(PeVec3){0,-2,0});
        CHECK(hit.hit&&fabsf(hit.distance-.8f)<1e-5f&&hit.normal.y>.99f);
        hit=pe_scene_capsule_sweep(scene,(PeVec3){0,0,0},.3f,1.8f,(PeVec3){0,3,0});
        CHECK(hit.hit&&fabsf(hit.distance-1.2f)<1e-5f&&hit.normal.y<-.99f);
        hit=pe_scene_capsule_sweep(scene,(PeVec3){1.7f,0,0},.3f,1.8f,(PeVec3){1,0,0});CHECK(hit.hit&&hit.fraction<1e-5f);
        CHECK(!pe_scene_capsule_sweep(scene,(PeVec3){1.7f,0,0},.3f,1.8f,(PeVec3){-1,0,0}).hit);
        /* A sphere is the zero-length-axis capsule case. */
        hit=pe_scene_capsule_sweep(scene,(PeVec3){0,1,0},.3f,.6f,(PeVec3){0,-2,0});CHECK(hit.hit&&fabsf(hit.distance-1)<1e-5f);
        CHECK(pe_scene_capsule_overlap(scene,(PeVec3){NAN,0,0},.3f,1.8f));
        CHECK(!pe_scene_capsule_sweep(scene,(PeVec3){0,0,0},.3f,1.8f,(PeVec3){NAN,0,0}).hit);
        pe_scene_free(scene);
    }
    const char ramp[]="v -4 0 -4\nv -4 0 4\nv 4 4 4\nv 4 4 -4\nf 1 2 3\nf 1 3 4\n";
    CHECK(fixture(path,ramp,sizeof ramp-1));scene=pe_scene_load_obj(path,error,sizeof error);CHECK(scene!=NULL);
    if(scene){
        PeSceneHit hit=pe_scene_capsule_sweep(scene,(PeVec3){0,5,0},.3f,1.8f,(PeVec3){0,-5,0});
        float support=2+.3f*(sqrtf(1.25f)-1);
        CHECK(hit.hit&&fabsf(5-hit.distance-support)<2e-5f&&fabsf(hit.normal.y-.89442719f)<2e-5f);
        CHECK(!pe_scene_capsule_overlap(scene,(PeVec3){0,support+.001f,0},.3f,1.8f));
        CHECK(pe_scene_capsule_overlap(scene,(PeVec3){0,support-.001f,0},.3f,1.8f));
        pe_scene_free(scene);
    }
    const char *bad[]={"v nan 0 0\n","v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf -1 2 3\n","v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3 1\n",
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1/1/1 2/2/2 3/3/3\n","v 0 0 0\nf 1 1 1\n"};
    for(size_t i=0;i<sizeof bad/sizeof *bad;i++){CHECK(fixture(path,bad[i],strlen(bad[i])));scene=pe_scene_load_obj(path,error,sizeof error);CHECK(scene==NULL&&error[0]);pe_scene_free(scene);}
    const char binary[]="v 0 0 0\n\0f 1 1 1\n";CHECK(fixture(path,binary,sizeof binary-1));scene=pe_scene_load_obj(path,error,sizeof error);CHECK(scene==NULL);pe_scene_free(scene);unlink(path);
    const char *actual=pe_test_path("rebuild/build/reference-assets/room-collision.obj");
    if(access(actual,R_OK)==0){
        scene=pe_scene_load_obj(actual,error,sizeof error);CHECK(scene!=NULL);
        if(scene){PeSceneStats stats=pe_scene_stats(scene);CHECK(stats.objects==3&&stats.triangles==56446&&stats.vertices==69618);
            PeSceneHit floor=pe_scene_raycast(scene,(PeVec3){27,2,11},(PeVec3){0,-1,0},3);CHECK(floor.hit&&floor.normal.y>.7f&&floor.position.y>1&&floor.position.y<2);
            PeSceneHit landing=pe_scene_capsule_sweep(scene,(PeVec3){27,2,11},.3f,1.8f,(PeVec3){0,-2,0});CHECK(landing.hit&&landing.normal.y>.7f);
            CHECK(!pe_scene_capsule_overlap(scene,(PeVec3){27,2,11},.3f,1.8f));
            pe_scene_free(scene);}
    }
    return failures+mesh_constructor_tests();
}
