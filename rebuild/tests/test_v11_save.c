#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_adventure_hunt.h"
#include "pe_adventure_medicine.h"
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"v11:%d %s\n",__LINE__,#c);failures++;}}while(0)
static void put32(unsigned char *p,uint32_t n){for(int i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static bool write_bytes(const char *path,unsigned char *bytes,size_t n,unsigned version){
    put32(bytes+4,version);put32(bytes+8,(uint32_t)n-16);uint32_t sum=2166136261u;
    for(size_t i=16;i<n;i++){sum^=bytes[i];sum*=16777619u;}put32(bytes+12,sum);
    FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(bytes,1,n,f)==n;if(fclose(f))ok=false;return ok;
}
int pe_test_v11_save(void){
    failures=0;char path[]="/tmp/pe-v11-XXXXXX",error[256];int fd=mkstemp(path);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);unsigned char *bytes=NULL,*snapshot=NULL;
    if(fd<0||!g||!loaded){failures++;goto done;}close(fd);
    CHECK(pe_game_init_mode(g,74,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(g));
    CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));g->annot_complete=true;
    /* Isolate save progression from the starter inventory capacity limit. */
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id,.categorized=g->mode==PE_GAME_ADVENTURE};
    CHECK(pe_adventure_medicine_accept(g));CHECK(pe_inventory_add(&g->inventory,917,5));CHECK(pe_adventure_medicine_complete(g));
    CHECK(pe_adventure_hunt_reference(&g->hunt_destination));
    for(unsigned kills=0;kills<=4;kills++){
        g->hunt_kills=kills;g->hunt_state=kills==4?PE_HUNT_RETURN_PENDING:PE_HUNT_ENCOUNTER_PENDING;
        CHECK(pe_adventure_hunt_state_valid(g));CHECK(pe_game_save(g,path,error,sizeof error));
        CHECK(pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->hunt_kills==kills&&loaded->hunt_state==g->hunt_state);
        CHECK(loaded->medicine_state==PE_MEDICINE_COMPLETE);
    }
    /* No invalid state/count combination may reach disk. */
    g->hunt_kills=3;CHECK(!pe_adventure_hunt_state_valid(g));CHECK(!pe_game_save(g,path,error,sizeof error));
    g->hunt_state=PE_HUNT_ENCOUNTER_PENDING;g->hunt_kills=4;CHECK(!pe_game_save(g,path,error,sizeof error));
    g->hunt_state=PE_HUNT_TRAVEL;g->hunt_kills=1;CHECK(!pe_game_save(g,path,error,sizeof error));
    g->hunt_state=PE_HUNT_ENCOUNTER_PENDING;g->hunt_kills=2;CHECK(pe_game_save(g,path,error,sizeof error));
    FILE *f=fopen(path,"rb");CHECK(f);if(!f)goto done;
    CHECK(!fseek(f,0,SEEK_END));long length=ftell(f);rewind(f);CHECK(length>1816);if(length<=1816){fclose(f);goto done;}
    size_t n=(size_t)length;bytes=malloc(n+1);snapshot=malloc(sizeof *loaded);CHECK(bytes&&snapshot);
    if(!bytes||!snapshot){fclose(f);goto done;}CHECK(fread(bytes,1,n,f)==n);fclose(f);
    CHECK(bytes[4]==19);CHECK(pe_game_load(loaded,path,error,sizeof error));memcpy(snapshot,loaded,sizeof *loaded);
    n-=(PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+PE_SAVE_V17_APPEND+PE_SAVE_V16_APPEND+PE_SAVE_V15_APPEND+PE_SAVE_V14_APPEND+PE_SAVE_V13_APPEND; /* Exercise literal v11 payload, excluding later append. */
    uint64_t hash=pe_world_hash(&loaded->world);
    put32(bytes+n-4,5);CHECK(write_bytes(path,bytes,n,11));CHECK(!pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(snapshot,loaded,sizeof *loaded)&&pe_world_hash(&loaded->world)==hash);
    put32(bytes+n-4,2);CHECK(write_bytes(path,bytes,n-1,11));CHECK(!pe_game_load(loaded,path,error,sizeof error));
    bytes[n]=0;CHECK(write_bytes(path,bytes,n+1,11));CHECK(!pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    /* Literal v10 prefix retains its active quest but defaults progress to0. */
    CHECK(write_bytes(path,bytes,n-4,10));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->hunt_state==PE_HUNT_ENCOUNTER_PENDING&&loaded->hunt_kills==0);
    /* New return state must not be smuggled into an old v10 payload. */
    put32(bytes+n-4-PE_SAVE_V10_APPEND,PE_HUNT_RETURN_PENDING);
    CHECK(write_bytes(path,bytes,n-4,10));CHECK(!pe_game_load(loaded,path,error,sizeof error));
    put32(bytes+n-4,4);CHECK(write_bytes(path,bytes,n,11));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->hunt_state==PE_HUNT_RETURN_PENDING&&loaded->hunt_kills==4);
 done:
    if(fd>=0)unlink(path);
    pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);free(bytes);free(snapshot);return failures;
}
