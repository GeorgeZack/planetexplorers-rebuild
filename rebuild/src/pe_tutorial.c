#include "pe_tutorial.h"
#include <string.h>

/* Source: localData.bytes Quest_List 739/747 (TalkED ->747/718),
 * Quest_EnterArea 4114..4119; i18n.db Translation 3102669..3102674;
 * TrainingField/NewMoveTask.cs OnControlModeChange and Update.
 * The original increments a callback count for camera modes; distinct target
 * bits deliberately prevent duplicate events from satisfying both modes. */
static const PeTutorialObjective camera[] = {
    {4114,3102669,PE_TUTORIAL_CONTROL_MODE,1u},
    {4115,3102670,PE_TUTORIAL_FIRST_PERSON,2u}
};
static const PeTutorialObjective movement[] = {
    {4116,3102671,PE_TUTORIAL_MOVE,1u},
    {4117,3102672,PE_TUTORIAL_JUMP,2u},
    {4118,3102673,PE_TUTORIAL_SPRINT,4u},
    {4119,3102674,PE_TUTORIAL_AUTORUN,8u}
};

static const PeTutorialObjective location[]={{4071,3100276,PE_TUTORIAL_LOCATION_REACHED,1u}};
static const PeTutorialObjective storage[]={
    {4121,3100387,PE_TUTORIAL_KIT_OPENED,1u},
    {2091,3100277,PE_TUTORIAL_JUICE_ACQUIRED,2u}
};
static const PeTutorialObjective returning[]={{4072,3100278,PE_TUTORIAL_ANDY_REACHED,1u}};
static const PeTutorialObjective inventory[]={{4102,3100279,PE_TUTORIAL_INVENTORY_OPENED,1u}};
static const PeTutorialObjective quickbar[]={{4103,3100280,PE_TUTORIAL_QUICKSLOT_ASSIGNED,1u}};
static const PeTutorialObjective equipment[]={{4104,3100281,PE_TUTORIAL_WEAPON_EQUIPPED,1u}};
static const PeTutorialObjective combat[]={{1025,3102200,PE_TUTORIAL_MONSTER_DEFEATED,1u}};
static const PeTutorialObjective gathering[]={{4066,3100282,PE_TUTORIAL_HERBS_DEPLETED,1u}};
static const PeTutorialObjective tree[]={{2093,3100283,PE_TUTORIAL_TREE_DEPLETED,1u}};
static const PeTutorialObjective script[]={{5023,3102511,PE_TUTORIAL_SCRIPT_LEARNED,1u}};
static const PeTutorialObjective replicate[]={{2089,3100284,PE_TUTORIAL_SHOVEL_ACQUIRED,1u}};
static const PeTutorialObjective consume[]={{5018,3100385,PE_TUTORIAL_JUICE_USED,1u}};
void pe_tutorial_init(PeTutorial *t) {
    if(t)*t=(PeTutorial){PE_TUTORIAL_CAMERA,0,0,0};
}
bool pe_tutorial_valid(const PeTutorial *t) {
    if(!t||t->camera_objectives>3u||t->movement_objectives>15u)return false;
    switch(t->mission_id){
    case PE_TUTORIAL_CAMERA:return t->camera_objectives!=3u&&t->movement_objectives==0&&t->stage_objectives==0;
    case PE_TUTORIAL_MOVEMENT:return t->camera_objectives==3u&&t->movement_objectives!=15u&&t->stage_objectives==0;
    case PE_TUTORIAL_MISSION_PANEL:case PE_TUTORIAL_LOCATION:case PE_TUTORIAL_INVENTORY:
    case PE_TUTORIAL_QUICKBAR:case PE_TUTORIAL_CONSUME:case PE_TUTORIAL_EQUIPMENT:case PE_TUTORIAL_GATHER:case PE_TUTORIAL_TREE:case PE_TUTORIAL_SCRIPT:case PE_TUTORIAL_REPLICATE:case PE_TUTORIAL_MINE:case PE_TUTORIAL_RECEIVERS:case PE_TUTORIAL_CREATION:
        return t->camera_objectives==3u&&t->movement_objectives==15u&&t->stage_objectives==0;
    case PE_TUTORIAL_STORAGE:case PE_TUTORIAL_RETURN:case PE_TUTORIAL_COMBAT:case PE_TUTORIAL_BUILD:
        return t->camera_objectives==3u&&t->movement_objectives==15u&&t->stage_objectives<=1u;
    default:return false;
    }
}
bool pe_tutorial_supported(const PeTutorial *t) {
    return pe_tutorial_valid(t)&&t->mission_id!=PE_TUTORIAL_CREATION;
}
const PeTutorialObjective *pe_tutorial_objectives(int mission,size_t *count) {
    const PeTutorialObjective *result=NULL;size_t n=0;
    if(mission==PE_TUTORIAL_CAMERA){result=camera;n=2;}
    else if(mission==PE_TUTORIAL_MOVEMENT){result=movement;n=4;}
    else if(mission==PE_TUTORIAL_LOCATION){result=location;n=1;}
    else if(mission==PE_TUTORIAL_STORAGE){result=storage;n=2;}
    else if(mission==PE_TUTORIAL_RETURN){result=returning;n=1;}
    else if(mission==PE_TUTORIAL_INVENTORY){result=inventory;n=1;}
    else if(mission==PE_TUTORIAL_QUICKBAR){result=quickbar;n=1;}
    else if(mission==PE_TUTORIAL_CONSUME){result=consume;n=1;}
    else if(mission==PE_TUTORIAL_EQUIPMENT){result=equipment;n=1;}
    else if(mission==PE_TUTORIAL_COMBAT){result=combat;n=1;}
    else if(mission==PE_TUTORIAL_GATHER){result=gathering;n=1;}
    else if(mission==PE_TUTORIAL_TREE){result=tree;n=1;}
    else if(mission==PE_TUTORIAL_SCRIPT){result=script;n=1;}
    else if(mission==PE_TUTORIAL_REPLICATE){result=replicate;n=1;}
    if(count)*count=n;
    return result;
}
bool pe_tutorial_objective_done(const PeTutorial *t,int target) {
    if(!pe_tutorial_valid(t))return false;
    for(size_t i=0;i<2;i++)if(camera[i].target_id==target)return (t->camera_objectives&camera[i].bit)!=0;
    for(size_t i=0;i<4;i++)if(movement[i].target_id==target)return (t->movement_objectives&movement[i].bit)!=0;
    bool after_inventory=t->mission_id==744||t->mission_id==749||t->mission_id==745||t->mission_id==746||t->mission_id==719||t->mission_id==748||t->mission_id==757||t->mission_id==721||t->mission_id==751||t->mission_id==752||t->mission_id==753||t->mission_id==756;
    if(target==4105)return (t->mission_id==752&&t->stage_objectives==1)||t->mission_id==753||t->mission_id==756;
    if(target==4106)return t->mission_id==756;
    if(target==4102)return after_inventory;
    if(target==4103)return t->mission_id==749||t->mission_id==745||t->mission_id==746||t->mission_id==719||t->mission_id==748||t->mission_id==757||t->mission_id==721||t->mission_id==751||t->mission_id==752||t->mission_id==753||t->mission_id==756;
    if(target==5018)return t->mission_id==745||t->mission_id==746||t->mission_id==719||t->mission_id==748||t->mission_id==757||t->mission_id==721||t->mission_id==751||t->mission_id==752||t->mission_id==753||t->mission_id==756;
    if(target==4104)return t->mission_id==746||t->mission_id==719||t->mission_id==748||t->mission_id==757||t->mission_id==721||t->mission_id==751||t->mission_id==752||t->mission_id==753||t->mission_id==756;
    if(target==4066)return t->mission_id==748||t->mission_id==757||t->mission_id==721||t->mission_id==751||t->mission_id==752||t->mission_id==753||t->mission_id==756;
    if(target==4122||target==2093)return t->mission_id==757||t->mission_id==721||t->mission_id==751||t->mission_id==752||t->mission_id==753||t->mission_id==756;
    if(target==5023)return t->mission_id==721||t->mission_id==751||t->mission_id==752||t->mission_id==753||t->mission_id==756;
    if(target==2089)return t->mission_id==751||t->mission_id==752||t->mission_id==753||t->mission_id==756;
    if(target==2032||target==4123)return t->mission_id==752||t->mission_id==753||t->mission_id==756;
    if(target==1025)return (t->mission_id==746&&t->stage_objectives==1)||t->mission_id==719||t->mission_id==748||t->mission_id==757||t->mission_id==721||t->mission_id==751||t->mission_id==752||t->mission_id==753||t->mission_id==756;
    if(after_inventory&&(target==4071||target==4121||target==2091||target==4072))return true;
    if(target==4071)return t->mission_id==741||t->mission_id==742||t->mission_id==743;
    if(target==4121)return (t->mission_id==741&&t->stage_objectives==1)||t->mission_id==742||t->mission_id==743;
    if(target==2091)return t->mission_id==742||t->mission_id==743;
    if(target==4072)return (t->mission_id==742&&t->stage_objectives==1)||t->mission_id==743;
    return false;
}
PeTutorialResult pe_tutorial_event(PeTutorial *t,PeTutorialEvent event,PeTutorialTransition *transition) {
    if(transition)*transition=(PeTutorialTransition){0,0};
    if(!pe_tutorial_supported(t))return PE_TUTORIAL_IGNORED;
    int previous=(int)t->mission_id,next=0;
    if(previous==739||previous==747){
        size_t count=0;const PeTutorialObjective *objectives=pe_tutorial_objectives(previous,&count);
        uint32_t bit=0;
        for(size_t i=0;i<count;i++)if(objectives[i].event==event){bit=objectives[i].bit;break;}
        uint32_t *mask=previous==739?&t->camera_objectives:&t->movement_objectives;
        if(!bit||(*mask&bit))return PE_TUTORIAL_IGNORED;
        *mask|=bit;
        if(*mask!=(previous==739?3u:15u))return PE_TUTORIAL_PROGRESS;
        next=previous==739?747:718;
    }else if(previous==718&&event==PE_TUTORIAL_INTERFACE_ACKNOWLEDGED)next=740;
    else if(previous==740&&event==PE_TUTORIAL_LOCATION_REACHED)next=741;
    else if(previous==741&&event==PE_TUTORIAL_KIT_OPENED&&t->stage_objectives==0){
        t->stage_objectives=1;return PE_TUTORIAL_PROGRESS;
    }else if(previous==741&&event==PE_TUTORIAL_JUICE_ACQUIRED&&t->stage_objectives==1)next=742;
    else if(previous==742&&event==PE_TUTORIAL_ANDY_REACHED&&t->stage_objectives==0){
        t->stage_objectives=1;return PE_TUTORIAL_PROGRESS;
    }else if(previous==742&&event==PE_TUTORIAL_ANDY_ACKNOWLEDGED&&t->stage_objectives==1)next=743;
    else if(previous==743&&event==PE_TUTORIAL_INVENTORY_OPENED)next=744;
    else if(previous==744&&event==PE_TUTORIAL_QUICKSLOT_ASSIGNED)next=749;
    else if(previous==749&&event==PE_TUTORIAL_JUICE_USED)next=745;
    else if(previous==745&&event==PE_TUTORIAL_WEAPON_EQUIPPED)next=746;
    else if(previous==746&&event==PE_TUTORIAL_MONSTER_DEFEATED&&t->stage_objectives==0){
        t->stage_objectives=1;return PE_TUTORIAL_PROGRESS;
    }else if(previous==746&&event==PE_TUTORIAL_COMBAT_ACKNOWLEDGED&&t->stage_objectives==1)next=719;
    else if(previous==719&&event==PE_TUTORIAL_HERBS_DEPLETED)next=748;
    else if(previous==748&&event==PE_TUTORIAL_TREE_DEPLETED)next=757;
    else if(previous==757&&event==PE_TUTORIAL_SCRIPT_LEARNED)next=721;
    else if(previous==721&&event==PE_TUTORIAL_SHOVEL_ACQUIRED)next=751;
    else if(previous==751&&event==PE_TUTORIAL_DIRT_ACQUIRED)next=752;
    else if(previous==752&&event==PE_TUTORIAL_BUILD_ENTERED&&t->stage_objectives==0){
        t->stage_objectives=1;return PE_TUTORIAL_PROGRESS;
    }else if(previous==752&&event==PE_TUTORIAL_BUILD_ACKNOWLEDGED&&t->stage_objectives==1)next=753;
    else if(previous==753&&event==PE_TUTORIAL_RECEIVERS_BLOCKED)next=756;
    else return PE_TUTORIAL_IGNORED;
    t->mission_id=(uint32_t)next;t->stage_objectives=0;
    if(transition)*transition=(PeTutorialTransition){previous,next};
    return PE_TUTORIAL_COMPLETED;
}
static bool snapshot_supported(const PeTutorial *t){
    return pe_tutorial_valid(t)&&(t->mission_id==739||t->mission_id==747||t->mission_id==718);
}
static void put32(uint8_t *b,uint32_t v) {
    for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(v>>(8*i));
}
static uint32_t get32(const uint8_t *b) {
    uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)b[i]<<(8*i);return v;
}
static uint32_t checksum(const uint8_t *b) {
    uint32_t h=UINT32_C(2166136261);
    for(size_t i=0;i<PE_TUTORIAL_SNAPSHOT_SIZE-4;i++){h^=b[i];h*=UINT32_C(16777619);}
    return h;
}
bool pe_tutorial_encode(const PeTutorial *t,uint8_t *b,size_t size) {
    if(!b||size!=PE_TUTORIAL_SNAPSHOT_SIZE||!snapshot_supported(t))return false;
    uint8_t next[PE_TUTORIAL_SNAPSHOT_SIZE];
    memcpy(next,"PETT",4);put32(next+4,1);put32(next+8,t->mission_id);
    put32(next+12,t->camera_objectives);put32(next+16,t->movement_objectives);
    put32(next+20,checksum(next));memcpy(b,next,sizeof next);return true;
}
bool pe_tutorial_decode(PeTutorial *t,const uint8_t *b,size_t size) {
    if(!t||!b||size!=PE_TUTORIAL_SNAPSHOT_SIZE||memcmp(b,"PETT",4)||
       get32(b+4)!=1u||get32(b+20)!=checksum(b))return false;
    PeTutorial next={get32(b+8),get32(b+12),get32(b+16),0};
    if(!snapshot_supported(&next))return false;
    *t=next;return true;
}
