#include "pe_world.h"
#include "pe_content.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Bootstrap generator preserves density storage, not the original seed algorithm.
 * Original CPU marching-cubes extraction lives in pe_mesh.c. */
int pe_world_depth(const PeWorld *w) { return !w?0:(!w->depth||w->depth==PE_WORLD_Z?PE_WORLD_Z:(w->depth==PE_WORLD_MAX_Z?PE_WORLD_MAX_Z:0)); }
size_t pe_world_voxel_count(const PeWorld *w) { return (size_t)pe_world_depth(w)*PE_WORLD_X*PE_WORLD_Y; }
static bool inside(const PeWorld *w,int x,int y,int z) { return x>=0&&x<PE_WORLD_X&&y>=0&&y<PE_WORLD_Y&&z>=0&&z<pe_world_depth(w); }
static size_t index_at(int x,int y,int z) { return ((size_t)z*PE_WORLD_Y+(size_t)y)*PE_WORLD_X+(size_t)x; }
static PeVec3 add(PeVec3 a,PeVec3 b) { return (PeVec3){a.x+b.x,a.y+b.y,a.z+b.z}; }
static PeVec3 scale(PeVec3 a,float s) { return (PeVec3){a.x*s,a.y*s,a.z*s}; }
static float dot(PeVec3 a,PeVec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static PeVec3 unit(PeVec3 a) { float d=sqrtf(dot(a,a)); return d>1e-8f?scale(a,1.0f/d):(PeVec3){0,1,0}; }
static bool finite3(PeVec3 p) { return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z); }
static float lerp(float a,float b,float t) { return a+(b-a)*t; }
static uint32_t mix(uint32_t x) { x^=x>>16; x*=UINT32_C(0x7feb352d); x^=x>>15; x*=UINT32_C(0x846ca68b); return x^(x>>16); }
static float lattice(uint32_t seed,int x,int z) { return (float)(mix(seed^(uint32_t)x*UINT32_C(0x9e3779b9)^(uint32_t)z*UINT32_C(0x85ebca6b))&65535)/65535.0f; }
static float noise(uint32_t seed,float x,float z) {
    int ix=(int)floorf(x),iz=(int)floorf(z); float u=x-ix,v=z-iz;
    u=u*u*(3-2*u);v=v*v*(3-2*v);
    return lerp(lerp(lattice(seed,ix,iz),lattice(seed,ix+1,iz),u),lerp(lattice(seed,ix,iz+1),lattice(seed,ix+1,iz+1),u),v);
}
void pe_world_init(PeWorld *w,uint32_t seed) {
    if(!w)return;
    memset(w,0,sizeof(*w));w->seed=seed;w->revision=1;
    for(int z=0;z<PE_WORLD_Z;z++)for(int x=0;x<PE_WORLD_X;x++) {
        float h=8.5f+5.0f*noise(seed,x/21.0f,z/21.0f)+1.4f*noise(seed+1,x/7.0f,z/7.0f);
        float dx=x-16.0f,dz=z-18.0f,basin=fmaxf(0,1-(dx*dx+dz*dz)/180.0f);
        h-=5*basin*basin;
        for(int y=0;y<PE_WORLD_Y;y++) {
            float v=fminf(255,fmaxf(0,128+(h-y)*180));
            uint8_t m=y<h-3?PE_MAT_STONE:(y<h-1?PE_MAT_DIRT:PE_MAT_GRASS);
            w->voxels[index_at(x,y,z)]=(PeVoxel){(uint8_t)v,v>0?m:PE_MAT_AIR};
        }
    }
}
PeVoxel pe_world_get(const PeWorld *w,int x,int y,int z) { return w&&inside(w,x,y,z)?w->voxels[index_at(x,y,z)]:(PeVoxel){0,0}; }
bool pe_world_set(PeWorld *w,int x,int y,int z,PeVoxel v) {
    if(!w||!inside(w,x,y,z))return false;
    PeVoxel *old=&w->voxels[index_at(x,y,z)];
    if(old->volume!=v.volume||old->material!=v.material){*old=v;w->revision++;}
    return true;
}
float pe_world_density(const PeWorld *w,PeVec3 p) {
    if(!w||!finite3(p)||p.x<0||p.y<0||p.z<0||p.x>PE_WORLD_X-1||p.y>PE_WORLD_Y-1||p.z>pe_world_depth(w)-1)return 0;
    int x=(int)floorf(p.x),y=(int)floorf(p.y),z=(int)floorf(p.z);
    if(x==PE_WORLD_X-1)x--;
    if(y==PE_WORLD_Y-1)y--;
    if(z==pe_world_depth(w)-1)z--;
    float fx=p.x-x,fy=p.y-y,fz=p.z-z,v[2];
    for(int k=0;k<2;k++) {
        float a=lerp(pe_world_get(w,x,y,z+k).volume,pe_world_get(w,x+1,y,z+k).volume,fx);
        float b=lerp(pe_world_get(w,x,y+1,z+k).volume,pe_world_get(w,x+1,y+1,z+k).volume,fx);
        v[k]=lerp(a,b,fy);
    }
    return lerp(v[0],v[1],fz);
}
static PeVec3 normal_at(const PeWorld *w,PeVec3 p) {
    const float d=.2f;
    PeVec3 n={pe_world_density(w,(PeVec3){p.x-d,p.y,p.z})-pe_world_density(w,(PeVec3){p.x+d,p.y,p.z}),
              pe_world_density(w,(PeVec3){p.x,p.y-d,p.z})-pe_world_density(w,(PeVec3){p.x,p.y+d,p.z}),
              pe_world_density(w,(PeVec3){p.x,p.y,p.z-d})-pe_world_density(w,(PeVec3){p.x,p.y,p.z+d})};
    return unit(n);
}
float pe_world_height(const PeWorld *w,float x,float z) {
    if(!w||!isfinite(x)||!isfinite(z)||x<0||x>PE_WORLD_X-1||z<0||z>pe_world_depth(w)-1)return 0;
    if(pe_world_density(w,(PeVec3){x,PE_WORLD_Y-1,z})>=128)return PE_WORLD_Y-1;
    PeHit hit=pe_world_raycast(w,(PeVec3){x,PE_WORLD_Y-1,z},(PeVec3){0,-1,0},PE_WORLD_Y-1);
    return hit.hit?hit.position.y:0;
}
bool pe_world_collides(const PeWorld *w,PeVec3 feet,float radius,float height) {
    if(!w||!finite3(feet)||!isfinite(radius)||!isfinite(height)||radius<0||height<=0)return true;
    if(feet.x-radius<0||feet.z-radius<0||feet.x+radius>PE_WORLD_X-1||feet.z+radius>pe_world_depth(w)-1||feet.y<0)return true;
    /* Cylinder probes are provisional character physics, not a rigid-body engine. */
    int steps=(int)ceilf(fminf(height,PE_WORLD_Y*2.0f)/.20f);
    static const float ring[8][2]={{1,0},{.70710678f,.70710678f},{0,1},{-.70710678f,.70710678f},{-1,0},{-.70710678f,-.70710678f},{0,-1},{.70710678f,-.70710678f}};
    for(int j=0;j<=steps;j++) {
        float y=feet.y+height*j/steps;
        if(pe_world_density(w,(PeVec3){feet.x,y,feet.z})>128.01f)return true;
        for(int k=0;k<8;k++)if(pe_world_density(w,(PeVec3){feet.x+radius*ring[k][0],y,feet.z+radius*ring[k][1]})>128.01f)return true;
    }
    return false;
}
/* Intersect the same source marching-cubes triangles rendered for this cell.
 * Double precision barycentrics retain grazing/edge hits during DDA traversal. */
