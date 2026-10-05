#include "pe_training_dig.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double loop_length(const PeTrainingDig*s){return s->male?1.6000003814697266:1.3333358764648438;}
static double change_time(const PeTrainingDig*s){return s->male?.12866613268852234:.04880186542868614;}
static double hit_time(const PeTrainingDig*s){return s->male?.6481015086174011:.6035147905349731;}
static bool vector_valid(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static bool valid(const PeTrainingDig*s){
    if(!s)return false;
    if(s->stage==PE_DIG_IDLE&&s->seconds!=0)return false;
    if(s->stage==PE_DIG_ENTRY&&s->seconds>.6999969482421875*.6)return false;
    if(s->stage==PE_DIG_EXIT&&s->seconds>.8333358764648438)return false;
    if(s->stage==PE_DIG_LOOP&&(s->change_count*loop_length(s)+change_time(s)<s->seconds-1e-9||
       s->hit_count*loop_length(s)+hit_time(s)<s->seconds-1e-9))return false;
    return s->stage>=PE_DIG_IDLE&&s->stage<=PE_DIG_EXIT&&isfinite(s->seconds)&&s->seconds>=0&&s->seconds<=1000000&&
        isfinite(s->exit_seconds)&&s->exit_seconds>=0&&s->exit_seconds<=1000002&&isfinite(s->speed)&&s->speed>=.5f&&s->speed<=1.5f&&
        s->change_count<=1000000&&s->hit_count<=1000000;
}
void pe_training_dig_init(PeTrainingDig*s,bool male){if(s){memset(s,0,sizeof*s);s->male=male;s->speed=1;}}
void pe_training_dig_cancel(PeTrainingDig*s){if(s){bool male=s->male;uint64_t swings=s->swings;pe_training_dig_init(s,male);s->swings=swings;}}
const char *pe_training_dig_clip(const PeTrainingDig*s){
    if(!valid(s))return NULL;
    return s->stage==PE_DIG_ENTRY?"Spade1":s->stage==PE_DIG_LOOP?"Spade2":s->stage==PE_DIG_EXIT?"Spade3":NULL;
}
double pe_training_dig_clip_seconds(const PeTrainingDig*s){return valid(s)?s->stage==PE_DIG_LOOP?fmod(s->seconds,loop_length(s)):s->seconds:0;}
static PeVec3 local(const PeWorld*w,PeVec3 p){return(PeVec3){p.x-w->origin_x,p.y-w->origin_y,p.z-w->origin_z};}
static PeHit target(const PeWorld*w,PeTrainingDigInput input){return pe_world_dig_target(w,local(w,input.ray_origin),input.ray_direction,local(w,input.player),3);}
static void set_target(PeTrainingDig*s,PeHit h){s->target_valid=h.hit;if(h.hit){s->target_x=h.x;s->target_y=h.y;s->target_z=h.z;}}
static void charge(PeTrainingDig*s,float*stamina){
    *stamina=fmaxf(0,*stamina-4);s->speed=1;
    if(*stamina<=40)s->speed*=.9f;
    if(*stamina<=20)s->speed*=.9f;
}
static bool strike(PeTrainingDig*s,PeWorld*w,PeTrainingGathering*inventory,PeTrainingShovel*tool,
                   float*stamina,PeHit current,PeTrainingDigUpdate*out){
    if(!s->first){if(!current.hit)return true;set_target(s,current);}
    if(!s->target_valid)return true;
    int x=s->target_x,y=s->target_y,z=s->target_z;
    PeVoxel old=pe_world_get(w,x,y,z),changed=old;
    if(old.volume){
        if(old.material!=1)return false; /* Audited lesson material only. */
        /* Source material1 duration.21167, attack(10+23)*5. Hardness reduction
         * is inactive: hpP .0646478 - damageP .1294118 < ReduceF .33. */
        float damage=165.0f*.21167f;
        changed.volume=old.volume>damage?(uint8_t)(old.volume-(uint8_t)damage):0;
        if(changed.volume<=127)changed=(PeVoxel){0,0};
        if(w->revision==UINT32_MAX)return false;
        PeWorld *next=malloc(sizeof*next);if(!next)return false;*next=*w;
        if(!pe_world_set(next,x,y,z,changed)){free(next);return false;}
        PeTrainingGathering ledger=*inventory;
        if(!changed.volume&&!pe_training_gathering_removed_voxel(&ledger,w,next,x,y,z)){
            free(next);out->capacity_blocked=true;s->first=false;return true;
        }
        *w=*next;free(next);*inventory=ledger;
        out->removed=!changed.volume;out->dirt_added=out->removed?2:0;
    }
    tool->durability=fmaxf(0,tool->durability-5);
    out->swung=true;out->x=x;out->y=y;out->z=z;s->swings++;s->first=false;
    if(tool->durability<=0)s->ending=true;
    if(!s->ending)charge(s,stamina);
    return true;
}
bool pe_training_dig_step(PeTrainingDig*s,PeWorld*w,PeTrainingGathering*inventory,
    PeTrainingShovel*tool,float*stamina,PeTrainingDigInput input,PeTrainingDigUpdate*out){
    if(!valid(s)||!out)return false;
    if(!input.enabled){bool active=s->stage!=PE_DIG_IDLE;pe_training_dig_cancel(s);*out=(PeTrainingDigUpdate){.finished=active};return true;}
    if(!w||w->source!=1||!inventory||inventory->mission!=751||!tool||!tool->instance_id||!stamina||
       !isfinite(*stamina)||*stamina<0||*stamina>100||!isfinite(tool->durability)||tool->durability<0||tool->durability>3553||
       !vector_valid(input.ray_origin)||!vector_valid(input.ray_direction)||!vector_valid(input.player)||
       hypot(hypot(input.ray_direction.x,input.ray_direction.y),input.ray_direction.z)<1e-12)return false;
    PeTrainingGathering checked=*inventory;
    if(!pe_training_gathering_step(&checked,0,(PeGatheringInput){0}))return false;
    PeTrainingDig next=*s;PeTrainingShovel next_tool=*tool;float next_stamina=*stamina;
    PeTrainingDigUpdate result={0};PeHit current=target(w,input);
    next.stamina_lock=next.stage!=PE_DIG_IDLE;
    if(next.stage==PE_DIG_IDLE){
        if(!input.held||!current.hit||tool->durability<=0||inventory->complete){*out=result;*s=next;return true;}
        next.stage=PE_DIG_ENTRY;next.seconds=0;next.ending=false;next.first=true;next.change_count=next.hit_count=0;
        set_target(&next,current);charge(&next,&next_stamina);result.started=true;next.stamina_lock=true;
    }
    if(tool->durability<=0){pe_training_dig_cancel(&next);result.finished=true;*s=next;*out=result;return true;}
    if(!input.held&&!next.ending){next.ending=true;next.exit_seconds=0;}
    double remaining=1.0/60.0;
    /* At most one hit per fixed tick; no later fallible operation follows a
     * committed strike. All validation and sample staging precede mutation. */
    for(unsigned guard=0;guard<8&&remaining>0;guard++){
        double rate=next.speed*(next.stage==PE_DIG_ENTRY?2:1),boundary;
        enum { ENTRY_END,CHANGE,HIT,LOOP_END,EXIT_END } event;
        if(next.stage==PE_DIG_ENTRY){boundary=.6999969482421875*.6;event=ENTRY_END;}
        else if(next.stage==PE_DIG_EXIT){boundary=.8333358764648438;event=EXIT_END;}
        else{
            double length=loop_length(&next);
            double change=next.change_count*length+change_time(&next),hit=next.hit_count*length+hit_time(&next);
            if(change<hit){boundary=change;event=CHANGE;}else{boundary=hit;event=HIT;}
            if(next.ending){
                if(!next.exit_seconds){next.exit_seconds=(floor(next.seconds/length)+.8)*length;if(next.exit_seconds<next.seconds)next.exit_seconds+=length;}
                if(next.exit_seconds<boundary){boundary=next.exit_seconds;event=LOOP_END;}
            }
        }
        double elapsed=fmax(0,(boundary-next.seconds)/rate);
        if(elapsed>remaining){next.seconds+=remaining*rate;remaining=0;break;}
        remaining-=elapsed;next.seconds=boundary;
        if(event==ENTRY_END){next.stage=PE_DIG_LOOP;next.seconds=0;next.exit_seconds=0;}
        else if(event==CHANGE){if(!next.first&&current.hit)set_target(&next,current);next.change_count++;}
        else if(event==HIT){
            if(!strike(&next,w,inventory,&next_tool,&next_stamina,current,&result))return false;
            next.first=false;next.hit_count++;
        }else if(event==LOOP_END){next.stage=PE_DIG_EXIT;next.seconds=0;}
        else{pe_training_dig_cancel(&next);next.stamina_lock=true;result.finished=true;break;}
    }
    *s=next;*tool=next_tool;*stamina=next_stamina;*out=result;return true;
}
