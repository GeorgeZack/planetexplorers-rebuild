#define _POSIX_C_SOURCE 200809L
#include "pe_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"inventory save:%d %s\n",__LINE__,#c);failures++;}}while(0)
static uint32_t get32(const unsigned char *p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static unsigned char *read_bytes(const char *path,size_t *n){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long size=ftell(f);rewind(f);
    if(size<16){fclose(f);return NULL;}unsigned char *p=malloc((size_t)size+1);
    bool ok=p&&fread(p,1,(size_t)size,f)==(size_t)size;if(fclose(f))ok=false;
    if(!ok){free(p);return NULL;}*n=(size_t)size;return p;
}
static bool write_bytes(const char *path,unsigned char *p,size_t n,uint32_t version){
    put32(p+4,version);put32(p+8,(uint32_t)n-16);uint32_t sum=2166136261u;
    for(size_t i=16;i<n;i++){sum^=p[i];sum*=16777619u;}put32(p+12,sum);
    FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(p,1,n,f)==n;return fclose(f)==0&&ok;
}
static int find_item(const PeGame *g,int id){for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==id)return i;return -1;}
int pe_test_inventory_save(void){
    failures=0;char path[1024],error[256]={0};
    snprintf(path,sizeof path,"%s/pe-inventory-save-XXXXXX",getenv("TMPDIR")?getenv("TMPDIR"):"/tmp");
    int fd=mkstemp(path);if(fd>=0)close(fd);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);unsigned char *bytes=NULL,*bad=NULL,*snapshot=NULL,*after=NULL;
    size_t n=0,m=0;CHECK(fd>=0&&g&&loaded);if(fd<0||!g||!loaded)goto done;
    CHECK(PE_INVENTORY_SLOTS==32&&PE_SAVE_V19_APPEND==26888);
    CHECK(pe_game_init_mode(g,719,PE_GAME_ADVENTURE)&&g->inventory.categorized);
    int axe=find_item(g,1527);CHECK(axe>=420);if(axe<0)goto done;
    CHECK(pe_game_equip_slot(g,axe)&&pe_game_quickslot_assign(g,8,axe));g->inventory.slots[axe].durability=37.5f;
    uint32_t identity=g->equipped_instance_id,counter=g->inventory.next_instance_id;
    CHECK(pe_game_save(g,path,error,sizeof error));bytes=read_bytes(path,&n);CHECK(bytes);if(!bytes||n<=(PE_SAVE_V21_APPEND+PE_SAVE_V20_APPEND)+PE_SAVE_V19_APPEND+16)goto done;
    CHECK(get32(bytes+4)==21&&get32(bytes+n-(PE_SAVE_V21_APPEND+PE_SAVE_V20_APPEND)-PE_SAVE_V19_APPEND)==1&&get32(bytes+n-(PE_SAVE_V21_APPEND+PE_SAVE_V20_APPEND)-PE_SAVE_V19_APPEND+4)==1680);
    /* A <=32-object source fixture projects every object into the unchanged old
     * wire layout. Loading that literal v18 migrates by source category. */
    CHECK(write_bytes(path,bytes,n-(PE_SAVE_V21_APPEND+PE_SAVE_V20_APPEND)-PE_SAVE_V19_APPEND,18)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->inventory.categorized&&loaded->equipped_instance_id==identity&&loaded->quickslots[8].instance_id==identity);
    CHECK(loaded->inventory.next_instance_id==counter&&pe_game_equipped_tool(loaded)&&pe_game_equipped_tool(loaded)->durability==37.5f);
    CHECK(!memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory));
    /* More than32 objects and an exact equipped instance beyond the old
     * prefix survive. Empty cells and deliberate last-page positions persist. */
    CHECK(pe_inventory_add(&g->inventory,26,200));
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,pe_item_def(PE_ITEM_WOOD)->stack_limit*100));
    CHECK(pe_inventory_move(&g->inventory,axe,839));
    CHECK(g->inventory.slots[839].instance_id==identity);
    int wood=find_item(g,PE_ITEM_WOOD),split=-1;CHECK(wood>=840);
    CHECK(pe_inventory_split(&g->inventory,wood,7,&split)&&pe_inventory_move(&g->inventory,split,1259));
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory)&&loaded->inventory.slots[839].instance_id==identity);
    CHECK(pe_game_equipped_tool(loaded)&&pe_game_equipped_tool(loaded)->durability==37.5f&&loaded->quickslots[8].instance_id==identity);
    free(bytes);bytes=read_bytes(path,&n);bad=malloc(n+1);snapshot=malloc(sizeof *loaded);CHECK(bytes&&bad&&snapshot);if(!bytes||!bad||!snapshot)goto done;
    memcpy(snapshot,loaded,sizeof *loaded);size_t tail=n-(PE_SAVE_V21_APPEND+PE_SAVE_V20_APPEND)-PE_SAVE_V19_APPEND;uint64_t hash=pe_world_hash(&loaded->world);
    const struct{size_t at;uint32_t value;} corruptions[]={
        {0,2},{0,0},{4,1679},{8,999999},{8+4,0x80000000},{8+12,0x7fc00000},
        {8+839*16+8,UINT32_MAX},{8+839*16+12,0xbf800000},
    };
    for(unsigned i=0;i<sizeof corruptions/sizeof corruptions[0];i++){
        memcpy(bad,bytes,n);put32(bad+tail+corruptions[i].at,corruptions[i].value);
        CHECK(write_bytes(path,bad,n,21)&&!pe_game_load(loaded,path,error,sizeof error));
        CHECK(!memcmp(snapshot,loaded,sizeof *loaded)&&pe_world_hash(&loaded->world)==hash);
    }
    /* Prefix tampering cannot be hidden by a valid authoritative suffix. */
    memcpy(bad,bytes,n);bad[16+52]^=1;
    CHECK(write_bytes(path,bad,n,21)&&!pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    /* Valid item in wrong source category, and duplicate exact identity. */
    memcpy(bad,bytes,n);memcpy(bad+tail+8+1260*16,bad+tail+8+839*16,16);
    CHECK(write_bytes(path,bad,n,21)&&!pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    memcpy(bad,bytes,n);memcpy(bad+tail+8+838*16,bad+tail+8+839*16,16);
    CHECK(write_bytes(path,bad,n,21)&&!pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n-1,21)&&!pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    memcpy(bad,bytes,n);bad[n]=0;CHECK(write_bytes(path,bad,n+1,21)&&!pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    CHECK(write_bytes(path,bytes,n,21));g->inventory.categorized=false;
    CHECK(!pe_game_save(g,path,error,sizeof error));after=read_bytes(path,&m);CHECK(after&&m==n);if(after&&m==n)CHECK(!memcmp(bytes,after,n));
    /* Legacy keeps32 shared slots and no data in the expanded tail. */
    pe_game_destroy(g);pe_game_init(g,719);
    CHECK(!g->inventory.categorized&&pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(!loaded->inventory.categorized&&pe_inventory_capacity(&loaded->inventory)==32&&!memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory));
done:
    if(fd>=0)unlink(path);
    free(bytes);free(bad);free(snapshot);free(after);
    pe_game_destroy(g);free(g);pe_game_destroy(loaded);free(loaded);return failures;
}
