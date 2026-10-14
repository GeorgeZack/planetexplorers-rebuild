#define _POSIX_C_SOURCE 200809L
#include "pe_game.h"
#include "../generated/pe_hunger_source.h"
#include "../generated/pe_stamina_recovery_source.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hunger game:%d %s\n",__LINE__,#c);failures++;}}while(0)
static uint32_t get32(const unsigned char*p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(unsigned char*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static uint32_t bits(float f){uint32_t n;memcpy(&n,&f,4);return n;}
static unsigned char*read_bytes(const char*path,size_t*n){
    FILE*f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long length=ftell(f);
    if(length<20||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char*p=malloc((size_t)length+1);if(!p){fclose(f);return NULL;}
    bool ok=fread(p,1,(size_t)length,f)==(size_t)length;if(fclose(f))ok=false;
    if(!ok){free(p);return NULL;}*n=(size_t)length;return p;
}
static bool write_bytes(const char*path,unsigned char*p,size_t n,unsigned version){
    put32(p+4,version);put32(p+8,(uint32_t)n-16);uint32_t sum=2166136261u;
    for(size_t i=16;i<n;i++){sum^=p[i];sum*=16777619u;}put32(p+12,sum);
    FILE*f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(p,1,n,f)==n;return fclose(f)==0&&ok;
}
static int slot(const PeGame*g,int id){for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].count&&g->inventory.slots[i].item_id==id)return i;return -1;}
static int count(const PeGame*g,int id){int n=0;for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==id)n+=g->inventory.slots[i].count;return n;}

