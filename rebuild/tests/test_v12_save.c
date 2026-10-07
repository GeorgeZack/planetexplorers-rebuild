#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_adventure_hunt.h"
#include "pe_adventure_medicine.h"
#include "pe_settlement.h"
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"v12:%d %s\n",__LINE__,#c);failures++;}}while(0)
static void put32(unsigned char*p,uint32_t n){for(int i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static bool write_bytes(const char*path,unsigned char*b,size_t n,unsigned version){
    put32(b+4,version);put32(b+8,(uint32_t)n-16);uint32_t sum=2166136261u;for(size_t i=16;i<n;i++){sum^=b[i];sum*=16777619u;}put32(b+12,sum);
    FILE*f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(b,1,n,f)==n;return fclose(f)==0&&ok;
}
static int awarded_reload(void){
    failures=0;
    const char*assets=PE_SOURCE_ROOT "/rebuild/build/reference-assets";
    const char*terrain=PE_SOURCE_ROOT "/rebuild/build/reference-assets/adventure-route/terrain.pevx";
    const char*collision=PE_SOURCE_ROOT "/rebuild/build/reference-assets/adventure-route/route-collision.obj";
    if(access(terrain,R_OK)||access(collision,R_OK)){
        fprintf(stderr,"v12: SKIP awarded return/reload real-route test; recovered assets unavailable\n");return 0;
    }
    PeGame*g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);char error[256];
    char path[]=PE_SOURCE_ROOT "/rebuild/build/v12-awarded-XXXXXX";int fd=mkstemp(path);
    if(!g||!loaded||fd<0){failures++;goto done;}close(fd);
    CHECK(pe_game_init_mode(g,89,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(g));
    CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));g->annot_complete=true;
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};CHECK(pe_adventure_medicine_accept(g));
    CHECK(pe_inventory_add(&g->inventory,917,5));CHECK(pe_adventure_medicine_complete(g));
    CHECK(pe_adventure_hunt_reference(&g->hunt_destination));g->hunt_state=PE_HUNT_RETURN_PENDING;g->hunt_kills=4;
    if(!pe_settlement_configure(assets,error,sizeof error)){CHECK(false);goto done;}
    pe_world_destroy(&g->world);
    if(!pe_settlement_load_world(terrain,&g->world,error,sizeof error)){CHECK(false);goto done;}
    g->tree_count=0;
    /* Elevated, clear fixture isolates XZ arrival plus serialization. It does
     * not claim walked arrival or ground support at the recorded destination. */
    g->player=(PeVec3){-266-g->world.origin_x,50-g->world.origin_y,136-g->world.origin_z};
    int prior[4];for(int i=0;i<4;i++)prior[i]=pe_inventory_count(&g->inventory,592+i);
    CHECK(pe_adventure_hunt_return_step(g));
    PeInventory awarded=g->inventory;
    for(unsigned state=PE_HUNT_RETURN_TALK1;state<=PE_HUNT_COMPLETE;state++){
        CHECK(g->hunt_state==state);
        bool saved=pe_game_save(g,path,error,sizeof error);if(!saved)fprintf(stderr,"v12 awarded save: %s\n",error);CHECK(saved);
        if(!saved)goto done;
        bool restored=pe_game_load(loaded,path,error,sizeof error);if(!restored)fprintf(stderr,"v12 awarded load: %s\n",error);CHECK(restored);
        if(!restored)goto done;
        CHECK(loaded->hunt_state==state&&loaded->hunt_kills==4);
        CHECK(!memcmp(&loaded->inventory,&awarded,sizeof awarded));
        for(int i=0;i<4;i++)CHECK(pe_inventory_count(&loaded->inventory,592+i)==prior[i]+1);
        CHECK(!pe_adventure_hunt_return_step(loaded)&&!memcmp(&loaded->inventory,&awarded,sizeof awarded));
        int talk=state==PE_HUNT_RETURN_TALK1?4105:state==PE_HUNT_RETURN_TALK2?4106:0;
        CHECK(pe_adventure_hunt_return_talk(loaded)==talk);
        if(talk){CHECK(pe_adventure_hunt_return_advance(loaded,talk));CHECK(!pe_adventure_hunt_return_advance(loaded,talk));}
        else CHECK(!pe_adventure_hunt_return_advance(loaded,4106));
        CHECK(!memcmp(&loaded->inventory,&awarded,sizeof awarded));
        pe_game_move(g,loaded);
    }
