#include "pe_training.h"
#include "pe_training_terrain.h"
#include <stdlib.h>
#include "pe_training_tree_hit.h"
#include <math.h>

/* Speeds and stamina: MaleModel/FemaleModel MoveParam serialized values and
 * Action_HumanMove.cs. Controller response and ballistic constants are new. */
#define WALK_SPEED 2.0f
#define RUN_SPEED 5.0f
#define SPRINT_SPEED 8.0f
#define SPRINT_ENTRY 15.0f
#define SPRINT_DRAIN 5.0f
#define JUMP_COST 10.0f
#define STAMINA_MAX 100.0f
#define STAMINA_RECOVERY 40.0f
#define RECOVERY_DELAY 0.1f
#define JUMP_ACTION_SECONDS 3.0f
#define SKIN 0.003f
#define STEP_HEIGHT 0.35f
#define WALKABLE_NORMAL_Y 0.65f

static PeVec3 add(PeVec3 a,PeVec3 b){return (PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 scale(PeVec3 a,float s){return (PeVec3){a.x*s,a.y*s,a.z*s};}
static float dot(PeVec3 a,PeVec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static float length(PeVec3 a){return sqrtf(dot(a,a));}
static bool clear(const PeSceneCollision *scene,const PeSceneCollision *terrain,const PeSceneCollision *build,bool tree_active,PeVec3 p){
    return (!build||!pe_scene_capsule_overlap(build,p,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT))&&(!terrain||!pe_scene_capsule_overlap(terrain,p,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT))&&(!tree_active||!pe_training_tree_overlap(p))&&!pe_scene_capsule_overlap(scene,p,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT);
}
static PeSceneHit sweep(const PeSceneCollision *scene,const PeSceneCollision *terrain,const PeSceneCollision *build,bool tree_active,PeVec3 p,PeVec3 delta){
    PeSceneHit hit=pe_scene_capsule_sweep(scene,p,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT,delta);
    if(terrain){PeSceneHit mound=pe_scene_capsule_sweep(terrain,p,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT,delta);
        if(mound.hit&&(!hit.hit||mound.fraction<hit.fraction))hit=mound;
    }
    if(build){PeSceneHit blocks=pe_scene_capsule_sweep(build,p,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT,delta);
        if(blocks.hit&&(!hit.hit||blocks.fraction<hit.fraction))hit=blocks;
    }
    if(tree_active){
        PeSceneHit tree=pe_training_tree_sweep(p,delta);
        if(tree.hit&&(!hit.hit||tree.fraction<hit.fraction))hit=tree;
    }
    return hit;
}
static PeVec3 before_hit(PeVec3 p,PeVec3 delta,PeSceneHit hit){
    float distance=length(delta);
    float fraction=hit.hit?fmaxf(0,hit.fraction-SKIN/fmaxf(distance,SKIN)):1;
    return add(p,scale(delta,fraction));
}
bool pe_training_init(PeTraining *t,const PeSceneCollision *scene){
    if(!t||!scene)return false;
    PeTraining next={0};next.equipped_shovel=-1;pe_training_dig_init(&next.dig,false);
    /* The authored source spawn is above its deck. First try it unchanged;
     * limited vertical lifting handles a surface/capsule tolerance mismatch. */
    for(int lift=0;lift<=12;lift++){
        PeVec3 start={27,2+(float)lift*0.25f,11};
        if(!clear(scene,NULL,NULL,false,start))continue;
        PeVec3 down={0,-5,0};PeSceneHit floor=sweep(scene,NULL,NULL,false,start,down);
        if(!floor.hit||floor.normal.y<WALKABLE_NORMAL_Y)continue;
        next.position=before_hit(start,down,floor);
        if(!clear(scene,NULL,NULL,false,next.position))continue;
        next.ready=true;next.grounded=true;next.stamina=STAMINA_MAX;next.hp=next.max_hp=500;
        pe_tutorial_init(&next.tutorial);*t=next;return true;
    }
    return false;
}
bool pe_training_camera(PeTraining *t,int mode){
    if(!t||!t->ready||mode<0||mode>2||mode==t->camera_mode||pe_training_dialogue(t))return false;
    int mission=(int)t->tutorial.mission_id;
    t->camera_mode=mode;
    if(mode==1)pe_tutorial_event(&t->tutorial,PE_TUTORIAL_CONTROL_MODE,NULL);
    else if(mode==2)pe_tutorial_event(&t->tutorial,PE_TUTORIAL_FIRST_PERSON,NULL);
    if((int)t->tutorial.mission_id!=mission)t->dialogue_index=0;
    return true;
}
static PeVec3 move_slide(const PeSceneCollision *scene,const PeSceneCollision *terrain,const PeSceneCollision *build,bool tree_active,PeVec3 p,PeVec3 delta,
                         PeVec3 *velocity,bool *steep){
    for(int attempt=0;attempt<5&&length(delta)>0.00001f;attempt++){
        PeSceneHit hit=sweep(scene,terrain,build,tree_active,p,delta);
        if(!hit.hit){p=add(p,delta);break;}
        /* Keep skin along the contact normal, not along travel: shortening
         * every diagonal floor approach would reduce horizontal speed. */
        p=add(add(p,scale(delta,hit.fraction)),scale(hit.normal,SKIN));
        if(hit.normal.y<WALKABLE_NORMAL_Y)*steep=true;
        delta=scale(delta,1-hit.fraction);
        float into=dot(delta,hit.normal);
        if(into<0)delta=add(delta,scale(hit.normal,-into));
        into=dot(*velocity,hit.normal);
        if(into<0)*velocity=add(*velocity,scale(hit.normal,-into));
    }
    return p;
}
static bool try_step(const PeSceneCollision *scene,const PeSceneCollision *terrain,const PeSceneCollision *build,bool tree_active,PeVec3 start,PeVec3 horizontal,PeVec3 *result){
    PeVec3 up={0,STEP_HEIGHT,0};
    if(sweep(scene,terrain,build,tree_active,start,up).hit)return false;
    PeVec3 raised=add(start,up);
    if(sweep(scene,terrain,build,tree_active,raised,horizontal).hit)return false;
    raised=add(raised,horizontal);
    PeVec3 down={0,-(STEP_HEIGHT+0.08f),0};PeSceneHit floor=sweep(scene,terrain,build,tree_active,raised,down);
    if(!floor.hit||floor.normal.y<WALKABLE_NORMAL_Y)return false;
    PeVec3 next=before_hit(raised,down,floor);
    if(next.y>start.y+STEP_HEIGHT+SKIN||!clear(scene,terrain,build,tree_active,next))return false;
    *result=next;return true;
}
static void progress_location(PeTraining *t);
static void juice_heal(PeTraining *t){
    if(t->hp>0)t->hp=fminf(t->max_hp,t->hp+30);
}
static void health_step(PeTraining *t){
    if(t->juice_cooldown_ticks)t->juice_cooldown_ticks--;
    t->juice_cooldown=(float)t->juice_cooldown_ticks/60.0f;
    if(!t->juice_buff_active)return;
    t->juice_buff_age_ticks++;
    /* SkBuffs.Exec: strict elapsed>n*interval, one tick, then expiry. Integer
     * fixed ticks preserve equality boundaries instead of accumulating drift. */
    if(t->juice_buff_age_ticks>t->juice_buff_tick*60u){juice_heal(t);t->juice_buff_tick++;}
    if(t->juice_buff_age_ticks>600u)t->juice_buff_active=false;
}
/* Accepted actions own their timers; later UI opening does not erase work.
 * These fixed lesson counts remain the sole inventory ledger for resources. */
static bool gathering_step(PeTraining *t,PeTrainingInput input){
    int mission=(int)t->tutorial.mission_id;
    if(t->gathering.mission!=mission||(mission!=719&&mission!=748&&mission!=721))return false;
    bool alive=isfinite(t->hp)&&t->hp>0;
    if(!alive&&mission!=721){
        for(unsigned i=0;i<PE_GATHERING_HERBS;i++)t->gathering.herb_active[i]=false;
        t->gathering.tree_stage=PE_TREE_IDLE;t->axe_held=false;
        return false;
    }
    bool locked=pe_training_gathering_movement_locked(&t->gathering);
    PeTraining next=*t;
    bool tree_ready=alive&&!pe_training_dialogue(t)&&!t->inventory_open&&!t->character_open&&!t->replicator_open;
    PeGatheringInput gather={
        .cut_held=tree_ready&&input.cut_held,
        .axe_present=t->axe_equipped&&pe_training_gathering_count(&t->gathering,1527)>0,
        .equipment_held=t->axe_held,
        .tree_raycast_hit=input.tree_raycast_hit,
        .animator_do_nothing=input.animator_do_nothing
    };
    double dt=1.0/60.0;
    if(next.gathering.crafting){
        if(next.craft_elapsed_ticks>=180)return locked;
        next.craft_elapsed_ticks++;
        /* Exact three-second boundary, independent of summed double error. */
        if(next.craft_elapsed_ticks==180)dt=3-next.gathering.craft_time;
    }
    if(!pe_training_gathering_step(&next.gathering,dt,gather))return locked;
    if(next.gathering.complete){
        int successor=mission==719?748:mission==748?757:751;
        PeTutorialEvent event=mission==719?PE_TUTORIAL_HERBS_DEPLETED:
            mission==748?PE_TUTORIAL_TREE_DEPLETED:PE_TUTORIAL_SHOVEL_ACQUIRED;
        PeTraining accepted=next;
        if(pe_training_gathering_enter(&accepted.gathering,successor)&&
           pe_tutorial_event(&accepted.tutorial,event,NULL)==PE_TUTORIAL_COMPLETED){
            accepted.dialogue_index=0;
            if(successor==751){
                accepted.replicator_open=false;
                accepted.shovels[0]=(PeTrainingShovel){1,3553};accepted.shovels[1]=(PeTrainingShovel){2,3553};
            }
            next=accepted;
        }else if(mission==719)return locked; /* Fifth herb and axe grant are atomic. */
        /* A blocked later acceptance keeps earned wood/paid output intact and
         * retries only the once-per-mission grant, never the completed action. */
    }
    *t=next;
    return locked||pe_training_gathering_movement_locked(&t->gathering);
}
void pe_training_step(PeTraining *t,const PeSceneCollision *scene,PeTrainingInput input){
    if(!t||!t->ready||!scene)return;
    const PeSceneCollision *terrain=input.terrain_scene,*build=input.build_scene;
    health_step(t);
    if(gathering_step(t,input)||t->dig.stage!=PE_DIG_IDLE||t->dig.stamina_lock){
        input.move_x=input.move_z=0;
        input.sprint=input.jump=input.autorun=false;
    }
    bool tree_active=t->tutorial.mission_id==PE_TUTORIAL_TREE&&t->gathering.tree_hp>0;
    if(!isfinite(input.move_x))input.move_x=0;
    if(!isfinite(input.move_z))input.move_z=0;
    double magnitude=hypot((double)input.move_x,(double)input.move_z);
    if(magnitude>1){input.move_x=(float)(input.move_x/magnitude);input.move_z=(float)(input.move_z/magnitude);}
    bool moving=magnitude>0.0001;
    PeSceneHit ground=sweep(scene,terrain,build,tree_active,t->position,(PeVec3){0,-0.06f,0});
    t->grounded=t->velocity.y<=0&&ground.hit&&ground.normal.y>=WALKABLE_NORMAL_Y;
    t->ground_contact=ground.hit;t->ground_normal=ground.hit?ground.normal:(PeVec3){0};
    bool was_grounded=t->grounded;
    if(t->grounded){t->jumping=false;t->jump_elapsed=0;}
    bool stamina_active=t->jumping||input.stamina_action||t->dig.stage!=PE_DIG_IDLE||t->dig.stamina_lock;
    if(!input.sprint||!moving||!t->grounded)t->sprinting=false;
    else if(!t->sprinting&&t->stamina>=SPRINT_ENTRY)t->sprinting=true;
    if(t->sprinting){
        t->stamina=fmaxf(0,t->stamina-SPRINT_DRAIN*PE_TRAINING_DT);stamina_active=true;
        if(t->stamina<=0)t->sprinting=false;
    }
    float speed=t->sprinting?SPRINT_SPEED:(input.walk?WALK_SPEED:RUN_SPEED);
    t->velocity.x=input.move_x*speed;t->velocity.z=input.move_z*speed;
    bool sprint_action=t->sprinting;
    if(input.jump&&t->grounded&&t->stamina>=JUMP_COST){
        t->stamina-=JUMP_COST;t->velocity.y=7;t->grounded=false;
        t->jumping=true;t->jump_elapsed=0;t->sprinting=false;stamina_active=true;
        pe_tutorial_event(&t->tutorial,PE_TUTORIAL_JUMP,NULL);
    }
    t->velocity.y-=20*PE_TRAINING_DT;
    PeVec3 start=t->position,delta=scale(t->velocity,PE_TRAINING_DT);bool steep=false;
    PeVec3 next=move_slide(scene,terrain,build,tree_active,start,delta,&t->velocity,&steep);
    if(steep&&was_grounded&&!t->jumping&&moving){
        PeVec3 stepped,horizontal={delta.x,0,delta.z};
        if(try_step(scene,terrain,build,tree_active,start,horizontal,&stepped)){
            float old_progress=hypotf(next.x-start.x,next.z-start.z);
            float step_progress=hypotf(stepped.x-start.x,stepped.z-start.z);
            if(step_progress>old_progress+0.00001f){next=stepped;t->velocity.y=0;}
        }
    }
    /* Contact projection may lift the feet around a step/ramp, but should not
     * turn horizontal walking speed into a new upward ballistic impulse. */
    if(!t->jumping&&t->velocity.y>0)t->velocity.y=0;
    if(!moving&&was_grounded&&!t->jumping){next.x=start.x;next.z=start.z;t->velocity.x=t->velocity.z=0;}
    if(clear(scene,terrain,build,tree_active,next))t->position=next;
    else t->velocity=(PeVec3){0}; /* Keep the last safe position on query disagreement. */
    if(t->velocity.y<=0){
        float distance=was_grounded&&!t->jumping?STEP_HEIGHT:0.06f;
        PeVec3 down={0,-distance,0};ground=sweep(scene,terrain,build,tree_active,t->position,down);
        if(ground.hit&&ground.normal.y>=WALKABLE_NORMAL_Y){
            PeVec3 landed=before_hit(t->position,down,ground);
            if(clear(scene,terrain,build,tree_active,landed)){t->position=landed;t->grounded=true;t->jumping=false;t->jump_elapsed=0;t->velocity.y=0;}
        }else t->grounded=false;
    }else t->grounded=false;
    /* Action_HumanMove.cs Action_Jump.Update refreshes the consumption time,
     * advances m_ElapseTime, then ends when elapsed > m_JumpEndTime (3s).
     * Keep this last action frame locked; recovery begins after the delay.
     * Timeout does not cancel falling or fabricate a grounded contact. */
    if(t->jumping){
        t->jump_elapsed+=PE_TRAINING_DT;
        if(t->jump_elapsed>JUMP_ACTION_SECONDS)t->jumping=false;
    }
    if(moving&&hypotf(t->position.x-start.x,t->position.z-start.z)>0.00001f){
        pe_tutorial_event(&t->tutorial,sprint_action?PE_TUTORIAL_SPRINT:PE_TUTORIAL_MOVE,NULL);
    }
    if(input.autorun)pe_tutorial_event(&t->tutorial,PE_TUTORIAL_AUTORUN,NULL);
    progress_location(t);
    if(stamina_active)t->stamina_idle=0;
    else{
        float recovered=fmaxf(0,t->stamina_idle+PE_TRAINING_DT-RECOVERY_DELAY)-fmaxf(0,t->stamina_idle-RECOVERY_DELAY);
        t->stamina=fminf(STAMINA_MAX,t->stamina+recovered*STAMINA_RECOVERY);
        t->stamina_idle=fminf(RECOVERY_DELAY+PE_TRAINING_DT,t->stamina_idle+PE_TRAINING_DT);
    }
}

/* Quest_List718 TalkOP4342..4346; 740/741/742 and NewMoveTask.Update.
 * The five-item transfer satisfies Quest_LootItem2091. The source UI's any-
 * 916 hide condition is not treated as a replacement completion condition.
 * Auxiliary Plot469 prompts4068..4070 are not yet presented. */
static bool near(PeTraining const *t,PeVec3 center,float radius){
    double x=(double)t->position.x-center.x,y=(double)t->position.y-center.y,z=(double)t->position.z-center.z;
    return x*x+y*y+z*z<=(double)radius*radius;
}
static bool active(const PeTraining *t){return t&&t->ready&&pe_tutorial_valid(&t->tutorial);}
static void progress_location(PeTraining *t){
    if(!active(t))return;
    if(t->kit_open&&!near(t,(PeVec3){7,1.5f,12},10))t->kit_open=false;
    if(t->tutorial.mission_id==PE_TUTORIAL_LOCATION&&t->dialogue_index==1&&near(t,(PeVec3){7,1.5f,12},3)){
        pe_tutorial_event(&t->tutorial,PE_TUTORIAL_LOCATION_REACHED,NULL);t->dialogue_index=0;
    }else if(t->tutorial.mission_id==PE_TUTORIAL_RETURN&&near(t,(PeVec3){32,4,12},3)){
        pe_tutorial_event(&t->tutorial,PE_TUTORIAL_ANDY_REACHED,NULL);t->andy_reached=true;
    }
}
int pe_training_dialogue(const PeTraining *t){
    if(!active(t))return 0;
    switch(t->tutorial.mission_id){
    case PE_TUTORIAL_CAMERA:return t->dialogue_index==0?300215:t->dialogue_index==1?300258:0;
    case PE_TUTORIAL_MISSION_PANEL:return t->dialogue_index<5?4342+(int)t->dialogue_index:0;
    case PE_TUTORIAL_LOCATION:return t->dialogue_index==0?300216:0;
    case PE_TUTORIAL_STORAGE:
        if(t->dialogue_index==0)return 300218;
        return t->kit_open&&!t->kit_intro_seen?300219:0;
    case PE_TUTORIAL_RETURN:
        if(t->dialogue_index==0)return 300220;
        return t->andy_dialogue_open?4067:0;
    case PE_TUTORIAL_INVENTORY:
        return t->inventory_lesson_accepted&&t->dialogue_index<2?300221+(int)t->dialogue_index:0;
    case PE_TUTORIAL_QUICKBAR:return t->dialogue_index==0?300224:0;
    case PE_TUTORIAL_CONSUME:return t->dialogue_index==0?4071:t->dialogue_index==1?4298:0;
    case PE_TUTORIAL_EQUIPMENT:{
        static const int dialogue[]={4074,300225,300226,300227,300228};
        return t->equipment_lesson_accepted&&t->dialogue_index<5?dialogue[t->dialogue_index]:0;
    }
    case PE_TUTORIAL_COMBAT:
        if(t->dialogue_index==2&&t->combat_kill_entity_id&&t->tutorial.stage_objectives==1)return 300230;
        return t->dialogue_index==0?300229:t->dialogue_index==1?4299:0;
    case PE_TUTORIAL_TREE:return t->dialogue_index<2?300233+(int)t->dialogue_index:0;
    case PE_TUTORIAL_SCRIPT:return t->dialogue_index<4?300235+(int)t->dialogue_index:0;
    case PE_TUTORIAL_REPLICATE:
        if(t->dialogue_index<2)return 300239+(int)t->dialogue_index;
        return t->replicator_open&&t->replicator_intro_pending&&!t->replicator_intro_seen?300241:0;
    case PE_TUTORIAL_BUILD:{
        static const int intro[]={4074,300244,300245},done[]={300246,4075,4076};
        if(t->tutorial.stage_objectives==1)return t->dialogue_index<3?done[t->dialogue_index]:0;
        return t->dialogue_index<3?intro[t->dialogue_index]:0;
    }
    case PE_TUTORIAL_RECEIVERS:{
        static const int dialogue[]={300249,300247,300248};
        return t->dialogue_index<3?dialogue[t->dialogue_index]:0;
    }
    case PE_TUTORIAL_MINE:{
        static const int dialogue[]={4073,300242,300243};
        return t->dialogue_index<3?dialogue[t->dialogue_index]:0;
    }
    case PE_TUTORIAL_GATHER:return t->dialogue_index==0?4072:t->dialogue_index==1?300231:0;
    default:return 0;
    }
}
bool pe_training_dialogue_response(PeTraining *t,unsigned response,bool advance){
    int talk=pe_training_dialogue(t);
    if(!talk)return false;
    if(response==1||advance)pe_training_advance_dialogue(t,talk);
    return true;
}
bool pe_training_advance_dialogue(PeTraining *t,int expected){
    if(expected<=0||pe_training_dialogue(t)!=expected)return false;
    if(t->tutorial.mission_id==PE_TUTORIAL_CAMERA){
        t->dialogue_index++;
    }else if(t->tutorial.mission_id==PE_TUTORIAL_MISSION_PANEL){
        if(++t->dialogue_index==5){
            pe_tutorial_event(&t->tutorial,PE_TUTORIAL_INTERFACE_ACKNOWLEDGED,NULL);t->dialogue_index=0;
        }
    }else if(t->tutorial.mission_id==PE_TUTORIAL_BUILD&&t->tutorial.stage_objectives==1){
        PeTraining next=*t;
        if(++next.dialogue_index==3){
            if(pe_tutorial_event(&next.tutorial,PE_TUTORIAL_BUILD_ACKNOWLEDGED,NULL)!=PE_TUTORIAL_COMPLETED)return false;
            next.dialogue_index=0;
        }
        *t=next;
    }else if(t->tutorial.mission_id==PE_TUTORIAL_RECEIVERS)t->dialogue_index++;
    else if(expected==300216||expected==300218||expected==300220)t->dialogue_index=1;
    else if(expected==300219)t->kit_intro_seen=true;
    else if(expected==300221||expected==300222||expected==4071||expected==4298)t->dialogue_index++;
    else if(expected==300224)t->dialogue_index=1;
    else if(expected==4074||expected==300225||expected==300226||expected==300227||expected==300228||expected==300229||expected==4299)t->dialogue_index++;
    else if(expected==300241){t->replicator_intro_seen=true;t->replicator_intro_pending=false;}
    else if(expected==4072||expected==300231||(expected>=300233&&expected<=300240)||
            expected==4073||expected==300242||expected==300243||expected==300244||expected==300245)t->dialogue_index++;
    else if(expected==300230){
        PeTraining next=*t;
        if(pe_tutorial_event(&next.tutorial,PE_TUTORIAL_COMBAT_ACKNOWLEDGED,NULL)!=PE_TUTORIAL_COMPLETED||
           !pe_training_gathering_enter(&next.gathering,719))return false;
        next.dialogue_index=0;*t=next;
    }else if(expected==4067){
        if(!near(t,(PeVec3){32,4,12},3)||t->juice_count<1)return false;
        pe_tutorial_event(&t->tutorial,PE_TUTORIAL_ANDY_ACKNOWLEDGED,NULL);
        t->andy_dialogue_open=false;t->dialogue_index=0;
    }else return false;
    progress_location(t);return true;
}
bool pe_training_interact_kit(PeTraining *t){
    if(!active(t)||t->tutorial.mission_id!=PE_TUTORIAL_STORAGE||t->dialogue_index==0||
       t->kit_open||t->character_open||t->kit_transferred||!near(t,(PeVec3){7,1.5f,12},10))return false;
    t->kit_open=true;pe_tutorial_event(&t->tutorial,PE_TUTORIAL_KIT_OPENED,NULL);return true;
}
void pe_training_close_kit(PeTraining *t){if(t)t->kit_open=false;}
bool pe_training_transfer_juice(PeTraining *t){
    if(!active(t)||t->tutorial.mission_id!=PE_TUTORIAL_STORAGE||!t->kit_open||!t->kit_intro_seen||
       t->kit_transferred||t->juice_count!=0||!near(t,(PeVec3){7,1.5f,12},10))return false;
    t->juice_count=5;t->kit_transferred=true;t->kit_open=false;
    pe_tutorial_event(&t->tutorial,PE_TUTORIAL_JUICE_ACQUIRED,NULL);t->dialogue_index=0;return true;
}
int pe_training_inventory_count(const PeTraining *t,int item_id){
    if(!active(t))return 0;
    if(item_id==916)return t->juice_count;
    if(item_id==33)return t->knife_owned&&!t->knife_equipped?1:0;
    uint32_t count=pe_training_gathering_count(&t->gathering,item_id);
    if(item_id==1527&&t->axe_equipped&&count)count--;
    if(item_id==12&&t->equipped_shovel>=0&&count)count--;
    return (int)count;
}
bool pe_training_interact_andy(PeTraining *t){
    if(!active(t)||t->tutorial.mission_id!=PE_TUTORIAL_RETURN||t->dialogue_index==0||
       t->andy_dialogue_open||t->juice_count<1||!near(t,(PeVec3){32,4,12},3))return false;
    progress_location(t);t->andy_dialogue_open=true;return true;
}

/* PlayerMission.cs m_Get_MissionItem grants once on accepting743, independent
 * of the check-only m_Get_DemandItem on742. UseItemTask attaches new event
 * listeners per lesson, so earlier inventory opens and drops never count. */
bool pe_training_accept_inventory_lesson(PeTraining *t){
    if(!active(t)||t->tutorial.mission_id!=PE_TUTORIAL_INVENTORY||t->inventory_lesson_accepted||
       t->juice_count<0||t->juice_count>95)return false;
    t->juice_count+=5;t->inventory_lesson_accepted=true;t->dialogue_index=0;return true;
}
bool pe_training_open_inventory(PeTraining *t){
    if(!active(t)||t->inventory_open)return false;
    t->inventory_open=true;
    if(t->tutorial.mission_id==PE_TUTORIAL_INVENTORY&&t->inventory_lesson_accepted&&t->dialogue_index==2){
        pe_tutorial_event(&t->tutorial,PE_TUTORIAL_INVENTORY_OPENED,NULL);t->dialogue_index=0;
    }
    return true;
}
void pe_training_close_inventory(PeTraining *t){if(t)t->inventory_open=false;}
bool pe_training_open_character(PeTraining *t){
    if(!active(t)||t->character_open||pe_training_dialogue(t)||!isfinite(t->hp)||t->hp<=0)return false;
    t->character_open=true;return true;
}
void pe_training_close_character(PeTraining *t){if(t)t->character_open=false;}
bool pe_training_assign_quickslot(PeTraining *t,int item_id){
    if(!active(t)||t->tutorial.mission_id!=PE_TUTORIAL_QUICKBAR||t->dialogue_index!=1||
       !t->inventory_open||item_id!=916||t->juice_count<=0||t->quickslot_item_id==item_id)return false;
    t->quickslot_item_id=item_id;
    pe_tutorial_event(&t->tutorial,PE_TUTORIAL_QUICKSLOT_ASSIGNED,NULL);t->dialogue_index=0;return true;
}
bool pe_training_use_quickslot(PeTraining *t){
    if(!active(t)||pe_training_dialogue(t)||t->quickslot_item_id!=916||t->juice_count<=0||
       !isfinite(t->hp)||!isfinite(t->max_hp)||t->hp<=0||t->max_hp<=0||t->juice_cooldown_ticks)return false;
    t->juice_count--;t->juice_cooldown_ticks=600;t->juice_cooldown=10;
    t->juice_buff_active=true;t->juice_buff_age_ticks=0;t->juice_buff_tick=1;
    juice_heal(t);
    if(pe_tutorial_event(&t->tutorial,PE_TUTORIAL_JUICE_USED,NULL)==PE_TUTORIAL_COMPLETED)t->dialogue_index=0;
    return true;
}
bool pe_training_accept_equipment_lesson(PeTraining *t){
    if(!active(t)||t->tutorial.mission_id!=PE_TUTORIAL_EQUIPMENT||t->equipment_lesson_accepted||t->knife_owned)return false;
    t->knife_owned=true;t->equipment_lesson_accepted=true;t->dialogue_index=0;return true;
}
bool pe_training_equip_knife(PeTraining *t){
    if(!active(t)||!t->equipment_lesson_accepted||!t->knife_owned||t->knife_equipped||
       !t->inventory_open||pe_training_dialogue(t)||!(t->hp>0))return false;
    t->knife_equipped=true;t->axe_equipped=t->axe_held=false;
    t->equipped_shovel=-1;t->shovel_held=false;pe_training_dig_cancel(&t->dig);
    if(pe_tutorial_event(&t->tutorial,PE_TUTORIAL_WEAPON_EQUIPPED,NULL)==PE_TUTORIAL_COMPLETED)t->dialogue_index=0;
    return true;
}
bool pe_training_unequip_knife(PeTraining *t){
    if(!active(t)||!t->knife_owned||!t->knife_equipped||(!t->inventory_open&&!t->character_open)||pe_training_dialogue(t)||!(t->hp>0))return false;
    t->knife_equipped=false;return true;
}

/* Quest_KillMonster1025 MonsterID13_1_1; SceneEntityMissionPointMonsterKill
 * accepts a matching prototype death, not a special spawn ID. Quest_List746
 * TalkED300230 transitions to719 only after the completion conversation. */
bool pe_training_monster_died(PeTraining *t,uint32_t prototype,uint64_t entity){
    if(!active(t)||t->tutorial.mission_id!=PE_TUTORIAL_COMBAT||t->dialogue_index!=2||
       prototype!=13||!entity||t->combat_kill_entity_id||t->tutorial.stage_objectives)return false;
    if(pe_tutorial_event(&t->tutorial,PE_TUTORIAL_MONSTER_DEFEATED,NULL)!=PE_TUTORIAL_PROGRESS)return false;
    t->combat_kill_entity_id=entity;return true;
}

bool pe_training_begin_herb(PeTraining *t,unsigned index){
    if(!active(t)||t->tutorial.mission_id!=PE_TUTORIAL_GATHER||
       t->dialogue_index!=2||t->inventory_open||t->character_open||t->replicator_open||!isfinite(t->hp)||t->hp<=0)return false;
    PeTraining next=*t;
    if(!pe_training_gathering_begin_herb(&next.gathering,index,t->position))return false;
    /* HoloherbGather.Update immediately ends Move and Sprint. */
    next.sprinting=false;next.velocity.x=next.velocity.z=0;*t=next;return true;
}

/* EquipmentCmpt.TryPutOn shares slot16 for knife33/safetyaxe1527 and moves
 * the old equipment back to inventory. No duplicate ownership is created. */
bool pe_training_equip_axe(PeTraining *t){
    if(!active(t)||t->axe_equipped||!t->inventory_open||pe_training_dialogue(t)||
       !isfinite(t->hp)||t->hp<=0||!pe_training_gathering_count(&t->gathering,1527))return false;
    t->knife_equipped=false;t->axe_equipped=true;t->axe_held=false;
    t->equipped_shovel=-1;t->shovel_held=false;pe_training_dig_cancel(&t->dig);return true;
}
bool pe_training_unequip_axe(PeTraining *t){
    if(!active(t)||!t->axe_equipped||(!t->inventory_open&&!t->character_open)||pe_training_dialogue(t)||
       !isfinite(t->hp)||t->hp<=0)return false;
    t->axe_equipped=t->axe_held=false;return true;
}
bool pe_training_set_axe_held(PeTraining *t,bool held){
    if(!active(t)||t->axe_held==held)return false;
    if(held&&(!t->axe_equipped||!pe_training_gathering_count(&t->gathering,1527)||
       t->inventory_open||t->character_open||t->replicator_open||pe_training_dialogue(t)||!isfinite(t->hp)||t->hp<=0))return false;
    t->axe_held=held;return true;
}
bool pe_training_learn_script(PeTraining *t){
    if(!active(t)||t->tutorial.mission_id!=PE_TUTORIAL_SCRIPT||!t->inventory_open||
       pe_training_dialogue(t)||!isfinite(t->hp)||t->hp<=0)return false;
    PeTraining next=*t;
    if(!pe_training_gathering_learn_script(&next.gathering)||
       !pe_training_gathering_enter(&next.gathering,721)||
       pe_tutorial_event(&next.tutorial,PE_TUTORIAL_SCRIPT_LEARNED,NULL)!=PE_TUTORIAL_COMPLETED)return false;
    next.dialogue_index=0;*t=next;return true;
}
bool pe_training_open_replicator(PeTraining *t){
    if(!active(t)||t->replicator_open||!isfinite(t->hp)||t->hp<=0)return false;
    t->replicator_open=true;
    if(t->tutorial.mission_id==PE_TUTORIAL_REPLICATE&&!t->replicator_intro_seen)t->replicator_intro_pending=true;
    return true;
}
void pe_training_close_replicator(PeTraining *t){if(t)t->replicator_open=false;}
bool pe_training_start_shovel_craft(PeTraining *t){
    if(!active(t)||t->tutorial.mission_id!=PE_TUTORIAL_REPLICATE||!t->replicator_open||
       !t->replicator_intro_seen||t->character_open||pe_training_dialogue(t)||!isfinite(t->hp)||t->hp<=0)return false;
    PeTraining next=*t;
    if(!pe_training_gathering_start_craft(&next.gathering))return false;
    next.craft_elapsed_ticks=0;*t=next;return true;
}

bool pe_training_equip_shovel(PeTraining*t,unsigned index){
    if(!active(t)||index>=2||t->equipped_shovel==(int)index||!t->inventory_open||t->replicator_open||
       pe_training_dialogue(t)||!isfinite(t->hp)||t->hp<=0||t->gathering.shovel<=index||!t->shovels[index].instance_id)return false;
    t->knife_equipped=t->axe_equipped=t->axe_held=false;t->equipped_shovel=(int)index;t->shovel_held=false;
    pe_training_dig_cancel(&t->dig);return true;
}
bool pe_training_unequip_shovel(PeTraining*t){
    if(!active(t)||t->equipped_shovel<0||(!t->inventory_open&&!t->character_open)||t->replicator_open||pe_training_dialogue(t)||!isfinite(t->hp)||t->hp<=0)return false;
    t->equipped_shovel=-1;t->shovel_held=false;pe_training_dig_cancel(&t->dig);return true;
}
bool pe_training_set_shovel_held(PeTraining*t,bool held){
    if(!active(t)||t->shovel_held==held)return false;
    if(held&&(t->equipped_shovel<0||t->equipped_shovel>=2||t->inventory_open||t->character_open||t->replicator_open||
        pe_training_dialogue(t)||!isfinite(t->hp)||t->hp<=0))return false;
    t->shovel_held=held;if(!held)pe_training_dig_cancel(&t->dig);return true;
}
static void finish_mining(PeTraining*t,PeWorld*w,PeTrainingDigUpdate*out){
    if(!t->gathering.complete)return;
    PeTraining next=*t;
    if(!pe_training_gathering_enter(&next.gathering,752)) {out->capacity_blocked=true;return;}
    PeWorld *cleared=calloc(1,sizeof*cleared);if(!cleared)return;
    if(!pe_world_clone(cleared,w)){free(cleared);return;}
    if(!pe_training_terrain_clear(cleared)||pe_tutorial_event(&next.tutorial,PE_TUTORIAL_DIRT_ACQUIRED,NULL)!=PE_TUTORIAL_COMPLETED){pe_world_destroy(cleared);free(cleared);return;}
    pe_training_dig_cancel(&next.dig);next.dialogue_index=0;next.shovel_held=false;
    pe_world_move(w,cleared);free(cleared);*t=next;out->finished=true;
}
bool pe_training_mine_step(PeTraining*t,PeWorld*w,PeVec3 origin,PeVec3 direction,bool held,bool move_requested,PeTrainingDigUpdate*out){
    if(!active(t)||!out)return false;
    if(t->tutorial.mission_id!=PE_TUTORIAL_MINE){pe_training_dig_cancel(&t->dig);*out=(PeTrainingDigUpdate){0};return true;}
    if(!pe_training_terrain_valid(w))return false;
    PeTraining next=*t;PeTrainingDigUpdate result={0};
    if(next.gathering.complete){finish_mining(&next,w,&result);*t=next;*out=result;return true;}
    bool permitted=next.equipped_shovel>=0&&next.equipped_shovel<2&&next.shovel_held&&next.hp>0&&
        next.grounded&&next.camera_mode!=2&&!move_requested&&!pe_training_dialogue(&next)&&
        !next.inventory_open&&!next.character_open&&!next.replicator_open;
    PeTrainingShovel *tool=permitted?&next.shovels[next.equipped_shovel]:NULL;
    PeTrainingDigInput input={origin,direction,next.position,permitted,held};
    if(!pe_training_dig_step(&next.dig,w,&next.gathering,tool,&next.stamina,input,&result))return false;
    if(next.dig.stamina_lock)next.stamina_idle=0;
    finish_mining(&next,w,&result);*t=next;*out=result;return true;
}

bool pe_training_resolve_obstacles(PeTraining*t,const PeSceneCollision*room,const PeSceneCollision*terrain,const PeSceneCollision*build){
    if(!t||!t->ready||!room||(!terrain&&!build)||!isfinite(t->position.x)||!isfinite(t->position.y)||!isfinite(t->position.z))return false;
    if(clear(room,terrain,build,false,t->position))return true;
    /* This repairs newly introduced terrain/build geometry only, never pre-existing room
     * penetration or a path through a room ceiling. */
    if(!clear(room,NULL,NULL,false,t->position))return false;
    bool airborne_found=false;PeVec3 airborne={0};
    for(unsigned i=1;i<=16;i++){
        PeVec3 up={0,.25f*(float)i,0};
        if(sweep(room,NULL,NULL,false,t->position,up).hit)break;
        PeVec3 candidate=add(t->position,up);
        if(!clear(room,terrain,build,false,candidate))continue;
        if(!airborne_found){airborne=candidate;airborne_found=true;}
        PeVec3 down={0,-up.y-.06f,0};PeSceneHit floor=sweep(room,terrain,build,false,candidate,down);
        if(!floor.hit||floor.normal.y<WALKABLE_NORMAL_Y)continue;
        PeVec3 settled=before_hit(candidate,down,floor);
        if(!clear(room,terrain,build,false,settled))continue;
        PeTraining next=*t;next.position=settled;next.velocity.y=0;
        next.grounded=true;next.jumping=false;next.jump_elapsed=0;*t=next;return true;
    }
    /* A rounded capsule at a slab edge can be clear above the geometry but
     * have only a steep first downward contact. Keep that clear position
     * airborne so ordinary gravity/slide can resolve it; do not fabricate a
     * grounded state or move horizontally during recovery. */
    if(airborne_found){
        PeTraining next=*t;next.position=airborne;next.velocity.y=0;
        next.grounded=false;next.jumping=false;next.jump_elapsed=0;*t=next;return true;
    }
    return false;
}

bool pe_training_resolve_terrain(PeTraining*t,const PeSceneCollision*room,const PeSceneCollision*terrain){
    return pe_training_resolve_obstacles(t,room,terrain,NULL);
}

bool pe_training_set_build_mode(PeTraining*t,bool enabled){
    if(!active(t)||t->build_mode==enabled)return false;
    if(!enabled){t->build_mode=false;return true;}
    if((t->tutorial.mission_id!=PE_TUTORIAL_BUILD&&t->tutorial.mission_id!=PE_TUTORIAL_RECEIVERS)||
       !isfinite(t->hp)||t->hp<=0||t->inventory_open||t->character_open||t->replicator_open||t->kit_open||pe_training_dialogue(t))return false;
    PeTraining next=*t;
    if(next.tutorial.mission_id==PE_TUTORIAL_BUILD&&next.tutorial.stage_objectives==0){
        if(!pe_training_gathering_enter_build_mode(&next.gathering)||
           pe_tutorial_event(&next.tutorial,PE_TUTORIAL_BUILD_ENTERED,NULL)!=PE_TUTORIAL_PROGRESS)return false;
        next.dialogue_index=0;
    }
    next.build_mode=true;next.axe_held=next.shovel_held=false;pe_training_dig_cancel(&next.dig);*t=next;return true;
}
bool pe_training_check_receivers(PeTraining*t,PeTrainingReceiverQuery query,void*context,PeTrainingReceiverResult*out){
    if(!active(t)||!out||t->tutorial.mission_id!=PE_TUTORIAL_RECEIVERS||pe_training_dialogue(t)||
       !isfinite(t->hp)||t->hp<=0)return false;
    PeTrainingReceiverResult result;
    if(!pe_training_receivers_evaluate(query,context,&result))return false;
    PeTraining next=*t;
    if(result.complete){
        if(pe_tutorial_event(&next.tutorial,PE_TUTORIAL_RECEIVERS_BLOCKED,NULL)!=PE_TUTORIAL_COMPLETED)return false;
        next.build_mode=false;next.dialogue_index=0;
    }
    *t=next;*out=result;return true;
}
