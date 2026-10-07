#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_adventure_dialogue.h"
#include "pe_adventure_medicine.h"
#include "pe_adventure_hunt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"adventure dialogue:%d: %s\n",__LINE__,#c);failures++;}}while(0)
/* CHECK is nonfatal. Missing dialogue/content must produce a failed assertion,
 * not a null dereference that hides the original failed prerequisite. */
static bool talk_is(const PeAdventureDialogueLine *line,int id){return line&&line->talk_id==id;}
static bool text_is(const char *actual,const char *expected){return actual&&strcmp(actual,expected)==0;}
static const char *line_text(int id){const PeAdventureDialogueLine *line=pe_adventure_dialogue_line(id);return line?line->text:NULL;}
static void initialize(PeGame *g,PeAdventureDialogue *d){CHECK(pe_game_init_mode(g,666,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(g));pe_adventure_dialogue_init(d);}
static void arrival(PeGame *g,PeAdventureDialogue *d){
    initialize(g,d);PeInventory inventory=g->inventory;
    g->player=(PeVec3){37,20,32};PeVec3 supplier={32.9f,0,32.9f};
    CHECK(!pe_adventure_arrival_step(g,supplier)); /* target truncates to32, strict radius5 */
    g->player.x=36.999f;g->player.y=-.01f;CHECK(!pe_adventure_arrival_step(g,supplier));
    g->player.y=20;g->hp=0;CHECK(!pe_adventure_arrival_step(g,supplier));g->hp=500;
    CHECK(!pe_adventure_arrival_step(g,(PeVec3){NAN,0,0}));
    CHECK(pe_adventure_dialogue_open(d,g,2,20016,true));
    CHECK(d->stage==PE_ADVENTURE_DIALOGUE_MENU&&!pe_adventure_dialogue_current(d,g));
    CHECK(!pe_adventure_dialogue_choose_supply(d,g));
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED);
    CHECK(!g->arrival_complete&&!g->supplies_claimed&&memcmp(&inventory,&g->inventory,sizeof inventory)==0);
    CHECK(pe_adventure_arrival_step(g,supplier));CHECK(g->arrival_complete&&!g->supplies_claimed);
    CHECK(!pe_adventure_arrival_step(g,supplier)&&memcmp(&inventory,&g->inventory,sizeof inventory)==0);
    CHECK(g->adventure_mission==9032&&g->known_recipe_count==0);
    CHECK(pe_adventure_dialogue_current(d,g)&&talk_is(pe_adventure_dialogue_current(d,g),4089));
    CHECK(!g->supplies_claimed);pe_adventure_dialogue_cancel(d);
}
static void close_only_menu(PeGame *g,PeAdventureDialogue *d){
    initialize(g,d);PeInventory inventory=g->inventory;
    g->hp=0;CHECK(!pe_adventure_dialogue_open(d,g,2,20016,true));
    g->hp=NAN;CHECK(!pe_adventure_dialogue_open(d,g,2,20016,true));g->hp=500;
    CHECK(!pe_adventure_dialogue_open(d,g,2,20016,false));
    CHECK(!pe_adventure_dialogue_open(d,g,3,20016,true));
    CHECK(!pe_adventure_dialogue_open(d,g,2,0,true));
    g->mode=PE_GAME_LEGACY;CHECK(!pe_adventure_dialogue_open(d,g,2,20016,true));g->mode=PE_GAME_ADVENTURE;
    g->adventure_mission=9033;CHECK(!pe_adventure_dialogue_open(d,g,2,20016,true));g->adventure_mission=PE_ADVENTURE_ARRIVAL;
    for(int repeat=0;repeat<2;repeat++){
        CHECK(pe_adventure_dialogue_open(d,g,2,20016,true));
        CHECK(!pe_adventure_dialogue_current(d,g)&&!pe_adventure_dialogue_choose_supply(d,g));
        CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED);
        pe_adventure_dialogue_cancel(d);CHECK(d->stage==PE_ADVENTURE_DIALOGUE_IDLE);
        CHECK(!g->arrival_complete&&!g->supplies_claimed&&g->adventure_mission==PE_ADVENTURE_ARRIVAL);
        CHECK(memcmp(&inventory,&g->inventory,sizeof inventory)==0);
    }
    /* Both persistent conditions are required, even for malformed host state. */
    g->adventure_mission=PE_ADVENTURE_SUPPLIES;
    CHECK(pe_adventure_dialogue_open(d,g,2,20016,true));CHECK(!pe_adventure_dialogue_choose_supply(d,g));
    g->arrival_complete=true;g->adventure_mission=PE_ADVENTURE_ARRIVAL;
    CHECK(!pe_adventure_dialogue_choose_supply(d,g)&&!pe_adventure_dialogue_current(d,g));
    CHECK(memcmp(&inventory,&g->inventory,sizeof inventory)==0);
}
static void pages(PeGame *g,PeAdventureDialogue *d){
    initialize(g,d);CHECK(pe_game_arrival_complete(g));PeInventory inventory=g->inventory;
    CHECK(!pe_adventure_dialogue_open(d,g,3,20016,true));CHECK(!pe_adventure_dialogue_open(d,g,2,0,true));
    CHECK(!pe_adventure_dialogue_open(d,g,2,20016,false));
    CHECK(pe_adventure_dialogue_open(d,g,2,20016,true)&&d->stage==PE_ADVENTURE_DIALOGUE_MENU&&d->entity_id==20016);
    CHECK(!pe_adventure_dialogue_open(d,g,2,20017,true));CHECK(d->entity_id==20016);
    const PeAdventureDialogueLine *line=pe_adventure_dialogue_current(d,g);
    CHECK(line&&line->talk_id==4089&&line->speaker==PE_ADVENTURE_SPEAKER_PLAYER);
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED);
    CHECK(pe_adventure_dialogue_choose_supply(d,g));CHECK(!pe_adventure_dialogue_choose_supply(d,g));
    for(int page=0;page<5;page++){
        line=pe_adventure_dialogue_current(d,g);CHECK(line&&line->talk_id==4090+page);
        CHECK(line&&line->speaker==(page%2?PE_ADVENTURE_SPEAKER_PLAYER:PE_ADVENTURE_SPEAKER_SUPPLIER));
        CHECK(memcmp(&inventory,&g->inventory,sizeof inventory)==0&&!g->supplies_claimed);
        if(page<4)CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED);
    }
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ACCEPTED);
    CHECK(g->supplies_claimed&&d->stage==PE_ADVENTURE_DIALOGUE_IDLE&&!pe_adventure_dialogue_current(d,g));
    for(size_t i=0;i<pe_adventure_supplies_count;i++)CHECK(pe_inventory_count(&g->inventory,pe_adventure_supplies[i].item_id)==
        pe_inventory_count(&inventory,pe_adventure_supplies[i].item_id)+pe_adventure_supplies[i].count);
    inventory=g->inventory;CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED);
    CHECK(pe_adventure_dialogue_open(d,g,2,20016,true)&&!pe_adventure_dialogue_current(d,g));
    CHECK(!pe_adventure_dialogue_choose_supply(d,g)&&memcmp(&inventory,&g->inventory,sizeof inventory)==0);
}
static void cancellation_capacity(PeGame *g,PeAdventureDialogue *d){
    initialize(g,d);CHECK(pe_game_arrival_complete(g));PeInventory initial=g->inventory;
    for(int stop=0;stop<5;stop++){
        CHECK(pe_adventure_dialogue_open(d,g,2,20016,true));CHECK(pe_adventure_dialogue_choose_supply(d,g));
        for(int page=0;page<stop;page++)CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED);
        pe_adventure_dialogue_cancel(d);CHECK(d->stage==PE_ADVENTURE_DIALOGUE_IDLE&&!g->supplies_claimed);
        CHECK(memcmp(&initial,&g->inventory,sizeof initial)==0);
    }
    CHECK(pe_adventure_dialogue_open(d,g,2,20016,true));CHECK(pe_adventure_dialogue_choose_supply(d,g));
    for(int page=0;page<4;page++)CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED);
    int added=0;while(pe_inventory_first_empty(&g->inventory,308)>=0){bool inserted=pe_inventory_add(&g->inventory,308,1);CHECK(inserted);if(!inserted)break;added++;}PeInventory full=g->inventory;
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_NO_SPACE);
    CHECK(d->page==4&&d->stage==PE_ADVENTURE_DIALOGUE_TALK&&!g->supplies_claimed&&strstr(g->message,"room"));
    CHECK(memcmp(&full,&g->inventory,sizeof full)==0);
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_NO_SPACE&&memcmp(&full,&g->inventory,sizeof full)==0);
    g->hp=0;CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED&&!pe_adventure_dialogue_current(d,g));
    CHECK(d->page==4&&!g->supplies_claimed);g->hp=500;
    pe_adventure_dialogue_cancel(d);CHECK(pe_inventory_remove(&g->inventory,308,added));
    CHECK(pe_adventure_dialogue_open(d,g,2,20016,true));CHECK(pe_adventure_dialogue_choose_supply(d,g));
    CHECK(talk_is(pe_adventure_dialogue_current(d,g),4090));
    for(int page=0;page<4;page++)CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED);
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ACCEPTED);
    /* Loaded persistent grant state prevents reopening after transient reset. */
    char path[]="/tmp/pe-adventure-dialogue-XXXXXX",error[200];int fd=mkstemp(path);CHECK(fd>=0);
    if(fd>=0){close(fd);CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(g,path,error,sizeof error));
        pe_adventure_dialogue_init(d);CHECK(pe_adventure_dialogue_open(d,g,2,20016,true));
        CHECK(!pe_adventure_dialogue_choose_supply(d,g)&&!pe_adventure_dialogue_current(d,g));unlink(path);}
    /* An external authoritative acceptance makes an existing UI page stale. */
    initialize(g,d);CHECK(pe_game_arrival_complete(g));CHECK(pe_adventure_dialogue_open(d,g,2,20016,true));
    CHECK(pe_adventure_dialogue_choose_supply(d,g));CHECK(pe_game_accept_supplies(g));full=g->inventory;
    CHECK(!pe_adventure_dialogue_current(d,g)&&pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED);
    CHECK(memcmp(&full,&g->inventory,sizeof full)==0);
}
static void text_tests(void){
    PeAdventureDialogueBindings bindings={.player_name="Player One",.welcome_name="Generated Welcome",.supplier_name="Generated Supplier",.town_name="Generated Town"};char output[2048]={0};
    CHECK(text_is(pe_adventure_mission_title(9031),"Arrival"));
    CHECK(text_is(pe_adventure_mission_title(9032),"The Delayed Supplies")&&!pe_adventure_mission_title(999));
    CHECK(pe_adventure_dialogue_format(pe_adventure_mission_objective(9031),&bindings,output,sizeof output));
    CHECK(strcmp(output,"Talk to Generated Supplier")==0);
    CHECK(pe_adventure_dialogue_format(line_text(4089),&bindings,output,sizeof output));
    CHECK(strcmp(output,"Generated Welcome said something about supplies?")==0);
    CHECK(pe_adventure_dialogue_format(line_text(4090),&bindings,output,sizeof output));
    CHECK(strstr(output,"Generated Town")&&!strstr(output,"Eitai")&&!strstr(output,"Annot"));
    CHECK(text_is(pe_adventure_dialogue_speaker(pe_adventure_dialogue_line(4091),&bindings),"Player One"));
    CHECK(text_is(pe_adventure_dialogue_speaker(pe_adventure_dialogue_line(4092),&bindings),"Generated Supplier"));
    bindings.supplier_full_name="Generated Supplier Family";
    CHECK(text_is(pe_adventure_dialogue_speaker(pe_adventure_dialogue_line(4092),&bindings),"Generated Supplier Family"));
    CHECK(pe_adventure_dialogue_format("\"name%\" / \"AdvNPC%\"002 / \"AdvNPC%\"002",&bindings,output,sizeof output));
    CHECK(strcmp(output,"Player One / Generated Supplier / Generated Supplier")==0);
    bindings.supplier_name=NULL;CHECK(pe_adventure_dialogue_format(pe_adventure_mission_objective(9031),&bindings,output,sizeof output));
    CHECK(strstr(output,"\"AdvNPC%\"002"));
    strcpy(output,"untouched");CHECK(!pe_adventure_dialogue_format("long output",&bindings,output,4));CHECK(strcmp(output,"untouched")==0);
    bindings.player_name="bad\nname";CHECK(!pe_adventure_dialogue_format("text",&bindings,output,sizeof output));CHECK(strcmp(output,"untouched")==0);
    CHECK(!pe_adventure_dialogue_line(0));
}
static void medicine(PeGame *g,PeAdventureDialogue *d){
    initialize(g,d);CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));g->annot_complete=true;
    for(int slot=0;slot<pe_inventory_capacity(&g->inventory);slot++)if(g->inventory.slots[slot].item_id==552||g->inventory.slots[slot].item_id==512)CHECK(pe_game_use_slot(g,slot));
    PeInventory before=g->inventory;
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));CHECK(talk_is(pe_adventure_dialogue_current(d,g),4095));
    CHECK(pe_adventure_dialogue_choose_supply(d,g));CHECK(talk_is(pe_adventure_dialogue_current(d,g),4096));
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED&&d->stage==PE_ADVENTURE_DIALOGUE_CHOICE);
    CHECK(talk_is(pe_adventure_dialogue_choice(d,g,0),4097)&&talk_is(pe_adventure_dialogue_choice(d,g,1),4098)&&!pe_adventure_dialogue_choice(d,g,2));
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED);
    CHECK(pe_adventure_dialogue_choose_medicine(d,g,false)==PE_ADVENTURE_DIALOGUE_ACCEPTED);
    CHECK(!memcmp(&before,&g->inventory,sizeof before)&&g->medicine_state==PE_MEDICINE_NONE);
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));CHECK(pe_adventure_dialogue_choose_supply(d,g));pe_adventure_dialogue_advance(d,g);
    pe_adventure_dialogue_cancel(d);CHECK(g->medicine_state==PE_MEDICINE_NONE);
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));CHECK(pe_adventure_dialogue_choose_supply(d,g));pe_adventure_dialogue_advance(d,g);
    PeInventory before_accept=g->inventory;while(pe_inventory_add(&g->inventory,308,1)){}PeInventory full=g->inventory;
    CHECK(pe_adventure_dialogue_choose_medicine(d,g,true)==PE_ADVENTURE_DIALOGUE_NO_SPACE&&d->stage==PE_ADVENTURE_DIALOGUE_CHOICE&&g->medicine_state==PE_MEDICINE_NONE);
    CHECK(!memcmp(&full,&g->inventory,sizeof full));g->inventory=before_accept;
    CHECK(pe_adventure_dialogue_choose_medicine(d,g,true)==PE_ADVENTURE_DIALOGUE_ACCEPTED&&g->medicine_state==PE_MEDICINE_ACTIVE);
    CHECK(pe_adventure_dialogue_choose_medicine(d,g,true)==PE_ADVENTURE_DIALOGUE_REJECTED&&pe_inventory_count(&g->inventory,877)==1);
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));CHECK(talk_is(pe_adventure_dialogue_current(d,g),4099));
    CHECK(!pe_adventure_dialogue_choose_supply(d,g));CHECK(pe_inventory_add(&g->inventory,917,5));
    CHECK(pe_adventure_dialogue_choose_supply(d,g));CHECK(talk_is(pe_adventure_dialogue_current(d,g),4100));
    CHECK(pe_inventory_remove(&g->inventory,917,1));CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED&&g->medicine_state==PE_MEDICINE_ACTIVE);
    CHECK(pe_inventory_add(&g->inventory,917,1));PeInventory before_delivery=g->inventory;while(pe_inventory_add(&g->inventory,308,1)){}full=g->inventory;
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_NO_SPACE&&d->stage==PE_ADVENTURE_DIALOGUE_TALK&&d->page==1&&g->medicine_state==PE_MEDICINE_ACTIVE);
    CHECK(!memcmp(&full,&g->inventory,sizeof full));g->inventory=before_delivery;
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ACCEPTED);
    CHECK(g->medicine_state==PE_MEDICINE_COMPLETE&&pe_inventory_count(&g->inventory,917)==0);
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));CHECK(talk_is(pe_adventure_dialogue_current(d,g),4101));
    CHECK(text_is(pe_adventure_mission_title(9072),"Herb Collecting"));
    char text[2048];PeAdventureDialogueBindings names={.item_name="Medicine",.item_count="5"};
    CHECK(pe_adventure_dialogue_format(pe_adventure_mission_objective(9072),&names,text,sizeof text));CHECK(strcmp(text,"Gather 5 units of Medicine")==0);
}
static void hunt_offer(PeGame *g,PeAdventureDialogue *d){
    /* Continue the real medicine delivery above. Source9074 offers9075, but
     * the current capture has no supported hunt target: no invented acceptance. */
    CHECK(g->medicine_state==PE_MEDICINE_COMPLETE&&g->hunt_state==PE_HUNT_NONE);
    PeInventory inventory=g->inventory;uint32_t money=g->money;
    int known=g->known_recipe_count;uint32_t equipped=g->equipped_instance_id;
    PeVec3 destination=g->hunt_destination;
    pe_adventure_dialogue_cancel(d);
    for(int retry=0;retry<2;retry++){
        CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));
        const PeAdventureDialogueLine *line=pe_adventure_dialogue_current(d,g);
        CHECK(line&&line->talk_id==4101&&line->speaker==PE_ADVENTURE_SPEAKER_PLAYER);
        CHECK(pe_adventure_dialogue_choose_supply(d,g));line=pe_adventure_dialogue_current(d,g);
        CHECK(line&&line->talk_id==4102&&line->speaker==PE_ADVENTURE_SPEAKER_ANNOT);
        CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED);
        CHECK(talk_is(pe_adventure_dialogue_choice(d,g,0),4103)&&talk_is(pe_adventure_dialogue_choice(d,g,1),4104));
        CHECK(!pe_adventure_dialogue_choice(d,g,2));
        CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED);
        if(retry){
            CHECK(!pe_adventure_hunt_available(g));
            for(int blocked=0;blocked<2;blocked++){
                CHECK(pe_adventure_dialogue_choose_medicine(d,g,true)==PE_ADVENTURE_DIALOGUE_UNAVAILABLE);
                CHECK(d->stage==PE_ADVENTURE_DIALOGUE_CHOICE&&d->mission_id==9074);
                CHECK(g->message[0]&&g->hunt_state==PE_HUNT_NONE&&g->medicine_state==PE_MEDICINE_COMPLETE);
                CHECK(!memcmp(&g->hunt_destination,&destination,sizeof destination));
                CHECK(!memcmp(&g->inventory,&inventory,sizeof inventory)&&g->money==money);
                CHECK(g->known_recipe_count==known&&g->equipped_instance_id==equipped);
            }
        }
        CHECK(pe_adventure_dialogue_choose_medicine(d,g,false)==PE_ADVENTURE_DIALOGUE_ACCEPTED);
        CHECK(d->stage==PE_ADVENTURE_DIALOGUE_IDLE&&g->hunt_state==PE_HUNT_NONE);
        CHECK(!memcmp(&g->inventory,&inventory,sizeof inventory)&&g->money==money);
    }
    /* A stale choice must not switch quests when progression changes outside
     * the dialog. Accepted/pending hunt stages must not offer another hunt. */
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));CHECK(pe_adventure_dialogue_choose_supply(d,g));
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED);
    g->medicine_state=PE_MEDICINE_NONE;
    CHECK(!pe_adventure_dialogue_current(d,g)&&!pe_adventure_dialogue_choice(d,g,0));
    CHECK(pe_adventure_dialogue_choose_medicine(d,g,true)==PE_ADVENTURE_DIALOGUE_REJECTED);
    CHECK(!memcmp(&g->inventory,&inventory,sizeof inventory));g->medicine_state=PE_MEDICINE_COMPLETE;
    g->hp=0;CHECK(!pe_adventure_dialogue_current(d,g));
    CHECK(pe_adventure_dialogue_choose_medicine(d,g,true)==PE_ADVENTURE_DIALOGUE_REJECTED);g->hp=500;
    pe_adventure_dialogue_cancel(d);g->medicine_state=PE_MEDICINE_NONE;
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));CHECK(pe_adventure_dialogue_choose_supply(d,g));
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED);
    g->medicine_state=PE_MEDICINE_COMPLETE;
    CHECK(!pe_adventure_dialogue_current(d,g));
    CHECK(pe_adventure_dialogue_choose_medicine(d,g,true)==PE_ADVENTURE_DIALOGUE_REJECTED);
    for(unsigned state=PE_HUNT_TRAVEL;state<=PE_HUNT_ENCOUNTER_PENDING;state++){
        pe_adventure_dialogue_cancel(d);g->hunt_state=state;
        CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));
        CHECK(!pe_adventure_dialogue_current(d,g)&&!pe_adventure_dialogue_choose_supply(d,g));
    }
    g->hunt_state=PE_HUNT_NONE;pe_adventure_dialogue_cancel(d);
}
static void hunt_return(PeGame *g,PeAdventureDialogue *d){
    /* Model restored, already-rewarded return progress. The authoritative
     * arrival/reward transaction has separate route and capacity coverage. */
    CHECK(pe_adventure_hunt_reference(&g->hunt_destination));
    g->hunt_kills=4;g->hunt_state=PE_HUNT_RETURN_TALK1;
    CHECK(pe_adventure_hunt_state_valid(g));
    PeInventory inventory=g->inventory;uint32_t money=g->money;int known=g->known_recipe_count;
    for(int retry=0;retry<2;retry++){
        CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));
        const PeAdventureDialogueLine *line=pe_adventure_dialogue_current(d,g);
        CHECK(d->stage==PE_ADVENTURE_DIALOGUE_TALK&&d->mission_id==9127&&d->page==0);
        CHECK(line&&line->talk_id==4105&&line->speaker==PE_ADVENTURE_SPEAKER_PLAYER);
        CHECK(!pe_adventure_dialogue_choose_supply(d,g)&&!pe_adventure_dialogue_choice(d,g,0));
        CHECK(pe_adventure_dialogue_choose_medicine(d,g,true)==PE_ADVENTURE_DIALOGUE_REJECTED);
        pe_adventure_dialogue_cancel(d);CHECK(g->hunt_state==PE_HUNT_RETURN_TALK1);
        CHECK(!memcmp(&inventory,&g->inventory,sizeof inventory));
    }
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));PeAdventureDialogue stale=*d;
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED);
    CHECK(g->hunt_state==PE_HUNT_RETURN_TALK2&&d->page==1);
    const PeAdventureDialogueLine *line=pe_adventure_dialogue_current(d,g);
    CHECK(line&&line->talk_id==4106&&line->speaker==PE_ADVENTURE_SPEAKER_ANNOT);
    CHECK(!pe_adventure_dialogue_current(&stale,g));
    CHECK(pe_adventure_dialogue_advance(&stale,g)==PE_ADVENTURE_DIALOGUE_REJECTED&&g->hunt_state==PE_HUNT_RETURN_TALK2);
    pe_adventure_dialogue_cancel(d);CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));
    CHECK(d->page==1&&talk_is(pe_adventure_dialogue_current(d,g),4106));
    g->hp=0;CHECK(!pe_adventure_dialogue_current(d,g));
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED&&g->hunt_state==PE_HUNT_RETURN_TALK2);
    g->hp=500;CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ACCEPTED);
    CHECK(g->hunt_state==PE_HUNT_COMPLETE&&d->stage==PE_ADVENTURE_DIALOGUE_IDLE);
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED);
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));
    CHECK(!pe_adventure_dialogue_current(d,g)&&!pe_adventure_dialogue_choose_supply(d,g)); /*9077 unported*/
    CHECK(!memcmp(&inventory,&g->inventory,sizeof inventory)&&g->money==money&&g->known_recipe_count==known);
    pe_adventure_dialogue_cancel(d);
}
static void hunt_reward_learning(PeGame *g,PeAdventureDialogue *d){
    initialize(g,d);
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id,.categorized=g->mode==PE_GAME_ADVENTURE};g->equipped_instance_id=0;
    memset(g->quickslots,0,sizeof g->quickslots);
    CHECK(pe_inventory_add(&g->inventory,592,2)&&pe_inventory_add(&g->inventory,594,1));
    const int formulas[]={187,42,41,51};
    for(unsigned i=0;i<4;i++)CHECK(!pe_game_recipe_known(g,formulas[i]));
    int jet=-1,glider=-1;
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++){
        if(g->inventory.slots[i].item_id==592)jet=i;
        if(g->inventory.slots[i].item_id==594)glider=i;
    }
    CHECK(jet>=0&&glider>=0&&pe_game_use_slot(g,jet));
    CHECK(g->known_recipe_count==4&&pe_inventory_count(&g->inventory,592)==1);
    for(unsigned i=0;i<4;i++)CHECK(pe_game_recipe_known(g,formulas[i]));
    CHECK(!pe_game_recipe_known(g,189)&&pe_game_use_slot(g,glider));
    CHECK(pe_game_recipe_known(g,189)&&g->known_recipe_count==5&&pe_inventory_count(&g->inventory,594)==0);
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==592)jet=i;
    PeInventory before=g->inventory;
    CHECK(!pe_game_use_slot(g,jet)&&!memcmp(&before,&g->inventory,sizeof before)&&g->known_recipe_count==5);
}
int pe_test_adventure_dialogue(void){
    failures=0;PeGame *g=malloc(sizeof *g);if(!g)return 1;PeAdventureDialogue d;
    arrival(g,&d);close_only_menu(g,&d);pages(g,&d);cancellation_capacity(g,&d);medicine(g,&d);hunt_offer(g,&d);hunt_return(g,&d);hunt_reward_learning(g,&d);text_tests();
    free(g);return failures;
}
