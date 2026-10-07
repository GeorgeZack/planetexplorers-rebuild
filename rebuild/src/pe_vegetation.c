#include "pe_vegetation.h"
#include "pe_harvest.h"
#include "pe_settlement.h"
#include "../generated/pe_vegetation_source.h"
#include "../generated/pe_adventure_population.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static bool alive(const PeGame *g){return g&&g->mode==PE_GAME_ADVENTURE&&isfinite(g->hp)&&g->hp>0&&finite3(g->player);}
const PePlantSource *pe_vegetation_source(uint32_t id){
    for(unsigned i=0;i<PE_VEGETATION_SOURCE_COUNT;i++)if(pe_vegetation_sources[i].prototype_id==id)return &pe_vegetation_sources[i];
    return NULL;
}
bool pe_vegetation_state_valid(const PeGame *g){
    if(!g||(g->mode!=PE_GAME_LEGACY&&g->mode!=PE_GAME_ADVENTURE))return false;
    const PeVegetation *v=&g->vegetation;
    if(v->count>PE_MAX_PLANTS||(!v->installed&&v->count)||(v->installed&&(g->mode!=PE_GAME_ADVENTURE||!v->count)))return false;
    for(unsigned i=0;i<PE_MAX_PLANTS;i++){
        const PePlant *p=&v->plants[i];
        if(i>=v->count){
            if(p->prototype_id||p->position.x!=0||p->position.y!=0||p->position.z!=0||
               signbit(p->position.x)||signbit(p->position.y)||signbit(p->position.z)||p->depleted)return false;
            continue;
        }
        const PePlantSource *s=pe_vegetation_source(p->prototype_id);
        if(!s||!finite3(p->position)||(p->depleted&&!s->harvestable)||p->position.x<0||p->position.y<0||p->position.z<0||
           p->position.x>=pe_world_width(&g->world)-1||p->position.y>=pe_world_size_y(&g->world)-1||p->position.z>=pe_world_depth(&g->world)-1)return false;
        for(unsigned j=0;j<i;j++)if(p->position.x==v->plants[j].position.x&&p->position.y==v->plants[j].position.y&&p->position.z==v->plants[j].position.z)return false;
    }
    return true;
}
void pe_vegetation_clear(PeGame *g){if(g)memset(&g->vegetation,0,sizeof g->vegetation);}
static bool reachable(const PeGame *g,const PePlant *p){
    double dx=(double)p->position.x-g->player.x,dz=(double)p->position.z-g->player.z;
    return dx*dx+dz*dz<=4&&fabs(floor(p->position.y)-floor(g->player.y))<=2;
}
static bool ray_box(PeVec3 origin,PeVec3 direction,const PePlant *p,const PePlantSource *s,float *distance){
    double low=0,high=100,origins[]={origin.x,origin.y,origin.z},dirs[]={direction.x,direction.y,direction.z};
    double roots[]={p->position.x,p->position.y,p->position.z};
    double mins[]={s->minimum.x,s->minimum.y,s->minimum.z},maxs[]={s->maximum.x,s->maximum.y,s->maximum.z};
    for(unsigned i=0;i<3;i++){
        double lo=roots[i]+mins[i],hi=roots[i]+maxs[i];
        if(fabs(dirs[i])<1e-12){if(origins[i]<lo||origins[i]>hi)return false;continue;}
        double a=(lo-origins[i])/dirs[i],b=(hi-origins[i])/dirs[i];
        if(a>b){double swap=a;a=b;b=swap;}if(a>low)low=a;if(b<high)high=b;if(low>high)return false;
    }
    *distance=(float)low;return true;
}
bool pe_vegetation_pick(const PeGame *g,PeVec3 origin,PeVec3 direction,float obstacle,unsigned *index,float *distance){
    if(!alive(g)||!index||!distance||!pe_vegetation_state_valid(g)||!finite3(origin)||!finite3(direction)||isnan(obstacle)||obstacle<0)return false;
    double length=hypot(hypot((double)direction.x,direction.y),direction.z);if(length==0)return false;
    direction=(PeVec3){(float)(direction.x/length),(float)(direction.y/length),(float)(direction.z/length)};
    float best=fminf(obstacle,100);unsigned found=PE_MAX_PLANTS;
    for(unsigned i=0;i<g->vegetation.count;i++){
        const PePlant *p=&g->vegetation.plants[i];const PePlantSource *s=pe_vegetation_source(p->prototype_id);float hit;
        if(p->depleted||!s->harvestable||!reachable(g,p)||!ray_box(origin,direction,p,s,&hit)||hit>best)continue;
        if(found==PE_MAX_PLANTS||hit<best){best=hit;found=i;}
    }
    if(found==PE_MAX_PLANTS)return false;
    *index=found;*distance=best;return true;
}
static const char *harvest_plan(const PeGame *g,unsigned index,const PeSandboxCollision *collision,PeInventory *inventory,uint32_t *rng){
    if(!alive(g)||!pe_vegetation_state_valid(g)||index>=g->vegetation.count||!collision||!pe_sandbox_collision_stats(collision).generation)return "Gathering is unavailable.";
    const PePlant *p=&g->vegetation.plants[index];const PePlantSource *s=pe_vegetation_source(p->prototype_id);
    if(!s->harvestable)return "This foliage is decorative.";
    if(p->depleted)return "This plant has already been gathered.";
    if(!reachable(g,p))return "Move closer to gather this plant.";
    PeVec3 eye={g->player.x,g->player.y+1,g->player.z};
    PeVec3 target={p->position.x+(s->minimum.x+s->maximum.x)*.5f,p->position.y+(s->minimum.y+s->maximum.y)*.5f,p->position.z+(s->minimum.z+s->maximum.z)*.5f};
    PeVec3 delta={target.x-eye.x,target.y-eye.y,target.z-eye.z};
    double length=hypot(hypot((double)delta.x,delta.y),delta.z);
    if(length>0.01&&pe_sandbox_collision_ray_perception(collision,eye,delta,(float)length-.01f).hit)return "A surface blocks this plant.";
    *inventory=g->inventory;*rng=g->rng_state;
    if(!pe_harvest_grant(inventory,rng,p->prototype_id,1,1,0,false))return "Make room for gathered plants.";
    return NULL;
}
bool pe_vegetation_can_harvest(const PeGame *g,unsigned index,const PeSandboxCollision *collision){
    PeInventory inventory;uint32_t rng;return !harvest_plan(g,index,collision,&inventory,&rng);
}
bool pe_vegetation_harvest(PeGame *g,unsigned index,const PeSandboxCollision *collision){
    PeInventory inventory;uint32_t rng;const char *error=harvest_plan(g,index,collision,&inventory,&rng);
    if(error){if(g)snprintf(g->message,sizeof g->message,"%s",error);return false;}
    g->inventory=inventory;g->rng_state=rng;g->vegetation.plants[index].depleted=true;if(g->gathered<UINT32_MAX)g->gathered++;
    snprintf(g->message,sizeof g->message,"Gathered wild plant resources.");return true;
}
static bool resident_clear(const PeGame *g,PeVec3 p){
    if(pe_settlement_matches(&g->world)){
        for(unsigned i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++){
            double x=p.x+g->world.origin_x-pe_adventure_population_sources[i].position[0];
            double z=p.z+g->world.origin_z-pe_adventure_population_sources[i].position[2];
            if(x*x+z*z<9)return false;
        }
    }else{
        /* Nonreference Adventure hosts place their two opening actors around
         * these bounded fallback anchors; do not plant solid trunks there. */
        const PeVec3 anchors[]={{32,0,27},{28,0,35}};
        for(unsigned i=0;i<2;i++){double x=p.x-anchors[i].x,z=p.z-anchors[i].z;if(x*x+z*z<9)return false;}
    }
    return true;
}
static bool candidate(PeGame *g,const PeSandboxCollision *collision,PeVec3 anchor,float dx,float dz,bool tree,const PePlantSource *source,
                      const PeTree trees[2],unsigned tree_count,const PeVegetation *plants,PeVec3 *out){
    float x=anchor.x+dx,z=anchor.z+dz;
    if(x<2||z<2||x>pe_world_width(&g->world)-3||z>pe_world_depth(&g->world)-3)return false;
    float height=pe_world_height(&g->world,x,z);
    if(!isfinite(height)||height<=0||fabsf(height-anchor.y)>4)return false;
    PeVec3 root;
    if(!pe_sandbox_collision_settle(collision,(PeVec3){x,height+.25f,z},.15f,.5f,0,.75f,&root))return false;
    if(hypotf(root.x-g->player.x,root.z-g->player.z)<(tree?3:1)||!resident_clear(g,root))return false;
    PeTreeCapsule capsule=pe_tree_collision_source(root);
    if(tree){
        if(pe_tree_collision_overlap(&capsule,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT))return false;
        /* The authored capsule extends.5m below its root. Ground contact is
         * intentional; require a near-flat trunk footprint and clear upper
         * capsule instead of rejecting every trunk for its buried hemisphere. */
        for(int i=0;i<4;i++){
            float ox=i==0?1:i==1?-1:0,oz=i==2?1:i==3?-1:0;
            float h=pe_world_height(&g->world,root.x+ox,root.z+oz);
            if(!isfinite(h)||fabsf(h-root.y)>.35f)return false;
        }
        PeVec3 feet={capsule.center.x,root.y+.05f,capsule.center.z};
        float upper=capsule.center.y+capsule.height*.5f-feet.y;
        if(upper<2*capsule.radius||pe_sandbox_collision_overlap_geometry(collision,feet,capsule.radius,upper))return false;
    }else{
        float radius=hypotf(fmaxf(fabsf(source->minimum.x),fabsf(source->maximum.x)),fmaxf(fabsf(source->minimum.z),fabsf(source->maximum.z)));
        radius=fmaxf(radius,.15f);float height=fmaxf(2*radius,source->maximum.y+.05f);
        if(pe_sandbox_collision_overlap(collision,(PeVec3){root.x,root.y+.02f,root.z},radius,height))return false;
    }
    for(int i=0;i<g->tree_count;i++)if(g->trees[i].remaining>0&&hypotf(root.x-g->trees[i].position.x,root.z-g->trees[i].position.z)<(tree?3:1.8f))return false;
    for(unsigned i=0;i<tree_count;i++)if(hypotf(root.x-trees[i].position.x,root.z-trees[i].position.z)<(tree?3:1.8f))return false;
    for(unsigned i=0;i<plants->count;i++)if(hypotf(root.x-plants->plants[i].position.x,root.z-plants->plants[i].position.z)<1.2f)return false;
    *out=root;return true;
}
bool pe_vegetation_install_fixture(PeGame *g,PeVec3 anchor){
    if(!alive(g)||!finite3(anchor)||!pe_vegetation_state_valid(g))return false;
    if(g->vegetation.installed)return true;
    if(g->tree_count<0||g->tree_count>PE_MAX_TREES-2){snprintf(g->message,sizeof g->message,"No room for the two example trees.");return false;}
    PeSandboxCollision *collision=pe_sandbox_collision_create();char error[160];
    if(!collision||!pe_game_collision_update(g,collision,error,sizeof error)){pe_sandbox_collision_free(collision);return false;}
    PeTree trees[2]={0};PeVegetation plants={.installed=true};bool ok=true;
    const uint32_t prototypes[]={15,37,40,146,15,37,40,146};
    /* Fixed world-axis candidates, no PRNG or original-spawn substitution.
     * Trees search the outer ring; plants search the nearby rectangle. */
    for(unsigned object=0;object<10&&ok;object++){
        bool tree=object<2,found=false;PeVec3 position={0};
        const PePlantSource *source=tree?NULL:pe_vegetation_source(prototypes[object-2]);
        if(!tree&&!source){ok=false;break;}
        int limit=tree?12:8,start=tree?5:2;
        for(int radius=start;radius<=limit&&!found;radius+=2)
            for(int side=0;side<4&&!found;side++)for(int step=-radius;step<=radius&&!found;step+=2){
                float dx=side==0?radius:side==1?-radius:step;
                float dz=side==2?radius:side==3?-radius:step;
                if(candidate(g,collision,anchor,dx,dz,tree,source,trees,object<2?object:2,&plants,&position))found=true;
            }
        if(!found){ok=false;break;}
        if(tree)trees[object]=(PeTree){position,8,255};
        else plants.plants[plants.count++]=(PePlant){prototypes[object-2],position,false};
    }
    pe_sandbox_collision_free(collision);
    if(!ok){snprintf(g->message,sizeof g->message,"No clear supported area for the vegetation examples.");return false;}
    memcpy(g->trees+g->tree_count,trees,sizeof trees);g->tree_count+=2;g->vegetation=plants;
    snprintf(g->message,sizeof g->message,"Placed fixed examples: two trees, six herbs and two foliage plants.");return true;
}