static bool cell_hit(const PeWorld *w,int x,int y,int z,PeVec3 origin,PeVec3 d,
                     double start,double end,double *distance,PeVec3 *normal) {
    PeVec3 triangles[15];int count=pe_world_cell_triangles(w,x,y,z,triangles);
    bool found=false;double best=end+1e-6;
    for(int i=0;i<count;i+=3) {
        PeVec3 a=triangles[i],b=triangles[i+1],c=triangles[i+2];
        double ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z,vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
        double px=d.y*vz-d.z*vy,py=d.z*vx-d.x*vz,pz=d.x*vy-d.y*vx;
        double det=ux*px+uy*py+uz*pz;if(fabs(det)<1e-12)continue;
        double ax=origin.x-a.x,ay=origin.y-a.y,az=origin.z-a.z;
        double u=(ax*px+ay*py+az*pz)/det;if(u< -1e-6||u>1+1e-6)continue;
        double qx=ay*uz-az*uy,qy=az*ux-ax*uz,qz=ax*uy-ay*ux;
        double v=(d.x*qx+d.y*qy+d.z*qz)/det;if(v< -1e-6||u+v>1+1e-6)continue;
        double t=(vx*qx+vy*qy+vz*qz)/det;
        if(t<start-1e-6||t>end+1e-6||t>=best)continue;
        best=fmax(start,fmin(end,t));found=true;
        *normal=unit((PeVec3){(float)(uy*vz-uz*vy),(float)(uz*vx-ux*vz),(float)(ux*vy-uy*vx)});
    }
    if(found)*distance=best;
    return found;
}
PeHit pe_world_raycast(const PeWorld *w,PeVec3 origin,PeVec3 direction,float range) {
    PeHit h={0};if(!w||!finite3(origin)||!finite3(direction)||!isfinite(range)||range<0)return h;
    double len=sqrt((double)direction.x*direction.x+(double)direction.y*direction.y+(double)direction.z*direction.z);
    if(len<1e-15)return h;
    PeVec3 d={(float)(direction.x/len),(float)(direction.y/len),(float)(direction.z/len)};
    float o[3]={origin.x,origin.y,origin.z},v[3]={d.x,d.y,d.z};
    int maxc[3]={PE_WORLD_X-1,PE_WORLD_Y-1,pe_world_depth(w)-1};
    double enter=0,leave=range;
    for(int a=0;a<3;a++) {
        if(fabs(v[a])<1e-15){if(o[a]<0||o[a]>maxc[a])return h;continue;}
        double t0=(0.0-o[a])/v[a],t1=((double)maxc[a]-o[a])/v[a];
        if(t0>t1){double tmp=t0;t0=t1;t1=tmp;}if(t0>enter)enter=t0;if(t1<leave)leave=t1;
    }
    if(enter>leave)return h;
    double t=enter;PeVec3 p=add(origin,scale(d,(float)t));
    p.x=fminf(maxc[0],fmaxf(0,p.x));p.y=fminf(maxc[1],fmaxf(0,p.y));p.z=fminf(maxc[2],fmaxf(0,p.z));
    if(pe_world_density(w,p)>=128){h.hit=true;h.distance=(float)t;h.position=p;}
    int cell[3];float pp[3]={p.x,p.y,p.z};
    for(int a=0;a<3;a++){cell[a]=(int)floorf(pp[a]);if(v[a]<0&&fabsf(pp[a]-cell[a])<1e-5f)cell[a]--;if(cell[a]<0)cell[a]=0;if(cell[a]>=maxc[a])cell[a]=maxc[a]-1;}
    for(int iter=0;!h.hit&&t<leave&&iter<PE_WORLD_X+PE_WORLD_Y+pe_world_depth(w)+8;iter++) {
        double next=leave,boundary[3];
        for(int a=0;a<3;a++){boundary[a]=DBL_MAX;if(v[a]!=0)boundary[a]=((cell[a]+(v[a]>0?1:0))-o[a])/(double)v[a];if(boundary[a]>t+1e-8&&boundary[a]<next)next=boundary[a];}
        double hit_distance;PeVec3 hit_normal;
        if(cell_hit(w,cell[0],cell[1],cell[2],origin,d,t,next,&hit_distance,&hit_normal)){
            h.hit=true;h.distance=(float)hit_distance;h.position=add(origin,scale(d,h.distance));
            if(dot(hit_normal,normal_at(w,h.position))<0)hit_normal=scale(hit_normal,-1);
            h.normal=hit_normal;break;
        }
        if(next>=leave)break;
        for(int a=0;a<3;a++)if(boundary[a]<=next+1e-7)cell[a]+=v[a]>0?1:-1;
        if(cell[0]<0||cell[1]<0||cell[2]<0||cell[0]>=maxc[0]||cell[1]>=maxc[1]||cell[2]>=maxc[2])break;
        t=next;
    }
    if(h.hit){if(dot(h.normal,h.normal)<.5f)h.normal=normal_at(w,h.position);h.x=(int)floorf(h.position.x);h.y=(int)floorf(h.position.y);h.z=(int)floorf(h.position.z);}
    return h;
}
static bool dig_supported(const PeWorld *w,int x,int y,int z) {
    if(!inside(w,x,y,z))return false;
    if(pe_world_get(w,x,y,z).volume>=128)return true;
    static const int offsets[6][3]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
    for(int i=0;i<6;i++)if(pe_world_get(w,x+offsets[i][0],y+offsets[i][1],z+offsets[i][2]).volume>=128)return true;
    return false;
}
static bool dig_index(const PeWorld *w,const double p[3],int index[3],bool nearest) {
    const int size[3]={PE_WORLD_X,PE_WORLD_Y,pe_world_depth(w)};
    const int32_t offset[3]={w->origin_x,w->origin_y,w->origin_z};
    for(int a=0;a<3;a++) {
        double n=floor(p[a]);
        /* Unity rounds absolute source coordinates, so a crop with an odd
         * origin changes which local sample wins an exact midpoint. */
        double absolute=n+(w->source==1?offset[a]:0);
        if(nearest&&(p[a]-n>.5||(p[a]-n==.5&&fmod(absolute,2)!=0)))n++;
        if(n<0||n>=size[a])return false;
        index[a]=(int)n;
    }
    return true;
}
static PeHit dig_box_ray(const PeWorld *w,PeVec3 origin,PeVec3 direction,int threshold) {
    PeHit hit={0};
    double o[3]={origin.x+.5,origin.y+.5,origin.z+.5};
    double d[3]={direction.x,direction.y,direction.z};
    int index[3],size[3]={PE_WORLD_X,PE_WORLD_Y,pe_world_depth(w)};
    if(dig_index(w,o,index,false)&&pe_world_get(w,index[0],index[1],index[2]).volume>=threshold)return hit;
    double enter[3]={10001,10001,10001};int samples[3][3]={{0}},step[3]={0};
    const int32_t offsets[3]={w->origin_x,w->origin_y,w->origin_z};
    for(int a=0;a<3;a++) {
        step[a]=d[a]>.0001?1:(d[a]<-.0001?-1:0);if(!step[a])continue;
        double offset=w->source==1?offsets[a]:0;
        int start=(int)fmax(0,fmin(size[a],trunc(trunc(o[a]+offset)+1+step[a]*.5)-offset));
        for(int plane=start;plane>=0&&plane<=size[a];plane+=step[a]) {
            double t=(plane-o[a])/d[a];
            double p[3]={o[0]+d[0]*t,o[1]+d[1]*t,o[2]+d[2]*t};
            p[a]=plane+step[a]*.5;
            if(dig_index(w,p,index,false)&&pe_world_get(w,index[0],index[1],index[2]).volume>=threshold) {
                /* Source finds the first axis candidate, then culls negatives. */
                if(t>=0&&t<=10000){enter[a]=t;memcpy(samples[a],index,sizeof index);}break;
            }
        }
    }
    int axis=-1;
    for(int a=0;a<3;a++)if(enter[a]<enter[(a+1)%3]&&enter[a]<enter[(a+2)%3])axis=a;
    if(axis<0||enter[axis]>10000)return hit;
    hit.hit=true;hit.distance=(float)enter[axis];hit.position=add(origin,scale(direction,hit.distance));
    hit.x=samples[axis][0];hit.y=samples[axis][1];hit.z=samples[axis][2];
    hit.normal=(PeVec3){axis==0?-step[axis]:0,axis==1?-step[axis]:0,axis==2?-step[axis]:0};
    return hit;
}
PeHit pe_world_dig_target(const PeWorld *w,PeVec3 origin,PeVec3 direction,PeVec3 feet,float reach) {
    PeHit hit={0};if(!w||!finite3(origin)||!finite3(direction)||!finite3(feet)||!isfinite(reach)||reach<=0)return hit;
    double length=sqrt((double)direction.x*direction.x+(double)direction.y*direction.y+(double)direction.z*direction.z);
    if(length<1e-15)return hit;
    direction=(PeVec3){(float)(direction.x/length),(float)(direction.y/length),(float)(direction.z/length)};
    for(int threshold=1;threshold<138;threshold+=10) {
        PeHit candidate=dig_box_ray(w,origin,direction,threshold);
        if(candidate.hit&&dig_supported(w,candidate.x,candidate.y,candidate.z)){hit=candidate;break;}
    }
    if(!hit.hit) {
        double shifted[3]={origin.x+.5,origin.y+.5,origin.z+.5};int p[3];
        if(dig_index(w,shifted,p,true)&&dig_supported(w,p[0],p[1],p[2])) {
            hit.hit=true;hit.x=p[0];hit.y=p[1];hit.z=p[2];hit.position=(PeVec3){(float)p[0],(float)p[1],(float)p[2]};
        }
    }
    if(hit.hit) {
        double x=hit.x-(double)feet.x,y=hit.y-((double)feet.y+1),z=hit.z-(double)feet.z;
        if(x*x+y*y+z*z>=(double)reach*reach)hit=(PeHit){0};
    }
    return hit;
}
int pe_world_dig(PeWorld *w,PeVec3 c,float radius) {
    if(!w||!pe_world_depth(w)||!finite3(c)||!isfinite(radius)||radius<=0)return 0;
    int removed=0;bool changed=false;
    /* Clamp before float-to-int conversion, including arbitrarily large brushes. */
    int lo[3]={(int)fmaxf(0,fminf(PE_WORLD_X-1,ceilf(c.x-radius-1))),(int)fmaxf(0,fminf(PE_WORLD_Y-1,ceilf(c.y-radius-1))),(int)fmaxf(0,fminf(pe_world_depth(w)-1,ceilf(c.z-radius-1)))};
    int hi[3]={(int)fmaxf(0,fminf(PE_WORLD_X-1,floorf(c.x+radius+1))),(int)fmaxf(0,fminf(PE_WORLD_Y-1,floorf(c.y+radius+1))),(int)fmaxf(0,fminf(pe_world_depth(w)-1,floorf(c.z+radius+1)))};
    for(int z=lo[2];z<=hi[2];z++)for(int y=lo[1];y<=hi[1];y++)for(int x=lo[0];x<=hi[0];x++) {
        double dx=x-(double)c.x,dy=y-(double)c.y,dz=z-(double)c.z;
        double dist=sqrt(dx*dx+dy*dy+dz*dz);
        int limit=(int)fmin(255,fmax(0,128+(dist-radius)*180));
        PeVoxel *v=&w->voxels[index_at(x,y,z)];
        if(limit<v->volume){if(v->volume>=128&&limit<128)removed++;v->volume=(uint8_t)limit;if(!limit)v->material=PE_MAT_AIR;changed=true;}
    }
    if(changed)w->revision++;
    return removed;
}

uint64_t pe_world_hash(const PeWorld *w) {
    if(!w)return 0;
    uint64_t h=UINT64_C(14695981039346656037);
    for(int i=0;i<4;i++){h^=(w->seed>>(8*i))&255;h*=UINT64_C(1099511628211);}
    for(size_t i=0;i<pe_world_voxel_count(w);i++){h^=w->voxels[i].volume;h*=UINT64_C(1099511628211);h^=w->voxels[i].material;h*=UINT64_C(1099511628211);}
    return h;
}

int pe_world_material_item(const PeWorld *world, uint8_t material) {
    if(!world)return 0;
    if(world->source==1) {
        int item=pe_terrain_material_item(material);
        return item>0?item:0;
    }
    switch(material) {
    case PE_MAT_STONE:return PE_ITEM_STONE;
    case PE_MAT_DIRT:case PE_MAT_GRASS:return PE_ITEM_DIRT;
    default:return 0;
    }
}
