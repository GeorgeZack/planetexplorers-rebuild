#include "pe_game.h"
#include "../generated/pe_inventory_source.h"
#include <limits.h>
#include <math.h>
#include <string.h>

_Static_assert(PE_INVENTORY_CATEGORY_SLOTS==PE_INVENTORY_SOURCE_CAPACITY,"source package capacity");
_Static_assert(PE_INVENTORY_CATEGORIES==4,"source slot types");
int pe_inventory_capacity(const PeInventory *p){return p?(p->categorized?PE_INVENTORY_MAX_SLOTS:PE_INVENTORY_SLOTS):0;}
int pe_inventory_category_start(const PeInventory *p,int category){
    return !p||category<0||category>=PE_INVENTORY_CATEGORIES?-1:(p->categorized?category*PE_INVENTORY_CATEGORY_SLOTS:0);
}
int pe_inventory_category_capacity(const PeInventory *p,int category){
    return pe_inventory_category_start(p,category)<0?0:(p->categorized?PE_INVENTORY_CATEGORY_SLOTS:PE_INVENTORY_SLOTS);
}
bool pe_inventory_slot_valid(const PeInventory *p,int index){return index>=0&&index<pe_inventory_capacity(p);}
bool pe_inventory_item_discardable(int id){
    if(id<=0||id/10000000==9)return false;
    size_t count=sizeof pe_inventory_source_items/sizeof *pe_inventory_source_items;
    for(size_t i=0;i<count;i++)if(pe_inventory_source_items[i].id==id)return !pe_inventory_source_items[i].quest_item;
    return false;
}
static bool valid_slot(PeSlot s){
    if(!s.count)return s.item_id==0&&s.instance_id==0&&s.durability==0&&!signbit(s.durability);
    const PeItemDef *d=pe_item_def(s.item_id);
    return d&&d->tab>=0&&d->tab<PE_INVENTORY_CATEGORIES&&s.count>0&&s.count<=d->stack_limit&&
        isfinite(s.durability)&&s.durability>=0&&s.durability<=d->durability_max&&
        (d->stack_limit==1?(s.instance_id>0&&s.count==1):(s.instance_id==0&&s.durability==0));
}
static bool valid_at(const PeInventory *p,int i){
    return pe_inventory_slot_valid(p,i)&&valid_slot(p->slots[i])&&(!p->categorized||!p->slots[i].count||pe_item_def(p->slots[i].item_id)->tab==i/PE_INVENTORY_CATEGORY_SLOTS);
}
bool pe_inventory_state_valid(const PeInventory *p){
    if(!p)return false;
    for(int i=0;i<PE_INVENTORY_MAX_SLOTS;i++){
        const PeSlot *s=&p->slots[i];
        if(!valid_slot(*s)||(i>=pe_inventory_capacity(p)&&s->count))return false;
        if(i<pe_inventory_capacity(p)&&!valid_at(p,i))return false;
        if(s->instance_id){
            if(!p->next_instance_id||s->instance_id>=p->next_instance_id)return false;
            for(int j=0;j<i;j++)if(p->slots[j].instance_id==s->instance_id)return false;
        }
    }
    return true;
}
int pe_inventory_first_empty(const PeInventory *p,int id){
    const PeItemDef *d=pe_item_def(id);if(!p||!d)return -1;
    int start=pe_inventory_category_start(p,d->tab),end=start+pe_inventory_category_capacity(p,d->tab);
    for(int i=start;i<end;i++)if(!p->slots[i].count&&valid_at(p,i))return i;
    return -1;
}
bool pe_inventory_insert(PeInventory *p,PeSlot s,int *destination){
    if(!p||!valid_slot(s)||!s.count)return false;
    int index=pe_inventory_first_empty(p,s.item_id);if(index<0)return false;
    uint32_t next=p->next_instance_id?p->next_instance_id:1;
    if(s.instance_id){
        if(s.instance_id==UINT32_MAX)return false;
        for(int i=0;i<pe_inventory_capacity(p);i++)if(p->slots[i].instance_id==s.instance_id)return false;
        if(s.instance_id>=next)next=s.instance_id+1;
    }
    p->slots[index]=s;p->next_instance_id=next;if(destination)*destination=index;return true;
}
bool pe_inventory_enable_categories(PeInventory *p){
    if(!p)return false;
    if(p->categorized)return pe_inventory_state_valid(p);
    if(!pe_inventory_state_valid(p))return false;
    PeInventory next={.next_instance_id=p->next_instance_id,.categorized=true};
    int counts[PE_INVENTORY_CATEGORIES]={0};
    for(int i=0;i<PE_INVENTORY_MAX_SLOTS;i++){
        PeSlot s=p->slots[i];if(!valid_slot(s)||(i>=PE_INVENTORY_SLOTS&&s.count))return false;
        if(s.count){int tab=pe_item_def(s.item_id)->tab;next.slots[tab*PE_INVENTORY_CATEGORY_SLOTS+counts[tab]++]=s;}
    }
    *p=next;return true;
}
int pe_inventory_count(const PeInventory *p,int id){
    if(!p)return 0;
    int n=0;
    for(int i=0;i<pe_inventory_capacity(p);i++)if(p->slots[i].item_id==id){
        if(p->slots[i].count<0||n>INT_MAX-p->slots[i].count)return INT_MAX;
        n+=p->slots[i].count;
    }
    return n;
}
bool pe_inventory_add(PeInventory *p,int id,int count){
    const PeItemDef *d=pe_item_def(id);
    if(!p||!d||d->stack_limit<1||d->tab<0||d->tab>=PE_INVENTORY_CATEGORIES||count<=0)return false;
    PeInventory copy=*p;
    if(!copy.next_instance_id)copy.next_instance_id=1;
    int start=pe_inventory_category_start(p,d->tab),end=start+pe_inventory_category_capacity(p,d->tab),left=count;
    if(d->stack_limit==1){
        for(int i=start;i<end&&left;i++)if(!copy.slots[i].count){
            if(!valid_at(&copy,i)||copy.next_instance_id==UINT32_MAX)return false;
            copy.slots[i]=(PeSlot){id,1,copy.next_instance_id++,(float)d->durability_max};--left;
        }
    }else{
        for(int pass=0;pass<2;pass++)for(int i=start;i<end&&left;i++){
            PeSlot *s=&copy.slots[i];
            if((pass==0&&s->item_id!=id)||(pass==1&&s->count))continue;
            if(!valid_at(&copy,i))return false;
            int take=d->stack_limit-s->count;if(take>left)take=left;
            s->item_id=id;s->count+=take;left-=take;
        }
    }
    if(left)return false;
    *p=copy;return true;
}
bool pe_inventory_remove(PeInventory *p,int id,int count){
    if(!p||count<=0||!pe_item_def(id)||pe_inventory_count(p,id)<count)return false;
    PeInventory copy=*p;
    for(int i=0;i<pe_inventory_capacity(p)&&count;i++){
        PeSlot *s=&copy.slots[i];if(s->item_id!=id)continue;
        if(!valid_at(&copy,i))return false;
        int take=s->count<count?s->count:count;s->count-=take;count-=take;
        if(!s->count)memset(s,0,sizeof *s);
    }
    *p=copy;return true;
}
bool pe_inventory_split(PeInventory *p,int from,int count,int *destination){
    if(!p||!valid_at(p,from)||count<1)return false;
    PeSlot s=p->slots[from];const PeItemDef *d=pe_item_def(s.item_id);
    if(!d||d->stack_limit<=1||count>=s.count)return false;
    int start=pe_inventory_category_start(p,d->tab),end=start+pe_inventory_category_capacity(p,d->tab);
    for(int i=start;i<end;i++)if(!p->slots[i].count){
        if(!valid_at(p,i))return false;
        p->slots[i]=(PeSlot){.item_id=s.item_id,.count=count};p->slots[from].count-=count;
        if(destination)*destination=i;
        return true;
    }
    return false;
}
bool pe_inventory_move(PeInventory *p,int from,int to){
    if(!p||!valid_at(p,from)||!valid_at(p,to)||!p->slots[from].count||from==to)return false;
    if(p->categorized&&from/PE_INVENTORY_CATEGORY_SLOTS!=to/PE_INVENTORY_CATEGORY_SLOTS)return false;
    PeSlot tmp=p->slots[from];p->slots[from]=p->slots[to];p->slots[to]=tmp;return true;
}
bool pe_inventory_merge(PeInventory *p,int from,int to){
    if(!p||!valid_at(p,from)||!valid_at(p,to)||from==to||!p->slots[from].count||p->slots[from].item_id!=p->slots[to].item_id)return false;
    const PeItemDef *d=pe_item_def(p->slots[from].item_id);
    if(d->stack_limit<=1||p->slots[to].count==d->stack_limit)return false;
    int take=d->stack_limit-p->slots[to].count;if(take>p->slots[from].count)take=p->slots[from].count;
    p->slots[to].count+=take;p->slots[from].count-=take;
    if(!p->slots[from].count)memset(&p->slots[from],0,sizeof p->slots[from]);
    return true;
}
static int sort_label(int id){
    size_t lo=0,hi=sizeof pe_inventory_source_items/sizeof *pe_inventory_source_items;
    while(lo<hi){size_t mid=lo+(hi-lo)/2;if(pe_inventory_source_items[mid].id<id)lo=mid+1;else hi=mid;}
    return lo<sizeof pe_inventory_source_items/sizeof *pe_inventory_source_items&&pe_inventory_source_items[lo].id==id?pe_inventory_source_items[lo].sort_label:INT_MAX;
}
static int compare(PeSlot a,PeSlot b){
    int x=sort_label(a.item_id),y=sort_label(b.item_id);
    if(x!=y)return x<y?-1:1;
    if(a.item_id!=b.item_id)return a.item_id<b.item_id?-1:1;
    return (a.count>b.count)-(a.count<b.count);
}
bool pe_inventory_sort(PeInventory *p,int category){
    int start=pe_inventory_category_start(p,category),capacity=pe_inventory_category_capacity(p,category);
    if(start<0)return false;
    PeSlot sorted[PE_INVENTORY_CATEGORY_SLOTS]={0};int count=0;
    /* Legacy shares one physical list: leave other tabs in place. */
    int positions[PE_INVENTORY_CATEGORY_SLOTS],npositions=0;
    for(int i=start;i<start+capacity;i++){
        if(!valid_at(p,i))return false;
        PeSlot s=p->slots[i];if(s.count&&pe_item_def(s.item_id)->tab!=category)continue;
        positions[npositions++]=i;if(!s.count)continue;
        const PeItemDef *d=pe_item_def(s.item_id);
        if(sort_label(s.item_id)==INT_MAX)return false;
        if(d->stack_limit>1)for(int j=0;j<count&&s.count;j++)if(sorted[j].item_id==s.item_id){
            int take=d->stack_limit-sorted[j].count;if(take>s.count)take=s.count;
            sorted[j].count+=take;s.count-=take;
        }
        if(s.count)sorted[count++]=s;
    }
    for(int i=1;i<count;i++){PeSlot s=sorted[i];int j=i;while(j>0&&compare(s,sorted[j-1])<0){sorted[j]=sorted[j-1];--j;}sorted[j]=s;}
    for(int i=0;i<npositions;i++)p->slots[positions[i]]=i<count?sorted[i]:(PeSlot){0};
    return true;
}
