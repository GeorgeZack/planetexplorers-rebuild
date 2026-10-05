#include "pe_training_gathering.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
static const int missions[]={719,748,757,721,751,752};
static const PeVec3 herbs[]={{23.277050f,1.882331f,5.659585f},{15.256583f,1.882331f,10.406357f},{9.546638f,1.882331f,16.267000f},{8.878967f,1.882331f,4.358108f},{21.384402f,1.882331f,16.221410f}};
static int index_of(int mission){for(int i=0;i<6;i++)if(missions[i]==mission)return i;return -1;}
static bool valid(const PeTrainingGathering*s){
    if(!s||s->wood>PE_GATHERING_ITEM_LIMIT||s->axe>PE_GATHERING_ITEM_LIMIT||s->script>PE_GATHERING_ITEM_LIMIT||s->shovel>PE_GATHERING_ITEM_LIMIT||s->dirt>PE_GATHERING_ITEM_LIMIT||s->accepted>63||s->herbs_depleted>5||s->tree_hp>7||s->tree_stage<PE_TREE_IDLE||s->tree_stage>PE_TREE_EXIT||!isfinite(s->tree_time)||s->tree_time<0||s->tree_time>60||!isfinite(s->craft_time)||s->craft_time<0||s->craft_time>3)return false;
    if(s->removed_count>3)return false;
    int i=index_of(s->mission);
    if(s->mission?(i<0||s->accepted!=(1u<<(i+1))-1):s->accepted!=0)return false;
    if(s->crafting&&(s->mission!=721||!s->formula1||s->shovel==PE_GATHERING_ITEM_LIMIT))return false;
    if(s->tree_stage!=PE_TREE_IDLE&&s->mission!=748)return false;
    unsigned dead=0;for(unsigned k=0;k<5;k++){if(!isfinite(s->herb_time[k])||s->herb_time[k]<0||s->herb_time[k]>1.966||(s->herb_active[k]&&s->mission!=719))return false;dead+=s->herb_dead[k]?1u:0u;}
    return dead==s->herbs_depleted;
}
static uint32_t *slot(PeTrainingGathering*s,int id){switch(id){case 279:return &s->wood;case 1527:return &s->axe;case 406:return &s->script;case 12:return &s->shovel;case 268:return &s->dirt;default:return NULL;}}
static void possession(PeTrainingGathering*s){if((s->mission==721&&s->shovel)||(s->mission==751&&s->dirt>=5))s->complete=true;}
void pe_training_gathering_init(PeTrainingGathering*s){if(s)memset(s,0,sizeof *s);}
int pe_training_gathering_next(const PeTrainingGathering*s){if(!valid(s)||!s->complete)return 0;int i=index_of(s->mission);return i<0?0:i==5?753:missions[i+1];}
bool pe_training_gathering_enter(PeTrainingGathering*s,int mission){
    if(!valid(s))return false;
    int i=index_of(mission);if(i<0)return false;
    if(s->mission==mission)return false;
    if((s->mission?pe_training_gathering_next(s)!=mission:mission!=719)||(s->accepted&(1u<<i))||s->crafting)return false;
    PeTrainingGathering next=*s;uint32_t *count=NULL,amount=0;
    switch(mission){case 748:count=&next.axe;amount=1;break;case 757:count=&next.script;amount=1;break;case 721:count=&next.wood;amount=20;break;case 751:count=&next.shovel;amount=1;break;case 752:count=&next.dirt;amount=9000;break;}
    if(count){if(*count>PE_GATHERING_ITEM_LIMIT-amount)return false;*count+=amount;}
    next.mission=mission;next.accepted|=1u<<i;next.complete=false;next.tree_stage=PE_TREE_IDLE;next.tree_time=0;next.tree_hit=false;next.tree_release=false;
    for(unsigned k=0;k<5;k++)next.herb_active[k]=false;
    if(mission==748)next.tree_hp=7;
    possession(&next);*s=next;return true;
}
uint32_t pe_training_gathering_count(const PeTrainingGathering*s,int item){if(!valid(s))return 0;PeTrainingGathering copy=*s;uint32_t*p=slot(&copy,item);return p?*p:0;}
bool pe_training_gathering_acquire(PeTrainingGathering*s,int item,uint32_t count){
    if(!valid(s)||!count||count>PE_GATHERING_ITEM_LIMIT)return false;
    uint32_t*p=slot(s,item);if(!p||*p>PE_GATHERING_ITEM_LIMIT-count||(item==12&&s->crafting&&*p>=PE_GATHERING_ITEM_LIMIT-count))return false;
    *p+=count;possession(s);return true;
}
PeVec3 pe_training_gathering_herb_position(unsigned i){return i<5?herbs[i]:(PeVec3){NAN,NAN,NAN};}
bool pe_training_gathering_begin_herb(PeTrainingGathering*s,unsigned i,PeVec3 p){
    if(!valid(s)||s->mission!=719||s->complete||i>=5||s->herb_dead[i]||s->herb_active[i]||!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z))return false;
    double x=(double)p.x-herbs[i].x,y=(double)p.y-herbs[i].y,z=(double)p.z-herbs[i].z;
    if(x*x+y*y+z*z>=7)return false;
    s->herb_time[i]=0;s->herb_active[i]=true;return true;
}
bool pe_training_gathering_learn_script(PeTrainingGathering*s){
    if(!valid(s)||s->mission!=757||s->complete||s->formula1||!s->script)return false;
    s->script--;s->formula1=true;s->complete=true;return true;
}
bool pe_training_gathering_start_craft(PeTrainingGathering*s){
    if(!valid(s)||s->mission!=721||s->complete||s->crafting||!s->formula1||s->wood<10||s->shovel>=PE_GATHERING_ITEM_LIMIT)return false;
    s->wood-=10;s->crafting=true;s->craft_time=0;return true;
}
bool pe_training_gathering_removed_voxel(PeTrainingGathering*s,const PeWorld*before,const PeWorld*after,int x,int y,int z){
    if(!valid(s)||s->mission!=751||s->complete||s->removed_count>=3||!before||!after||x<0||x>=PE_WORLD_X||y<0||y>=PE_WORLD_Y||z<0||z>=PE_WORLD_Z||
       before->source!=1||before->seed!=after->seed||before->source!=after->source||before->origin_x!=after->origin_x||before->origin_y!=after->origin_y||before->origin_z!=after->origin_z||before->revision==after->revision)return false;
    size_t voxel=((size_t)z*PE_WORLD_Y+(size_t)y)*PE_WORLD_X+(size_t)x;
    PeVoxel old=before->voxels[voxel],now=after->voxels[voxel];
    if(!old.volume||old.material!=1||now.volume)return false;
    int64_t coordinate[3]={(int64_t)before->origin_x+x,(int64_t)before->origin_y+y,(int64_t)before->origin_z+z};
    for(unsigned i=0;i<s->removed_count;i++)if(!memcmp(coordinate,s->removed[i],sizeof coordinate))return false;
    PeTrainingGathering next=*s;if(!pe_training_gathering_acquire(&next,268,2))return false;
    memcpy(next.removed[next.removed_count++],coordinate,sizeof coordinate);*s=next;return true;
}
bool pe_training_gathering_enter_build_mode(PeTrainingGathering*s){if(!valid(s)||s->mission!=752||s->complete)return false;s->complete=true;return true;}
bool pe_training_gathering_movement_locked(const PeTrainingGathering*s){
    if(!valid(s))return false;
    for(unsigned k=0;k<5;k++)if(s->herb_active[k]&&!s->herb_dead[k])return true;
    return s->tree_stage!=PE_TREE_IDLE;
}
static void tree_step(PeTrainingGathering*s,double dt,PeGatheringInput input){
    if(s->tree_stage==PE_TREE_IDLE){
        if(s->complete||!input.cut_held||!input.axe_present||!input.equipment_held||!input.tree_raycast_hit)return;
        s->tree_stage=PE_TREE_WAIT;s->tree_time=0;s->tree_release=false;
    }
    if(!input.cut_held)s->tree_release=true;
    if(s->tree_stage==PE_TREE_WAIT){
        /* Source !cutting can re-arm Fell while waiting, but not after entry. */
        if(input.cut_held&&input.axe_present&&input.equipment_held&&input.tree_raycast_hit)s->tree_release=false;
        if(!input.animator_do_nothing)return;
        s->tree_stage=PE_TREE_ENTRY;return;
    }
    /* Event-boundary stepping retains held-release semantics and cannot skip
     * a source hit under a long frame. At most7 productive hits can occur. */
    for(unsigned guard=0;guard<256;guard++){
        double boundary=s->tree_stage==PE_TREE_ENTRY?.6:s->tree_stage==PE_TREE_EXIT?.533:s->tree_hit?1.1:.6;
        double remaining=fmax(0,boundary-s->tree_time);
        if(dt<remaining||(s->tree_stage==PE_TREE_ENTRY&&dt==remaining)){s->tree_time+=dt;return;}
        dt-=remaining;s->tree_time=boundary;
        if(s->tree_stage==PE_TREE_ENTRY){s->tree_stage=PE_TREE_CUT;s->tree_time=0;s->tree_hit=false;}
        else if(s->tree_stage==PE_TREE_EXIT){s->tree_stage=PE_TREE_IDLE;s->tree_time=0;return;}
        else if(!s->tree_hit){
            if(!input.axe_present){s->tree_stage=PE_TREE_IDLE;s->tree_time=0;return;}
            /* A blocked reward spends neither HP nor wood, but consumes this
             * loop's hit opportunity so release can still reach EXIT. */
            if(s->tree_hp&&!(s->tree_hp==1&&s->wood==PE_GATHERING_ITEM_LIMIT)){
                s->tree_hp--;if(!s->tree_hp){s->wood++;s->complete=true;}
            }
            s->tree_hit=true;
        }else{
            s->tree_time=0;s->tree_hit=false;
            if(s->tree_release||!s->tree_hp)s->tree_stage=PE_TREE_EXIT;
        }
        if(dt<=0)return;
    }
}
bool pe_training_gathering_step(PeTrainingGathering*s,double dt,PeGatheringInput input){
    if(!valid(s)||!isfinite(dt)||dt<0||dt>60)return false;
    if(dt==0)return true;
    PeTrainingGathering next=*s;
    if(next.mission==719)for(unsigned k=0;k<5;k++)if(next.herb_active[k]){
        next.herb_time[k]=fmin(1.966,next.herb_time[k]+dt);
        if(!next.herb_dead[k]&&next.herb_time[k]>=1.1){next.herb_dead[k]=true;next.herbs_depleted++;if(next.herbs_depleted==5)next.complete=true;}
        if(next.herb_time[k]>=1.966)next.herb_active[k]=false;
    }
    if(next.mission==748)tree_step(&next,dt,input);
    if(next.crafting){next.craft_time=fmin(3,next.craft_time+dt);if(next.craft_time>=3){if(next.shovel==PE_GATHERING_ITEM_LIMIT)return false;next.shovel++;next.crafting=false;possession(&next);}}
    *s=next;return true;
}
