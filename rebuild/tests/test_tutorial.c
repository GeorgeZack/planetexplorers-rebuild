#include "pe_tutorial.h"
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL tutorial %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static bool equal(PeTutorial a,PeTutorial b) {
    return a.mission_id==b.mission_id&&a.camera_objectives==b.camera_objectives&&a.movement_objectives==b.movement_objectives&&a.stage_objectives==b.stage_objectives;
}
static void put32(uint8_t *b,uint32_t v) {for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(v>>(8*i));}
static void repair_checksum(uint8_t *b) {
    uint32_t h=UINT32_C(2166136261);
    for(unsigned i=0;i<20;i++){h^=b[i];h*=UINT32_C(16777619);}put32(b+20,h);
}
static void event_tests(void) {
    PeTutorial t;PeTutorialTransition transition={12,34};pe_tutorial_init(&t);
    CHECK(pe_tutorial_valid(&t)&&pe_tutorial_supported(&t));CHECK(t.mission_id==739);
    PeTutorial before=t;
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_JUMP,&transition)==PE_TUTORIAL_IGNORED);
    CHECK(equal(t,before)&&transition.completed_mission_id==0&&transition.next_mission_id==0);
    CHECK(pe_tutorial_event(&t,(PeTutorialEvent)99,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_FIRST_PERSON,&transition)==PE_TUTORIAL_PROGRESS);
    CHECK(pe_tutorial_objective_done(&t,4115)&&!pe_tutorial_objective_done(&t,4114));
    before=t;
    for(int i=0;i<10;i++)CHECK(pe_tutorial_event(&t,PE_TUTORIAL_FIRST_PERSON,&transition)==PE_TUTORIAL_IGNORED);
    CHECK(equal(t,before)&&transition.completed_mission_id==0);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_CONTROL_MODE,&transition)==PE_TUTORIAL_COMPLETED);
    CHECK(transition.completed_mission_id==739&&transition.next_mission_id==747&&t.mission_id==747);
    CHECK(t.movement_objectives==0); /* Earlier jump did not pre-complete it. */
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_CONTROL_MODE,&transition)==PE_TUTORIAL_IGNORED);
    CHECK(transition.completed_mission_id==0);
    /* The four successful actions can occur in any order, exactly once. */
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_SPRINT,NULL)==PE_TUTORIAL_PROGRESS);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_JUMP,NULL)==PE_TUTORIAL_PROGRESS);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_AUTORUN,NULL)==PE_TUTORIAL_PROGRESS);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_SPRINT,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_MOVE,&transition)==PE_TUTORIAL_COMPLETED);
    CHECK(transition.completed_mission_id==747&&transition.next_mission_id==718);
    CHECK(pe_tutorial_valid(&t)&&pe_tutorial_supported(&t));
    before=t;
    for(int event=1;event<=6;event++)CHECK(pe_tutorial_event(&t,(PeTutorialEvent)event,&transition)==PE_TUTORIAL_IGNORED);
    CHECK(equal(t,before)&&transition.completed_mission_id==0);
    for(int id=4114;id<=4119;id++)CHECK(pe_tutorial_objective_done(&t,id));
    CHECK(!pe_tutorial_objective_done(&t,4120));
    size_t count=999;const PeTutorialObjective *d=pe_tutorial_objectives(739,&count);
    CHECK(d&&count==2&&d[0].target_id==4114&&d[0].text_id==3102669);
    d=pe_tutorial_objectives(747,&count);CHECK(d&&count==4&&d[3].text_id==3102674);
    CHECK(pe_tutorial_objectives(718,&count)==NULL&&count==0);
}
static void snapshot_tests(void) {
    uint8_t bytes[PE_TUTORIAL_SNAPSHOT_SIZE],bad[PE_TUTORIAL_SNAPSHOT_SIZE];
    PeTutorial t,restored;pe_tutorial_init(&t);pe_tutorial_init(&restored);
    const PeTutorialEvent events[]={PE_TUTORIAL_CONTROL_MODE,PE_TUTORIAL_FIRST_PERSON,
        PE_TUTORIAL_MOVE,PE_TUTORIAL_JUMP,PE_TUTORIAL_SPRINT,PE_TUTORIAL_AUTORUN};
    for(size_t i=0;i<sizeof events/sizeof events[0];i++){
        PeTutorialResult result=pe_tutorial_event(&t,events[i],NULL);
        CHECK(result!=PE_TUTORIAL_IGNORED);
        CHECK(pe_tutorial_encode(&t,bytes,sizeof bytes));
        CHECK(pe_tutorial_decode(&restored,bytes,sizeof bytes));CHECK(equal(t,restored));
        PeTutorialTransition transition={739,747};
        CHECK(pe_tutorial_event(&restored,events[i],&transition)==PE_TUTORIAL_IGNORED);
        CHECK(transition.completed_mission_id==0&&transition.next_mission_id==0);
    }
    /* The snapshot is byte-defined, including IDs (718 == 0x2ce). */
    CHECK(!memcmp(bytes,"PETT",4)&&bytes[4]==1&&bytes[5]==0);
    CHECK(bytes[8]==0xce&&bytes[9]==2&&bytes[10]==0&&bytes[11]==0);
    PeTutorial before=restored;
    for(size_t n=0;n<sizeof bytes;n++){
        CHECK(!pe_tutorial_decode(&restored,bytes,n));CHECK(equal(restored,before));
    }
    CHECK(!pe_tutorial_decode(&restored,bytes,sizeof bytes+1));
    memcpy(bad,bytes,sizeof bad);bad[12]^=1;
    CHECK(!pe_tutorial_decode(&restored,bad,sizeof bad)&&equal(restored,before));
    const struct {unsigned offset;uint32_t value;} invalid[]={
        {0,0},{4,2},{8,999},{8,739},{8,747},{12,1},{12,4},{16,7},{16,16}
    };
    for(size_t i=0;i<sizeof invalid/sizeof invalid[0];i++){
        memcpy(bad,bytes,sizeof bad);put32(bad+invalid[i].offset,invalid[i].value);repair_checksum(bad);
        CHECK(!pe_tutorial_decode(&restored,bad,sizeof bad));CHECK(equal(restored,before));
    }
    PeTutorial invalid_states[]={{739,3,0,0},{739,0,1,0},{747,1,0,0},{747,3,15,0},{718,3,7,0},{0,0,0,0}};
    for(size_t i=0;i<sizeof invalid_states/sizeof invalid_states[0];i++){
        memcpy(bad,bytes,sizeof bad);CHECK(!pe_tutorial_encode(&invalid_states[i],bad,sizeof bad));
        CHECK(!memcmp(bytes,bad,sizeof bad));
        PeTutorial old=invalid_states[i];CHECK(pe_tutorial_event(&invalid_states[i],PE_TUTORIAL_MOVE,NULL)==PE_TUTORIAL_IGNORED);
        CHECK(equal(old,invalid_states[i]));
    }
    CHECK(!pe_tutorial_encode(&t,bytes,sizeof bytes-1));
    CHECK(!pe_tutorial_decode(NULL,bytes,sizeof bytes));CHECK(!pe_tutorial_decode(&restored,NULL,sizeof bytes));
    CHECK(!pe_tutorial_encode(NULL,bytes,sizeof bytes));CHECK(!pe_tutorial_encode(&t,NULL,sizeof bytes));
    CHECK(!pe_tutorial_valid(NULL)&&!pe_tutorial_supported(NULL));
}
static void extended_tests(void){
    PeTutorial t={718,3,15,0};PeTutorialTransition transition;
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_JUICE_ACQUIRED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_INTERFACE_ACKNOWLEDGED,&transition)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==740&&transition.completed_mission_id==718);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_INTERFACE_ACKNOWLEDGED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_LOCATION_REACHED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==741&&pe_tutorial_objective_done(&t,4071));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_JUICE_ACQUIRED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_KIT_OPENED,NULL)==PE_TUTORIAL_PROGRESS);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_KIT_OPENED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_objective_done(&t,4121)&&!pe_tutorial_objective_done(&t,2091));
    uint8_t bytes[24],before[24];memset(bytes,0x55,sizeof bytes);memcpy(before,bytes,sizeof bytes);
    CHECK(!pe_tutorial_encode(&t,bytes,sizeof bytes)&&!memcmp(bytes,before,sizeof bytes));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_JUICE_ACQUIRED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==742&&pe_tutorial_objective_done(&t,2091));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_ANDY_ACKNOWLEDGED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_ANDY_REACHED,NULL)==PE_TUTORIAL_PROGRESS);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_ANDY_ACKNOWLEDGED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==743&&pe_tutorial_supported(&t)&&pe_tutorial_valid(&t));
    CHECK(pe_tutorial_objective_done(&t,4072));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_ANDY_ACKNOWLEDGED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_JUICE_USED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_INVENTORY_OPENED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==744&&pe_tutorial_objective_done(&t,4102));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_QUICKSLOT_ASSIGNED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==749&&pe_tutorial_objective_done(&t,4103));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_JUICE_USED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==745&&pe_tutorial_objective_done(&t,5018)&&pe_tutorial_supported(&t));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_JUICE_USED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_WEAPON_EQUIPPED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==746&&pe_tutorial_supported(&t)&&pe_tutorial_objective_done(&t,4104));
    CHECK(pe_tutorial_objective_done(&t,4103)&&pe_tutorial_objective_done(&t,5018));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_WEAPON_EQUIPPED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(!pe_tutorial_objective_done(&t,1025));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_COMBAT_ACKNOWLEDGED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_MONSTER_DEFEATED,NULL)==PE_TUTORIAL_PROGRESS);
    CHECK(t.mission_id==746&&t.stage_objectives==1&&pe_tutorial_objective_done(&t,1025));
    PeTutorial killed=t;CHECK(pe_tutorial_event(&t,PE_TUTORIAL_MONSTER_DEFEATED,NULL)==PE_TUTORIAL_IGNORED&&equal(t,killed));
    PeTutorialTransition death_transition={0};
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_COMBAT_ACKNOWLEDGED,&death_transition)==PE_TUTORIAL_COMPLETED);
    CHECK(death_transition.completed_mission_id==746&&death_transition.next_mission_id==719);
    CHECK(t.mission_id==719&&t.stage_objectives==0&&pe_tutorial_supported(&t)&&pe_tutorial_valid(&t));
    CHECK(pe_tutorial_objective_done(&t,1025)&&pe_tutorial_objective_done(&t,4104)&&pe_tutorial_objective_done(&t,5018));
    CHECK(pe_tutorial_objective_done(&t,4102)&&pe_tutorial_objective_done(&t,4103)&&pe_tutorial_objective_done(&t,4071));
    CHECK(!pe_tutorial_encode(&t,bytes,sizeof bytes));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_MONSTER_DEFEATED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_COMBAT_ACKNOWLEDGED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(!pe_tutorial_objective_done(&t,4066));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_HERBS_DEPLETED,&death_transition)==PE_TUTORIAL_COMPLETED);
    CHECK(death_transition.completed_mission_id==719&&death_transition.next_mission_id==748);
    CHECK(t.mission_id==748&&pe_tutorial_valid(&t)&&pe_tutorial_supported(&t));
    CHECK(pe_tutorial_objective_done(&t,4066)&&pe_tutorial_objective_done(&t,1025));
    killed=t;CHECK(pe_tutorial_event(&t,PE_TUTORIAL_HERBS_DEPLETED,NULL)==PE_TUTORIAL_IGNORED&&equal(t,killed));
    CHECK(!pe_tutorial_encode(&t,bytes,sizeof bytes));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_SCRIPT_LEARNED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_TREE_DEPLETED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==757&&pe_tutorial_objective_done(&t,2093));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_SCRIPT_LEARNED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==721&&pe_tutorial_objective_done(&t,5023));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_SHOVEL_ACQUIRED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==751&&pe_tutorial_valid(&t)&&pe_tutorial_supported(&t)&&pe_tutorial_objective_done(&t,2089));
    killed=t;CHECK(pe_tutorial_event(&t,PE_TUTORIAL_SHOVEL_ACQUIRED,NULL)==PE_TUTORIAL_IGNORED&&equal(t,killed));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_DIRT_ACQUIRED,NULL)==PE_TUTORIAL_COMPLETED);
    CHECK(t.mission_id==752&&pe_tutorial_valid(&t)&&pe_tutorial_supported(&t)&&pe_tutorial_objective_done(&t,2032));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_BUILD_ACKNOWLEDGED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_BUILD_ENTERED,NULL)==PE_TUTORIAL_PROGRESS);
    CHECK(pe_tutorial_objective_done(&t,4105));
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_BUILD_ENTERED,NULL)==PE_TUTORIAL_IGNORED);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_BUILD_ACKNOWLEDGED,NULL)==PE_TUTORIAL_COMPLETED&&t.mission_id==753);
    CHECK(pe_tutorial_event(&t,PE_TUTORIAL_RECEIVERS_BLOCKED,NULL)==PE_TUTORIAL_COMPLETED&&t.mission_id==756);
    CHECK(pe_tutorial_objective_done(&t,4106)&&!pe_tutorial_supported(&t)&&pe_tutorial_valid(&t));
    /* Recomputed valid checksum cannot smuggle an extended mission into v1. */
    t=(PeTutorial){718,3,15,0};CHECK(pe_tutorial_encode(&t,bytes,sizeof bytes));
    for(unsigned mission=740;mission<=749;mission++){
        put32(bytes+8,mission);repair_checksum(bytes);PeTutorial old=t;
        CHECK(!pe_tutorial_decode(&t,bytes,sizeof bytes)&&equal(t,old));
    }
}
int pe_test_tutorial(void) {
    failures=0;event_tests();snapshot_tests();extended_tests();return failures;
}
