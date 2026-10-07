#define _POSIX_C_SOURCE 200809L
#include "pe_game.h"
#include "pe_town_wallet.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"town wallet save:%d %s\n",__LINE__,#c);failures++;}}while(0)
static uint32_t get32(const unsigned char *p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static unsigned char *read_bytes(const char *path,size_t *n){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long length=ftell(f);
    if(length<16||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)length+1);if(!p){fclose(f);return NULL;}
    bool ok=fread(p,1,(size_t)length,f)==(size_t)length;if(fclose(f))ok=false;
    if(!ok){free(p);return NULL;}*n=(size_t)length;return p;
}
static bool write_bytes(const char *path,unsigned char *p,size_t n,uint32_t version){
    put32(p+4,version);put32(p+8,(uint32_t)n-16);uint32_t sum=2166136261u;
    for(size_t i=16;i<n;i++){sum^=p[i];sum*=16777619u;}put32(p+12,sum);
    FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(p,1,n,f)==n;return fclose(f)==0&&ok;
}
static void rejected(PeGame *g,const char *path,const PeGame *before,uint64_t hash){
    char error[256];CHECK(!pe_game_load(g,path,error,sizeof error));
    CHECK(!memcmp(g,before,sizeof *g)&&pe_world_hash(&g->world)==hash);
}
static void archived(const PeGame *g){
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++){
        const PeTownWalletSource *s=pe_town_wallet_source_at(i);CHECK(s);if(!s)continue;
        CHECK(g->town_wallet[i].balance==s->initial_balance&&g->town_wallet[i].added==s->initial_added);
    }
}
int pe_test_town_wallet_save(void){
    failures=0;char path[]="/tmp/pe-town-wallet-save-XXXXXX",error[256];int fd=mkstemp(path);
    if(fd>=0)close(fd);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*snapshot=malloc(sizeof *snapshot);
    PeWorld *world=calloc(1,sizeof *world);unsigned char *bytes=NULL,*bad=NULL,*after=NULL;
    size_t n=0,after_n=0;CHECK(fd>=0&&g&&loaded&&snapshot&&world);
    if(fd<0||!g||!loaded||!snapshot||!world)goto done;
    CHECK(PE_SAVE_V17_APPEND==100&&PE_TOWN_WALLET_CAPACITY==8);
    CHECK(pe_game_init_mode(g,717,PE_GAME_ADVENTURE));archived(g);
    CHECK(pe_world_resize(world,80,32,80));
    for(int z=0;z<80;z++)for(int y=0;y<=8;y++)for(int x=0;x<80;x++)
        CHECK(pe_world_set(world,x,y,z,(PeVoxel){255,PE_MAT_DIRT}));
    CHECK(pe_game_set_world(g,world,(PeVec3){30,10,30}));
    g->comfort=234.5f;g->money=7654;g->medpack_cooldown=2.75f;
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++)g->town_wallet[i]=(PeTownWallet){.balance=(uint32_t)(123+i*543),.added=i%2!=0};
    g->town_wallet[0].balance=INT32_MAX;g->town_wallet[7].balance=0;
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->world.extended&&loaded->world.extended!=g->world.extended&&pe_world_hash(&loaded->world)==pe_world_hash(&g->world));
    CHECK(!memcmp(g->town_wallet,loaded->town_wallet,sizeof g->town_wallet));
    CHECK(!memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory)&&!memcmp(&g->outfit,&loaded->outfit,sizeof g->outfit));
    CHECK(loaded->comfort==234.5f&&loaded->money==7654&&loaded->medpack_cooldown==2.75f);
    bytes=read_bytes(path,&n);CHECK(bytes&&n>(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+PE_SAVE_V17_APPEND+16);
    if(!bytes||n<=(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+PE_SAVE_V17_APPEND+16)goto done;
    CHECK(get32(bytes+4)==20);if(n<=(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+2048){CHECK(false);goto done;}
    n-=(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND); /* Exercise actual literalv17 tails below. */
    bad=malloc(n+1);CHECK(bad);if(!bad)goto done;
    size_t tail=n-PE_SAVE_V17_APPEND;
    CHECK(get32(bytes+tail)==8);
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++){
        const PeTownWalletSource *s=pe_town_wallet_source_at(i);CHECK(s);if(!s)continue;
        CHECK(get32(bytes+tail+4+12*i)==s->entity_id);
        CHECK(get32(bytes+tail+8+12*i)==g->town_wallet[i].balance);
        CHECK(get32(bytes+tail+12+12*i)==(g->town_wallet[i].added?1u:0u));
    }
    /* Same retained V16 bytes, independently repaired envelope; historical
     * vendor transactions cannot be inferred from the old player money. */
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,tail,16)&&pe_game_load(loaded,path,error,sizeof error));
    archived(loaded);CHECK(loaded->money==7654&&loaded->comfort==234.5f);
    CHECK(pe_game_save(loaded,path,error,sizeof error));after=read_bytes(path,&after_n);
    CHECK(after&&after_n==n+(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND));if(after)CHECK(get32(after+4)==20&&!memcmp(after+16,bytes+16,tail-16));
    CHECK(pe_game_load(loaded,path,error,sizeof error));archived(loaded);
    /* V15 also defaults raw comfort; all earlier migration rules still run. */
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,tail-PE_SAVE_V16_APPEND,15)&&pe_game_load(loaded,path,error,sizeof error));
    archived(loaded);CHECK(loaded->comfort==500&&loaded->money==7654);
    memcpy(snapshot,loaded,sizeof *loaded);uint64_t hash=pe_world_hash(&loaded->world);
    const struct {size_t offset;uint32_t value;} malformed[]={
        {0,0},{0,7},{0,9},{4,0},{4,UINT32_MAX},
        {8,UINT32_C(0x80000000)},{8,UINT32_MAX},{12,2},{12,UINT32_MAX},
        {4+7*12,0},{8+7*12,UINT32_C(0x80000000)},{12+7*12,2}
    };
    for(size_t i=0;i<sizeof malformed/sizeof *malformed;i++){
        memcpy(bad,bytes,n);put32(bad+tail+malformed[i].offset,malformed[i].value);
        CHECK(write_bytes(path,bad,n,17));rejected(loaded,path,snapshot,hash);
    }
    /* Identity permutation and duplicate IDs must fail even with valid
     * amounts/latches and an otherwise valid envelope. */
    memcpy(bad,bytes,n);memcpy(bad+tail+16,bad+tail+4,4);
    CHECK(write_bytes(path,bad,n,17));rejected(loaded,path,snapshot,hash);
    memcpy(bad,bytes,n);uint32_t first=get32(bad+tail+4);
    put32(bad+tail+4,get32(bad+tail+16));put32(bad+tail+16,first);
    CHECK(write_bytes(path,bad,n,17));rejected(loaded,path,snapshot,hash);
    const unsigned missing[]={1,4,12,99,100};
    for(size_t i=0;i<sizeof missing/sizeof *missing;i++){
        memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n-missing[i],17));rejected(loaded,path,snapshot,hash);
    }
    memcpy(bad,bytes,n);bad[n]=0;CHECK(write_bytes(path,bad,n+1,17));rejected(loaded,path,snapshot,hash);
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n,16));rejected(loaded,path,snapshot,hash); /* Old envelope cannot absorb a new tail. */
    CHECK(write_bytes(path,bytes,n,17));g->town_wallet[3].balance=UINT32_C(0x80000000);
    CHECK(!pe_game_save(g,path,error,sizeof error));free(after);after=read_bytes(path,&after_n);
    CHECK(after&&after_n==n&&!memcmp(after,bytes,n));
    /* Legacy writes the same identified records, all with zero state. It
     * never gains Adventure wallets through old-save migration or v17 load. */
    pe_game_destroy(g);pe_game_init(g,718);CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->mode==PE_GAME_LEGACY&&!memcmp(loaded->town_wallet,(PeTownWallet[8]){{0}},sizeof loaded->town_wallet));
    free(bytes);bytes=read_bytes(path,&n);CHECK(bytes);if(!bytes)goto done;CHECK(get32(bytes+4)==20);if(n<=(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+2048){CHECK(false);goto done;}
    n-=(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND);tail=n-PE_SAVE_V17_APPEND;
    CHECK(write_bytes(path,bytes,tail,16)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(loaded->town_wallet,(PeTownWallet[8]){{0}},sizeof loaded->town_wallet));
    memcpy(snapshot,loaded,sizeof *loaded);hash=pe_world_hash(&loaded->world);
    for(unsigned field=0;field<2;field++){
        memcpy(bad,bytes,n);put32(bad+tail+8+4*field,1);CHECK(write_bytes(path,bad,n,17));rejected(loaded,path,snapshot,hash);
    }
    g->town_wallet[0].added=true;CHECK(!pe_game_save(g,path,error,sizeof error));
done:
    if(fd>=0)unlink(path);
    free(bytes);free(bad);free(after);free(snapshot);pe_world_destroy(world);free(world);
    pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);return failures;
}
