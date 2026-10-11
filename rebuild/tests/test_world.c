#include "pe_test_paths.h"
#include "pe_world.h"
#include "pe_legacy.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if(!(expr)){fprintf(stderr,"world: %s:%d: %s\n",__FILE__,__LINE__,#expr);failures++;} } while(0)
static void plane(PeWorld *w) {
    memset(w,0,sizeof(*w));
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++)
        w->voxels[(z*PE_WORLD_Y+y)*PE_WORLD_X+x]=(PeVoxel){y<=8?255:0,y<=8?PE_MAT_STONE:PE_MAT_AIR};
}
int pe_test_world(void) {
    int failures=0;
    PeWorld *w=malloc(sizeof(*w)),*copy=malloc(sizeof(*copy));
    if(!w||!copy){free(w);free(copy);return 1;}
    CHECK(sizeof(PeVoxel)==2);
    pe_world_init(w,666);pe_world_init(copy,666);
    CHECK(pe_world_hash(w)==pe_world_hash(copy));
    CHECK(pe_world_hash(w)==UINT64_C(14854765168810625299));
    pe_world_init(copy,667);CHECK(pe_world_hash(w)!=pe_world_hash(copy));
    CHECK(pe_world_height(w,32,32)>8&&pe_world_height(w,32,32)<16);
    CHECK(!pe_world_set(w,-1,0,0,(PeVoxel){255,1}));
    CHECK(pe_world_get(w,0,-1,0).volume==0);
    CHECK(pe_world_set(w,63,31,63,(PeVoxel){127,3}));
    CHECK(w->voxels[PE_VOXEL_COUNT-1].volume==127);
    CHECK(pe_world_density(w,(PeVec3){NAN,0,0})==0);
    plane(w);
    const float surface=8+127.0f/255.0f;
    CHECK(fabsf(pe_world_height(w,20.3f,20.7f)-surface)<1e-5f);
    CHECK(fabsf(pe_world_density(w,(PeVec3){20,8.5f,20})-127.5f)<1e-5f);
    PeHit h=pe_world_raycast(w,(PeVec3){20,20,20},(PeVec3){0,-10,0},20);
    CHECK(h.hit&&fabsf(h.distance-(20-surface))<1e-4f);
    CHECK(fabsf(h.position.y-surface)<1e-4f&&h.normal.y>.99f);
    CHECK(!pe_world_raycast(w,(PeVec3){20,20,20},(PeVec3){0,-1,0},5).hit);
    CHECK(!pe_world_raycast(w,(PeVec3){20,20,20},(PeVec3){0,0,0},20).hit);
    CHECK(!pe_world_raycast(w,(PeVec3){20,20,20},(PeVec3){NAN,0,0},20).hit);
    h=pe_world_raycast(w,(PeVec3){20,40,20},(PeVec3){0,-1,0},50);
    CHECK(h.hit&&fabsf(h.distance-(40-surface))<1e-4f);
    h=pe_world_raycast(w,(PeVec3){-10,4,20},(PeVec3){1,0,0},30);
    CHECK(h.hit&&fabsf(h.distance-10)<1e-5f);
    CHECK(!pe_world_raycast(w,(PeVec3){-10,20,20},(PeVec3){0,-1,0},30).hit);
    h=pe_world_raycast(w,(PeVec3){20,4,20},(PeVec3){1,0,0},0);
    CHECK(h.hit&&h.distance==0);
    CHECK(pe_world_collides(w,(PeVec3){20,8,20},.3f,1.75f));
    CHECK(!pe_world_collides(w,(PeVec3){20,surface+.01f,20},.3f,1.75f));
    CHECK(pe_world_collides(w,(PeVec3){-.1f,20,20},.3f,1.75f));
    PeMeshData mesh={0};CHECK(pe_world_mesh(w,&mesh));CHECK(mesh.count>0&&mesh.count%3==0);
    for(size_t i=0;i<mesh.count;i+=3) {
        PeVertex a=mesh.vertices[i],b=mesh.vertices[i+1],c=mesh.vertices[i+2];
        CHECK(isfinite(a.normal.x)&&isfinite(a.normal.y)&&isfinite(a.normal.z));
        float len=a.normal.x*a.normal.x+a.normal.y*a.normal.y+a.normal.z*a.normal.z;
        CHECK(fabsf(len-1)<1e-4f);
        CHECK(fabsf(a.position.y-surface)<1e-5f);
        float cross_y=(b.position.z-a.position.z)*(c.position.x-a.position.x)-(b.position.x-a.position.x)*(c.position.z-a.position.z);
        CHECK(cross_y>0);
    }
    pe_mesh_free(&mesh);CHECK(mesh.vertices==NULL&&mesh.count==0);
    uint64_t before=pe_world_hash(w);uint32_t revision=w->revision;
    int removed=pe_world_dig(w,(PeVec3){20,8,20},2);
    CHECK(removed>0&&w->revision>revision&&before!=pe_world_hash(w));
    CHECK(pe_world_get(w,20,8,20).volume==0);
    CHECK(pe_world_height(w,20,20)<7);
    CHECK(pe_world_dig(w,(PeVec3){20,8,20},2)==0);
    CHECK(pe_world_dig(w,(PeVec3){-100,8,-100},2)==0);
    CHECK(pe_world_dig(w,(PeVec3){0,0,0},NAN)==0);
    CHECK(pe_world_dig(w,(PeVec3){0,0,0},2)>0);
    CHECK(pe_world_mesh(w,&mesh));
    for(size_t i=0;i<mesh.count;i+=3) {
        PeVertex a=mesh.vertices[i],b=mesh.vertices[i+1],c=mesh.vertices[i+2];
        float ux=b.position.x-a.position.x,uy=b.position.y-a.position.y,uz=b.position.z-a.position.z;
        float vx=c.position.x-a.position.x,vy=c.position.y-a.position.y,vz=c.position.z-a.position.z;
        float nx=a.normal.x+b.normal.x+c.normal.x,ny=a.normal.y+b.normal.y+c.normal.y,nz=a.normal.z+b.normal.z+c.normal.z;
        CHECK((uy*vz-uz*vy)*nx+(uz*vx-ux*vz)*ny+(ux*vy-uy*vx)*nz>=-1e-6f);
    }
    pe_mesh_free(&mesh);
    memset(w,0,sizeof(*w));CHECK(pe_world_mesh(w,&mesh));CHECK(mesh.count==0);pe_mesh_free(&mesh);
    CHECK(!pe_world_raycast(w,(PeVec3){20,20,20},(PeVec3){0,-1,0},40).hit);
    for(size_t i=0;i<PE_VOXEL_COUNT;i++)w->voxels[i]=(PeVoxel){255,PE_MAT_STONE};
    CHECK(pe_world_mesh(w,&mesh));CHECK(mesh.count==0);pe_mesh_free(&mesh);
    /* A single solid sample creates an octahedron. Implicit trilinear density
     * used to hit 0.117647m above its visible triangle at this offset. */
    memset(w,0,sizeof(*w));pe_world_set(w,10,10,10,(PeVoxel){255,1});
    float triangle_y=10+127.0f/255.0f-.4f;
    h=pe_world_raycast(w,(PeVec3){10.2f,12,10.2f},(PeVec3){0,-7,0},4);
    CHECK(h.hit&&fabsf(h.position.y-triangle_y)<2e-5f);
    CHECK(fabsf(h.normal.x-.57735027f)<1e-5f&&fabsf(h.normal.y-.57735027f)<1e-5f&&fabsf(h.normal.z-.57735027f)<1e-5f);
    CHECK(fabsf(pe_world_height(w,10.2f,10.2f)-triangle_y)<2e-5f);
    CHECK(!pe_world_raycast(w,(PeVec3){10.2f,12,10.2f},(PeVec3){0,-1,0},12-triangle_y-.001f).hit);
    /* The implicit field still has a lobe here, but no rendered triangle exists. */
    CHECK(!pe_world_raycast(w,(PeVec3){10.25f,12,10.25f},(PeVec3){0,-1,0},4).hit);
    h=pe_world_raycast(w,(PeVec3){10.2f,8,10.2f},(PeVec3){0,1,0},4);
    CHECK(h.hit&&fabsf(h.position.y-(20-triangle_y))<2e-5f&&h.normal.y<-.57f);
    for(int axis=0;axis<3;axis++)for(int sign=-1;sign<=1;sign+=2) {
        float from[3]={10.2f,10.2f,10.2f},direction[3]={0};from[axis]=10+2*sign;direction[axis]=(float)-sign;
        h=pe_world_raycast(w,(PeVec3){from[0],from[1],from[2]},(PeVec3){direction[0],direction[1],direction[2]},4);
        CHECK(h.hit&&fabsf(h.distance-(12-triangle_y))<2e-5f);
    }
    /* A diagonal ray can enter and exit triangles inside a single cell. */
    memset(w,0,sizeof(*w));
    pe_world_set(w,10,10,10,(PeVoxel){255,1});
    pe_world_set(w,11,11,10,(PeVoxel){255,1});
    for(int y=10;y<=11;y++)for(int x=10;x<=11;x++)pe_world_set(w,x,y,11,(PeVoxel){255,1});
    h=pe_world_raycast(w,(PeVec3){10,11,10.1f},(PeVec3){1,-1,0},1.414214f);
    CHECK(h.hit&&h.distance>0&&h.distance<1.414214f);
    /* Authored tutorial has non-axis-aligned source triangles which the old
     * trilinear height query missed completely at x13.3,z2.4. */
    char error[256];
    PeLegacyFile *tutorial=pe_legacy_open(pe_test_path("TutorialMode/Mainland/map_x0_y0.voxelform"),0,error,sizeof error);
    CHECK(tutorial!=NULL);
    if(tutorial) {
        CHECK(pe_legacy_crop(tutorial,0,0,0,w,error,sizeof error));
        CHECK(fabsf(pe_world_height(w,12.2f,12.2f)-2.8194712f)<2e-5f);
        CHECK(fabsf(pe_world_height(w,13.3f,2.4f)-1.0275230f)<2e-5f);
        h=pe_world_raycast(w,(PeVec3){13.3f,5,2.4f},(PeVec3){0,-1,0},5);
        CHECK(h.hit&&fabsf(h.position.y-1.0275230f)<2e-5f);
        pe_legacy_close(tutorial);
    }
    /* Original indicator targets centered sample boxes, including low-density
     * boundary samples with axial support, rather than the visible MC surface. */
    memset(w,0,sizeof(*w));pe_world_set(w,10,5,10,(PeVoxel){255,2});
    pe_world_set(w,10,6,10,(PeVoxel){5,1});
    h=pe_world_dig_target(w,(PeVec3){10,12,10},(PeVec3){0,-10,0},(PeVec3){10,5,10},3);
    CHECK(h.hit&&h.x==10&&h.y==6&&h.z==10&&fabsf(h.distance-5.5f)<1e-6f&&h.normal.y==1);
    /* Reach is measured from player+up, independent of camera distance. */
    CHECK(!pe_world_dig_target(w,(PeVec3){10,12,10},(PeVec3){0,-1,0},(PeVec3){13,5,10},3).hit);
    CHECK(pe_world_dig_target(w,(PeVec3){10,12,10},(PeVec3){0,-1,0},(PeVec3){12.999f,5,10},3).hit);
    /* Unsupported wisps reject the first threshold, then a later threshold
     * passes through that wisp and selects supported material behind it. */
    pe_world_set(w,10,6,10,(PeVoxel){0,0});pe_world_set(w,10,8,10,(PeVoxel){5,1});
    h=pe_world_dig_target(w,(PeVec3){10,12,10},(PeVec3){0,-1,0},(PeVec3){10,5,10},3);
    CHECK(h.hit&&h.y==5&&fabsf(h.distance-6.5f)<1e-6f);
    /* Six axial neighbors do not include diagonal solids. */
    memset(w,0,sizeof(*w));pe_world_set(w,10,6,10,(PeVoxel){1,1});pe_world_set(w,11,5,10,(PeVoxel){255,2});
    CHECK(!pe_world_dig_target(w,(PeVec3){10,12,10},(PeVec3){0,-1,0},(PeVec3){10,5,10},3).hit);
    /* Preserve source strict axis ties; a corner hit does not select a voxel. */
    memset(w,0,sizeof(*w));pe_world_set(w,10,10,10,(PeVoxel){255,1});
    CHECK(!pe_world_dig_target(w,(PeVec3){8,8,8},(PeVec3){1,1,1},(PeVec3){10,9,10},3).hit);
    /* Source origin-near fallback uses shifted coordinates and nearest-even
     * rounding. It can select zero density supported by an adjacent solid. */
    memset(w,0,sizeof(*w));pe_world_set(w,10,5,10,(PeVoxel){255,1});
    h=pe_world_dig_target(w,(PeVec3){10,5,10},(PeVec3){0,1,0},(PeVec3){10,5,10},3);
    CHECK(h.hit&&h.x==10&&h.y==6&&h.z==10&&pe_world_get(w,h.x,h.y,h.z).volume==0);
    /* Imported odd origins change nearest-even parity in absolute coordinates.
     * Source y=6.5 rounds to6, which is crop-local5, not local6. */
    w->source=1;w->origin_y=1;
    h=pe_world_dig_target(w,(PeVec3){10,5,10},(PeVec3){0,1,0},(PeVec3){10,5,10},3);
    CHECK(h.hit&&h.y==5&&pe_world_get(w,h.x,h.y,h.z).volume==255);
    w->origin_y=-1;
    h=pe_world_dig_target(w,(PeVec3){10,5,10},(PeVec3){0,1,0},(PeVec3){10,5,10},3);
    CHECK(h.hit&&h.y==5);
    CHECK(!pe_world_dig_target(w,(PeVec3){NAN,5,10},(PeVec3){0,1,0},(PeVec3){10,5,10},3).hit);
    CHECK(!pe_world_dig_target(w,(PeVec3){10,5,10},(PeVec3){0,0,0},(PeVec3){10,5,10},3).hit);
    CHECK(!pe_world_dig_target(w,(PeVec3){1e30f,5,10},(PeVec3){1,0,0},(PeVec3){10,5,10},3).hit);
    free(w);free(copy);
    return failures;
}
