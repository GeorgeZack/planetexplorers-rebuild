#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_shield.h"
#include <math.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"shield save:%d %s\n",__LINE__,#c);failures++;}}while(0)
static uint32_t get32(const unsigned char*p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(unsigned char*p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static unsigned char*read_bytes(const char*path,size_t*n){
    FILE*f=fopen(path,"rb");if(!f)return NULL;if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long length=ftell(f);if(length<16||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char*p=malloc((size_t)length+1);if(!p){fclose(f);return NULL;}
    bool ok=fread(p,1,(size_t)length,f)==(size_t)length;if(fclose(f))ok=false;
    if(!ok){free(p);return NULL;}*n=(size_t)length;return p;
}
static bool write_bytes(const char*path,unsigned char*p,size_t n,unsigned version){
    put32(p+4,version);put32(p+8,(uint32_t)n-16);uint32_t sum=2166136261u;
    for(size_t i=16;i<n;i++){sum^=p[i];sum*=16777619u;}put32(p+12,sum);
    FILE*f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(p,1,n,f)==n;return fclose(f)==0&&ok;
}
static int slot(const PeGame*g,int item){for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].count&&g->inventory.slots[i].item_id==item)return i;return -1;}
static void rejected(PeGame*g,const char*path,const void*snapshot,uint64_t hash){char error[256];CHECK(!pe_game_load(g,path,error,sizeof error));CHECK(!memcmp(g,snapshot,sizeof *g)&&pe_world_hash(&g->world)==hash);}
int pe_test_shield_save(void){
    failures=0;char path[]="/tmp/pe-shield-save-XXXXXX",error[256];int fd=mkstemp(path);
    PeGame*g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);PeWorld*w=calloc(1,sizeof *w);
    unsigned char*bytes=NULL,*bad=NULL,*snapshot=NULL,*after=NULL;size_t n=0,after_n=0;
    CHECK(fd>=0&&g&&loaded&&w);if(fd<0||!g||!loaded||!w)goto done;close(fd);
    CHECK(PE_SAVE_V15_APPEND==4&&pe_game_init_mode(g,105,PE_GAME_ADVENTURE));
    CHECK(pe_world_resize(w,80,32,80));for(int z=0;z<80;z++)for(int y=0;y<=8;y++)for(int x=0;x<80;x++)CHECK(pe_world_set(w,x,y,z,(PeVoxel){255,PE_MAT_DIRT}));
    CHECK(pe_game_set_world(g,w,(PeVec3){30,10,30}));
    CHECK(pe_inventory_add(&g->inventory,1,1)&&pe_inventory_add(&g->inventory,34,1)&&pe_inventory_add(&g->inventory,19,1));
    int sword=slot(g,1),shield=slot(g,34),pick=slot(g,19);CHECK(sword>=0&&shield>=0&&pick>=0);if(sword<0||shield<0||pick<0)goto done;
    CHECK(pe_game_equip_slot(g,sword)&&pe_game_quickslot_assign(g,0,shield)&&pe_shield_equip(g,shield));
    uint32_t id=g->shield_instance_id;g->inventory.slots[shield].durability=321.25f;CHECK(pe_outfit_wear(&g->outfit,123));
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->world.extended&&loaded->world.extended!=g->world.extended&&loaded->shield_instance_id==id);
    CHECK(!memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory)&&!memcmp(&loaded->outfit,&g->outfit,sizeof g->outfit));
    CHECK(loaded->equipped_instance_id==g->equipped_instance_id&&pe_shield_equipped(loaded)->durability==321.25f);
    bytes=read_bytes(path,&n);bad=malloc(n+1);snapshot=malloc(sizeof *loaded);CHECK(bytes&&bad&&snapshot&&n>20);if(!bytes||!bad||!snapshot||n<=20)goto done;
    CHECK(get32(bytes+4)==16&&get32(bytes+n-PE_SAVE_V16_APPEND-4)==id);
    memcpy(snapshot,loaded,sizeof *loaded);uint64_t hash=pe_world_hash(&loaded->world);
    uint32_t invalid[]={g->inventory.next_instance_id,UINT32_MAX,g->equipped_instance_id,g->outfit.items[0].instance_id,g->inventory.slots[pick].instance_id};
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){memcpy(bad,bytes,n);put32(bad+n-PE_SAVE_V16_APPEND-4,invalid[i]);CHECK(write_bytes(path,bad,n,16));rejected(loaded,path,snapshot,hash);}
    size_t counter=pe_test_save_counter_offset(g);
    memcpy(bad,bytes,n);put32(bad+counter+4+8*(size_t)shield+4,0x7fc00000u);CHECK(write_bytes(path,bad,n,16));rejected(loaded,path,snapshot,hash);
    memcpy(bad,bytes,n);put32(bad+counter+4+8*(size_t)pick,id);CHECK(write_bytes(path,bad,n,16));rejected(loaded,path,snapshot,hash);
    for(unsigned missing=1;missing<=PE_SAVE_V16_APPEND+4;missing++){memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n-missing,16));rejected(loaded,path,snapshot,hash);}
    memcpy(bad,bytes,n);bad[n]=0;CHECK(write_bytes(path,bad,n+1,16));rejected(loaded,path,snapshot,hash);
    /* Literal V14 keeps every field, including worn clothes, but never infers
     * shield equipment merely from item34 possession or a quickslot reference. */
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n-PE_SAVE_V16_APPEND-4,14)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(!loaded->shield_instance_id&&loaded->inventory.next_instance_id==g->inventory.next_instance_id);
    CHECK(!memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory)&&!memcmp(&loaded->outfit,&g->outfit,sizeof g->outfit));
    CHECK(pe_game_save(loaded,path,error,sizeof error));after=read_bytes(path,&after_n);CHECK(after&&after_n==n);
    if(after)CHECK(get32(after+4)==16&&get32(after+n-PE_SAVE_V16_APPEND-4)==0&&!memcmp(after+16,bytes+16,n-PE_SAVE_V16_APPEND-4-16));
    /* V13 outfit migration remains intact; shield still defaults absent. */
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n-PE_SAVE_V16_APPEND-4-PE_SAVE_V14_APPEND,13)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(!loaded->shield_instance_id&&loaded->inventory.next_instance_id==g->inventory.next_instance_id+3&&pe_outfit_defense(&loaded->outfit)==12);
    CHECK(loaded->inventory.slots[shield].durability==321.25f);
    /* Already equipped depletion is preserved rather than unequipped/repaired. */
    g->inventory.slots[shield].durability=0;CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->shield_instance_id==id&&pe_shield_equipped(loaded)->durability==0&&pe_shield_defense(pe_shield_equipped(loaded))==0);
    free(after);after=read_bytes(path,&after_n);CHECK(after);
    g->shield_instance_id=g->equipped_instance_id;CHECK(!pe_game_save(g,path,error,sizeof error));size_t current_n=0;unsigned char*current=read_bytes(path,&current_n);
    CHECK(current&&after&&current_n==after_n&&!memcmp(current,after,after_n));free(current);
    /* Legacy stores zero and old Legacy loads cannot gain a shield. */
    pe_game_destroy(g);pe_game_init(g,106);CHECK(pe_game_save(g,path,error,sizeof error));free(after);after=read_bytes(path,&after_n);CHECK(after);
    if(after){CHECK(get32(after+after_n-PE_SAVE_V16_APPEND-4)==0&&write_bytes(path,after,after_n-PE_SAVE_V16_APPEND-4,14)&&pe_game_load(loaded,path,error,sizeof error));CHECK(!loaded->shield_instance_id&&loaded->mode==PE_GAME_LEGACY);}
 done:
    if(fd>=0)unlink(path);
    free(bytes);free(bad);free(snapshot);free(after);pe_world_destroy(w);free(w);
    pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);return failures;
}