done:
    if(fd>=0)unlink(path);
    pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);return failures;
}
static int awarded_reload_isolated(void){
    /* The real geometry cache is process-global; do not affect later fixtures. */
    pid_t pid=fork();if(pid<0)return 1;if(pid==0)_exit(awarded_reload()?1:0);
    int status=0;pid_t waited;
    do{waited=waitpid(pid,&status,0);}while(waited<0&&errno==EINTR);
    return waited!=pid||!WIFEXITED(status)||WEXITSTATUS(status)!=0;
}
int pe_test_v12_save(void){
    failures=awarded_reload_isolated();char path[]="/tmp/pe-v12-XXXXXX",error[256];int fd=mkstemp(path);PeGame*g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);
    unsigned char*bytes=NULL,*after=NULL,*snapshot=NULL;if(fd<0||!g||!loaded){failures++;goto done;}close(fd);
    CHECK(pe_game_init_mode(g,88,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));g->annot_complete=true;
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};CHECK(pe_adventure_medicine_accept(g));CHECK(pe_inventory_add(&g->inventory,917,5));CHECK(pe_adventure_medicine_complete(g));
    CHECK(pe_adventure_hunt_reference(&g->hunt_destination));g->hunt_kills=4;
    for(unsigned state=PE_HUNT_RETURN_TALK1;state<=PE_HUNT_COMPLETE;state++){
        g->hunt_state=state;CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));
        CHECK(loaded->hunt_state==state&&loaded->hunt_kills==4);
        CHECK(pe_adventure_hunt_return_talk(loaded)==(state==4?4105:state==5?4106:0));
        CHECK(!memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory));
    }
    FILE*f=fopen(path,"rb");CHECK(f);if(!f)goto done;CHECK(!fseek(f,0,SEEK_END));long length=ftell(f);rewind(f);CHECK(length>1816);if(length<=1816){fclose(f);goto done;}
    size_t n=(size_t)length;bytes=malloc(n);after=malloc(n+1);snapshot=malloc(sizeof *loaded);CHECK(bytes&&after&&snapshot);if(!bytes||!after||!snapshot){fclose(f);goto done;}
    CHECK(fread(bytes,1,n,f)==n);fclose(f);CHECK(bytes[4]==18);n-=PE_SAVE_V18_APPEND+PE_SAVE_V17_APPEND+PE_SAVE_V16_APPEND+PE_SAVE_V15_APPEND+PE_SAVE_V14_APPEND+PE_SAVE_V13_APPEND;size_t state_at=n-PE_SAVE_V11_APPEND-PE_SAVE_V10_APPEND;
    memcpy(snapshot,loaded,sizeof *loaded);uint64_t hash=pe_world_hash(&loaded->world);
    /* New semantic states are impossible in every older payload version. */
    for(unsigned state=4;state<=6;state++){
        put32(bytes+state_at,state);CHECK(write_bytes(path,bytes,n,11));CHECK(!pe_game_load(loaded,path,error,sizeof error));
        CHECK(!memcmp(snapshot,loaded,sizeof *loaded)&&pe_world_hash(&loaded->world)==hash);
    }
    put32(bytes+state_at,7);CHECK(write_bytes(path,bytes,n,12));CHECK(!pe_game_load(loaded,path,error,sizeof error));
    put32(bytes+state_at,4);put32(bytes+n-4,3);CHECK(write_bytes(path,bytes,n,12));CHECK(!pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    /* The V11 layout remains literal; outfit migration advances its counter. */
    put32(bytes+state_at,PE_HUNT_RETURN_PENDING);put32(bytes+n-4,4);CHECK(write_bytes(path,bytes,n,11));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->hunt_state==PE_HUNT_RETURN_PENDING&&loaded->hunt_kills==4);
    CHECK(pe_game_save(loaded,path,error,sizeof error));
    put32(bytes+pe_test_save_counter_offset(loaded),loaded->inventory.next_instance_id);
    f=fopen(path,"rb");CHECK(f);
    if(f){size_t got=fread(after,1,n+PE_SAVE_V18_APPEND+PE_SAVE_V17_APPEND+PE_SAVE_V16_APPEND+PE_SAVE_V15_APPEND+PE_SAVE_V14_APPEND+PE_SAVE_V13_APPEND+1,f);fclose(f);CHECK(got==n+PE_SAVE_V18_APPEND+PE_SAVE_V17_APPEND+PE_SAVE_V16_APPEND+PE_SAVE_V15_APPEND+PE_SAVE_V14_APPEND+PE_SAVE_V13_APPEND&&after[4]==18&&!memcmp(bytes+16,after+16,n-16));}
 done:
    if(fd>=0)unlink(path);
    pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);free(bytes);free(after);free(snapshot);return failures;
}
