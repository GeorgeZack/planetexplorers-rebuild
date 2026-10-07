#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_deployable.h"
#include <math.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"outfit save:%d %s\n",__LINE__,#c);failures++;}}while(0)
static uint32_t get32(const unsigned char*p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(unsigned char*p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static uint32_t bits(float x){uint32_t v;memcpy(&v,&x,4);return v;}
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
static int slot(const PeGame*g,int id){for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].count&&g->inventory.slots[i].item_id==id)return i;return -1;}
static void rejected(PeGame*loaded,const char*path,const void*snapshot,uint64_t hash){
    char error[256];CHECK(!pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(loaded,snapshot,sizeof *loaded)&&pe_world_hash(&loaded->world)==hash);
}
static void unchanged_file(const char*path,const unsigned char*before,size_t n){
    size_t after_n=0;unsigned char*after=read_bytes(path,&after_n);CHECK(after&&after_n==n&&memcmp(after,before,n)==0);free(after);
}
int pe_test_outfit_save(void){
    failures=0;char path[]="/tmp/pe-outfit-save-XXXXXX",error[256];int fd=mkstemp(path);
    PeGame*g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*legacy=calloc(1,sizeof *legacy);
    PeWorld*w=calloc(1,sizeof *w);PeSandboxCollision*c=pe_sandbox_collision_create();
    unsigned char*bytes=NULL,*bad=NULL,*snapshot=NULL,*after=NULL;size_t n=0,after_n=0;
    CHECK(fd>=0&&g&&loaded&&legacy&&w&&c);if(fd<0||!g||!loaded||!legacy||!w||!c)goto done;close(fd);
    CHECK(PE_SAVE_V14_APPEND==36&&PE_OUTFIT_SLOTS==3);
    CHECK(pe_game_init_mode(g,96,PE_GAME_ADVENTURE)&&pe_outfit_state_valid(g));
    CHECK(pe_world_resize(w,80,32,80));
    for(int z=0;z<80;z++)for(int y=0;y<=8;y++)for(int x=0;x<80;x++)CHECK(pe_world_set(w,x,y,z,(PeVoxel){255,PE_MAT_DIRT}));
    CHECK(pe_game_set_world(g,w,(PeVec3){30,10,30})&&g->world.extended);
    /* Keep the allocator that already owns the clothes when emptying the bag. */
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id,.categorized=g->mode==PE_GAME_ADVENTURE};g->equipped_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    CHECK(pe_inventory_add(&g->inventory,33,1)&&pe_inventory_add(&g->inventory,19,1)&&pe_inventory_add(&g->inventory,10,1));
    int knife=slot(g,33),pickaxe=slot(g,19),torch=slot(g,10);CHECK(knife>=0&&pickaxe>=0&&torch>=0);if(knife<0||pickaxe<0||torch<0)goto done;
    CHECK(pe_game_equip_slot(g,knife)&&pe_game_quickslot_assign(g,0,knife));
    uint32_t deployed=g->inventory.slots[torch].instance_id;
    CHECK(pe_game_collision_update(g,c,error,sizeof error));
    CHECK(pe_deployable_place(g,c,deployed,(PeVec3){32,15,30},(PeVec3){0,-1,0}));
    CHECK(pe_outfit_wear(&g->outfit,100));
    g->outfit.items[0].durability=2100.25f;g->outfit.items[1].durability=1000.5f;g->outfit.items[2].durability=0;
    CHECK(pe_outfit_defense(&g->outfit)==11);g->medpack_cooldown=3.5f;g->juice_cooldown=1.25;
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->world.extended&&loaded->world.extended!=g->world.extended);
    CHECK(!memcmp(&loaded->outfit,&g->outfit,sizeof g->outfit)&&!memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory));
    CHECK(loaded->deployed_count==1&&loaded->deployed[0].instance_id==deployed&&loaded->equipped_instance_id==g->equipped_instance_id);
    CHECK(pe_outfit_defense(&loaded->outfit)==11&&loaded->medpack_cooldown==3.5f&&loaded->juice_cooldown==1.25);
    bytes=read_bytes(path,&n);CHECK(bytes&&n>PE_SAVE_V14_APPEND+16);if(!bytes||n<=PE_SAVE_V14_APPEND+16)goto done;
    CHECK(get32(bytes+4)==20);if(n<=(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+PE_SAVE_V17_APPEND+2048){CHECK(false);goto done;}
    n-=(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+PE_SAVE_V17_APPEND; /* Literal V16 malformed/migration fixtures. */
    size_t footer=n-PE_SAVE_V16_APPEND-PE_SAVE_V15_APPEND-PE_SAVE_V14_APPEND,counter=pe_test_save_counter_offset(g);
    CHECK(get32(bytes+counter)==g->inventory.next_instance_id);
    for(unsigned i=0;i<3;i++){
        CHECK(get32(bytes+footer+i*12)==(uint32_t)g->outfit.items[i].item_id);
        CHECK(get32(bytes+footer+i*12+4)==g->outfit.items[i].instance_id);
        CHECK(get32(bytes+footer+i*12+8)==bits(g->outfit.items[i].durability));
    }
    bad=malloc(n+1);snapshot=malloc(sizeof *loaded);CHECK(bad&&snapshot);if(!bad||!snapshot)goto done;
    memcpy(snapshot,loaded,sizeof *loaded);uint64_t hash=pe_world_hash(&loaded->world);
    const struct{size_t offset;uint32_t value;} malformed[]={
        {0,113},{0,131},{0,UINT32_MAX},{4,0},{4,g->inventory.next_instance_id},
        {4,g->outfit.items[1].instance_id},{4,g->inventory.slots[knife].instance_id},{4,deployed},
        {8,0x7fc00000u},{8,0x7f800000u},{8,0xbf800000u},{8,0x7f7fffffu},
        {12+8,0x7fc00000u},{24+8,0x7fc00000u}
    };
    for(size_t i=0;i<sizeof malformed/sizeof malformed[0];i++){
        memcpy(bad,bytes,n);put32(bad+footer+malformed[i].offset,malformed[i].value);
        CHECK(write_bytes(path,bad,n,16));rejected(loaded,path,snapshot,hash);
    }
    memcpy(bad,bytes,n);memset(bad+footer,0,36);CHECK(write_bytes(path,bad,n,16));rejected(loaded,path,snapshot,hash); /* Never repair missing v14 clothes. */
    memcpy(bad,bytes,n);memset(bad+footer+12,0,12);CHECK(write_bytes(path,bad,n,16));rejected(loaded,path,snapshot,hash);
    for(size_t missing=1;missing<=PE_SAVE_V16_APPEND+36;missing++){
        memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n-missing,16));rejected(loaded,path,snapshot,hash);
    }
    memcpy(bad,bytes,n);bad[n]=0;CHECK(write_bytes(path,bad,n+1,16));rejected(loaded,path,snapshot,hash);
    /* Save rejection is atomic too, including references which must never be
     * allowed to point at worn-only instances. */
    CHECK(write_bytes(path,bytes,n,16));
    PeOutfit outfit=g->outfit;
    for(unsigned i=0;i<3;i++){
        g->outfit.items[i].durability=NAN;CHECK(!pe_game_save(g,path,error,sizeof error));unchanged_file(path,bytes,n);g->outfit=outfit;
    }
    uint32_t equipped=g->equipped_instance_id;g->equipped_instance_id=outfit.items[0].instance_id;
    CHECK(!pe_game_save(g,path,error,sizeof error));unchanged_file(path,bytes,n);g->equipped_instance_id=equipped;
    PeQuickslot quick=g->quickslots[0];g->quickslots[0].instance_id=outfit.items[0].instance_id;
    CHECK(!pe_game_save(g,path,error,sizeof error));unchanged_file(path,bytes,n);g->quickslots[0]=quick;
    g->deployed[0].instance_id=outfit.items[0].instance_id;CHECK(!pe_game_save(g,path,error,sizeof error));unchanged_file(path,bytes,n);g->deployed[0].instance_id=deployed;
    /* Pre-v14 adapters allocate only after validating existing owners. A bad
     * counter or duplicate bag identity must not be repaired by adding three. */
    uint32_t next=g->inventory.next_instance_id;
    const uint32_t bad_counters[]={0,g->inventory.slots[knife].instance_id,UINT32_MAX,UINT32_MAX-2};
    for(unsigned i=0;i<sizeof bad_counters/sizeof bad_counters[0];i++){
        memcpy(bad,bytes,n);put32(bad+counter,bad_counters[i]);CHECK(write_bytes(path,bad,footer,13));rejected(loaded,path,snapshot,hash);
    }
    memcpy(bad,bytes,n);put32(bad+counter+4+8*(size_t)pe_test_save_prefix_index(g,pickaxe),g->inventory.slots[knife].instance_id);
    CHECK(write_bytes(path,bad,footer,13));rejected(loaded,path,snapshot,hash);
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,footer,13));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->inventory.next_instance_id==next+3&&pe_outfit_defense(&loaded->outfit)==12);
    CHECK(!memcmp(loaded->inventory.slots,g->inventory.slots,sizeof g->inventory.slots));
    CHECK(loaded->equipped_instance_id==equipped&&loaded->deployed[0].instance_id==deployed&&pe_world_hash(&loaded->world)==hash);
    CHECK(loaded->medpack_cooldown==3.5f&&loaded->juice_cooldown==1.25);
    for(unsigned i=0;i<3;i++)CHECK(loaded->outfit.items[i].instance_id==next+i&&loaded->outfit.items[i].durability==(i==2?862:2156));
    /* Migrating the same checkpoint is deterministic, not repeated in-memory
     * allocation. Subsequent v14 saves preserve wear, including exhausted shoes. */
    PeOutfit migrated=loaded->outfit;CHECK(pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(&migrated,&loaded->outfit,sizeof migrated)&&loaded->inventory.next_instance_id==next+3);
    CHECK(pe_game_save(loaded,path,error,sizeof error));after=read_bytes(path,&after_n);CHECK(after&&after_n==n+(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+PE_SAVE_V17_APPEND);
    if(after){put32(bad+counter,next+3);CHECK(!memcmp(after+16,bad+16,footer-16));} /* Only required allocator field changed in retained prefix. */
    CHECK(pe_outfit_wear(&loaded->outfit,50));loaded->outfit.items[2].durability=0;migrated=loaded->outfit;
    CHECK(pe_game_save(loaded,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(&migrated,&loaded->outfit,sizeof migrated));
    /* Legacy retains zero outfit and allocates nothing on old load. */
    pe_game_init(legacy,97);CHECK(pe_game_save(legacy,path,error,sizeof error));free(after);after=read_bytes(path,&after_n);CHECK(after);
    if(after){after_n-=(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+PE_SAVE_V17_APPEND;CHECK(!memcmp(after+after_n-PE_SAVE_V16_APPEND-PE_SAVE_V15_APPEND-36,(unsigned char[36]){0},36));
        CHECK(write_bytes(path,after,after_n-PE_SAVE_V16_APPEND-PE_SAVE_V15_APPEND-36,13)&&pe_game_load(loaded,path,error,sizeof error));
        CHECK(loaded->mode==PE_GAME_LEGACY&&pe_outfit_defense(&loaded->outfit)==0&&loaded->inventory.next_instance_id==legacy->inventory.next_instance_id);
        put32(after+after_n-PE_SAVE_V16_APPEND-PE_SAVE_V15_APPEND-36,95);CHECK(write_bytes(path,after,after_n,16));
        memcpy(snapshot,loaded,sizeof *loaded);hash=pe_world_hash(&loaded->world);rejected(loaded,path,snapshot,hash);
    }
 done:
    if(fd>=0)unlink(path);
    free(bytes);free(bad);free(snapshot);free(after);pe_sandbox_collision_free(c);pe_world_destroy(w);free(w);
    pe_game_destroy(g);pe_game_destroy(loaded);pe_game_destroy(legacy);free(g);free(loaded);free(legacy);return failures;
}
