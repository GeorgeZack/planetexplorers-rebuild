#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_adventure_dialogue.h"
#include <math.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(e) do{if(!(e)){fprintf(stderr,"adventure welcome:%d: %s\n",__LINE__,#e);failures++;}}while(0)
int pe_test_adventure_welcome(void){
    failures=0;PeGame *g=malloc(sizeof *g),*loaded=calloc(1,sizeof *loaded);
    CHECK(g&&loaded);if(!g||!loaded){free(g);free(loaded);return failures;}
    char path[]="/tmp/pe-welcome-XXXXXX",error[200];int fd=mkstemp(path);CHECK(fd>=0);
    if(fd<0){free(g);free(loaded);return failures;}close(fd);
    CHECK(pe_game_init_mode(g,666,PE_GAME_ADVENTURE));
    CHECK(!g->adventure_mission&&!g->welcome_page&&!g->arrival_complete&&!g->supplies_claimed);
    PeInventory initial=g->inventory;PeAdventureDialogue d;pe_adventure_dialogue_init(&d);
    CHECK(!pe_adventure_dialogue_open(&d,g,1,20008,false));
    CHECK(!pe_game_welcome_advance(g,1)&&g->welcome_page==0);
    g->hp=0;CHECK(!pe_game_welcome_advance(g,0));g->hp=NAN;CHECK(!pe_game_welcome_advance(g,0));g->hp=500;
    CHECK(!pe_game_arrival_complete(g)&&!pe_game_accept_supplies(g));
    CHECK(pe_adventure_dialogue_open(&d,g,2,20016,true));
    CHECK(!pe_adventure_dialogue_current(&d,g)&&!pe_adventure_dialogue_choose_supply(&d,g));
    pe_adventure_dialogue_cancel(&d);
    for(unsigned page=0;page<PE_ADVENTURE_WELCOME_PAGES;page++){
        CHECK(pe_adventure_dialogue_open(&d,g,1,20008,true));
        const PeAdventureDialogueLine *line=pe_adventure_dialogue_current(&d,g);
        CHECK(line&&line->talk_id==(page?4080+(int)page:1949));
        CHECK(line&&line->speaker==(page%2?PE_ADVENTURE_SPEAKER_PLAYER:PE_ADVENTURE_SPEAKER_WELCOME));
        CHECK(!g->adventure_mission&&!g->arrival_complete&&!g->supplies_claimed);
        CHECK(!pe_game_arrival_complete(g)&&!pe_game_accept_supplies(g));
        CHECK(!pe_adventure_arrival_step(g,g->player));
        CHECK(!pe_adventure_dialogue_choose_supply(&d,g));
        CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));
        CHECK(loaded->welcome_page==page&&!loaded->adventure_mission);
        CHECK(!memcmp(&loaded->inventory,&initial,sizeof initial));
        pe_adventure_dialogue_cancel(&d);CHECK(d.stage==PE_ADVENTURE_DIALOGUE_IDLE);
        CHECK(g->welcome_page==page&&!memcmp(&g->inventory,&initial,sizeof initial));
        *g=*loaded;CHECK(pe_adventure_dialogue_open(&d,g,1,20008,true));
        line=pe_adventure_dialogue_current(&d,g);CHECK(line&&line->talk_id==(page?4080+(int)page:1949));
        CHECK(pe_adventure_dialogue_advance(&d,g)==(page==8?PE_ADVENTURE_DIALOGUE_ACCEPTED:PE_ADVENTURE_DIALOGUE_ADVANCED));
        CHECK(g->welcome_page==page+1&&!memcmp(&g->inventory,&initial,sizeof initial));
        CHECK(!pe_game_welcome_advance(g,page)); /* stale ACK must not skip another page */
        if(page<8)pe_adventure_dialogue_cancel(&d);
    }
    CHECK(g->adventure_mission==PE_ADVENTURE_ARRIVAL&&!g->arrival_complete&&!g->supplies_claimed);
    CHECK(d.stage==PE_ADVENTURE_DIALOGUE_IDLE&&!pe_game_welcome_advance(g,9));
    CHECK(pe_adventure_dialogue_open(&d,g,1,20008,true));
    CHECK(d.stage==PE_ADVENTURE_DIALOGUE_MENU&&!pe_adventure_dialogue_current(&d,g));
    CHECK(pe_adventure_dialogue_advance(&d,g)==PE_ADVENTURE_DIALOGUE_REJECTED);
    pe_adventure_dialogue_cancel(&d);
    PeAdventureDialogueBindings bindings={.welcome_name="Westina",.welcome_full_name="Westina Nangle",.supplier_name="Eitai",.seed_name="audit"};char text[2048];
    CHECK(pe_adventure_dialogue_format(pe_adventure_dialogue_line(1949)->text,&bindings,text,sizeof text));CHECK(strstr(text,"welcome to audit!"));
    CHECK(pe_adventure_dialogue_format(pe_adventure_dialogue_line(4088)->text,&bindings,text,sizeof text));CHECK(strstr(text,"talk with Eitai"));
    CHECK(!strcmp(pe_adventure_dialogue_speaker(pe_adventure_dialogue_line(1949),&bindings),"Westina Nangle"));
    /* Old Adventure saves have already accepted Arrival. Preserve grants/items. */
    for(unsigned phase=0;phase<3;phase++){
        if(phase==1)CHECK(pe_game_arrival_complete(g));
        if(phase==2)CHECK(pe_game_accept_supplies(g));
        PeInventory before=g->inventory;
        for(unsigned version=4;version<=5;version++){
            CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_test_save_pre_welcome(path,version));
            CHECK(pe_game_load(loaded,path,error,sizeof error));
            CHECK(loaded->welcome_page==9&&loaded->adventure_mission==g->adventure_mission);
            CHECK(loaded->arrival_complete==g->arrival_complete&&loaded->supplies_claimed==g->supplies_claimed);
            CHECK(!memcmp(loaded->inventory.slots,before.slots,sizeof before.slots));
            CHECK(loaded->inventory.next_instance_id==before.next_instance_id+PE_OUTFIT_SLOTS);
            CHECK(pe_outfit_state_valid(loaded));
            CHECK(!pe_game_welcome_advance(loaded,9));
        }
    }
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->welcome_page==9);
    g->welcome_page=8;CHECK(!pe_game_save(g,path,error,sizeof error));g->welcome_page=10;CHECK(!pe_game_save(g,path,error,sizeof error));
    pe_game_init(g,1);CHECK(!pe_game_welcome_advance(g,0));
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_test_save_pre_welcome(path,5));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->mode==PE_GAME_LEGACY&&!loaded->welcome_page);
    unlink(path);free(g);free(loaded);return failures;
}
