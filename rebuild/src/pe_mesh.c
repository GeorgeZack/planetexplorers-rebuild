#include "pe_world.h"
#include "../generated/pe_mc_tables.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Topology, corner order, and interpolation follow MarchingCubesSW.cs.
 * The renderer uses explicit normal/material attributes rather than Unity UV packing. */
typedef struct { PeMeshData data; size_t capacity; bool ok; } Builder;
bool pe_world_mesh_region_changed(const PeWorld *w,const PeWorld *old,
                                 int x,int y,int z,int nx,int ny,int nz) {
    if(!w||!old||w->source!=old->source||x<0||y<0||z<0||nx<0||ny<0||nz<0||
       x>=PE_WORLD_X||y>=PE_WORLD_Y||z>=PE_WORLD_Z||nx>PE_WORLD_X-1-x||
       ny>PE_WORLD_Y-1-y||nz>PE_WORLD_Z-1-z)return true;
    if(!nx||!ny||!nz)return false;
    int xmax=x+nx+1<PE_WORLD_X?x+nx+1:PE_WORLD_X-1;
    int ymax=y+ny+1<PE_WORLD_Y?y+ny+1:PE_WORLD_Y-1;
    int zmax=z+nz+1<PE_WORLD_Z?z+nz+1:PE_WORLD_Z-1;
    int xmin=x>0?x-1:0,ymin=y>0?y-1:0,zmin=z>0?z-1:0;
    for(int iz=zmin;iz<=zmax;iz++)for(int iy=ymin;iy<=ymax;iy++) {
        size_t start=((size_t)iz*PE_WORLD_Y+(size_t)iy)*PE_WORLD_X+(size_t)xmin;
        if(memcmp(&w->voxels[start],&old->voxels[start],(size_t)(xmax-xmin+1)*sizeof(PeVoxel)))return true;
    }
    return false;
}
static PeVec3 sub(PeVec3 a, PeVec3 b) { return (PeVec3){a.x-b.x,a.y-b.y,a.z-b.z}; }
static PeVec3 cross(PeVec3 a, PeVec3 b) { return (PeVec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static float dot(PeVec3 a, PeVec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static PeVec3 normal(PeVec3 a) {
    float n=sqrtf(dot(a,a));
    return n>1e-8f ? (PeVec3){a.x/n,a.y/n,a.z/n} : (PeVec3){0,1,0};
}
int pe_world_cell_triangles(const PeWorld *w,int x,int y,int z,PeVec3 out[15]) {
    if(!w||!out||x<0||y<0||z<0||x>=PE_WORLD_X-1||y>=PE_WORLD_Y-1||z>=PE_WORLD_Z-1)return 0;
    static const int corners[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    PeVec3 positions[8],edges[12];int volumes[8];unsigned mask=0;
    for(int c=0;c<8;c++) {
        int cx=x+corners[c][0],cy=y+corners[c][1],cz=z+corners[c][2];
        positions[c]=(PeVec3){(float)cx,(float)cy,(float)cz};volumes[c]=pe_world_get(w,cx,cy,cz).volume;
        if(volumes[c]<PE_ISO_LEVEL)mask|=1u<<c;
    }
    if(!pe_mc_edges[mask])return 0;
    for(int e=0;e<12;e++)if(pe_mc_edges[mask]&(1u<<e)) {
        int a=pe_mc_edge_corners[e][0],b=pe_mc_edge_corners[e][1];
        float t=(128.0f-volumes[a])/(volumes[b]-volumes[a]);
        edges[e]=(PeVec3){positions[a].x+(positions[b].x-positions[a].x)*t,
            positions[a].y+(positions[b].y-positions[a].y)*t,positions[a].z+(positions[b].z-positions[a].z)*t};
    }
    int count=0;
    for(int i=0;i<16&&pe_mc_triangles[mask][i]>=0;i+=3) {
        PeVec3 a=edges[pe_mc_triangles[mask][i]],b=edges[pe_mc_triangles[mask][i+1]],c=edges[pe_mc_triangles[mask][i+2]];
        PeVec3 face=cross(sub(b,a),sub(c,a));
        if(dot(face,face)<1e-12f)continue;
        out[count++]=a;out[count++]=b;out[count++]=c;
    }
    return count;
}
static PeVec3 gradient(const PeWorld *w, int x, int y, int z) {
    /* Clamp only at bounded crop edges. Interior chunk boundaries share neighbors. */
    int xl=x?x-1:x, xr=x+1<PE_WORLD_X?x+1:x;
    int yl=y?y-1:y, yr=y+1<PE_WORLD_Y?y+1:y;
    int zl=z?z-1:z, zr=z+1<PE_WORLD_Z?z+1:z;
    return (PeVec3){(float)pe_world_get(w,xl,y,z).volume-pe_world_get(w,xr,y,z).volume,
                    (float)pe_world_get(w,x,yl,z).volume-pe_world_get(w,x,yr,z).volume,
                    (float)pe_world_get(w,x,y,zl).volume-pe_world_get(w,x,y,zr).volume};
}
static void emit(Builder *b, PeVertex a, PeVertex c, PeVertex d) {
    PeVec3 face=cross(sub(c.position,a.position),sub(d.position,a.position));
    if(dot(face,face)<1e-12f) return;
    PeVec3 n={a.normal.x+c.normal.x+d.normal.x,a.normal.y+c.normal.y+d.normal.y,a.normal.z+c.normal.z+d.normal.z};
    /* Convert the original table to outward winding for our OpenGL convention. */
    if(dot(face,n)<0) { PeVertex temp=c;c=d;d=temp; }
    if(b->data.count+3>b->capacity) {
        size_t capacity=b->capacity ? b->capacity*2 : 8192;
        if(capacity>SIZE_MAX/sizeof(PeVertex)) { b->ok=false;return; }
        PeVertex *vertices=realloc(b->data.vertices,capacity*sizeof(*vertices));
        if(!vertices) { b->ok=false;return; }
        b->data.vertices=vertices;b->capacity=capacity;
    }
    b->data.vertices[b->data.count++]=a;
    b->data.vertices[b->data.count++]=c;
    b->data.vertices[b->data.count++]=d;
}
bool pe_world_mesh_region(const PeWorld *w,int x0,int y0,int z0,int nx,int ny,int nz,PeMeshData *mesh) {
    if(!w||!mesh||x0<0||y0<0||z0<0||nx<0||ny<0||nz<0||
       x0>=PE_WORLD_X||y0>=PE_WORLD_Y||z0>=PE_WORLD_Z||
       nx>PE_WORLD_X-1-x0||ny>PE_WORLD_Y-1-y0||nz>PE_WORLD_Z-1-z0) return false;
    static const int corners[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    Builder builder={.ok=true};
    for(int z=z0;z<z0+nz&&builder.ok;z++) for(int y=y0;y<y0+ny&&builder.ok;y++) for(int x=x0;x<x0+nx&&builder.ok;x++) {
        PeVoxel values[8];PeVec3 positions[8],normals[8];PeVertex edges[12];unsigned int mask=0;
        for(int c=0;c<8;c++) {
            int cx=x+corners[c][0],cy=y+corners[c][1],cz=z+corners[c][2];
            positions[c]=(PeVec3){(float)cx,(float)cy,(float)cz};
            values[c]=pe_world_get(w,cx,cy,cz);
            if(values[c].volume<PE_ISO_LEVEL) mask|=1u<<c;
        }
        if(!pe_mc_edges[mask]) continue;
        for(int c=0;c<8;c++) normals[c]=gradient(w,(int)positions[c].x,(int)positions[c].y,(int)positions[c].z);
        for(int e=0;e<12;e++) if(pe_mc_edges[mask]&(1u<<e)) {
            int a=pe_mc_edge_corners[e][0],b=pe_mc_edge_corners[e][1];
            float t=(128.0f-values[a].volume)/((float)values[b].volume-values[a].volume);
            edges[e].position=(PeVec3){positions[a].x+(positions[b].x-positions[a].x)*t,
                positions[a].y+(positions[b].y-positions[a].y)*t,positions[a].z+(positions[b].z-positions[a].z)*t};
            edges[e].normal=normal((PeVec3){normals[a].x+(normals[b].x-normals[a].x)*t,
                normals[a].y+(normals[b].y-normals[a].y)*t,normals[a].z+(normals[b].z-normals[a].z)*t});
            edges[e].material=values[a].volume>=128?values[a].material:values[b].material;
        }
        for(int t=0;t<16&&pe_mc_triangles[mask][t]>=0&&builder.ok;t+=3)
            emit(&builder,edges[pe_mc_triangles[mask][t]],edges[pe_mc_triangles[mask][t+1]],edges[pe_mc_triangles[mask][t+2]]);
    }
    if(!builder.ok) {free(builder.data.vertices);return false;}
    pe_mesh_free(mesh);*mesh=builder.data;return true;
}
bool pe_world_mesh(const PeWorld *world,PeMeshData *mesh) {
    return pe_world_mesh_region(world,0,0,0,PE_WORLD_X-1,PE_WORLD_Y-1,PE_WORLD_Z-1,mesh);
}
void pe_mesh_free(PeMeshData *mesh) {
    if(mesh) {free(mesh->vertices);mesh->vertices=NULL;mesh->count=0;}
}
