#include "pe_sandbox_actions.h"
#include <math.h>
#include <string.h>

/* Installed sharedassets1 FemaleAnimator2(779)/MaleAnimator(780), inventoried
 * in sandbox-action-animation.json; source Action_DigTerrain/Action_Fell.
 * The exact authored events do not establish Unity transition-blend parity. */
typedef struct {
    int item_id;
    PeSandboxActionKind kind;
    float speed,slow_threshold;
    double entry_rate,change[2],hit[2],loop[2]; /* female, male authored seconds */
    const char *clips[4];
} ToolTiming;
static const ToolTiming timings[]={
    {12,PE_SANDBOX_ACTION_DIG,1,40,2,{.04880186542868614,.12866613268852234},
        {.6035147905349731,.6481015086174011},{1.3333358764648438,1.6000003814697266},{NULL,"Spade1","Spade2","Spade3"}},
    {19,PE_SANDBOX_ACTION_DIG,1,50,1.5,{.16452839970588684,.17536334693431854},
        {.838300883769989,.9432708024978638},{1.3333358764648438,1.6000022888183594},{NULL,"Mine1","Mine2","Mine3"}},
    {26,PE_SANDBOX_ACTION_FELL,1,50,1,{0,0},
        {.612334668636322,.3177316188812256},{1.100006103515625,1.0000038146972656},{NULL,"Felling1","Felling2","Felling3"}},
    {1527,PE_SANDBOX_ACTION_FELL,1.2200000286102295f,50,1,{0,0},
        {.612334668636322,.3177316188812256},{1.100006103515625,1.0000038146972656},{NULL,"Felling1","Felling2","Felling3"}}
};
static const ToolTiming *timing(int item){
    for(size_t i=0;i<sizeof timings/sizeof timings[0];i++)if(timings[i].item_id==item)return &timings[i];
    return NULL;
}
static double entry_length(const PeSandboxActions *a){
    return a->kind==PE_SANDBOX_ACTION_DIG?.6999969482421875:(a->male?.6000022888183594:.6000003814697266);
}
static double loop_length(const PeSandboxActions *a){return timing(a->item_id)->loop[a->male?1:0];}
static double exit_length(const PeSandboxActions *a){
    return a->kind==PE_SANDBOX_ACTION_DIG?.8333358764648438:(a->male?.6000022888183594:.6000003814697266);
}
static double entry_boundary(const PeSandboxActions *a){return entry_length(a)*(a->kind==PE_SANDBOX_ACTION_DIG?.6000000238418579:.8999999761581421);}
static double change_time(const PeSandboxActions *a){return timing(a->item_id)->change[a->male?1:0];}
static double hit_time(const PeSandboxActions *a){return timing(a->item_id)->hit[a->male?1:0];}
static bool vector_valid(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
bool pe_sandbox_actions_supported(int item){return timing(item)!=NULL;}
static bool valid(const PeSandboxActions *a,const PeGame *g){
    if(!a||!g||a->stage<PE_SANDBOX_ACTION_IDLE||a->stage>PE_SANDBOX_ACTION_EXIT||
       !isfinite(a->seconds)||a->seconds<0||a->seconds>1000000||
       !isfinite(a->exit_seconds)||a->exit_seconds<0||a->exit_seconds>1000002||
       !isfinite(a->ending_seconds)||a->ending_seconds<0||a->ending_seconds>6||
       !isfinite(a->since_hit)||a->since_hit<0||a->since_hit>1000000||
       !isfinite(a->speed)||a->speed<.5f||a->speed>1.5f||
       a->change_count>1000000||a->hit_count>1000000)return false;
    if(a->stage==PE_SANDBOX_ACTION_IDLE)return a->kind==PE_SANDBOX_ACTION_NONE&&a->seconds==0;
    if(!a->instance_id||!a->held||!pe_sandbox_actions_supported(a->item_id)||
       a->kind!=timing(a->item_id)->kind)return false;
    if(a->kind==PE_SANDBOX_ACTION_DIG&&(a->target_x<0||a->target_x>=pe_world_width(&g->world)||
       a->target_y<0||a->target_y>=pe_world_size_y(&g->world)||a->target_z<0||a->target_z>=pe_world_depth(&g->world)))return false;
    if(a->kind==PE_SANDBOX_ACTION_FELL&&(a->tree_index<0||a->tree_index>=PE_MAX_TREES))return false;
    if(a->stage==PE_SANDBOX_ACTION_ENTRY&&a->seconds>entry_boundary(a)+1e-9)return false;
    if(a->stage==PE_SANDBOX_ACTION_EXIT&&a->seconds>exit_length(a)+1e-9)return false;
    if(a->stage==PE_SANDBOX_ACTION_LOOP){
        if(a->hit_count*loop_length(a)+hit_time(a)<a->seconds-1e-9)return false;
        if(a->kind==PE_SANDBOX_ACTION_DIG&&a->change_count*loop_length(a)+change_time(a)<a->seconds-1e-9)return false;
    }
    return true;
}
void pe_sandbox_actions_init(PeSandboxActions *a,bool male){
    if(a){memset(a,0,sizeof *a);a->male=male;a->speed=1;a->tree_index=-1;}
}
void pe_sandbox_actions_cancel(PeSandboxActions *a){
    if(a){bool male=a->male;uint64_t swings=a->swings;pe_sandbox_actions_init(a,male);a->swings=swings;}
}
bool pe_sandbox_actions_pose(const PeSandboxActions *a,const PeGame *g,PeSandboxActionPose *out){
    if(!valid(a,g)||!out)return false;
    const PeSlot *tool=pe_game_equipped_tool(g);PeSandboxActionPose pose={0};
    if(tool){pose.item_id=tool->item_id;pose.instance_id=tool->instance_id;pose.held=a->held;}
    if(a->stage!=PE_SANDBOX_ACTION_IDLE){
        if(!tool||tool->instance_id!=a->instance_id||tool->item_id!=a->item_id)return false;
        pose.clip=timing(a->item_id)->clips[a->stage];
        pose.seconds=a->stage==PE_SANDBOX_ACTION_LOOP?fmod(a->seconds,loop_length(a)):a->seconds;
    }
    *out=pose;return true;
}
static void target(PeSandboxActions *a,PeHit h){a->target_x=h.x;a->target_y=h.y;a->target_z=h.z;}
static void charge(PeSandboxActions *a,float *stamina){
    const PeItemDef *item=pe_item_def(a->item_id);
    *stamina=fmaxf(0,*stamina-item->stamina_cost*pe_player_defaults.stamina_reduce_percent);
    a->speed=timing(a->item_id)->speed;
    if(*stamina<=timing(a->item_id)->slow_threshold)a->speed*=.9f;
    if(*stamina<=20)a->speed*=.9f;
    a->speed=fminf(1.5f,fmaxf(.5f,a->speed));
}
static bool strike(PeSandboxActions *a,PeGame *g,PeHit current,PeSlot *tool,float *stamina,PeSandboxActionUpdate *out){
    if(tool->durability<=0){a->ending=true;return true;}
    PeResourceEdit edit;
    if(a->kind==PE_SANDBOX_ACTION_DIG){
        if(!a->first){if(!current.hit)return true;target(a,current);}
        if(!pe_game_dig_sample(g,a->target_x,a->target_y,a->target_z,&edit))return false;
        out->x=a->target_x;out->y=a->target_y;out->z=a->target_z;
    }else{
        if(a->tree_index>=g->tree_count)return false;
        if(!g->trees[a->tree_index].remaining||g->trees[a->tree_index].health<=0){a->ending=true;return true;}
        if(!pe_game_cut_tree(g,a->tree_index,&edit))return false;
        out->tree_index=a->tree_index;
        if(edit.removed)a->ending=true;
    }
    a->since_hit=0;
    if(edit.capacity_blocked){out->capacity_blocked=true;return true;}
    tool->durability=fmaxf(0,tool->durability-pe_item_def(a->item_id)->durability_cost);
    out->swung=true;out->changed=edit.changed;out->removed=edit.removed;
    if(a->swings!=UINT64_MAX)a->swings++;
    if(tool->durability<=0)a->ending=true;
    if(!a->ending)charge(a,stamina);
    return true;
}
bool pe_sandbox_actions_step(PeSandboxActions *a,PeGame *g,PeSandboxActionInput input,PeSandboxActionUpdate *out){
    if(!valid(a,g)||!out)return false;
    PeSandboxActionUpdate result={.tree_index=-1};
    const PeSlot *equipped=pe_game_equipped_tool(g);
    if(!input.enabled||!input.tool_drawn||!(g->hp>0)||!equipped||
       (a->stage!=PE_SANDBOX_ACTION_IDLE&&(equipped->instance_id!=a->instance_id||equipped->item_id!=a->item_id))){
        result.finished=a->stage!=PE_SANDBOX_ACTION_IDLE;pe_sandbox_actions_cancel(a);
        a->held=input.tool_drawn;*out=result;return true;
    }
    if(!isfinite(g->stamina)||g->stamina<0||g->stamina>g->stamina_max||
       !isfinite(g->stamina_max)||!isfinite(equipped->durability)||equipped->durability<0||
       !vector_valid(input.ray_origin)||!vector_valid(input.ray_direction)||!vector_valid(g->player)||
       g->tree_count<0||g->tree_count>PE_MAX_TREES||input.camera_tree< -1||input.forward_tree< -1||
       input.camera_tree>=g->tree_count||input.forward_tree>=g->tree_count)return false;
    if(!pe_sandbox_actions_supported(equipped->item_id)){
        pe_sandbox_actions_cancel(a);a->held=true;result.unsupported=input.held;*out=result;return true;
    }
    PeSandboxActions next=*a;PeSlot next_tool=*equipped;float stamina=g->stamina;
    next.held=true;next.stamina_lock=next.stage!=PE_SANDBOX_ACTION_IDLE;
    if(equipped->durability<=0&&next.stage==PE_SANDBOX_ACTION_IDLE){*a=next;*out=result;return true;}
    if(equipped->durability<=0)next.ending=true;
    PeHit current={0};
    if(timing(equipped->item_id)->kind==PE_SANDBOX_ACTION_DIG){
        double magnitude=hypot(hypot(input.ray_direction.x,input.ray_direction.y),input.ray_direction.z);
        if(!isfinite(magnitude)||magnitude<1e-12)return false;
        current=pe_world_dig_target(&g->world,input.ray_origin,input.ray_direction,g->player,3);
        if(current.hit&&(!pe_game_static_segment_clear(g,input.ray_origin,current.position)||
            !pe_game_static_segment_clear(g,(PeVec3){g->player.x,g->player.y+1,g->player.z},
                (PeVec3){current.x,current.y,current.z})))current=(PeHit){0};
    }
    if(next.stage==PE_SANDBOX_ACTION_IDLE){
        bool can_start=input.held&&g->tool_cooldown<=0;
        if(timing(equipped->item_id)->kind==PE_SANDBOX_ACTION_DIG)can_start=can_start&&current.hit;
        else can_start=can_start&&input.camera_tree>=0&&input.camera_tree==input.forward_tree&&
            g->trees[input.camera_tree].remaining>0&&g->trees[input.camera_tree].health>0;
        if(can_start&&timing(equipped->item_id)->kind!=PE_SANDBOX_ACTION_DIG){
            PeVec3 tree=g->trees[input.camera_tree].position;tree.y+=1;
            can_start=pe_game_static_segment_clear(g,(PeVec3){g->player.x,g->player.y+1,g->player.z},tree)&&
                pe_game_static_segment_clear(g,input.ray_origin,tree);
        }
        if(!can_start){*a=next;*out=result;return true;}
        next.kind=timing(equipped->item_id)->kind;
        next.item_id=equipped->item_id;next.instance_id=equipped->instance_id;next.stage=PE_SANDBOX_ACTION_ENTRY;
        next.seconds=next.exit_seconds=next.ending_seconds=next.since_hit=0;
        next.first=true;next.ending=false;next.change_count=next.hit_count=0;next.stamina_lock=true;
        next.tree_index=input.camera_tree;if(next.kind==PE_SANDBOX_ACTION_DIG)target(&next,current);
        charge(&next,&stamina);result.started=true;
    }
    if(!input.held&&!next.ending){next.ending=true;next.ending_seconds=0;}
    next.since_hit+=1.0/60;
    if(next.ending)next.ending_seconds+=1.0/60;
    if(next.ending&&((next.kind==PE_SANDBOX_ACTION_DIG&&next.ending_seconds>=5)||
                      (next.kind==PE_SANDBOX_ACTION_FELL&&next.since_hit>3))){
        pe_sandbox_actions_cancel(&next);next.held=true;next.stamina_lock=true;result.finished=true;
    }else{
        double remaining=1.0/60;
        for(unsigned guard=0;guard<8&&remaining>0;guard++){
            double rate=next.speed*(next.stage==PE_SANDBOX_ACTION_ENTRY?timing(next.item_id)->entry_rate:1),boundary;
            enum { ENTRY_END,CHANGE,HIT,LOOP_END,EXIT_END } event;
            if(next.stage==PE_SANDBOX_ACTION_ENTRY){boundary=entry_boundary(&next);event=ENTRY_END;}
            else if(next.stage==PE_SANDBOX_ACTION_EXIT){boundary=exit_length(&next);event=EXIT_END;}
            else{
                double length=loop_length(&next),hit=next.hit_count*length+hit_time(&next);
                double change=next.kind==PE_SANDBOX_ACTION_DIG?next.change_count*length+change_time(&next):INFINITY;
                if(change<hit){boundary=change;event=CHANGE;}else{boundary=hit;event=HIT;}
                if(next.ending){
                    if(!next.exit_seconds){
                        double exit_normalized=next.kind==PE_SANDBOX_ACTION_DIG?.800000011920929:.8999999761581421;
                        next.exit_seconds=(floor(next.seconds/length)+exit_normalized)*length;
                        if(next.exit_seconds<next.seconds)next.exit_seconds+=length;
                    }
                    if(next.exit_seconds<boundary){boundary=next.exit_seconds;event=LOOP_END;}
                }
            }
            double elapsed=fmax(0,(boundary-next.seconds)/rate);
            if(elapsed>remaining){next.seconds+=remaining*rate;break;}
            remaining-=elapsed;next.seconds=boundary;
            if(event==ENTRY_END){next.stage=PE_SANDBOX_ACTION_LOOP;next.seconds=0;next.exit_seconds=0;}
            else if(event==CHANGE){if(!next.first&&current.hit)target(&next,current);next.change_count++;}
            else if(event==HIT){
                if(!strike(&next,g,current,&next_tool,&stamina,&result))return false;
                next.first=false;next.hit_count++;
            }else if(event==LOOP_END){next.stage=PE_SANDBOX_ACTION_EXIT;next.seconds=0;}
            else{pe_sandbox_actions_cancel(&next);next.held=true;next.stamina_lock=true;result.finished=true;break;}
        }
    }
    /* Resource helpers may replace the inventory value; reacquire its instance. */
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].instance_id==next_tool.instance_id)
        g->inventory.slots[i].durability=next_tool.durability;
    g->stamina=stamina;if(next.stamina_lock)g->stamina_idle=0;
    *a=next;*out=result;return true;
}
