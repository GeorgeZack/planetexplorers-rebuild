#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_adventure_dialogue.h"
#include "pe_adventure_medicine.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"adventure dialogue:%d: %s\n",__LINE__,#c);failures++;}}while(0)
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
    CHECK(pe_adventure_dialogue_current(d,g)&&pe_adventure_dialogue_current(d,g)->talk_id==4089);
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
    CHECK(pe_inventory_add(&g->inventory,308,2));PeInventory full=g->inventory;
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_NO_SPACE);
    CHECK(d->page==4&&d->stage==PE_ADVENTURE_DIALOGUE_TALK&&!g->supplies_claimed&&strstr(g->message,"room"));
    CHECK(memcmp(&full,&g->inventory,sizeof full)==0);
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_NO_SPACE&&memcmp(&full,&g->inventory,sizeof full)==0);
    g->hp=0;CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED&&!pe_adventure_dialogue_current(d,g));
    CHECK(d->page==4&&!g->supplies_claimed);g->hp=500;
    pe_adventure_dialogue_cancel(d);CHECK(pe_inventory_remove(&g->inventory,308,2));
    CHECK(pe_adventure_dialogue_open(d,g,2,20016,true));CHECK(pe_adventure_dialogue_choose_supply(d,g));
    CHECK(pe_adventure_dialogue_current(d,g)->talk_id==4090);
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
    PeAdventureDialogueBindings bindings={.player_name="Player One",.welcome_name="Generated Welcome",.supplier_name="Generated Supplier",.town_name="Generated Town"};char output[2048];
    CHECK(strcmp(pe_adventure_mission_title(9031),"Arrival")==0);
    CHECK(strcmp(pe_adventure_mission_title(9032),"The Delayed Supplies")==0&&!pe_adventure_mission_title(999));
    CHECK(pe_adventure_dialogue_format(pe_adventure_mission_objective(9031),&bindings,output,sizeof output));
    CHECK(strcmp(output,"Talk to Generated Supplier")==0);
    CHECK(pe_adventure_dialogue_format(pe_adventure_dialogue_line(4089)->text,&bindings,output,sizeof output));
    CHECK(strcmp(output,"Generated Welcome said something about supplies?")==0);
    CHECK(pe_adventure_dialogue_format(pe_adventure_dialogue_line(4090)->text,&bindings,output,sizeof output));
    CHECK(strstr(output,"Generated Town")&&!strstr(output,"Eitai")&&!strstr(output,"Annot"));
    CHECK(strcmp(pe_adventure_dialogue_speaker(pe_adventure_dialogue_line(4091),&bindings),"Player One")==0);
    CHECK(strcmp(pe_adventure_dialogue_speaker(pe_adventure_dialogue_line(4092),&bindings),"Generated Supplier")==0);
    bindings.supplier_full_name="Generated Supplier Family";
    CHECK(strcmp(pe_adventure_dialogue_speaker(pe_adventure_dialogue_line(4092),&bindings),"Generated Supplier Family")==0);
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
    for(int slot=0;slot<PE_INVENTORY_SLOTS;slot++)if(g->inventory.slots[slot].item_id==552||g->inventory.slots[slot].item_id==512)CHECK(pe_game_use_slot(g,slot));
    PeInventory before=g->inventory;
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));CHECK(pe_adventure_dialogue_current(d,g)->talk_id==4095);
    CHECK(pe_adventure_dialogue_choose_supply(d,g));CHECK(pe_adventure_dialogue_current(d,g)->talk_id==4096);
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED&&d->stage==PE_ADVENTURE_DIALOGUE_CHOICE);
    CHECK(pe_adventure_dialogue_choice(d,g,0)->talk_id==4097&&pe_adventure_dialogue_choice(d,g,1)->talk_id==4098&&!pe_adventure_dialogue_choice(d,g,2));
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
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));CHECK(pe_adventure_dialogue_current(d,g)->talk_id==4099);
    CHECK(!pe_adventure_dialogue_choose_supply(d,g));CHECK(pe_inventory_add(&g->inventory,917,5));
    CHECK(pe_adventure_dialogue_choose_supply(d,g));CHECK(pe_adventure_dialogue_current(d,g)->talk_id==4100);
    CHECK(pe_inventory_remove(&g->inventory,917,1));CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_REJECTED&&g->medicine_state==PE_MEDICINE_ACTIVE);
    CHECK(pe_inventory_add(&g->inventory,917,1));PeInventory before_delivery=g->inventory;while(pe_inventory_add(&g->inventory,308,1)){}full=g->inventory;
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_NO_SPACE&&d->stage==PE_ADVENTURE_DIALOGUE_TALK&&d->page==1&&g->medicine_state==PE_MEDICINE_ACTIVE);
    CHECK(!memcmp(&full,&g->inventory,sizeof full));g->inventory=before_delivery;
    CHECK(pe_adventure_dialogue_advance(d,g)==PE_ADVENTURE_DIALOGUE_ACCEPTED);
    CHECK(g->medicine_state==PE_MEDICINE_COMPLETE&&pe_inventory_count(&g->inventory,917)==0);
    CHECK(pe_adventure_dialogue_open(d,g,3,20018,true));CHECK(!pe_adventure_dialogue_current(d,g)&&!pe_adventure_dialogue_choose_supply(d,g));
    CHECK(strcmp(pe_adventure_mission_title(9072),"Herb Collecting")==0);
    char text[2048];PeAdventureDialogueBindings names={.item_name="Medicine",.item_count="5"};
    CHECK(pe_adventure_dialogue_format(pe_adventure_mission_objective(9072),&names,text,sizeof text));CHECK(strcmp(text,"Gather 5 units of Medicine")==0);
}
int pe_test_adventure_dialogue(void){
    failures=0;PeGame *g=malloc(sizeof *g);if(!g)return 1;PeAdventureDialogue d;
    arrival(g,&d);close_only_menu(g,&d);pages(g,&d);cancellation_capacity(g,&d);medicine(g,&d);text_tests();
    free(g);return failures;
}
