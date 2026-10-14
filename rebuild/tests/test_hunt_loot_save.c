#define _POSIX_C_SOURCE 200809L
#include "pe_hunt_loot.h"
#include "pe_settlement.h"
#include <assert.h>
#include <errno.h>
#include <sys/wait.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint32_t get32(const unsigned char *p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static void put_double(unsigned char *p,double value){uint64_t bits;memcpy(&bits,&value,8);put32(p,(uint32_t)bits);put32(p+4,(uint32_t)(bits>>32));}
static unsigned char *read_file(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");assert(f&&fseek(f,0,SEEK_END)==0);long n=ftell(f);assert(n>=16);rewind(f);
    unsigned char *bytes=malloc((size_t)n+1);assert(bytes&&fread(bytes,1,(size_t)n,f)==(size_t)n&&fclose(f)==0);*size=(size_t)n;return bytes;
}
static void write_file(const char *path,unsigned char *bytes,size_t n,uint32_t version){
    assert(n>=16);put32(bytes+4,version);put32(bytes+8,(uint32_t)(n-16));uint32_t hash=2166136261u;
    for(size_t i=16;i<n;i++){hash^=bytes[i];hash*=16777619u;}put32(bytes+12,hash);
    FILE *f=fopen(path,"wb");assert(f&&fwrite(bytes,1,n,f)==n&&fclose(f)==0);
}
static int loot_cases(void){
    char path[1024],error[256];snprintf(path,sizeof path,"%s/pe-hunt-loot-XXXXXX",getenv("TMPDIR")?getenv("TMPDIR"):"/tmp");
    int fd=mkstemp(path);assert(fd>=0);close(fd);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*copy=calloc(1,sizeof *copy),*snapshot=malloc(sizeof *snapshot);
    PeWorld *world=calloc(1,sizeof *world);assert(g&&loaded&&copy&&snapshot&&world);
    assert(PE_SAVE_V22_APPEND==2052&&PE_HUNT_LOOT_CAPACITY==64);
    assert(pe_game_init_mode(g,722,PE_GAME_ADVENTURE));
    if(!pe_settlement_start(g,error,sizeof error)){fprintf(stderr,"loot save route fixture: %s\n",error);abort();}
    assert(pe_conversation_history_add(&g->conversation_history,"Westina","Existing history survives loot.",NULL));
    g->rng_state=1;PeVec3 point=g->player;point.x+=8;
    assert(pe_hunt_loot_spawn(g,&point,1)&&g->hunt_loot.count==4);
    assert(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    assert(!memcmp(&g->hunt_loot,&loaded->hunt_loot,sizeof g->hunt_loot)&&g->rng_state==loaded->rng_state&&!memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory));
    size_t n,m;unsigned char *bytes=read_file(path,&n);assert(n>(PE_SAVE_V23_APPEND+PE_SAVE_V22_APPEND)+16);
    size_t tail=n-(PE_SAVE_V23_APPEND+PE_SAVE_V22_APPEND),row=tail+4;assert(get32(bytes+4)==PE_SAVE_VERSION&&get32(bytes+tail)==4);
    assert(get32(bytes+row)==g->hunt_loot.drops[0].instance_id&&get32(bytes+row+4)==229&&get32(bytes+row+8)==30);
    for(size_t i=row+32*g->hunt_loot.count;i<n-PE_SAVE_V23_APPEND;i++)assert(bytes[i]==0);
    /* A literal v21 knows nothing of world loot; it preserves the existing RNG
     * and already allocated counter rather than attempting to recreate drops. */
    write_file(path,bytes,tail,21);assert(pe_game_load(loaded,path,error,sizeof error));
    assert(!loaded->hunt_loot.count&&pe_hunt_loot_valid(loaded));
    assert(loaded->rng_state==g->rng_state&&!memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory)&&!memcmp(&loaded->conversation_history,&g->conversation_history,sizeof g->conversation_history));
    assert(pe_game_save(loaded,path,error,sizeof error));unsigned char *again=read_file(path,&m);
    assert(m==n&&!memcmp(bytes+16,again+16,tail-16));for(size_t i=tail;i<n-PE_SAVE_V23_APPEND;i++)assert(!again[i]);free(again);
    write_file(path,bytes,n,PE_SAVE_VERSION);assert(pe_game_load(loaded,path,error,sizeof error));
    unsigned picked=7,expired=8;assert(pe_hunt_loot_update(loaded,&picked,&expired)&&!picked&&!expired);
    assert(!memcmp(&g->hunt_loot,&loaded->hunt_loot,sizeof g->hunt_loot));
    /* Reload does not roll again. A subsequent actual death consumes the same
     * saved RNG suffix and instance sequence as uninterrupted execution. */
    assert(pe_game_clone(copy,g));assert(pe_hunt_loot_spawn(copy,&point,1)&&pe_hunt_loot_spawn(loaded,&point,1));
    assert(!memcmp(&copy->hunt_loot,&loaded->hunt_loot,sizeof copy->hunt_loot)&&copy->rng_state==loaded->rng_state&&copy->inventory.next_instance_id==loaded->inventory.next_instance_id);
    assert(pe_game_load(loaded,path,error,sizeof error));memcpy(snapshot,loaded,sizeof *loaded);uint64_t hash=pe_world_hash(&loaded->world);
    unsigned char *bad=malloc(n+1);assert(bad);
    for(unsigned fault=0;fault<19;fault++){
        memcpy(bad,bytes,n);
        switch(fault){
        case 0:put32(bad+tail,65);break;
        case 1:put32(bad+tail,0);break;
        case 2:put32(bad+row,0);break;
        case 3:put32(bad+row,g->inventory.next_instance_id);break;
        case 4:put32(bad+row,g->outfit.items[0].instance_id);break;
        case 5:put32(bad+row+32,g->hunt_loot.drops[0].instance_id);break;
        case 6:put32(bad+row+4,279);break;
        case 7:put32(bad+row+8,0);break;
        case 8:put32(bad+row+8,10000);break;
        case 9:put32(bad+row+12,0x7fc00000u);break;
        case 10:put32(bad+row+16,0xbf800000u);break;
        case 11:{float width=(float)pe_world_width(&g->world);uint32_t bits;memcpy(&bits,&width,4);put32(bad+row+12,bits);break;}
        case 12:put_double(bad+row+24,g->game_seconds+1);break;
        case 13:put_double(bad+row+24,-1);break;
        case 14:put_double(bad+row+24,NAN);break;
        case 15:put_double(bad+row+24,-0.0);break;
        case 16:bad[n-1]=1;break;
        case 17:put32(bad+row+32*g->hunt_loot.count+12,0x80000000u);break;
        case 18:put32(bad+row,g->inventory.slots[420].instance_id);break;
        }
        write_file(path,bad,n,PE_SAVE_VERSION);assert(!pe_game_load(loaded,path,error,sizeof error));
        assert(!memcmp(snapshot,loaded,sizeof *loaded)&&pe_world_hash(&loaded->world)==hash);
    }
    memcpy(bad,bytes,n);write_file(path,bad,n-1,PE_SAVE_VERSION);assert(!pe_game_load(loaded,path,error,sizeof error));
    memcpy(bad,bytes,n);bad[n]=0;write_file(path,bad,n+1,PE_SAVE_VERSION);assert(!pe_game_load(loaded,path,error,sizeof error)&&!memcmp(snapshot,loaded,sizeof *loaded));
    write_file(path,bytes,n,PE_SAVE_VERSION);g->hunt_loot.drops[63].count=1;
    assert(!pe_game_save(g,path,error,sizeof error));again=read_file(path,&m);assert(m==n&&!memcmp(bytes,again,n));free(again);
    assert(!pe_game_clone(loaded,g)&&!memcmp(snapshot,loaded,sizeof *loaded));g->hunt_loot.drops[63].count=0;
    /* Owning copies preserve loot and its item IDs independently. World reset
     * cannot silently strand drops; clear is an explicit accepted lifecycle. */
    assert(pe_game_clone(copy,g)&&copy->world.extended&&copy->world.extended!=g->world.extended&&!memcmp(&copy->hunt_loot,&g->hunt_loot,sizeof g->hunt_loot));
    pe_game_move(loaded,copy);assert(!copy->world.extended&&!copy->hunt_loot.count&&loaded->hunt_loot.count==g->hunt_loot.count);
    assert(pe_world_resize(world,96,32,64));world->seed=88;
    for(int z=0;z<64;z++)for(int y=0;y<4;y++)for(int x=0;x<96;x++)pe_world_set(world,x,y,z,(PeVoxel){255,PE_MAT_DIRT});
    memcpy(snapshot,loaded,sizeof *loaded);assert(!pe_game_set_world(loaded,world,(PeVec3){32,6,32})&&!memcmp(snapshot,loaded,sizeof *loaded));
    uint32_t rng=loaded->rng_state,counter=loaded->inventory.next_instance_id;pe_hunt_loot_clear(loaded);
    assert(pe_game_set_world(loaded,world,(PeVec3){32,6,32})&&!loaded->hunt_loot.count&&loaded->rng_state==rng&&loaded->inventory.next_instance_id==counter);
    assert(pe_game_save(loaded,path,error,sizeof error)&&pe_game_load(copy,path,error,sizeof error)&&!copy->hunt_loot.count);
    pe_world_destroy(world);free(world);pe_game_destroy(g);pe_game_destroy(loaded);pe_game_destroy(copy);
    free(g);free(loaded);free(copy);free(snapshot);free(bytes);free(bad);unlink(path);
    puts("hunt loot save: v22 roundtrip, literal v21 migration, corruption rollback, no rerolls and owning lifecycle pass");return 0;
}
/* Installed settlement geometry is process-immutable. Isolate its caches so
 * later missing/custom-asset tests can still configure their own roots.
 * Standalone sanitizer main bypasses fork for complete teardown/leak scanning. */
int pe_test_hunt_loot_save(void){
    fflush(NULL);pid_t pid=fork();if(pid<0)return 1;
    if(!pid)exit(loot_cases()?1:0);
    int status=0;pid_t waited;
    do{waited=waitpid(pid,&status,0);}while(waited<0&&errno==EINTR);
    return waited!=pid||!WIFEXITED(status)||WEXITSTATUS(status)!=0;
}
#ifdef PE_HUNT_LOOT_SAVE_TEST_MAIN
int main(void){return loot_cases();}
#endif
