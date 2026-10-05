#include "pe_world.h"
#include "../generated/pe_mc_tables.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do {if(!(e)){fprintf(stderr,"mesh:%d: %s\n",__LINE__,#e);failures++;}}while(0)
static PeVec3 difference(PeVec3 a,PeVec3 b){return(PeVec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static PeVec3 vector_cross(PeVec3 a,PeVec3 b){return(PeVec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static float vector_dot(PeVec3 a,PeVec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
/* Brute-force rendered-mesh oracle: intentionally no cell lookup or DDA. */
static float mesh_ray(const PeMeshData *mesh,PeVec3 origin,PeVec3 direction) {
    float nearest=100;
    for(size_t i=0;i<mesh->count;i+=3) {
        PeVec3 a=mesh->vertices[i].position,e=difference(mesh->vertices[i+1].position,a),f=difference(mesh->vertices[i+2].position,a);
        PeVec3 p=vector_cross(direction,f);float det=vector_dot(e,p);if(fabsf(det)<1e-7f)continue;
        PeVec3 s=difference(origin,a);float u=vector_dot(s,p)/det;if(u<0||u>1)continue;
        PeVec3 q=vector_cross(s,e);float v=vector_dot(direction,q)/det;if(v<0||u+v>1)continue;
        float t=vector_dot(f,q)/det;if(t>=0&&t<nearest)nearest=t;
    }
    return nearest;
}
static float ray_random(uint32_t *state){*state=*state*1664525u+1013904223u;return(*state>>8)*(1.f/16777216.f);}
int pe_test_mesh(void) {
    int failures=0;
    PeWorld *w=calloc(1,sizeof(*w));
    if(!w)return 1;
    PeMeshData mesh={0};
    static const int corners[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    /* Exhaustive nondegenerate source topology cases for one interior cell. */
    for(unsigned int mask=0;mask<256;mask++) {
        memset(w,0,sizeof(*w));
        for(int c=0;c<8;c++) pe_world_set(w,20+corners[c][0],10+corners[c][1],20+corners[c][2],
            (PeVoxel){mask&(1u<<c)?0:255,PE_MAT_STONE});
        CHECK(pe_world_mesh_region(w,20,10,20,1,1,1,&mesh));
        size_t expected=0;
        while(expected<16&&pe_mc_triangles[mask][expected]>=0)expected++;
        CHECK(mesh.count==expected);
        PeVec3 surface[15];int count=pe_world_cell_triangles(w,20,10,20,surface);
        CHECK((size_t)count==mesh.count);
        /* Query geometry and rendered geometry must agree for all 256 cases;
         * winding can differ because render normals select outward order. */
        for(int j=0;j<count;j++) {
            bool found=false;
            for(size_t k=0;k<mesh.count;k++)if(surface[j].x==mesh.vertices[k].position.x&&
                surface[j].y==mesh.vertices[k].position.y&&surface[j].z==mesh.vertices[k].position.z)found=true;
            CHECK(found);
        }
        for(size_t i=0;i<mesh.count;i++) {
            PeVertex v=mesh.vertices[i];
            CHECK(isfinite(v.position.x)&&isfinite(v.position.y)&&isfinite(v.position.z));
            CHECK(v.position.x>=20&&v.position.x<=21&&v.position.y>=10&&v.position.y<=11&&v.position.z>=20&&v.position.z<=21);
            CHECK(v.material==PE_MAT_STONE);
            float length=v.normal.x*v.normal.x+v.normal.y*v.normal.y+v.normal.z*v.normal.z;
            CHECK(fabsf(length-1)<1e-4f);
            /* A marching-cubes vertex must lie on a cube edge, never a diagonal. */
            int integral=(fabsf(v.position.x-roundf(v.position.x))<1e-5f)+
                         (fabsf(v.position.y-roundf(v.position.y))<1e-5f)+
                         (fabsf(v.position.z-roundf(v.position.z))<1e-5f);
            CHECK(integral==2);
        }
        if(mask==1) {
            /* Air corner0: source edges 0,8,3 meet at 128/255 on each axis. */
            bool axis[3]={false,false,false};
            for(size_t i=0;i<mesh.count;i++) {
                PeVec3 p=mesh.vertices[i].position;float t=128.0f/255.0f;
                if(fabsf(p.x-20-t)<1e-5f&&p.y==10&&p.z==20)axis[0]=true;
                if(fabsf(p.y-10-t)<1e-5f&&p.x==20&&p.z==20)axis[1]=true;
                if(fabsf(p.z-20-t)<1e-5f&&p.x==20&&p.y==10)axis[2]=true;
            }
            CHECK(axis[0]&&axis[1]&&axis[2]);
        }
    }
    pe_world_init(w,666);
    CHECK(pe_world_mesh(w,&mesh));size_t whole=mesh.count,split=0;
    uint32_t random=32;
    for(int i=0;i<128;i++) {
        PeVec3 origin={1+ray_random(&random)*61,29,1+ray_random(&random)*61};
        PeVec3 direction={ray_random(&random)*2-1,-.2f-ray_random(&random),ray_random(&random)*2-1};
        float length=sqrtf(vector_dot(direction,direction));direction.x/=length;direction.y/=length;direction.z/=length;
        float expected=mesh_ray(&mesh,origin,direction);PeHit hit=pe_world_raycast(w,origin,direction,100);
        CHECK(hit.hit==(expected<100));
        if(hit.hit)CHECK(fabsf(hit.distance-expected)<.001f);
    }
    for(int x=0;x<2;x++) for(int z=0;z<2;z++) {
        PeMeshData chunk={0};
        CHECK(pe_world_mesh_region(w,x*32,0,z*32,x?31:32,31,z?31:32,&chunk));
        split+=chunk.count;
        for(size_t i=0;i<chunk.count;i++) {
            CHECK(chunk.vertices[i].position.x>=x*32&&chunk.vertices[i].position.x<=(x?63:32));
            CHECK(chunk.vertices[i].position.z>=z*32&&chunk.vertices[i].position.z<=(z?63:32));
        }
        pe_mesh_free(&chunk);
    }
    CHECK(split==whole);
    PeWorld *old=malloc(sizeof(*old));
    CHECK(old!=NULL);
    if(old) {
        *old=*w;
        CHECK(!pe_world_mesh_region_changed(w,old,0,0,0,32,31,32));
        /* The right chunk's x31 normal halo must invalidate on a left edit. */
        PeVoxel v=pe_world_get(w,31,12,20);v.volume^=255;
        pe_world_set(w,31,12,20,v);
        CHECK(pe_world_mesh_region_changed(w,old,0,0,0,32,31,32));
        CHECK(pe_world_mesh_region_changed(w,old,32,0,0,31,31,32));
        CHECK(!pe_world_mesh_region_changed(w,old,0,0,32,32,31,31));
        *w=*old;
        v=pe_world_get(w,34,12,20);v.material^=1;
        pe_world_set(w,34,12,20,v);
        CHECK(!pe_world_mesh_region_changed(w,old,0,0,0,32,31,32));
        CHECK(pe_world_mesh_region_changed(w,old,32,0,0,31,31,32));
        *w=*old;w->source=1;
        CHECK(pe_world_mesh_region_changed(w,old,0,0,32,32,31,31));
        *w=*old;free(old);
    }
    PeVertex *before=mesh.vertices;
    CHECK(!pe_world_mesh_region(w,-1,0,0,1,1,1,&mesh));CHECK(mesh.vertices==before&&mesh.count==whole);
    CHECK(!pe_world_mesh_region(w,63,0,0,2,1,1,&mesh));CHECK(mesh.vertices==before);
    pe_mesh_free(&mesh);free(w);return failures;
}
