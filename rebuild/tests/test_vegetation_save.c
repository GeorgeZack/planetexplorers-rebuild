#define _POSIX_C_SOURCE 200809L
#include "pe_vegetation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"vegetation save:%d %s\n",__LINE__,#c);failures++;}}while(0)
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
int pe_test_vegetation_save(void){
    failures=0;char path[1024],error[256];
    snprintf(path,sizeof path,"%s/pe-vegetation-save-XXXXXX",getenv("TMPDIR")?getenv("TMPDIR"):"/tmp");
    int fd=mkstemp(path);if(fd>=0)close(fd);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);
    PeWorld *world=calloc(1,sizeof *world);unsigned char *bytes=NULL,*bad=NULL,*snapshot=NULL,*after=NULL;
    size_t n=0,after_n=0;CHECK(fd>=0&&g&&loaded&&world);if(fd<0||!g||!loaded||!world)goto done;
    CHECK(PE_SAVE_V18_APPEND==648&&PE_MAX_PLANTS==32);
    CHECK(pe_game_init_mode(g,718,PE_GAME_ADVENTURE));
    CHECK(pe_world_resize(world,80,32,80));
    for(int z=0;z<80;z++)for(int y=0;y<=8;y++)for(int x=0;x<80;x++)
        pe_world_set(world,x,y,z,(PeVoxel){255,PE_MAT_DIRT});
    CHECK(pe_game_set_world(g,world,(PeVec3){32,10,32}));
    CHECK(pe_vegetation_install_fixture(g,g->player));
    CHECK(g->vegetation.installed&&g->vegetation.count==8&&g->tree_count==2);
    g->vegetation.plants[0].depleted=true;g->trees[0].remaining=0;g->trees[0].health=0;
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(&g->vegetation,&loaded->vegetation,sizeof g->vegetation));
    CHECK(loaded->trees[0].remaining==0&&loaded->trees[0].health==0&&loaded->tree_count==2);
    CHECK(loaded->world.extended&&loaded->world.extended!=g->world.extended);
    CHECK(pe_vegetation_install_fixture(loaded,(PeVec3){40,10,40}));
    CHECK(!memcmp(&g->vegetation,&loaded->vegetation,sizeof g->vegetation)&&loaded->tree_count==2);
    bytes=read_bytes(path,&n);CHECK(bytes&&n>((PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)+PE_SAVE_V20_APPEND)+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND+16);if(!bytes||n<=((PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)+PE_SAVE_V20_APPEND)+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND+16)goto done;
    size_t tail=n-((PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)+PE_SAVE_V20_APPEND)-PE_SAVE_V19_APPEND-PE_SAVE_V18_APPEND;
    CHECK(get32(bytes+4)==PE_SAVE_VERSION&&get32(bytes+tail)==1&&get32(bytes+tail+4)==8);
    CHECK(get32(bytes+tail+8)==g->vegetation.plants[0].prototype_id&&get32(bytes+tail+24)==1);
    for(size_t i=tail+8+8*20;i<tail+PE_SAVE_V18_APPEND;i++)CHECK(bytes[i]==0);
    bad=malloc(n+1);snapshot=malloc(sizeof *loaded);CHECK(bad&&snapshot);if(!bad||!snapshot)goto done;
    memcpy(snapshot,loaded,sizeof *loaded);uint64_t world_hash=pe_world_hash(&loaded->world);
    /* Corrupt each independent serialized invariant, repairing the checksum so
     * validation, rather than only the envelope, must reject it atomically. */
    const struct{size_t offset;uint32_t value;} corruptions[]={
        {0,2},{0,0},{4,33},{4,0},{8,99999},{12,0x7fc00000},{12,0xbf800000},
        {16,0x7f800000},{20,0x43000000},{24,2},{8+8*20,15},{12+8*20,0x80000000},
    };
    for(unsigned i=0;i<sizeof corruptions/sizeof corruptions[0];i++){
        memcpy(bad,bytes,n);put32(bad+tail+corruptions[i].offset,corruptions[i].value);
        CHECK(write_bytes(path,bad,n,PE_SAVE_VERSION));CHECK(!pe_game_load(loaded,path,error,sizeof error));
        CHECK(!memcmp(snapshot,loaded,sizeof *loaded)&&pe_world_hash(&loaded->world)==world_hash);
    }
    memcpy(bad,bytes,n);memcpy(bad+tail+8+20+4,bad+tail+8+4,12); /* duplicate position */
    CHECK(write_bytes(path,bad,n,PE_SAVE_VERSION)&&!pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,n-1,PE_SAVE_VERSION)&&!pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    memcpy(bad,bytes,n);bad[n]=0;CHECK(write_bytes(path,bad,n+1,PE_SAVE_VERSION)&&!pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(snapshot,loaded,sizeof *loaded));
    /* Literal v17 retains the exact tree owner, but cannot infer plants. */
    memcpy(bad,bytes,n);CHECK(write_bytes(path,bad,tail,17)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(!loaded->vegetation.installed&&!loaded->vegetation.count&&loaded->tree_count==2&&loaded->trees[0].remaining==0);
    CHECK(!memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory));
    CHECK(pe_game_save(loaded,path,error,sizeof error));after=read_bytes(path,&after_n);CHECK(after&&after_n==n);
    if(after&&after_n==n){CHECK(!memcmp(bytes+16,after+16,tail-16));for(size_t i=tail;i<tail+PE_SAVE_V18_APPEND;i++)CHECK(after[i]==0);CHECK(!memcmp(bytes+tail+PE_SAVE_V18_APPEND,after+tail+PE_SAVE_V18_APPEND,PE_SAVE_V19_APPEND));}
    free(after);after=NULL;
    CHECK(write_bytes(path,bytes,n,PE_SAVE_VERSION));g->vegetation.count=PE_MAX_PLANTS+1;
    CHECK(!pe_game_save(g,path,error,sizeof error));after=read_bytes(path,&after_n);
    CHECK(after&&after_n==n);if(after&&after_n==n)CHECK(!memcmp(bytes,after,n));
done:
    if(fd>=0)unlink(path);
    free(bytes);free(bad);free(snapshot);free(after);
    pe_world_destroy(world);free(world);pe_game_destroy(g);free(g);pe_game_destroy(loaded);free(loaded);
    return failures;
}