int pe_test_hunger_game(void){
    failures=0;char path[]="/tmp/pe-hunger-save-XXXXXX",error[256];int fd=mkstemp(path);if(fd>=0)close(fd);
    PeGame*g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*before=malloc(sizeof *before);
    unsigned char*bytes=NULL;size_t n=0;
    CHECK(fd>=0&&g&&loaded&&before);if(fd<0||!g||!loaded||!before)goto done;
    CHECK(PE_SAVE_VERSION==23&&PE_SAVE_V23_APPEND==8);

    /* A fresh Adventure player starts at the prototype's hunger. */
    CHECK(pe_game_init_mode(g,77,PE_GAME_ADVENTURE));CHECK(g->hunger.hunger==300&&!g->hunger.starving);
    CHECK(pe_game_stamina_reduce_percent(g)==1);

    /* Eating: one food used, raw hunger up, the 4 s group cooldown shown and enforced. */
    CHECK(pe_inventory_add(&g->inventory,386,2)&&pe_inventory_add(&g->inventory,384,1));
    CHECK(pe_game_item_is_consumable(g,386)&&!pe_game_item_is_consumable(g,33));
    g->hunger.initialized=true;   /* a first attribute frame has run */
    CHECK(pe_game_use_slot(g,slot(g,386)));CHECK(count(g,386)==1&&g->hunger.raw==380&&strstr(g->message,"eaten"));
    CHECK(pe_game_item_cooldown(g,384)==4&&pe_game_item_cooldown(g,386)==4);
    memcpy(before,g,sizeof *g);CHECK(!pe_game_use_slot(g,slot(g,384)));CHECK(count(g,384)==1&&g->hunger.raw==380);
    g->hunger.food_cooldown=0;CHECK(pe_game_quickslot_assign(g,0,slot(g,384))&&pe_game_quickslot_use(g,0));
    CHECK(count(g,384)==0&&g->hunger.raw==410);
    /* A dead player cannot eat. */
    g->hunger.food_cooldown=0;g->hp=0;CHECK(!pe_game_use_slot(g,slot(g,386))&&count(g,386)==1);g->hp=g->max_hp;

    /* Starvation doubles every stamina cost and the recovery pulse's percent. */
    g->hunger.starving=true;CHECK(pe_game_stamina_reduce_percent(g)==2);
    PeToolStats stats;CHECK(pe_inventory_add(&g->inventory,12,1)&&pe_game_equip_slot(g,slot(g,12)));
    CHECK(pe_game_tool_stats(g,&stats));float starving_cost=stats.stamina_cost;
    g->hunger.starving=false;CHECK(pe_game_tool_stats(g,&stats)&&starving_cost==2*stats.stamina_cost&&stats.stamina_cost>0);
    PeStaminaRecovery rec={.initialized=true,.frame=1,.stamina_ticks=1,.comfort_ticks=1,.delay_ticks=1};
    PePassiveRecovery life={.initialized=true,.mounted=true,.initialization_checked=true,.evaluations=1,.frame=1};
    PePassiveRecovery next_life=life;next_life.last_time=1.5f;next_life.frame=9;
    PeStaminaVitals v={0,100,40-10,0+2,500,1,1};
    CHECK(pe_stamina_recovery_frame(&rec,&life,&next_life,(PeAttributeFrame){1.5f,9,true},&v));
    CHECK(v.stamina==60);   /* (40 - 10) * (1 + 1) */
    rec=(PeStaminaRecovery){.initialized=true,.frame=1,.stamina_ticks=1,.comfort_ticks=1,.delay_ticks=1};
    next_life.last_time=5.5f;v=(PeStaminaVitals){100,100,30,2,500,1,1};
    CHECK(pe_stamina_recovery_frame(&rec,&life,&next_life,(PeAttributeFrame){5.5f,9,true},&v));
    CHECK(fabsf(v.comfort-(500+pe_stamina_recovery_source.comfort_recovery_speed*2*5))<1e-4f);

    /* Legacy has no hunger: food is neither consumable nor eaten. */
    CHECK(pe_game_init_mode(loaded,5,PE_GAME_LEGACY));CHECK(!pe_game_item_is_consumable(loaded,386));
    CHECK(pe_inventory_add(&loaded->inventory,386,1));loaded->hunger.initialized=true;
    CHECK(!pe_game_use_slot(loaded,slot(loaded,386))&&count(loaded,386)==1);CHECK(pe_game_stamina_reduce_percent(loaded)==1);
    pe_game_destroy(loaded);memset(loaded,0,sizeof *loaded);

    /* PECSv23 keeps the summed value and the condition; timers and phase are transient. */
    g->hunger.hunger=g->hunger.raw=42.5f;g->hunger.starving=true;g->hunger.start_elapsed=.7f;g->hunger.drain_ticks=9;
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->hunger.hunger==42.5f&&loaded->hunger.raw==42.5f&&loaded->hunger.starving);
    CHECK(!loaded->hunger.drain_ticks&&loaded->hunger.start_elapsed==0&&loaded->hunger.food_cooldown==0&&!loaded->hunger.initialized);
    bytes=read_bytes(path,&n);CHECK(bytes&&n>40);if(!bytes)goto done;
    CHECK(get32(bytes+4)==23&&get32(bytes+n-8)==bits(42.5f)&&get32(bytes+n-4)==1);
    /* A literal v22 starts at 300, not starving; every earlier byte is unchanged on resave. */
    CHECK(write_bytes(path,bytes,n-PE_SAVE_V23_APPEND,22)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->hunger.hunger==300&&!loaded->hunger.starving);
    /* Malformed suffixes fail without replacing the live game. */
    memcpy(before,loaded,sizeof *loaded);
    put32(bytes+n-4,2);CHECK(write_bytes(path,bytes,n,23)&&!pe_game_load(loaded,path,error,sizeof error));
    put32(bytes+n-4,0);put32(bytes+n-8,bits(500.5f));CHECK(write_bytes(path,bytes,n,23)&&!pe_game_load(loaded,path,error,sizeof error));
    put32(bytes+n-8,bits(NAN));CHECK(write_bytes(path,bytes,n,23)&&!pe_game_load(loaded,path,error,sizeof error));
    put32(bytes+n-8,bits(-1));CHECK(write_bytes(path,bytes,n,23)&&!pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->hunger.hunger==before->hunger.hunger&&loaded->hunger.starving==before->hunger.starving);
    CHECK(write_bytes(path,bytes,n-4,23)&&!pe_game_load(loaded,path,error,sizeof error));
done:
    free(bytes);if(g)pe_game_destroy(g);if(loaded)pe_game_destroy(loaded);free(g);free(loaded);free(before);
    if(fd>=0)unlink(path);
    return failures?1:0;
}
