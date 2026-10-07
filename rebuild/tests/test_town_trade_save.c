#define _POSIX_C_SOURCE 200809L
#include "pe_town_trade.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if(!(c)){fprintf(stderr,"town trade lifecycle:%d %s\n",__LINE__,#c);failures++;} } while(0)
static int slot_for(const PeGame *g,int item){
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].count&&g->inventory.slots[i].item_id==item)return i;
    return -1;
}
int pe_test_town_trade_save(void){
    failures=0;
    char path[1024],error[256];
    snprintf(path,sizeof path,"%s/pe-town-trade-XXXXXX",getenv("TMPDIR")?getenv("TMPDIR"):"/tmp");
    int fd=mkstemp(path);if(fd>=0)close(fd);
    PeGame *g=calloc(1,sizeof *g),*copy=calloc(1,sizeof *copy),*moved=calloc(1,sizeof *moved);
    PeWorld *world=calloc(1,sizeof *world);
    CHECK(fd>=0&&g&&copy&&moved&&world);
    if(fd<0||!g||!copy||!moved||!world)goto done;
    CHECK(pe_game_init_mode(g,718,PE_GAME_ADVENTURE));
    CHECK(pe_inventory_add(&g->inventory,12,1));
    int slot=slot_for(g,12);CHECK(slot>=0);if(slot<0)goto done;
    g->inventory.slots[slot].durability=12.5f;
    PeSlot sold=g->inventory.slots[slot];
    CHECK(pe_town_trade_sell(g,20002,slot,1));
    CHECK(pe_town_trade_count(g,20002)==1);
    /* Owning game copies retain a session's repurchase pool, with independent
     * worlds. World import retains inventory and therefore this owner too. */
    CHECK(pe_game_clone(copy,g));
    CHECK(pe_town_trade_count(copy,20002)==1);
    const PeSlot *entry=pe_town_trade_at(copy,20002,0);
    CHECK(entry&&entry->instance_id==sold.instance_id&&entry->durability==sold.durability);
    CHECK(pe_world_resize(world,80,32,80));
    for(int z=0;z<80;z++)for(int y=0;y<=8;y++)for(int x=0;x<80;x++)
        CHECK(pe_world_set(world,x,y,z,(PeVoxel){255,PE_MAT_DIRT}));
    CHECK(pe_game_set_world(copy,world,(PeVec3){30,10,30}));
    CHECK(pe_town_trade_state_valid(copy)&&pe_town_trade_count(copy,20002)==1);
    pe_game_move(moved,copy);
    CHECK(pe_town_trade_count(moved,20002)==1&&pe_town_trade_count(copy,20002)==0);
    CHECK(pe_game_save(moved,path,error,sizeof error));
    uint32_t money=moved->money,next=moved->inventory.next_instance_id;
    PeTownWallet wallet=moved->town_wallet[0];
    /* Save does not end the live session. Failed load must retain it too. */
    CHECK(pe_town_trade_count(moved,20002)==1);
    char missing[1100];snprintf(missing,sizeof missing,"%s.missing",path);
    CHECK(!pe_game_load(moved,missing,error,sizeof error));
    CHECK(pe_town_trade_count(moved,20002)==1&&moved->money==money);
    CHECK(pe_game_load(copy,path,error,sizeof error));
    CHECK(pe_town_trade_count(copy,20002)==0&&copy->money==money&&copy->inventory.next_instance_id==next);
    CHECK(copy->town_wallet[0].balance==wallet.balance&&copy->town_wallet[0].added==wallet.added);
    CHECK(slot_for(copy,12)<0); /* Sold item must not reappear on reload. */
    /* New transient state must not change the v17 wire format: resaving the
     * loaded checkpoint reproduces the same authoritative payload exactly. */
    FILE *f=fopen(path,"rb");unsigned char *before=NULL;long length=0;
    CHECK(f!=NULL);
    if(f){CHECK(fseek(f,0,SEEK_END)==0);length=ftell(f);rewind(f);if(length>0)before=malloc((size_t)length);
        CHECK(before!=NULL);if(before)CHECK(fread(before,1,(size_t)length,f)==(size_t)length);fclose(f);}
    CHECK(pe_game_save(copy,path,error,sizeof error));
    f=fopen(path,"rb");CHECK(f!=NULL);
    if(f&&before){for(long i=0;i<length;i++)CHECK(fgetc(f)==before[i]);CHECK(fgetc(f)==EOF);}
    if(f)fclose(f);
    free(before);
    /* The original live session can still buy its exact worn instance back. */
    CHECK(pe_town_trade_buyback(moved,20002,0,1));
    slot=slot_for(moved,12);CHECK(slot>=0);
    if(slot>=0)CHECK(moved->inventory.slots[slot].instance_id==sold.instance_id&&moved->inventory.slots[slot].durability==12.5f);
    CHECK(pe_town_trade_count(g,20002)==1); /* independent clone */
    CHECK(pe_game_load(g,path,error,sizeof error)&&pe_town_trade_count(g,20002)==0);
done:
    if(fd>=0)unlink(path);
    pe_world_destroy(world);free(world);
    pe_game_destroy(g);free(g);pe_game_destroy(copy);free(copy);pe_game_destroy(moved);free(moved);
    return failures;
}
