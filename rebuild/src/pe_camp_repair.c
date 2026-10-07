#include "pe_camp_repair.h"
#include "../generated/pe_camp_repair_source.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static const PeCampRepairItemSource *profile(int id){
    for(size_t i=0;i<sizeof pe_camp_repair_items/sizeof *pe_camp_repair_items;i++)
        if(pe_camp_repair_items[i].item_id==id)return &pe_camp_repair_items[i];
    return NULL;
}
static bool empty(PeSlot s){return !s.item_id&&!s.count&&!s.instance_id&&s.durability==0&&!signbit(s.durability);}
static bool same(PeSlot a,PeSlot b){return a.item_id==b.item_id&&a.count==b.count&&a.instance_id==b.instance_id&&a.durability==b.durability;}
static int64_t duration(void){return (int64_t)((double)PE_CAMP_REPAIR_BASE_SECONDS*PE_CAMP_REPAIR_TICKS_PER_SECOND);}
bool pe_camp_repair_item_supported(const PeSlot *s){
    if(!s||s->count!=1||!s->instance_id||s->instance_id==UINT32_MAX||!isfinite(s->durability)||s->durability<0)return false;
    const PeItemDef *d=pe_item_def(s->item_id);const PeCampRepairItemSource *p=profile(s->item_id);
    if(!d||!p||d->stack_limit!=1||d->tab<0||d->tab>=PE_INVENTORY_CATEGORIES||d->durability_max!=p->durability_max||s->durability>p->durability_max||p->durability_max<0||!p->material_count||p->material_count>PE_CAMP_REPAIR_COSTS)return false;
    for(unsigned i=0;i<p->material_count;i++){
        const PeItemDef *m=pe_item_def(p->materials[i].item_id);
        if(!m||m->stack_limit<=1||p->materials[i].count<=0)return false;
    }
    return true;
}
bool pe_camp_repair_quote(const PeSlot *s,PeCampRepairCost *out){
    if(!out||!pe_camp_repair_item_supported(s))return false;
    const PeCampRepairItemSource *p=profile(s->item_id);
    /* A zero maximum has no meaningful percent and source buttons disable it. */
    if(p->durability_max==0)return false;
    PeCampRepairCost q={.count=p->material_count};float missing=1.0f-s->durability/p->durability_max;
    for(unsigned i=0;i<q.count;i++){
        float amount=ceilf((float)p->materials[i].count*missing);
        if(!isfinite(amount)||amount<0||(double)amount>INT_MAX)return false;
        q.materials[i]=(PeIngredient){p->materials[i].item_id,(int)amount};
    }
    *out=q;return true;
}
bool pe_camp_repair_valid(const PeCampRepair *r,const PeCampRepairAccess *a){
    if(!r||!a||!a->inventory||!a->unbound||!pe_inventory_state_valid(a->inventory))return false;
    if(empty(r->item))return !r->running&&!r->remaining_ticks;
    if(!pe_camp_repair_item_supported(&r->item)||!a->inventory->next_instance_id||r->item.instance_id>=a->inventory->next_instance_id||!a->unbound(a->context,r->item.instance_id))return false;
    for(int i=0;i<pe_inventory_capacity(a->inventory);i++)if(a->inventory->slots[i].instance_id==r->item.instance_id)return false;
    if(!r->running)return r->remaining_ticks==0;
    return r->remaining_ticks>0&&r->remaining_ticks<=duration()&&profile(r->item.item_id)->durability_max-r->item.durability>=0x1p-23f;
}
bool pe_camp_repair_insert(PeCampRepair *r,const PeCampRepairAccess *a,int index,const PeSlot *expected){
    if(!expected||!pe_camp_repair_valid(r,a)||r->running||!pe_inventory_slot_valid(a->inventory,index))return false;
    PeSlot incoming=a->inventory->slots[index];
    if(!same(incoming,*expected)||!pe_camp_repair_item_supported(&incoming)||!a->unbound(a->context,incoming.instance_id))return false;
    PeInventory bag=*a->inventory;
    if(empty(r->item))memset(&bag.slots[index],0,sizeof bag.slots[index]);
    else if(pe_item_def(r->item.item_id)->tab==pe_item_def(incoming.item_id)->tab)bag.slots[index]=r->item;
    else{
        /* Match source cross-category return-before-removal admission. */
        if(!pe_inventory_insert(&bag,r->item,NULL))return false;
        memset(&bag.slots[index],0,sizeof bag.slots[index]);
    }
    *a->inventory=bag;*r=(PeCampRepair){.item=incoming};return true;
}
bool pe_camp_repair_start(PeCampRepair *r,const PeCampRepairAccess *a){
    if(!pe_camp_repair_valid(r,a)||r->running||empty(r->item))return false;
    const PeCampRepairItemSource *p=profile(r->item.item_id);
    if(p->durability_max-r->item.durability<0x1p-23f)return false;
    PeCampRepairCost q;if(!pe_camp_repair_quote(&r->item,&q))return false;
    PeInventory bag=*a->inventory;
    for(unsigned i=0;i<q.count;i++)if(q.materials[i].count&&!pe_inventory_remove(&bag,q.materials[i].item_id,q.materials[i].count))return false;
    *a->inventory=bag;r->running=true;r->remaining_ticks=duration();return true;
}
bool pe_camp_repair_cancel(PeCampRepair *r,const PeCampRepairAccess *a){
    if(!pe_camp_repair_valid(r,a)||!r->running)return false;
    r->running=false;r->remaining_ticks=0;return true;
}
bool pe_camp_repair_take(PeCampRepair *r,const PeCampRepairAccess *a,int *destination){
    if(!pe_camp_repair_valid(r,a)||r->running||empty(r->item))return false;
    PeInventory bag=*a->inventory;int where;
    if(!pe_inventory_insert(&bag,r->item,&where))return false;
    *a->inventory=bag;*r=(PeCampRepair){0};if(destination)*destination=where;return true;
}
bool pe_camp_repair_update(PeCampRepair *r,const PeCampRepairAccess *a,float dt,bool enabled,bool *completed){
    if(!pe_camp_repair_valid(r,a)||!isfinite(dt)||dt<0)return false;
    float ticks=dt*(float)PE_CAMP_REPAIR_TICKS_PER_SECOND;
    if(!isfinite(ticks)||(double)ticks>=0x1p63)return false;
    bool done=false;
    if(r->running&&enabled){
        /* Compare before conversion so ordinary overshoot completes once. */
        if((double)ticks>=(double)r->remaining_ticks){
            r->item.durability=profile(r->item.item_id)->durability_max;
            r->running=false;r->remaining_ticks=0;done=true;
        }else r->remaining_ticks-=(int64_t)ticks;
    }
    if(completed)*completed=done;
    return true;
}
float pe_camp_repair_seconds(const PeCampRepair *r){return r&&r->running?(float)((double)r->remaining_ticks/PE_CAMP_REPAIR_TICKS_PER_SECOND):0;}
bool pe_camp_repair_restore(PeCampRepair *r,const PeCampRepair *s,const PeCampRepairAccess *a){
    if(!r||!empty(r->item)||r->running||r->remaining_ticks||!pe_camp_repair_valid(s,a))return false;
    *r=*s;return true;
}
