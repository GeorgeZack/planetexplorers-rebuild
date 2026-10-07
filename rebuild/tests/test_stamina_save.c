#define _POSIX_C_SOURCE 200809L
#include "pe_game.h"
#include "pe_shield.h"
#include "../generated/pe_stamina_recovery_source.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"stamina save:%d %s\n",__LINE__,#c);failures++;}}while(0)
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
static void rejected(PeGame*g,const char*path,const PeGame*before,uint64_t hash){
    char error[256];CHECK(!pe_game_load(g,path,error,sizeof error));
    CHECK(!memcmp(g,before,sizeof *g)&&pe_world_hash(&g->world)==hash);
}
static int slot(const PeGame*g,int id){for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].count&&g->inventory.slots[i].item_id==id)return i;return -1;}
int pe_test_stamina_save(void){
    failures=0;char path[]="/tmp/pe-stamina-save-XXXXXX",error[256];int fd=mkstemp(path);
    if(fd>=0)close(fd);
    PeGame*g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*snapshot=malloc(sizeof *snapshot);PeWorld*w=calloc(1,sizeof *w);
    unsigned char*bytes=NULL,*bad=NULL,*after=NULL;size_t n=0,after_n=0;
    CHECK(fd>=0&&g&&loaded&&snapshot&&w);if(fd<0||!g||!loaded||!snapshot||!w)goto done;
    CHECK(PE_SAVE_V16_APPEND==4&&pe_game_init_mode(g,952,PE_GAME_ADVENTURE));
    CHECK(g->comfort==pe_stamina_recovery_source.initial_comfort);
    CHECK(pe_world_resize(w,80,32,80));
    for(int z=0;z<80;z++)for(int y=0;y<=8;y++)for(int x=0;x<80;x++)CHECK(pe_world_set(w,x,y,z,(PeVoxel){255,PE_MAT_DIRT}));
    CHECK(pe_game_set_world(g,w,(PeVec3){30,10,30}));
    CHECK(pe_inventory_add(&g->inventory,1,1)&&pe_inventory_add(&g->inventory,34,1));
    CHECK(pe_game_equip_slot(g,slot(g,1))&&pe_shield_equip(g,slot(g,34))&&pe_outfit_wear(&g->outfit,123));
    g->stamina=31;g->stamina_regen=23;g->stamina_recover_delay=.375f;g->stamina_idle=.625f;g->tool_cooldown=.25f;
    g->stamina_recovery=(PeStaminaRecovery){.initialized=true,.frame=123,.last_consumption=9,
        .summed_delay=1.5f,.stamina_ticks=5,.comfort_ticks=2,.delay_ticks=5};
    const float values[]={0,275.125f,500};
    for(unsigned i=0;i<sizeof values/sizeof values[0];i++){
        g->comfort=values[i];CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
        CHECK(loaded->comfort==values[i]&&loaded->world.extended&&loaded->world.extended!=g->world.extended);
        CHECK(loaded->stamina==31&&loaded->stamina_regen==23&&loaded->stamina_recover_delay==.375f&&loaded->stamina_idle==.625f&&loaded->tool_cooldown==.25f);
        CHECK(!memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory)&&!memcmp(&loaded->outfit,&g->outfit,sizeof g->outfit));
        CHECK(loaded->shield_instance_id==g->shield_instance_id&&loaded->equipped_instance_id==g->equipped_instance_id);
        CHECK(!memcmp(&loaded->stamina_recovery,&(PeStaminaRecovery){0},sizeof loaded->stamina_recovery));
    }
    g->comfort=275.125f;CHECK(pe_game_save(g,path,error,sizeof error));bytes=read_bytes(path,&n);bad=malloc(n+1);
    CHECK(bytes&&bad&&n>20);if(!bytes||!bad||n<=20)goto done;
    CHECK(get32(bytes+4)==17);n-=PE_SAVE_V17_APPEND; /* Literal V16 malformed/migration fixtures. */
    CHECK(get32(bytes+n-4)==bits(275.125f));
    /* Exact V15 prefix retains its shield, clothes, existing raw stamina fields
     * and dynamic world. No old-world-time inference invents historical comfort. */
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n-PE_SAVE_V16_APPEND,15)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->comfort==pe_stamina_recovery_source.initial_comfort&&loaded->stamina_recover_delay==.375f);
    CHECK(!memcmp(&loaded->stamina_recovery,&(PeStaminaRecovery){0},sizeof loaded->stamina_recovery));
    CHECK(loaded->shield_instance_id==g->shield_instance_id&&!memcmp(&loaded->outfit,&g->outfit,sizeof g->outfit));
    CHECK(pe_world_hash(&loaded->world)==pe_world_hash(&g->world));
    CHECK(pe_game_save(loaded,path,error,sizeof error));after=read_bytes(path,&after_n);CHECK(after&&after_n==n+PE_SAVE_V17_APPEND);
    if(after)CHECK(get32(after+4)==17&&!memcmp(after+16,bytes+16,n-PE_SAVE_V16_APPEND-16)&&get32(after+n-4)==bits(500));
    /* Invalid serialized comfort cannot mutate either the owned world or the
     * existing gameplay/transient state, even with a valid envelope checksum. */
    memcpy(snapshot,loaded,sizeof *loaded);uint64_t hash=pe_world_hash(&loaded->world);
    const float invalid[]={-.01f,500.01f,NAN,INFINITY,-INFINITY};
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){
        memcpy(bad,bytes,n);put32(bad+n-4,bits(invalid[i]));CHECK(write_bytes(path,bad,n,16));rejected(loaded,path,snapshot,hash);
    }
    for(unsigned missing=1;missing<=PE_SAVE_V16_APPEND;missing++){
        memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n-missing,16));rejected(loaded,path,snapshot,hash);
    }
    memcpy(bad,bytes,n);bad[n]=0;CHECK(write_bytes(path,bad,n+1,16));rejected(loaded,path,snapshot,hash);
    /* A V15 envelope may not silently accept the new append. */
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n,15));rejected(loaded,path,snapshot,hash);
    CHECK(write_bytes(path,bytes,n,16));
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){
        g->comfort=invalid[i];CHECK(!pe_game_save(g,path,error,sizeof error));size_t current_n=0;unsigned char*current=read_bytes(path,&current_n);
        CHECK(current&&current_n==n&&!memcmp(current,bytes,n));free(current);
    }
    /* Legacy retains its old gameplay policy while the newly serialized raw
     * field remains well-defined and old payloads receive the same default. */
    pe_game_destroy(g);pe_game_init(g,953);CHECK(g->comfort==500);g->comfort=123.25f;
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->comfort==123.25f&&loaded->mode==PE_GAME_LEGACY);
    free(after);after=read_bytes(path,&after_n);CHECK(after);
    if(after){after_n-=PE_SAVE_V17_APPEND;CHECK(write_bytes(path,after,after_n-PE_SAVE_V16_APPEND,15)&&pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->comfort==500&&loaded->mode==PE_GAME_LEGACY);}
done:
    if(fd>=0)unlink(path);
    free(bytes);free(bad);free(after);free(snapshot);pe_world_destroy(w);free(w);pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);
    return failures;
}
