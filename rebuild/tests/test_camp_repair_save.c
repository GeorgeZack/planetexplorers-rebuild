#define _POSIX_C_SOURCE 200809L
#include "pe_camp_repair.h"
#include "pe_settlement.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"repair save:%d %s\n",__LINE__,#x);failures++;}}while(0)
static uint32_t u32(const unsigned char *p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static unsigned char *read_bytes(const char *path,size_t *n){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long z=ftell(f);rewind(f);
    if(z<16){fclose(f);return NULL;}unsigned char *p=malloc((size_t)z+1);
    bool ok=p&&fread(p,1,(size_t)z,f)==(size_t)z;if(fclose(f))ok=false;
    if(!ok){free(p);return NULL;}*n=(size_t)z;return p;
}
static bool write_bytes(const char *path,unsigned char *p,size_t n,unsigned version){
    if(n<16)return false;
    put(p+4,version);put(p+8,(uint32_t)n-16);uint32_t hash=2166136261u;
    for(size_t i=16;i<n;i++){hash^=p[i];hash*=16777619u;}put(p+12,hash);
    FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(p,1,n,f)==n;return fclose(f)==0&&ok;
}
static int find(const PeInventory *bag,int id){for(int i=0;i<pe_inventory_capacity(bag);i++)if(bag->slots[i].item_id==id)return i;return -1;}
static bool same_repair(PeCampRepair a,PeCampRepair b){return a.item.item_id==b.item.item_id&&a.item.count==b.item.count&&a.item.instance_id==b.item.instance_id&&a.item.durability==b.item.durability&&a.running==b.running&&a.remaining_ticks==b.remaining_ticks;}
static int repair_save_cases(void){
    failures=0;char path[1024],error[256];
    snprintf(path,sizeof path,"%s/pe-repair-save-XXXXXX",getenv("TMPDIR")?getenv("TMPDIR"):"/tmp");int fd=mkstemp(path);if(fd>=0)close(fd);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*clone=calloc(1,sizeof *clone),*snapshot=malloc(sizeof *snapshot);
    PeWorld *world=calloc(1,sizeof *world);unsigned char *bytes=NULL,*bad=NULL,*after=NULL;size_t n=0,m=0;
    CHECK(fd>=0&&g&&loaded&&clone&&snapshot&&world);if(fd<0||!g||!loaded||!clone||!snapshot||!world)goto done;
    CHECK(pe_game_init_mode(g,720,PE_GAME_ADVENTURE));CHECK(pe_game_repair_state_valid(g));CHECK(!pe_game_repair_can_start(g));
    bool complete=true;
    CHECK(pe_game_repair_frame(g,0,true,&complete)&&!complete);
    const float invalid_empty[]={-1,NAN,INFINITY,0x1p63f/100000};
    for(unsigned i=0;i<sizeof invalid_empty/sizeof *invalid_empty;i++){complete=true;CHECK(!pe_game_repair_frame(g,invalid_empty[i],true,&complete)&&complete);}
    g->repair.item.item_id=1;CHECK(!pe_game_repair_frame(g,0,true,&complete));g->repair=(PeCampRepair){0};
    g->repair.item.durability=-0.0f;CHECK(!pe_game_repair_frame(g,0,true,&complete));g->repair=(PeCampRepair){0};
    g->repair.remaining_ticks=1;CHECK(!pe_game_repair_frame(g,0,true,&complete));g->repair=(PeCampRepair){0};
    g->repair.running=true;CHECK(!pe_game_repair_frame(g,0,true,&complete));g->repair=(PeCampRepair){0};
    g->mode=(PeGameMode)2;CHECK(!pe_game_repair_frame(g,0,true,&complete));g->mode=PE_GAME_ADVENTURE;
    CHECK(!pe_game_repair_frame(NULL,0,true,&complete));
    CHECK(pe_inventory_add(&g->inventory,1,40));int index=find(&g->inventory,1);CHECK(index>=0);if(index<0)goto done;
    CHECK(pe_inventory_move(&g->inventory,index,839));index=839;g->inventory.slots[index].durability=2369*.5f;PeSlot sword=g->inventory.slots[index];
    CHECK(!pe_game_repair_can_insert(g,index)); /* Ordinary worlds have no machine. */
    bool started=pe_settlement_start(g,error,sizeof error);CHECK(started);if(!started){fprintf(stderr,"repair fixture requires installed route: %s\n",error);goto done;}
    CHECK(g->world.extended&&pe_settlement_is_route(&g->world));CHECK(pe_game_quickslot_assign(g,8,index));
    /* Literal19 migration keeps authoritative high-index inventory and IDs. */
    CHECK(pe_game_save(g,path,error,sizeof error));bytes=read_bytes(path,&n);CHECK(bytes&&n>((PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)+PE_SAVE_V20_APPEND)+PE_SAVE_V19_APPEND+16);if(!bytes||n<=((PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)+PE_SAVE_V20_APPEND)+PE_SAVE_V19_APPEND+16)goto done;
    CHECK(u32(bytes+4)==PE_SAVE_VERSION);for(size_t i=n-(PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)-32;i<n;i++)CHECK(!bytes[i]);
    CHECK(write_bytes(path,bytes,n-(PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)-32,19)&&pe_game_load(loaded,path,error,sizeof error));CHECK(!loaded->repair.item.count&&!loaded->repair.running&&!loaded->repair.remaining_ticks&&loaded->inventory.categorized);
    CHECK(!memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory)&&loaded->quickslots[8].instance_id==sword.instance_id);
    CHECK(pe_game_save(loaded,path,error,sizeof error));after=read_bytes(path,&m);CHECK(after&&m==n);if(after&&m==n)CHECK(!memcmp(bytes+16,after+16,n-(PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)-32-16));free(after);after=NULL;
    /* Aliases and dead/revival transactions reject without side effects. */
    CHECK(pe_game_repair_candidate(g,index)&&pe_game_repair_can_insert(g,index));g->equipped_instance_id=sword.instance_id;CHECK(!pe_game_repair_candidate(g,index)&&!pe_game_repair_can_insert(g,index));g->equipped_instance_id=0;
    g->shield_instance_id=sword.instance_id;CHECK(!pe_game_repair_can_insert(g,index));g->shield_instance_id=0;
    uint32_t clothing=g->outfit.items[0].instance_id;g->outfit.items[0].instance_id=sword.instance_id;CHECK(!pe_game_repair_can_insert(g,index));g->outfit.items[0].instance_id=clothing;
    g->deployed[0].instance_id=sword.instance_id;CHECK(!pe_game_repair_can_insert(g,index));g->deployed[0].instance_id=0;
    g->town_buyback[0].slots[0].instance_id=sword.instance_id;CHECK(!pe_game_repair_can_insert(g,index));g->town_buyback[0].slots[0].instance_id=0;
    float hp=g->hp;g->hp=0;CHECK(!pe_game_repair_can_insert(g,index));g->hp=hp;
    g->revival.phase=PE_REVIVAL_GETTING_UP;CHECK(!pe_game_repair_can_insert(g,index));g->revival=(PeRevival){0};
    memcpy(snapshot,g,sizeof *g);PeSlot stale=sword;stale.durability--;
    CHECK(!pe_game_repair_insert(g,index,&stale)&&!memcmp(snapshot,g,sizeof *g));
    CHECK(pe_game_repair_insert(g,index,&sword)&&!g->quickslots[8].item_id&&!g->inventory.slots[index].count&&g->repair.item.instance_id==sword.instance_id&&pe_game_repair_state_valid(g));
    int prior_wood=pe_inventory_count(&g->inventory,279);CHECK(pe_inventory_add(&g->inventory,279,4));CHECK(pe_game_repair_can_start(g)&&pe_game_repair_start(g));int wood=pe_inventory_count(&g->inventory,279);CHECK(wood==prior_wood+2);
    complete=true;CHECK(pe_game_repair_frame(g,.000001f,true,&complete)&&!complete&&g->repair.remaining_ticks==6000000);
    CHECK(pe_game_repair_frame(g,7.25f,true,&complete)&&!complete&&g->repair.remaining_ticks==5275000);
    /* Neither fixed catch-up nor pause/closed display time consumes job ticks. */
    PeCampRepair running=g->repair;for(int i=0;i<3;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);CHECK(same_repair(running,g->repair));
    CHECK(pe_game_repair_frame(g,5,false,&complete)&&same_repair(running,g->repair));
    CHECK(pe_game_clone(clone,g)&&clone->world.extended!=g->world.extended&&same_repair(clone->repair,g->repair));
    pe_game_move(loaded,clone);CHECK(!clone->repair.item.count&&!clone->world.extended&&!clone->world.seed&&same_repair(loaded->repair,g->repair));
    pe_world_init(world,3);memcpy(snapshot,g,sizeof *g);uint64_t hash=pe_world_hash(&g->world);
    CHECK(!pe_game_set_world(g,world,(PeVec3){32,20,32})&&!memcmp(snapshot,g,sizeof *g)&&pe_world_hash(&g->world)==hash);
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));CHECK(same_repair(g->repair,loaded->repair)&&!memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory));
    free(bytes);bytes=read_bytes(path,&n);CHECK(bytes&&n>(PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)+32);if(!bytes||n<=(PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)+32)goto done;
    size_t tail=n-(PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)-32;CHECK(u32(bytes+tail)==20006&&u32(bytes+tail+4)==1&&u32(bytes+tail+8)==1&&u32(bytes+tail+12)==sword.instance_id&&u32(bytes+tail+20)==1&&u32(bytes+tail+24)==5275000&&!u32(bytes+tail+28));
    bad=malloc(n+1);CHECK(bad);if(!bad)goto done;memcpy(snapshot,loaded,sizeof *loaded);hash=pe_world_hash(&loaded->world);
    const struct{unsigned at;uint32_t value;} corruption[]={
        {0,0},{0,20007},{4,33},{4,0x80000000},{8,0},{8,2},{12,0},{12,UINT32_MAX},
        {16,0x7fc00000},{16,0xbf800000},{16,0x45141000}, /* >max */
        {20,2},{20,0},{24,0},{24,6000001},{28,1},
    };
    for(unsigned i=0;i<sizeof corruption/sizeof *corruption;i++){
        memcpy(bad,bytes,n);put(bad+tail+corruption[i].at,corruption[i].value);CHECK(write_bytes(path,bad,n,PE_SAVE_VERSION)&&!pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(snapshot,loaded,sizeof *loaded)&&pe_world_hash(&loaded->world)==hash);
    }
    /* Existing bag identity and outfit identity cannot be a detached owner. */
    int other=find(&g->inventory,1);CHECK(other>=0);if(other<0)goto done;
    const uint32_t aliases[]={g->inventory.slots[other].instance_id,g->outfit.items[0].instance_id,g->inventory.next_instance_id};
    for(unsigned i=0;i<3;i++){memcpy(bad,bytes,n);put(bad+tail+12,aliases[i]);CHECK(write_bytes(path,bad,n,PE_SAVE_VERSION)&&!pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(snapshot,loaded,sizeof *loaded));}
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n-1,PE_SAVE_VERSION)&&!pe_game_load(loaded,path,error,sizeof error));memcpy(bad,bytes,n);bad[n]=0;CHECK(write_bytes(path,bad,n+1,PE_SAVE_VERSION)&&!pe_game_load(loaded,path,error,sizeof error));CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    CHECK(write_bytes(path,bytes,n,PE_SAVE_VERSION));PeCampRepair saved=g->repair;g->repair.remaining_ticks=-1;
    CHECK(!pe_game_save(g,path,error,sizeof error));after=read_bytes(path,&m);CHECK(after&&m==n);if(after&&m==n)CHECK(!memcmp(bytes,after,n));free(after);after=NULL;g->repair=saved;
    g->quickslots[0]=(PeQuickslot){1,sword.instance_id};CHECK(!pe_game_repair_state_valid(g)&&!pe_game_save(g,path,error,sizeof error));g->quickslots[0]=(PeQuickslot){0};
    /* Source death doesn't stop the machine; transactions still require life. */
    loaded->hp=0;CHECK(!pe_game_repair_take(loaded,NULL)&&!pe_game_repair_cancel(loaded));CHECK(pe_game_repair_frame(loaded,52.75f,true,&complete)&&complete&&loaded->repair.item.durability==2369&&!loaded->repair.running);
    CHECK(pe_game_save(loaded,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->hp==0&&loaded->repair.item.durability==2369&&!loaded->repair.running);
    CHECK(pe_game_repair_frame(loaded,1,true,&complete)&&!complete);loaded->hp=hp;
    CHECK(pe_game_repair_take(loaded,&index)&&loaded->inventory.slots[index].instance_id==sword.instance_id&&loaded->inventory.slots[index].durability==2369&&!loaded->quickslots[8].item_id);
    CHECK(pe_game_repair_cancel(g)&&!g->repair.running&&g->repair.item.durability==sword.durability&&pe_inventory_count(&g->inventory,279)==wood);
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error)&&same_repair(g->repair,loaded->repair));CHECK(pe_game_repair_start(loaded)&&pe_inventory_count(&loaded->inventory,279)==wood-2&&loaded->repair.remaining_ticks==6000000);
    /* Returning a completed job to a full category remains retryable on disk. */
    CHECK(pe_game_repair_frame(loaded,60,true,&complete)&&complete);int occupied=0;for(int i=420;i<840;i++)occupied+=loaded->inventory.slots[i].count!=0;
    CHECK(pe_inventory_add(&loaded->inventory,34,420-occupied));memcpy(snapshot,loaded,sizeof *loaded);CHECK(!pe_game_repair_take(loaded,NULL)&&!memcmp(snapshot,loaded,sizeof *loaded));
    CHECK(pe_game_save(loaded,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error)&&loaded->repair.item.instance_id==sword.instance_id&&!loaded->repair.running);
    CHECK(pe_inventory_remove(&loaded->inventory,34,1)&&pe_game_repair_take(loaded,NULL)&&!loaded->repair.item.count);
    CHECK(pe_game_set_world(loaded,world,(PeVec3){32,20,32})&&!loaded->repair.item.count);
    /* Legacy never acquires machine ownership; zero suffix remains mandatory. */
    pe_game_destroy(loaded);pe_game_init(loaded,720);CHECK(pe_game_repair_state_valid(loaded));loaded->repair=saved;CHECK(!pe_game_repair_state_valid(loaded)&&!pe_game_save(loaded,path,error,sizeof error));loaded->repair=(PeCampRepair){0};
    CHECK(pe_game_save(loaded,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error)&&!loaded->repair.item.count);
 done:
    if(fd>=0)unlink(path);
    free(bytes);free(bad);free(after);pe_world_destroy(world);free(world);pe_game_destroy(g);pe_game_destroy(loaded);pe_game_destroy(clone);free(g);free(loaded);free(clone);free(snapshot);return failures;
}
int pe_test_camp_repair_save(void){
    /* Installed geometry is immutable for the process. Keep it out of the
     * parent so later missing-asset fixtures can configure their own roots.
     * exit (not _exit) retains LeakSanitizer coverage inside the child. */
    fflush(NULL);pid_t pid=fork();if(pid<0)return 1;
    if(!pid)exit(repair_save_cases()?1:0);
    int status=0;pid_t waited;
    do{waited=waitpid(pid,&status,0);}while(waited<0&&errno==EINTR);
    return waited!=pid||!WIFEXITED(status)||WEXITSTATUS(status)!=0;
}
#ifdef PE_CAMP_REPAIR_SAVE_TEST_MAIN
int main(void){return pe_test_camp_repair_save();}
#endif
