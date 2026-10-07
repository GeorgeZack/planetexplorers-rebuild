#include "pe_harvest.h"
#include "../generated/pe_harvest_source.h"
#include <math.h>
#include <limits.h>
#include <string.h>
#define PE_HARVEST_MAX_TRIALS 65536u
const PeHarvestSource *pe_harvest_source(uint32_t id){
    for(unsigned i=0;i<sizeof pe_harvest_sources/sizeof *pe_harvest_sources;i++)if(pe_harvest_sources[i].prototype_id==id)return &pe_harvest_sources[i];
    return NULL;
}
static bool scales(float w,float h){return isfinite(w)&&isfinite(h)&&w>0&&h>0&&w<=64&&h<=64&&isfinite(w*h)&&w*h>0;}
bool pe_harvest_damage(uint32_t id,float w,float h,float hp,float damage,float *remaining){
    const PeHarvestSource *s=pe_harvest_source(id);
    if(!s||!remaining||!scales(w,h)||!isfinite(hp)||hp<0||hp>255||!isfinite(damage)||damage<0)return false;
    float taken=damage*s->duration/(h*w);if(!isfinite(taken))return false;
    *remaining=fmaxf(0,hp-taken);return true;
}
static uint32_t next(uint32_t *state){
    uint32_t x=*state?*state:UINT32_C(0x9e3779b9);x^=x<<13;x^=x>>17;x^=x<<5;return *state=x;
}
static bool trial(PeInventory *bag,uint32_t *rng,const PeHarvestDrop *drops,unsigned count){
    unsigned roll=next(rng)%100u;
    for(unsigned i=0;i<count;i++)if(roll<drops[i].threshold)return pe_inventory_add(bag,drops[i].item_id,1);
    return true;
}
static bool extras(PeInventory *bag,uint32_t *rng,const PeHarvestExtra *extra,float quantity){
    if(extra->chance<=0)return true;
    float trials=quantity*extra->chance;
    if(!isfinite(trials)||trials>PE_HARVEST_MAX_TRIALS)return false;
    /* Unity Random.value includes both endpoints. Keep a draw even when the
     * gate exceeds1; this matters for deterministic retries and later rolls. */
    float gate=(float)(next(rng)>>8)*(1.0f/16777215.0f);
    if(gate<trials)for(unsigned i=0;(float)i<trials;i++)if(!trial(bag,rng,extra->drops,extra->count))return false;
    return true;
}
bool pe_harvest_grant(PeInventory *inventory,uint32_t *rng,uint32_t id,float w,float h,float bonus,bool rare){
    const PeHarvestSource *s=pe_harvest_source(id);
    if(!inventory||!rng||!s||!scales(w,h)||!isfinite(bonus)||bonus<0)return false;
    float quantity=s->fixed_count>0?s->fixed_count:s->self_count*w*w*h*(1+bonus);
    quantity=fmaxf(1,quantity);
    if(!isfinite(quantity)||quantity>PE_HARVEST_MAX_TRIALS)return false;
    PeInventory staged=*inventory;uint32_t state=*rng;
    for(unsigned i=0;i<(unsigned)quantity;i++)if(!trial(&staged,&state,s->primary,s->primary_count))return false;
    if(!extras(&staged,&state,&s->optional,quantity)||(rare&&!extras(&staged,&state,&s->rare,quantity)))return false;
    *inventory=staged;*rng=state;return true;
}
