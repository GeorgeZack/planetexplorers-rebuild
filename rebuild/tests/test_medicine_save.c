#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_adventure_medicine.h"
#include "pe_town_interaction.h"
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"medicine-save:%d %s\n",__LINE__,#c);failures++;}}while(0)
#define V9_APPEND (8u+16u*(PE_TOWN_STOCK_CAPACITY-PE_TOWN_V8_STOCK_CAPACITY))
static void put32(unsigned char *p,uint32_t value){for(int i=0;i<4;i++)p[i]=(unsigned char)(value>>(8*i));}
static uint32_t get32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static unsigned char *read_save(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);rewind(f);
    if(n<20){fclose(f);return NULL;}unsigned char *p=malloc((size_t)n+1);
    if(!p){fclose(f);return NULL;}bool ok=fread(p,1,(size_t)n,f)==(size_t)n;
    if(fclose(f))ok=false;
    if(!ok){free(p);return NULL;}*size=(size_t)n;return p;
}
static bool write_save(const char *path,unsigned char *p,size_t size,unsigned version){
    put32(p+4,version);put32(p+8,(uint32_t)size-16);uint32_t sum=2166136261u;
    for(size_t i=16;i<size;i++){sum^=p[i];sum*=16777619u;}put32(p+12,sum);
    FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(p,1,size,f)==size;
    if(fclose(f))ok=false;
    return ok;
}
static void same_stocks(const PeGame *a,const PeGame *b,int start,int end){
    for(int i=start;i<end;i++){CHECK(a->town_stock[i].initialized==b->town_stock[i].initialized);
        CHECK(a->town_stock[i].remaining==b->town_stock[i].remaining);
        CHECK(a->town_stock[i].create_time==b->town_stock[i].create_time);}
}
int pe_test_medicine_save(void){
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);
    PeWorld *w=calloc(1,sizeof *w);unsigned char *snapshot=malloc(sizeof *g);
    char path[]="/tmp/pe-medicine-save-XXXXXX",error[256];int fd=mkstemp(path);
    if(!g||!loaded||!w||!snapshot||fd<0){failures++;goto done;}close(fd);
    CHECK(pe_game_init_mode(g,77,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(g));
    CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));g->annot_complete=true;
    /* Use an owned extended world so rejected loads must preserve both the
     * object and the destination's heap storage, not merely scalar state. */
    CHECK(pe_world_resize(w,80,32,80));
    for(int z=0;z<80;z++)for(int y=0;y<=8;y++)for(int x=0;x<80;x++)CHECK(pe_world_set(w,x,y,z,(PeVoxel){255,PE_MAT_DIRT}));
    CHECK(pe_game_set_world(g,w,(PeVec3){30,10,30}));CHECK(g->world.extended);
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};CHECK(pe_adventure_medicine_accept(g));
    g->game_seconds=100;CHECK(pe_town_shop_open(g,20011));CHECK(pe_town_shop_open(g,20028));
    CHECK(pe_town_shop_buy(g,20011,338,1));CHECK(pe_town_shop_buy(g,20028,212,2));
    bool saved=pe_game_save(g,path,error,sizeof error);if(!saved)fprintf(stderr,"medicine-save: %s\n",error);CHECK(saved);if(!saved)goto done;
    CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->medicine_state==PE_MEDICINE_ACTIVE&&loaded->annot_complete);
    CHECK(loaded->money==g->money&&pe_inventory_count(&loaded->inventory,877)==1);
    CHECK(pe_world_width(&loaded->world)==80&&loaded->world.extended!=g->world.extended);
    CHECK(pe_world_hash(&loaded->world)==pe_world_hash(&g->world));same_stocks(g,loaded,0,PE_TOWN_STOCK_CAPACITY);
    size_t size=0;unsigned char *original=read_save(path,&size);CHECK(original&&size>V9_APPEND+PE_SAVE_V10_APPEND);
    if(original&&size>V9_APPEND+PE_SAVE_V10_APPEND){
        CHECK(get32(original+4)==18);size-=PE_SAVE_V18_APPEND+PE_SAVE_V17_APPEND; /* Keep malformed fixtures in their literal V16 envelope. */
        size_t footer=size-PE_SAVE_V16_APPEND-PE_SAVE_V15_APPEND-PE_SAVE_V14_APPEND-PE_SAVE_V13_APPEND-PE_SAVE_V11_APPEND-PE_SAVE_V10_APPEND-V9_APPEND;CHECK(get32(original+footer)==PE_MEDICINE_ACTIVE);
        CHECK(get32(original+footer+4)==PE_TOWN_STOCK_CAPACITY-PE_TOWN_V8_STOCK_CAPACITY);
        /* A literal v8 prefix retains first seventeen offer clocks and Annot;
         * newly appended offers and medicine start uninitialized. */
        CHECK(write_save(path,original,footer,8));CHECK(pe_game_load(loaded,path,error,sizeof error));
        CHECK(loaded->medicine_state==PE_MEDICINE_NONE&&loaded->annot_complete);
        same_stocks(g,loaded,0,PE_TOWN_V8_STOCK_CAPACITY);
        for(int i=PE_TOWN_V8_STOCK_CAPACITY;i<PE_TOWN_STOCK_CAPACITY;i++)CHECK(!loaded->town_stock[i].initialized&&!loaded->town_stock[i].remaining&&loaded->town_stock[i].create_time==0);
        CHECK(pe_world_hash(&loaded->world)==pe_world_hash(&g->world));
        CHECK(pe_game_load(loaded,path,error,sizeof error));
        memcpy(snapshot,loaded,sizeof *loaded);uint64_t world_hash=pe_world_hash(&loaded->world);
        unsigned char *bad=malloc(size+1);CHECK(bad);
        if(bad)for(int mode=0;mode<9;mode++){
            memcpy(bad,original,size);size_t output_size=size;
            if(mode==0)put32(bad+footer,3); /* unknown medicine state */
            if(mode==1)put32(bad+footer+4,266); /* truncated catalog declaration */
            if(mode==2)put32(bad+footer+8,2); /* non-boolean initialized */
            if(mode==3)put32(bad+footer+12,UINT32_MAX); /* impossible stock */
            if(mode==4){put32(bad+footer+16,0);put32(bad+footer+20,0x7ff80000u);} /* NaN clock */
            if(mode==5)put32(bad+footer-(4+16*PE_TOWN_V8_STOCK_CAPACITY),0); /* medicine without Annot */
            if(mode==6){put32(bad+footer+8,0);put32(bad+footer+12,1);} /* uninitialized but nonempty */
            if(mode==7)output_size--;
            if(mode==8)bad[output_size++]=0;
            CHECK(write_save(path,bad,output_size,16));CHECK(!pe_game_load(loaded,path,error,sizeof error));
            CHECK(!memcmp(snapshot,loaded,sizeof *loaded));CHECK(world_hash==pe_world_hash(&loaded->world));
        }
        free(bad);
    }
    free(original);
    CHECK(pe_inventory_add(&g->inventory,917,5));CHECK(pe_adventure_medicine_complete(g));
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->medicine_state==PE_MEDICINE_COMPLETE);CHECK(!pe_adventure_medicine_accept(loaded));CHECK(!pe_adventure_medicine_complete(loaded));
    CHECK(pe_inventory_count(&loaded->inventory,925)==10&&pe_inventory_count(&loaded->inventory,554)==1);
    same_stocks(g,loaded,0,PE_TOWN_STOCK_CAPACITY);
    g->medicine_state=3;size_t before_size=0,after_size=0;unsigned char *before=read_save(path,&before_size);
    CHECK(!pe_game_save(g,path,error,sizeof error));unsigned char *after=read_save(path,&after_size);
    CHECK(before&&after&&before_size==after_size&&!memcmp(before,after,before_size));free(before);free(after);
 done:
    if(fd>=0)unlink(path);
    pe_game_destroy(g);pe_game_destroy(loaded);pe_world_destroy(w);
    free(g);free(loaded);free(w);free(snapshot);return failures;
}
