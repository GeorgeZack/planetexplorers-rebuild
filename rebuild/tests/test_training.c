#define _POSIX_C_SOURCE 200809L
#include "pe_training.h"
#include "pe_training_terrain.h"
#include "pe_training_text.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL training %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static void quad(FILE *f,int *index,PeVec3 a,PeVec3 b,PeVec3 c,PeVec3 d){
    PeVec3 v[]={a,b,c,d};for(int i=0;i<4;i++)fprintf(f,"v %.8g %.8g %.8g\n",v[i].x,v[i].y,v[i].z);
    fprintf(f,"f %d %d %d\nf %d %d %d\n",*index,*index+1,*index+2,*index,*index+2,*index+3);*index+=4;
}
static PeSceneCollision *fixture(bool ceiling,bool floor){
    char path[]="/tmp/pe-training-test-XXXXXX",error[160];int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return NULL;
    FILE *f=fdopen(fd,"w");if(!f){close(fd);unlink(path);CHECK(false);return NULL;}int index=1;
    if(floor)quad(f,&index,(PeVec3){0,1.5f,0},(PeVec3){60,1.5f,0},(PeVec3){60,1.5f,30},(PeVec3){0,1.5f,30});
    quad(f,&index,(PeVec3){32,1.5f,0},(PeVec3){32,8,0},(PeVec3){32,8,30},(PeVec3){32,1.5f,30});
    if(ceiling)quad(f,&index,(PeVec3){0,3.55f,0},(PeVec3){60,3.55f,0},(PeVec3){60,3.55f,30},(PeVec3){0,3.55f,30});
    else if(floor){
        quad(f,&index,(PeVec3){29,1.5f,9},(PeVec3){29,1.75f,9},(PeVec3){29,1.75f,13},(PeVec3){29,1.5f,13});
        quad(f,&index,(PeVec3){29,1.75f,9},(PeVec3){30,1.75f,9},(PeVec3){30,1.75f,13},(PeVec3){29,1.75f,13});
        quad(f,&index,(PeVec3){30,1.5f,9},(PeVec3){30,1.75f,9},(PeVec3){30,1.75f,13},(PeVec3){30,1.5f,13});
    }
    CHECK(fclose(f)==0);PeSceneCollision *scene=pe_scene_load_obj(path,error,sizeof error);unlink(path);
    CHECK(scene!=NULL);if(!scene)fprintf(stderr,"fixture: %s\n",error);return scene;
}
static void ticks(PeTraining *t,const PeSceneCollision *scene,PeTrainingInput input,int count){
    for(int i=0;i<count;i++){
        pe_training_step(t,scene,input);
        CHECK(!pe_scene_capsule_overlap(scene,t->position,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT));
    }
}
static void synthetic_tests(void){
    PeSceneCollision *scene=fixture(false,true);if(!scene)return;
    PeTraining t;CHECK(pe_training_init(&t,scene));CHECK(t.ready&&t.grounded&&t.stamina==100);
    CHECK(t.position.x==27&&t.position.z==11&&t.position.y>1.5f&&t.position.y<1.51f);
    float y=t.position.y;ticks(&t,scene,(PeTrainingInput){0},180);CHECK(fabsf(t.position.y-y)<0.01f&&t.grounded);
    t.stamina=70;
    ticks(&t,scene,(PeTrainingInput){.stamina_action=true},120);
    CHECK(t.stamina==70&&t.stamina_idle==0);
    ticks(&t,scene,(PeTrainingInput){0},6);CHECK(fabsf(t.stamina-70)<.001f);
    ticks(&t,scene,(PeTrainingInput){0},6);CHECK(fabsf(t.stamina-74)<.01f);
    for(int mode=0;mode<3;mode++){
        CHECK(pe_training_init(&t,scene));float z=t.position.z;
        ticks(&t,scene,(PeTrainingInput){.move_z=1,.walk=mode==0,.sprint=mode==2},60);
        float expected=mode==0?2:mode==1?5:8;
        CHECK(fabsf(t.position.z-z-expected)<0.04f);
        CHECK(fabsf(t.stamina-(mode==2?95:100))<0.01f);
    }
    CHECK(pe_training_init(&t,scene));ticks(&t,scene,(PeTrainingInput){.move_x=1},30);
    CHECK(t.position.x>29.3f&&t.position.y>1.65f); /* Climb the 25cm step. */
    ticks(&t,scene,(PeTrainingInput){.move_x=1},180);CHECK(t.position.x<=31.701f&&t.grounded);
    t.position=(PeVec3){31.6f,1.503f,16};t.velocity=(PeVec3){0};
    ticks(&t,scene,(PeTrainingInput){.move_x=1,.move_z=1},60);
    CHECK(t.position.x<=31.701f&&t.position.z>19); /* Slide along the wall. */
    CHECK(pe_training_init(&t,scene));CHECK(!pe_training_camera(&t,3));CHECK(!pe_training_camera(&t,0));
    CHECK(pe_training_dialogue(&t)==300215);
    PeTraining welcome=t;CHECK(!pe_training_camera(&t,1)&&!memcmp(&t,&welcome,sizeof t));
    CHECK(!pe_training_advance_dialogue(&t,300258)&&!memcmp(&t,&welcome,sizeof t));
    const PeTrainingDialogue *first=pe_training_dialogue_entry(300215),*second=pe_training_dialogue_entry(300258);
    CHECK(first&&first->speaker_npc_id==9041&&strstr(first->english,"Welcome to the simulation deck!"));
    CHECK(second&&second->speaker_npc_id==-9995&&strstr(second->english,"Press F5"));
    CHECK(pe_training_dialogue_response(&t,9,false)&&!memcmp(&t,&welcome,sizeof t));
    CHECK(pe_training_dialogue_response(&t,0,false)&&!memcmp(&t,&welcome,sizeof t));
    CHECK(pe_training_dialogue_response(&t,1,false)&&pe_training_dialogue(&t)==300258);
    welcome=t;CHECK(!pe_training_advance_dialogue(&t,300215)&&!pe_training_camera(&t,1)&&!memcmp(&t,&welcome,sizeof t));
    CHECK(pe_training_dialogue_response(&t,1,false)&&!pe_training_dialogue(&t));
    welcome=t;CHECK(!pe_training_dialogue_response(&t,1,false)&&!memcmp(&t,&welcome,sizeof t));
    CHECK(!pe_training_dialogue_response(NULL,1,true));
    CHECK(t.tutorial.mission_id==739&&t.tutorial.camera_objectives==0);

    CHECK(pe_training_camera(&t,1));CHECK(!pe_training_camera(&t,1));CHECK(t.tutorial.mission_id==739);
    CHECK(pe_training_camera(&t,2));CHECK(t.tutorial.mission_id==747&&t.dialogue_index==0);
    t.stamina=0;pe_training_step(&t,scene,(PeTrainingInput){.jump=true,.sprint=true});
    CHECK(!pe_tutorial_objective_done(&t.tutorial,4117)&&!pe_tutorial_objective_done(&t.tutorial,4118));
    t.stamina=100;ticks(&t,scene,(PeTrainingInput){.move_z=1},1);
    CHECK(pe_tutorial_objective_done(&t.tutorial,4116));
    ticks(&t,scene,(PeTrainingInput){.move_z=1,.sprint=true},1);
    CHECK(pe_tutorial_objective_done(&t.tutorial,4118));
    float before=t.stamina;pe_training_step(&t,scene,(PeTrainingInput){.jump=true});
    CHECK(t.jumping&&!t.grounded&&fabsf(t.stamina-(before-10))<0.001f);
    ticks(&t,scene,(PeTrainingInput){0},10);CHECK(fabsf(t.stamina-(before-10))<0.001f);
    CHECK(pe_tutorial_objective_done(&t.tutorial,4117));
    ticks(&t,scene,(PeTrainingInput){.autorun=true,.move_z=1},1);CHECK(t.tutorial.mission_id==718);
    ticks(&t,scene,(PeTrainingInput){0},180);CHECK(t.grounded&&t.stamina==100&&t.tutorial.mission_id==718);
    CHECK(pe_training_init(&t,scene));t.stamina=14.9f;
    pe_training_step(&t,scene,(PeTrainingInput){.move_z=1,.sprint=true});CHECK(!t.sprinting);
    t.stamina=15;pe_training_step(&t,scene,(PeTrainingInput){.move_z=1,.sprint=true});CHECK(t.sprinting);
    t.stamina=0.04f;pe_training_step(&t,scene,(PeTrainingInput){.move_z=1,.sprint=true});CHECK(!t.sprinting&&t.stamina==0);
    ticks(&t,scene,(PeTrainingInput){0},6);CHECK(t.stamina<0.01f);
    ticks(&t,scene,(PeTrainingInput){0},6);CHECK(t.stamina>3.9f&&t.stamina<4.1f);
    PeSceneCollision *roof=fixture(true,true);
    if(roof){
        CHECK(pe_training_init(&t,scene));float highest=t.position.y;
        pe_training_step(&t,roof,(PeTrainingInput){.jump=true});
        for(int i=0;i<90;i++){pe_training_step(&t,roof,(PeTrainingInput){0});highest=fmaxf(highest,t.position.y);CHECK(!pe_scene_capsule_overlap(roof,t.position,.3f,1.8f));}
        CHECK(highest<1.76f&&t.grounded);pe_scene_free(roof);
    }
    PeSceneCollision *no_floor=fixture(false,false);
    if(no_floor){PeTraining before_state=t;CHECK(!pe_training_init(&t,no_floor));CHECK(!memcmp(&t,&before_state,sizeof t));pe_scene_free(no_floor);}
    pe_scene_free(scene);
}
static void original_room_test(void){
    const char *path=PE_SOURCE_ROOT "/rebuild/build/reference-assets/room-collision.obj";
    FILE *f=fopen(path,"rb");if(!f){puts("Training original-room fixture: SKIP (export optional reference assets)");return;}fclose(f);
    char error[160];PeSceneCollision *scene=pe_scene_load_obj(path,error,sizeof error);CHECK(scene!=NULL);if(!scene)return;
    PeTraining t;CHECK(pe_training_init(&t,scene));
    if(t.ready){
        CHECK(t.position.x==27&&t.position.z==11&&t.position.y>1.4f&&t.position.y<1.6f);
        float floor=t.position.y;ticks(&t,scene,(PeTrainingInput){0},300);
        CHECK(t.grounded&&fabsf(t.position.y-floor)<0.03f&&t.position.x==27&&t.position.z==11);
        CHECK(pe_training_advance_dialogue(&t,300215)&&pe_training_advance_dialogue(&t,300258));
        CHECK(pe_training_camera(&t,1));CHECK(pe_training_camera(&t,2));
        ticks(&t,scene,(PeTrainingInput){0},10);
        CHECK(t.tutorial.movement_objectives==0); /* Sloped-floor contacts are not a Move action. */
        ticks(&t,scene,(PeTrainingInput){.move_x=-1},5);
        ticks(&t,scene,(PeTrainingInput){.move_x=-1,.sprint=true},5);
        ticks(&t,scene,(PeTrainingInput){.jump=true},1);
        ticks(&t,scene,(PeTrainingInput){.move_x=-1,.autorun=true},1);
        CHECK(t.tutorial.mission_id==718);ticks(&t,scene,(PeTrainingInput){0},120);
        CHECK(t.grounded);puts("Training original-room floor, capsule and initial lessons: OK");
    }
    pe_scene_free(scene);
}
static void long_fall_test(void){
    char path[]="/tmp/pe-training-ledge-XXXXXX",error[160];int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return;
    FILE *f=fdopen(fd,"w");if(!f){close(fd);unlink(path);CHECK(false);return;}int index=1;
    quad(f,&index,(PeVec3){20,1.5f,8},(PeVec3){30,1.5f,8},(PeVec3){30,1.5f,12},(PeVec3){20,1.5f,12});
    CHECK(fclose(f)==0);PeSceneCollision *scene=pe_scene_load_obj(path,error,sizeof error);unlink(path);
    CHECK(scene!=NULL);if(!scene)return;PeTraining t;CHECK(pe_training_init(&t,scene));
    pe_training_step(&t,scene,(PeTrainingInput){.move_z=1,.jump=true});
    ticks(&t,scene,(PeTrainingInput){.move_z=1},120);
    CHECK(t.position.z>12&&t.position.y<0&&!t.grounded&&t.jumping);
    CHECK(t.stamina==90&&t.stamina_idle==0);
    int frames=0;
    while(t.jumping&&frames++<120)pe_training_step(&t,scene,(PeTrainingInput){0});
    CHECK(!t.jumping&&!t.grounded&&t.velocity.y<0);
    CHECK(t.jump_elapsed>3&&t.jump_elapsed<3.04f);CHECK(t.stamina==90&&t.stamina_idle==0);
    float height=t.position.y;
    ticks(&t,scene,(PeTrainingInput){0},6);CHECK(fabsf(t.stamina-90)<0.001f);
    ticks(&t,scene,(PeTrainingInput){0},6);CHECK(fabsf(t.stamina-94)<0.01f);
    CHECK(t.position.y<height&&!t.grounded); /* Action timeout did not arrest the fall. */
    pe_scene_free(scene);
}
static void extended_lesson_tests(void){
    PeSceneCollision *scene=fixture(false,true);if(!scene)return;
    PeTraining t;CHECK(pe_training_init(&t,scene));
    CHECK(!pe_training_transfer_juice(&t)&&!pe_training_interact_kit(&t)&&!pe_training_interact_andy(&t));
    CHECK(!pe_training_advance_dialogue(&t,4342));
    t.tutorial=(PeTutorial){718,3,15,0};
    for(int id=4342;id<=4346;id++){
        CHECK(pe_training_dialogue(&t)==id);
        CHECK(!pe_training_advance_dialogue(&t,id+1));
        CHECK(pe_training_advance_dialogue(&t,id));
        CHECK(!pe_training_advance_dialogue(&t,id));
    }
    CHECK(t.tutorial.mission_id==740&&pe_training_dialogue(&t)==300216);
    t.position=(PeVec3){10.0001f,1.5f,12};
    CHECK(pe_training_advance_dialogue(&t,300216));CHECK(t.tutorial.mission_id==740);
    /* Acknowledgement at exactly the authored inclusive radius completes740. */
    t.dialogue_index=0;t.position=(PeVec3){10,1.5f,12};
    CHECK(pe_training_advance_dialogue(&t,300216));CHECK(t.tutorial.mission_id==741);
    CHECK(pe_training_dialogue(&t)==300218&&!pe_training_interact_kit(&t));
    CHECK(pe_training_advance_dialogue(&t,300218));
    t.position=(PeVec3){17.0001f,1.5f,12};CHECK(!pe_training_interact_kit(&t));
    t.position=(PeVec3){17,1.5f,12};CHECK(pe_training_interact_kit(&t));
    CHECK(!pe_training_interact_kit(&t));CHECK(pe_training_dialogue(&t)==300219);
    CHECK(!pe_training_transfer_juice(&t)&&pe_training_inventory_count(&t,916)==0);
    pe_training_close_kit(&t);CHECK(!t.kit_open&&pe_training_dialogue(&t)==0);
    CHECK(pe_training_interact_kit(&t)&&pe_training_dialogue(&t)==300219);
    CHECK(pe_training_advance_dialogue(&t,300219));CHECK(!pe_training_advance_dialogue(&t,300219));
    t.position.x=17.0001f;CHECK(!pe_training_transfer_juice(&t)&&t.juice_count==0);
    pe_training_step(&t,scene,(PeTrainingInput){0});CHECK(!t.kit_open);
    t.position=(PeVec3){17,1.5f,12};CHECK(pe_training_interact_kit(&t));
    CHECK(pe_training_dialogue(&t)==0);CHECK(pe_training_transfer_juice(&t));
    CHECK(t.tutorial.mission_id==742&&t.kit_transferred&&!t.kit_open);
    CHECK(pe_training_inventory_count(&t,916)==5&&pe_training_inventory_count(&t,1)==0);
    CHECK(!pe_training_transfer_juice(&t)&&!pe_training_interact_kit(&t));
    CHECK(pe_training_dialogue(&t)==300220&&!pe_training_interact_andy(&t));
    CHECK(pe_training_advance_dialogue(&t,300220));
    t.position=(PeVec3){35.0001f,4,12};CHECK(!pe_training_interact_andy(&t));
    t.position=(PeVec3){35,4,12};CHECK(pe_training_interact_andy(&t));
    CHECK(t.andy_reached&&pe_tutorial_objective_done(&t.tutorial,4072));
    CHECK(pe_training_dialogue(&t)==4067&&!pe_training_interact_andy(&t));
    t.position.x=35.0001f;CHECK(!pe_training_advance_dialogue(&t,4067));
    t.position.x=35;CHECK(pe_training_advance_dialogue(&t,4067));
    CHECK(t.tutorial.mission_id==743&&pe_tutorial_supported(&t.tutorial));
    CHECK(t.juice_count==5&&pe_training_dialogue(&t)==0); /* No743entry grant or demand consumption. */
    CHECK(!pe_training_advance_dialogue(&t,4067)&&!pe_training_transfer_juice(&t));
    CHECK(!pe_training_interact_andy(&t));
    CHECK(pe_training_open_inventory(&t)); /* An already-open window cannot pre-complete743. */
    CHECK(!pe_training_assign_quickslot(&t,916)&&!pe_training_use_quickslot(&t));
    CHECK(pe_training_accept_inventory_lesson(&t));
    CHECK(t.juice_count==10&&!pe_training_accept_inventory_lesson(&t));
    CHECK(pe_training_dialogue(&t)==300221);
    CHECK(!pe_training_advance_dialogue(&t,300222));
    CHECK(pe_training_advance_dialogue(&t,300221));
    CHECK(!pe_training_advance_dialogue(&t,300221));
    CHECK(pe_training_dialogue(&t)==300222);
    pe_training_close_inventory(&t);CHECK(pe_training_open_inventory(&t));
    CHECK(t.tutorial.mission_id==743); /* Open before second acknowledgement does not count. */
    CHECK(pe_training_advance_dialogue(&t,300222));
    CHECK(!pe_training_open_inventory(&t)&&t.tutorial.mission_id==743);
    pe_training_close_inventory(&t);CHECK(pe_training_open_inventory(&t));
    CHECK(t.tutorial.mission_id==744&&t.juice_count==10&&pe_training_dialogue(&t)==300224);
    CHECK(!pe_training_assign_quickslot(&t,916));
    CHECK(pe_training_advance_dialogue(&t,300224));
    CHECK(!pe_training_assign_quickslot(&t,1)&&t.quickslot_item_id==0);
    pe_training_close_inventory(&t);CHECK(!pe_training_assign_quickslot(&t,916));
    CHECK(pe_training_open_inventory(&t));
    t.juice_count=0;CHECK(!pe_training_assign_quickslot(&t,916));t.juice_count=10;
    CHECK(pe_training_assign_quickslot(&t,916));
    CHECK(t.quickslot_item_id==916&&t.juice_count==10&&t.tutorial.mission_id==749);
    CHECK(!pe_training_assign_quickslot(&t,916)&&!pe_training_use_quickslot(&t));
    CHECK(pe_training_dialogue(&t)==4071&&pe_training_advance_dialogue(&t,4071));
    CHECK(pe_training_dialogue(&t)==4298&&!pe_training_use_quickslot(&t));
    CHECK(pe_training_advance_dialogue(&t,4298));
    t.quickslot_item_id=0;CHECK(!pe_training_use_quickslot(&t));t.quickslot_item_id=916;
    t.juice_count=0;CHECK(!pe_training_use_quickslot(&t));t.juice_count=10;
    float stamina=t.stamina;CHECK(pe_training_use_quickslot(&t));
    CHECK(t.juice_count==9&&t.tutorial.mission_id==745&&pe_tutorial_supported(&t.tutorial));
    CHECK(t.stamina==stamina&&pe_training_inventory_count(&t,33)==0); /* Juice is not stamina; no knife grant. */
    CHECK(!pe_training_use_quickslot(&t)&&t.juice_count==9&&pe_training_dialogue(&t)==0);
    CHECK(!pe_training_accept_inventory_lesson(&t));
    CHECK(!pe_training_equip_knife(&t)&&!pe_training_unequip_knife(&t));
    CHECK(pe_training_accept_equipment_lesson(&t));
    CHECK(t.knife_owned&&!t.knife_equipped&&pe_training_inventory_count(&t,33)==1);
    CHECK(!pe_training_accept_equipment_lesson(&t));
    const int equipment_talk[]={4074,300225,300226,300227,300228};
    for(size_t i=0;i<sizeof equipment_talk/sizeof equipment_talk[0];i++){
        CHECK(pe_training_dialogue(&t)==equipment_talk[i]&&!pe_training_equip_knife(&t));
        CHECK(pe_training_advance_dialogue(&t,equipment_talk[i]));
        CHECK(!pe_training_advance_dialogue(&t,equipment_talk[i]));
    }
    pe_training_close_inventory(&t);CHECK(!pe_training_equip_knife(&t));
    CHECK(pe_training_open_inventory(&t));t.hp=0;CHECK(!pe_training_equip_knife(&t));t.hp=500;
    CHECK(pe_training_equip_knife(&t));
    CHECK(t.tutorial.mission_id==746&&t.knife_owned&&t.knife_equipped&&pe_training_inventory_count(&t,33)==0);
    CHECK(!pe_training_equip_knife(&t)&&!pe_training_unequip_knife(&t));
    PeTraining before_death=t;
    CHECK(!pe_training_monster_died(&t,13,261)&&!memcmp(&t,&before_death,sizeof t));
    CHECK(!pe_training_advance_dialogue(&t,300230));
    CHECK(pe_training_dialogue(&t)==300229&&pe_training_advance_dialogue(&t,300229));
    before_death=t;CHECK(!pe_training_monster_died(&t,13,261)&&!memcmp(&t,&before_death,sizeof t));
    CHECK(pe_training_dialogue(&t)==4299&&pe_training_advance_dialogue(&t,4299));
    CHECK(pe_training_dialogue(&t)==0&&pe_tutorial_supported(&t.tutorial));
    CHECK(pe_training_unequip_knife(&t)&&pe_training_inventory_count(&t,33)==1);
    CHECK(!pe_training_unequip_knife(&t));CHECK(pe_training_equip_knife(&t));
    CHECK(t.tutorial.mission_id==746&&t.juice_count==9&&pe_training_dialogue(&t)==0);
    CHECK(!pe_training_accept_equipment_lesson(&t));
    before_death=t;
    CHECK(!pe_training_monster_died(&t,12,261)&&!pe_training_monster_died(&t,13,0));
    CHECK(!memcmp(&t,&before_death,sizeof t));
    CHECK(pe_training_monster_died(&t,13,UINT64_C(987654321)));
    CHECK(t.tutorial.mission_id==746&&t.combat_kill_entity_id==UINT64_C(987654321));
    CHECK(pe_tutorial_objective_done(&t.tutorial,1025)&&pe_training_dialogue(&t)==300230);
    CHECK(t.juice_count==before_death.juice_count&&t.knife_owned==before_death.knife_owned&&t.hp==before_death.hp&&t.stamina==before_death.stamina);
    before_death=t;
    CHECK(!pe_training_monster_died(&t,13,UINT64_C(987654321))&&!pe_training_monster_died(&t,13,2));
    CHECK(!pe_training_advance_dialogue(&t,4299)&&!pe_training_advance_dialogue(&t,4072));
    CHECK(!memcmp(&t,&before_death,sizeof t));
    CHECK(pe_training_advance_dialogue(&t,300230));
    CHECK(t.tutorial.mission_id==719&&pe_tutorial_supported(&t.tutorial)&&pe_training_dialogue(&t)==4072);
    CHECK(t.gathering.mission==719&&t.gathering.accepted==1&&t.gathering.herbs_depleted==0);
    CHECK(!pe_training_advance_dialogue(&t,300230)&&!pe_training_advance_dialogue(&t,300231));
    CHECK(pe_training_advance_dialogue(&t,4072)&&pe_training_dialogue(&t)==300231);
    CHECK(!pe_training_advance_dialogue(&t,4072));
    CHECK(pe_training_advance_dialogue(&t,300231)&&pe_training_dialogue(&t)==0);
    before_death=t;
    CHECK(!pe_training_advance_dialogue(&t,300231)&&!pe_training_monster_died(&t,13,3));
    CHECK(!memcmp(&t,&before_death,sizeof t)&&t.tutorial.mission_id==719);
    CHECK(!pe_training_monster_died(NULL,13,1));
    CHECK(!pe_training_accept_equipment_lesson(NULL)&&!pe_training_equip_knife(NULL)&&!pe_training_unequip_knife(NULL));
    CHECK(!pe_training_accept_inventory_lesson(NULL)&&!pe_training_open_inventory(NULL));
    CHECK(!pe_training_assign_quickslot(NULL,916)&&!pe_training_use_quickslot(NULL));
    pe_training_close_inventory(NULL);
    CHECK(pe_training_dialogue(NULL)==0&&pe_training_inventory_count(NULL,916)==0);
    CHECK(!pe_training_advance_dialogue(NULL,4342)&&!pe_training_interact_kit(NULL));
    pe_training_close_kit(NULL);pe_scene_free(scene);
}
static void herb_tests(void){
    PeSceneCollision *scene=fixture(false,true);if(!scene)return;
    PeTraining t;CHECK(pe_training_init(&t,scene));
    CHECK(!pe_training_begin_herb(NULL,0)&&!pe_training_begin_herb(&t,0));
    t.tutorial=(PeTutorial){746,3,15,0};t.dialogue_index=2;
    CHECK(pe_training_monster_died(&t,13,261));
    /* Entry failure cannot advance the mission or acknowledge its dialogue. */
    t.gathering.accepted=63;PeTraining before=t;
    CHECK(!pe_training_advance_dialogue(&t,300230)&&!memcmp(&t,&before,sizeof t));
    t.gathering.accepted=0;
    CHECK(pe_training_advance_dialogue(&t,300230));
    t.position=pe_training_gathering_herb_position(0);
    before=t;CHECK(!pe_training_begin_herb(&t,0)&&!memcmp(&t,&before,sizeof t));
    CHECK(pe_training_advance_dialogue(&t,4072)&&pe_training_advance_dialogue(&t,300231));
    CHECK(pe_training_open_inventory(&t));before=t;
    CHECK(!pe_training_begin_herb(&t,0)&&!memcmp(&t,&before,sizeof t));
    pe_training_close_inventory(&t);
    CHECK(pe_training_open_replicator(&t));before=t;
    CHECK(!pe_training_begin_herb(&t,0)&&!memcmp(&t,&before,sizeof t));
    pe_training_close_replicator(&t);t.hp=0;before=t;
    CHECK(!pe_training_begin_herb(&t,0)&&!memcmp(&t,&before,sizeof t));
    t.hp=500;t.position.x+=3;before=t;
    CHECK(!pe_training_begin_herb(&t,0)&&!memcmp(&t,&before,sizeof t));
    t.position=pe_training_gathering_herb_position(0);
    t.knife_owned=t.knife_equipped=true; /* Source allows gathering armed. */
    CHECK(pe_training_begin_herb(&t,0));ticks(&t,scene,(PeTrainingInput){0},30);
    t.hp=0;ticks(&t,scene,(PeTrainingInput){0},90);
    CHECK(!t.gathering.herb_dead[0]&&!t.gathering.herb_active[0]&&t.gathering.herbs_depleted==0);
    t.hp=500;
    for(unsigned i=0;i<PE_GATHERING_HERBS;i++){
        t.position=pe_training_gathering_herb_position(i);t.position.y=1.503f;t.velocity=(PeVec3){0};
        t.sprinting=true;CHECK(pe_training_begin_herb(&t,i));CHECK(!t.sprinting);
        before=t;CHECK(!pe_training_begin_herb(&t,i)&&!pe_training_begin_herb(&t,5));
        CHECK(!memcmp(&t,&before,sizeof t));
        PeVec3 position=t.position;float stamina=t.stamina;
        if(i==0)CHECK(pe_training_open_inventory(&t)); /* Accepted action survives a later UI open. */
        ticks(&t,scene,(PeTrainingInput){.move_x=1,.sprint=true,.jump=true},65);
        CHECK(!t.gathering.herb_dead[i]&&t.gathering.herbs_depleted==i);
        CHECK(fabsf(t.position.x-position.x)<.001f&&!t.jumping&&t.stamina>=stamina);
        if(i==4){
            /* The fifth depletion and its successor grant are one transaction. */
            t.gathering.axe=PE_GATHERING_ITEM_LIMIT;PeTrainingGathering old=t.gathering;
            ticks(&t,scene,(PeTrainingInput){0},1);
            CHECK(!memcmp(&old,&t.gathering,sizeof old)&&t.tutorial.mission_id==719);
            t.gathering.axe=0;
        }
        ticks(&t,scene,(PeTrainingInput){.move_x=1,.jump=true},1);
        CHECK(t.gathering.herb_dead[i]&&t.gathering.herbs_depleted==i+1);
        CHECK(fabsf(t.position.x-position.x)<.001f&&!t.jumping);
        CHECK(pe_training_inventory_count(&t,279)==0); /* Herbs yield no item. */
        before=t;CHECK(!pe_training_begin_herb(&t,i)&&!memcmp(&t,&before,sizeof t));
        if(i==0){
            pe_training_close_inventory(&t);
            ticks(&t,scene,(PeTrainingInput){.move_x=1},1);
            CHECK(t.position.x>position.x+.07f&&t.gathering.herb_active[i]);
            ticks(&t,scene,(PeTrainingInput){0},53);
            CHECK(!t.gathering.herb_active[i]); /* The local1.966s tail does not lock movement. */
        }
    }
    CHECK(t.tutorial.mission_id==748&&t.gathering.mission==748&&t.gathering.accepted==3);
    CHECK(pe_training_inventory_count(&t,1527)==1&&pe_training_dialogue(&t)==300233);
    CHECK(pe_tutorial_objective_done(&t.tutorial,4066));
    CHECK(pe_training_advance_dialogue(&t,300233)&&!pe_training_advance_dialogue(&t,300233));
    ticks(&t,scene,(PeTrainingInput){0},300);
    CHECK(t.tutorial.mission_id==748&&pe_training_inventory_count(&t,1527)==1);
    CHECK(!pe_training_begin_herb(&t,0)&&!pe_training_advance_dialogue(&t,300230));
    pe_scene_free(scene);
}
static void tree_craft_tests(void){
    PeSceneCollision *scene=fixture(false,true);if(!scene)return;
    PeTraining t;CHECK(pe_training_init(&t,scene));
    /* Fixture enters through five real timed herb depletions, without forging
     * tree HP, script knowledge or inventory receipts. */
    t.tutorial=(PeTutorial){746,3,15,0};t.dialogue_index=2;
    t.knife_owned=t.knife_equipped=t.equipment_lesson_accepted=true;
    CHECK(pe_training_monster_died(&t,13,261)&&pe_training_advance_dialogue(&t,300230));
    CHECK(pe_training_advance_dialogue(&t,4072)&&pe_training_advance_dialogue(&t,300231));
    for(unsigned i=0;i<5;i++){
        t.position=pe_training_gathering_herb_position(i);t.position.y=1.503f;
        CHECK(pe_training_begin_herb(&t,i));ticks(&t,scene,(PeTrainingInput){0},66);
    }
    CHECK(t.tutorial.mission_id==748&&pe_training_inventory_count(&t,1527)==1);
    CHECK(pe_training_open_inventory(&t));PeTraining before=t;
    CHECK(!pe_training_equip_axe(&t)&&!memcmp(&t,&before,sizeof t));
    CHECK(pe_training_advance_dialogue(&t,300233)&&pe_training_advance_dialogue(&t,300234));
    CHECK(pe_training_equip_axe(&t)&&t.axe_equipped&&!t.knife_equipped);
    CHECK(pe_training_inventory_count(&t,33)==1&&pe_training_inventory_count(&t,1527)==0);
    CHECK(!pe_training_equip_axe(&t)&&!pe_training_set_axe_held(&t,true));
    CHECK(pe_training_equip_knife(&t)&&!t.axe_equipped&&t.knife_equipped);
    CHECK(pe_training_inventory_count(&t,1527)==1&&pe_training_inventory_count(&t,33)==0);
    CHECK(pe_training_equip_axe(&t));pe_training_close_inventory(&t);
    PeTrainingInput cut={.cut_held=true,.tree_raycast_hit=true,.animator_do_nothing=true};
    ticks(&t,scene,cut,80);CHECK(t.gathering.tree_stage==PE_TREE_IDLE&&t.gathering.tree_hp==7);
    CHECK(pe_training_set_axe_held(&t,true));
    CHECK(pe_training_open_inventory(&t));ticks(&t,scene,cut,80);
    CHECK(t.gathering.tree_stage==PE_TREE_IDLE&&t.gathering.tree_hp==7);
    pe_training_close_inventory(&t);t.hp=0;ticks(&t,scene,cut,80);
    CHECK(t.gathering.tree_stage==PE_TREE_IDLE&&!t.axe_held&&t.gathering.tree_hp==7);
    CHECK(!pe_training_set_axe_held(&t,true));t.hp=500;CHECK(pe_training_set_axe_held(&t,true));
    cut.tree_raycast_hit=false;
    ticks(&t,scene,cut,80);CHECK(t.gathering.tree_stage==PE_TREE_IDLE);
    cut.tree_raycast_hit=true;cut.animator_do_nothing=false;
    ticks(&t,scene,cut,80);CHECK(t.gathering.tree_stage==PE_TREE_WAIT&&t.gathering.tree_hp==7);
    cut.animator_do_nothing=true;cut.move_x=1;cut.jump=true;
    PeVec3 position=t.position;float stamina=t.stamina;
    ticks(&t,scene,cut,1);CHECK(t.gathering.tree_stage==PE_TREE_ENTRY);
    ticks(&t,scene,cut,35);CHECK(t.gathering.tree_hp==7&&!t.jumping);
    CHECK(fabsf(t.position.x-position.x)<.001f&&t.stamina>=stamina);
    /* Release must finish the current loop and leave movement unlocked. */
    cut.cut_held=false;cut.move_x=0;cut.jump=false;
    ticks(&t,scene,cut,150);CHECK(t.gathering.tree_stage==PE_TREE_IDLE&&t.gathering.tree_hp==6);
    CHECK(pe_training_inventory_count(&t,279)==0);
    cut.cut_held=true;
    for(unsigned i=0;i<1000&&t.gathering.tree_hp>1;i++)ticks(&t,scene,cut,1);
    CHECK(t.gathering.tree_hp==1);
    t.gathering.script=PE_GATHERING_ITEM_LIMIT;
    for(unsigned i=0;i<1000&&!t.gathering.complete;i++)ticks(&t,scene,cut,1);
    CHECK(t.gathering.complete&&t.tutorial.mission_id==748&&t.gathering.wood==1);
    ticks(&t,scene,cut,120);CHECK(t.gathering.wood==1&&t.gathering.script==PE_GATHERING_ITEM_LIMIT);
    t.gathering.script=0;ticks(&t,scene,cut,1); /* Retry only the blocked grant. */
    CHECK(t.tutorial.mission_id==757&&t.gathering.tree_hp==0);
    CHECK(pe_training_inventory_count(&t,279)==1&&pe_training_inventory_count(&t,406)==1);
    ticks(&t,scene,cut,120);CHECK(pe_training_inventory_count(&t,279)==1&&pe_training_inventory_count(&t,406)==1);
    CHECK(pe_training_open_inventory(&t));before=t;
    CHECK(!pe_training_learn_script(&t)&&!memcmp(&t,&before,sizeof t));
    for(int id=300235;id<=300238;id++)CHECK(pe_training_advance_dialogue(&t,id));
    /* Script use and the next acceptance grant are atomic under capacity. */
    t.gathering.wood=PE_GATHERING_ITEM_LIMIT-19;before=t;
    CHECK(!pe_training_learn_script(&t)&&!memcmp(&t,&before,sizeof t));
    t.gathering.wood=1;
    CHECK(pe_training_open_replicator(&t)); /* Earlier opening is not a721 event. */
    CHECK(pe_training_learn_script(&t));
    CHECK(t.tutorial.mission_id==721&&t.gathering.formula1&&pe_training_inventory_count(&t,406)==0);
    CHECK(pe_training_inventory_count(&t,279)==21);
    before=t;CHECK(!pe_training_learn_script(&t)&&!memcmp(&t,&before,sizeof t));
    CHECK(pe_training_advance_dialogue(&t,300239)&&pe_training_advance_dialogue(&t,300240));
    CHECK(pe_training_dialogue(&t)==0&&!pe_training_start_shovel_craft(&t));
    pe_training_close_inventory(&t);pe_training_close_replicator(&t);
    CHECK(pe_training_open_replicator(&t)&&pe_training_dialogue(&t)==300241);
    CHECK(!pe_training_start_shovel_craft(&t)&&pe_training_advance_dialogue(&t,300241));
    CHECK(pe_training_start_shovel_craft(&t));before=t;
    CHECK(!pe_training_start_shovel_craft(&t)&&!memcmp(&t,&before,sizeof t));
    CHECK(pe_training_inventory_count(&t,279)==11&&pe_training_inventory_count(&t,12)==0);
    pe_training_close_replicator(&t); /* Paid work continues after closing UI. */
    ticks(&t,scene,(PeTrainingInput){0},179);
    CHECK(t.tutorial.mission_id==721&&t.gathering.crafting&&pe_training_inventory_count(&t,12)==0);
    PeTraining blocked=t;blocked.gathering.shovel=PE_GATHERING_ITEM_LIMIT-1;
    ticks(&blocked,scene,(PeTrainingInput){0},1);
    CHECK(blocked.tutorial.mission_id==721&&!blocked.gathering.crafting&&blocked.gathering.shovel==PE_GATHERING_ITEM_LIMIT);
    ticks(&blocked,scene,(PeTrainingInput){0},4);
    CHECK(blocked.gathering.shovel==PE_GATHERING_ITEM_LIMIT&&blocked.gathering.wood==11);
    blocked.gathering.shovel=1;ticks(&blocked,scene,(PeTrainingInput){0},1);
    CHECK(blocked.tutorial.mission_id==751&&blocked.gathering.shovel==2); /* No repeated recipe output. */
    ticks(&t,scene,(PeTrainingInput){0},1);
    CHECK(t.tutorial.mission_id==751&&!t.gathering.crafting&&t.craft_elapsed_ticks==180);
    CHECK(t.shovels[0].instance_id==1&&t.shovels[1].instance_id==2&&t.shovels[0].durability==3553&&t.shovels[1].durability==3553);
    CHECK(pe_training_inventory_count(&t,12)==2&&pe_training_inventory_count(&t,279)==11);
    CHECK(pe_training_advance_dialogue(&t,4073)&&pe_training_advance_dialogue(&t,300242)&&pe_training_advance_dialogue(&t,300243));
    ticks(&t,scene,(PeTrainingInput){0},200);
    CHECK(t.tutorial.mission_id==751&&pe_training_inventory_count(&t,12)==2&&t.gathering.axe==1);
    CHECK(!pe_training_learn_script(&t)&&!pe_training_start_shovel_craft(&t));
    CHECK(pe_training_open_inventory(&t)&&pe_training_unequip_axe(&t));
    CHECK(pe_training_inventory_count(&t,1527)==1&&!t.axe_held);
    pe_scene_free(scene);
}
static void terrain_recovery_tests(void){
    PeSceneCollision *room=fixture(false,true),*ceiling=fixture(true,true),*terrain=NULL;
    PeWorld *world=malloc(sizeof*world);CHECK(world&&room&&ceiling);
    if(!world||!room||!ceiling){free(world);pe_scene_free(room);pe_scene_free(ceiling);return;}
    pe_training_terrain_init(world);char error[160];
    CHECK(pe_training_terrain_collision(world,&terrain,error,sizeof error));
    if(!terrain){free(world);pe_scene_free(room);pe_scene_free(ceiling);return;}
    PeTraining t;CHECK(pe_training_init(&t,room));PeTraining before=t;
    CHECK(pe_training_resolve_terrain(&t,room,terrain)&&!memcmp(&t,&before,sizeof t));
    t.position=(PeVec3){6,1.503f,11};t.velocity.y=-3;t.jumping=true;
    CHECK(pe_scene_capsule_overlap(terrain,t.position,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT));
    before=t;CHECK(pe_training_resolve_terrain(&t,room,terrain));
    CHECK(t.position.x==before.position.x&&t.position.z==before.position.z&&t.position.y>before.position.y&&t.position.y<=before.position.y+4);
    CHECK(!pe_scene_capsule_overlap(terrain,t.position,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT));
    CHECK(t.grounded&&!t.jumping&&t.velocity.y==0);
    ticks(&t,room,(PeTrainingInput){.terrain_scene=terrain},60);
    CHECK(!pe_scene_capsule_overlap(terrain,t.position,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT)&&t.position.y>2);
    t=before;before=t;
    CHECK(!pe_training_resolve_terrain(&t,ceiling,terrain)&&!memcmp(&t,&before,sizeof t));
    CHECK(!pe_training_resolve_terrain(&t,room,NULL)&&!memcmp(&t,&before,sizeof t));
    CHECK(pe_training_terrain_clear(world)&&pe_training_terrain_build_anchors(world,false));
    CHECK(pe_training_terrain_collision(world,&terrain,error,sizeof error));
    PeMeshData receivers={0};CHECK(pe_training_receiver_mesh(&receivers));
    PeSceneCollision*slabs=pe_scene_from_mesh(&receivers,(PeVec3){0},error,sizeof error);CHECK(slabs!=NULL);pe_mesh_free(&receivers);
    if(slabs){
        CHECK(pe_training_init(&t,room));t.position=(PeVec3){11.5f,1.503f,9.85f};before=t;
        CHECK(!pe_scene_capsule_overlap(terrain,t.position,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT));
        CHECK(pe_scene_capsule_overlap(slabs,t.position,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT));
        CHECK(pe_training_resolve_terrain(&t,room,terrain)&&!memcmp(&t,&before,sizeof t)); /* Old terrain-only path misses slab. */
        CHECK(pe_training_resolve_obstacles(&t,room,terrain,slabs));
        CHECK(!pe_scene_capsule_overlap(slabs,t.position,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT)&&t.position.x==before.position.x&&t.position.z==before.position.z);
        ticks(&t,room,(PeTrainingInput){.terrain_scene=terrain,.build_scene=slabs},60);
        CHECK(!pe_scene_capsule_overlap(slabs,t.position,PE_TRAINING_RADIUS,PE_TRAINING_HEIGHT));
        t=before;CHECK(!pe_training_resolve_obstacles(&t,ceiling,terrain,slabs)&&!memcmp(&t,&before,sizeof t));
        pe_scene_free(slabs);
    }
    pe_scene_free(terrain);pe_scene_free(room);pe_scene_free(ceiling);free(world);
}
static void health_tests(void){
    PeSceneCollision *scene=fixture(false,true);if(!scene)return;
    PeTraining t;CHECK(pe_training_init(&t,scene));CHECK(t.hp==500&&t.max_hp==500);
    CHECK(!pe_training_accept_equipment_lesson(&t));
    t.tutorial=(PeTutorial){745,3,15,0};t.juice_count=10;t.quickslot_item_id=916;t.hp=100;
    float stamina=t.stamina;
    CHECK(pe_training_use_quickslot(&t));
    CHECK(t.hp==130&&t.juice_count==9&&t.stamina==stamina&&t.juice_cooldown==10);
    CHECK(!pe_training_use_quickslot(&t));
    ticks(&t,scene,(PeTrainingInput){0},60);CHECK(t.hp==130&&t.juice_buff_active);
    ticks(&t,scene,(PeTrainingInput){0},1);CHECK(t.hp==160&&t.juice_buff_tick==2);
    ticks(&t,scene,(PeTrainingInput){0},539);
    CHECK(t.hp==400&&t.juice_buff_active&&t.juice_buff_age_ticks==600&&t.juice_cooldown==0);
    ticks(&t,scene,(PeTrainingInput){0},1);CHECK(t.hp==430&&!t.juice_buff_active);
    ticks(&t,scene,(PeTrainingInput){0},120);CHECK(t.hp==430); /* No passive HP regeneration. */
    CHECK(pe_training_use_quickslot(&t)&&t.hp==460&&t.juice_count==8);
    ticks(&t,scene,(PeTrainingInput){0},121);CHECK(t.hp==500);
    CHECK(t.tutorial.mission_id==745&&t.stamina==100); /* Reuse never replays749 or restores stamina. */
    t.hp=0;ticks(&t,scene,(PeTrainingInput){0},600);CHECK(t.hp==0&&!t.juice_buff_active);
    int count=t.juice_count;CHECK(!pe_training_use_quickslot(&t)&&t.juice_count==count);
    /* Replacing the buff at exactly cooldown expiry prevents the old final tick. */
    t.hp=100;CHECK(pe_training_use_quickslot(&t));ticks(&t,scene,(PeTrainingInput){0},599);
    CHECK(!pe_training_use_quickslot(&t)&&t.juice_cooldown_ticks==1);
    ticks(&t,scene,(PeTrainingInput){0},1);CHECK(t.hp==400&&t.juice_buff_active);
    CHECK(pe_training_use_quickslot(&t)&&t.hp==430&&t.juice_buff_age_ticks==0&&t.juice_buff_tick==1);
    ticks(&t,scene,(PeTrainingInput){0},1);CHECK(t.hp==430);
    ticks(&t,scene,(PeTrainingInput){0},59);CHECK(t.hp==430);
    ticks(&t,scene,(PeTrainingInput){0},1);CHECK(t.hp==460);
    t.hp=NAN;CHECK(!pe_training_use_quickslot(&t));t.hp=100;t.max_hp=NAN;CHECK(!pe_training_use_quickslot(&t));
    pe_scene_free(scene);
}
static bool receiver_query(void*context,const PeTrainingReceiverRay*ray,PeTrainingReceiverHit*hit){
    int mode=*(int*)context;
    if(mode<0)return false;
    if(mode==0)return pe_training_receiver_boxes(ray,hit);
    *hit=(PeTrainingReceiverHit){true,0,5};return true;
}
static void build_lesson_tests(void){
    PeSceneCollision*scene=fixture(false,true);if(!scene)return;
    PeTraining t;CHECK(pe_training_init(&t,scene));
    t.tutorial=(PeTutorial){752,3,15,0};t.gathering=(PeTrainingGathering){.mission=752,.accepted=63,.dirt=9006};
    CHECK(!pe_training_set_build_mode(&t,true));
    CHECK(pe_training_advance_dialogue(&t,4074)&&pe_training_advance_dialogue(&t,300244)&&pe_training_advance_dialogue(&t,300245));
    PeTraining saved=t;t.inventory_open=true;CHECK(!pe_training_set_build_mode(&t,true));t=saved;t.hp=0;CHECK(!pe_training_set_build_mode(&t,true));t=saved;
    CHECK(pe_training_set_build_mode(&t,true)&&t.build_mode&&t.tutorial.mission_id==752&&t.gathering.complete&&t.gathering.dirt==9006);
    CHECK(!pe_training_set_build_mode(&t,true)&&pe_training_dialogue(&t)==300246);
    CHECK(pe_training_set_build_mode(&t,false));CHECK(!pe_training_set_build_mode(&t,true));
    CHECK(!pe_training_advance_dialogue(&t,4076));
    CHECK(pe_training_advance_dialogue(&t,300246));CHECK(!pe_training_advance_dialogue(&t,300246));
    CHECK(pe_training_advance_dialogue(&t,4075)&&pe_training_advance_dialogue(&t,4076)&&t.tutorial.mission_id==753);
    int mode=1;PeTrainingReceiverResult r={0};CHECK(!pe_training_check_receivers(&t,receiver_query,&mode,&r));
    CHECK(pe_training_advance_dialogue(&t,300249)&&pe_training_advance_dialogue(&t,300247)&&pe_training_advance_dialogue(&t,300248));
    CHECK(pe_training_set_build_mode(&t,true)&&t.gathering.dirt==9006);
    mode=0;CHECK(pe_training_check_receivers(&t,receiver_query,&mode,&r)&&!r.complete&&r.score==-5000&&t.tutorial.mission_id==753);
    mode=-1;saved=t;PeTrainingReceiverResult old=r;
    CHECK(!pe_training_check_receivers(&t,receiver_query,&mode,&r)&&!memcmp(&t,&saved,sizeof t)&&!memcmp(&r,&old,sizeof r));
    mode=1;CHECK(pe_training_check_receivers(&t,receiver_query,&mode,&r)&&r.complete&&t.tutorial.mission_id==756&&!t.build_mode&&t.gathering.dirt==9006);
    saved=t;CHECK(!pe_training_check_receivers(&t,receiver_query,&mode,&r)&&!memcmp(&t,&saved,sizeof t));
    /* A separate dynamic build scene must stop walking even though room is clear. */
    PeVertex v[6]={0};PeVec3 p[]={{28,1,8},{28,5,8},{28,5,14},{28,1,8},{28,5,14},{28,1,14}};
    for(unsigned i=0;i<6;i++)v[i].position=p[i];
    PeMeshData mesh={0};mesh.vertices=v;mesh.count=6;char error[128];
    PeSceneCollision*blocks=pe_scene_from_mesh(&mesh,(PeVec3){0},error,sizeof error);CHECK(blocks!=NULL);
    if(blocks){CHECK(pe_training_init(&t,scene));ticks(&t,scene,(PeTrainingInput){.build_scene=blocks,.move_x=1},60);CHECK(t.position.x<27.71f&&t.position.x>27.5f);pe_scene_free(blocks);}
    pe_scene_free(scene);
}
static void character_panel_tests(void){
    PeSceneCollision *scene=fixture(false,true);if(!scene)return;
    PeTraining t;CHECK(pe_training_init(&t,scene));
    t.tutorial=(PeTutorial){743,3,15,0};t.inventory_lesson_accepted=true;t.dialogue_index=2;t.juice_count=10;
    PeTutorial lesson=t.tutorial;
    CHECK(pe_training_open_character(&t)&&t.character_open&&!t.inventory_open);
    CHECK(!memcmp(&t.tutorial,&lesson,sizeof lesson)&&t.juice_count==10);
    PeTraining before=t;CHECK(!pe_training_open_character(&t)&&!memcmp(&t,&before,sizeof t));
    pe_training_close_character(&t);CHECK(!t.character_open&&!t.inventory_open);
    CHECK(pe_training_open_inventory(&t)&&t.tutorial.mission_id==744);
    pe_training_close_inventory(&t);
    /* Set up owned equipment; the panel never creates copies or resets wear. */
    t.tutorial=(PeTutorial){751,3,15,0};t.dialogue_index=3;
    t.knife_owned=t.equipment_lesson_accepted=true;t.gathering.axe=1;t.gathering.shovel=2;
    t.shovels[0]=(PeTrainingShovel){31,3528};t.shovels[1]=(PeTrainingShovel){32,3503};
    CHECK(pe_training_open_character(&t));
    CHECK(!pe_training_equip_knife(&t)&&!pe_training_equip_axe(&t)&&!pe_training_equip_shovel(&t,0));
    for(unsigned tool=0;tool<3;tool++){
        t.knife_equipped=tool==0;t.axe_equipped=tool==1;t.axe_held=tool==1;
        t.equipped_shovel=tool==2?1:-1;t.shovel_held=tool==2;
        int item=tool==0?33:tool==1?1527:12;
        int count=pe_training_inventory_count(&t,item);
        bool (*unequip)(PeTraining*)=tool==0?pe_training_unequip_knife:tool==1?pe_training_unequip_axe:pe_training_unequip_shovel;
        PeTraining equipped=t;
        t.character_open=false;before=t;CHECK(!unequip(&t)&&!memcmp(&t,&before,sizeof t));t=equipped;
        t.hp=0;before=t;CHECK(!unequip(&t)&&!memcmp(&t,&before,sizeof t));t=equipped;
        t.dialogue_index=0;before=t;CHECK(!unequip(&t)&&!memcmp(&t,&before,sizeof t));t=equipped;
        CHECK(unequip(&t)&&pe_training_inventory_count(&t,item)==count+1);
        CHECK(t.knife_owned&&t.gathering.axe==1&&t.gathering.shovel==2);
        CHECK(t.shovels[0].instance_id==31&&t.shovels[0].durability==3528&&
              t.shovels[1].instance_id==32&&t.shovels[1].durability==3503);
        before=t;CHECK(!unequip(&t)&&!memcmp(&t,&before,sizeof t));
    }
    /* Both windows may be open: source equip still comes from the bag. */
    CHECK(pe_training_open_inventory(&t)&&pe_training_equip_shovel(&t,0));
    pe_training_close_inventory(&t);
    CHECK(!pe_training_set_shovel_held(&t,true));
    t.equipped_shovel=-1;t.axe_equipped=true;CHECK(!pe_training_set_axe_held(&t,true));
    t.tutorial=(PeTutorial){719,3,15,0};t.dialogue_index=2;t.position=pe_training_gathering_herb_position(0);
    pe_training_gathering_init(&t.gathering);CHECK(pe_training_gathering_enter(&t.gathering,719));
    before=t;CHECK(!pe_training_begin_herb(&t,0)&&!memcmp(&t,&before,sizeof t));
    t.tutorial=(PeTutorial){752,3,15,0};t.dialogue_index=2;
    before=t;CHECK(!pe_training_set_build_mode(&t,true)&&!memcmp(&t,&before,sizeof t));
    pe_training_close_character(&t);t.hp=0;before=t;
    CHECK(!pe_training_open_character(&t)&&!memcmp(&t,&before,sizeof t));
    t.hp=500;t.dialogue_index=0;before=t;
    CHECK(!pe_training_open_character(&t)&&!memcmp(&t,&before,sizeof t));
    CHECK(!pe_training_open_character(NULL));pe_training_close_character(NULL);
    pe_scene_free(scene);
}
int pe_test_training(void){failures=0;synthetic_tests();long_fall_test();original_room_test();extended_lesson_tests();herb_tests();tree_craft_tests();terrain_recovery_tests();health_tests();build_lesson_tests();character_panel_tests();return failures;}
