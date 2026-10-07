#define _POSIX_C_SOURCE 200809L
#include "pe_game.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static uint32_t get(const unsigned char *p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static unsigned char *read_file(const char *path,size_t *n){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long z=ftell(f);rewind(f);
    if(z<16){fclose(f);return NULL;}unsigned char *p=malloc((size_t)z+1);
    bool ok=p&&fread(p,1,(size_t)z,f)==(size_t)z;if(fclose(f))ok=false;
    if(!ok){free(p);return NULL;}*n=(size_t)z;return p;
}
static bool write_file(const char *path,unsigned char *p,size_t n,unsigned version){
    if(n<16)return false;
    put(p+4,version);put(p+8,(uint32_t)n-16);uint32_t sum=2166136261u;
    for(size_t i=16;i<n;i++){sum^=p[i];sum*=16777619u;}put(p+12,sum);
    FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(p,1,n,f)==n;return fclose(f)==0&&ok;
}
int pe_test_conversation_history_save(void){
    char path[1024],error[256];snprintf(path,sizeof path,"%s/pe-history-save-XXXXXX",getenv("TMPDIR")?getenv("TMPDIR"):"/tmp");
    int fd=mkstemp(path);assert(fd>=0);close(fd);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*copy=calloc(1,sizeof *copy),*snapshot=malloc(sizeof *snapshot);
    PeWorld *world=calloc(1,sizeof *world);assert(g&&loaded&&copy&&snapshot&&world);
    for(int mode=PE_GAME_LEGACY;mode<=PE_GAME_ADVENTURE;mode++){
        pe_game_destroy(g);assert(pe_game_init_mode(g,411,(PeGameMode)mode));
        assert(!g->conversation_history.count&&pe_conversation_history_valid(&g->conversation_history));
        assert(pe_conversation_history_add(&g->conversation_history,"Westina","Hello, explorer.\nWelcome!",NULL));
        assert(pe_conversation_history_add(&g->conversation_history,"名字","café 🌱",NULL));
        assert(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
        assert(!memcmp(&g->conversation_history,&loaded->conversation_history,sizeof g->conversation_history));
        size_t n=0,m=0;unsigned char *bytes=read_file(path,&n);assert(bytes&&n>(PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND)+16);
        size_t tail=n-(PE_SAVE_V22_APPEND+PE_SAVE_V21_APPEND);assert(get(bytes+4)==PE_SAVE_VERSION&&get(bytes+tail)==2);
        assert(!strcmp((char *)bytes+tail+4,"Westina"));
        assert(!strcmp((char *)bytes+tail+4+PE_CONVERSATION_SPEAKER_CAPACITY,"Hello, explorer.\nWelcome!"));
        /* V20 is a literal prefix; old files acquire no invented past rows. */
        assert(write_file(path,bytes,tail,20)&&pe_game_load(loaded,path,error,sizeof error));
        assert(!loaded->conversation_history.count&&pe_conversation_history_valid(&loaded->conversation_history));
        assert(!memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory));
        assert(pe_game_save(loaded,path,error,sizeof error));unsigned char *again=read_file(path,&m);
        assert(again&&m==n&&!memcmp(bytes+16,again+16,tail-16));
        for(size_t i=tail;i<m;i++)assert(!again[i]);
        free(again);
        assert(write_file(path,bytes,tail-PE_SAVE_V20_APPEND,19)&&pe_game_load(loaded,path,error,sizeof error)&&!loaded->conversation_history.count);
        assert(write_file(path,bytes,n,PE_SAVE_VERSION)&&pe_game_load(loaded,path,error,sizeof error));
        unsigned char *bad=malloc(n+1);assert(bad);memcpy(snapshot,loaded,sizeof *loaded);
        uint64_t hash=pe_world_hash(&loaded->world);
        for(unsigned fault=0;fault<8;fault++){
            memcpy(bad,bytes,n);
            size_t row=tail+4,second=row+PE_CONVERSATION_SPEAKER_CAPACITY+PE_CONVERSATION_CONTENT_CAPACITY;
            if(fault==0)put(bad+tail,21);
            if(fault==1)memset(bad+row,'x',PE_CONVERSATION_SPEAKER_CAPACITY);
            if(fault==2)memset(bad+row+PE_CONVERSATION_SPEAKER_CAPACITY,'x',PE_CONVERSATION_CONTENT_CAPACITY);
            if(fault==3)bad[row+8]=1; /* nonzero after speaker NUL */
            if(fault==4)bad[row]=0xff; /* invalid UTF-8 */
            if(fault==5)memcpy(bad+second+PE_CONVERSATION_SPEAKER_CAPACITY,bad+row+PE_CONVERSATION_SPEAKER_CAPACITY,PE_CONVERSATION_CONTENT_CAPACITY);
            if(fault==6)bad[tail+PE_SAVE_V21_APPEND-1]=1; /* unused row must be all zero */
            if(fault==7)put(bad+tail,0); /* live strings in unused rows */
            assert(write_file(path,bad,n,PE_SAVE_VERSION)&&!pe_game_load(loaded,path,error,sizeof error));
            assert(!memcmp(snapshot,loaded,sizeof *loaded)&&pe_world_hash(&loaded->world)==hash);
        }
        memcpy(bad,bytes,n);assert(write_file(path,bad,n-1,PE_SAVE_VERSION)&&!pe_game_load(loaded,path,error,sizeof error));
        memcpy(bad,bytes,n);bad[n]=0;assert(write_file(path,bad,n+1,PE_SAVE_VERSION)&&!pe_game_load(loaded,path,error,sizeof error));
        assert(!memcmp(snapshot,loaded,sizeof *loaded));
        assert(write_file(path,bytes,n,PE_SAVE_VERSION));g->conversation_history.entries[19].content[1]='x';
        assert(!pe_game_save(g,path,error,sizeof error));again=read_file(path,&m);assert(again&&m==n&&!memcmp(bytes,again,n));free(again);
        assert(!pe_game_clone(loaded,g)&&!memcmp(snapshot,loaded,sizeof *loaded));
        g->conversation_history.entries[19].content[1]=0;
        pe_conversation_history_clear(&g->conversation_history);
        for(unsigned i=0;i<20;i++){char line[32];snprintf(line,sizeof line,"Retained %u",i);assert(pe_conversation_history_add(&g->conversation_history,"Tips",line,NULL));}
        assert(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
        assert(!memcmp(&g->conversation_history,&loaded->conversation_history,sizeof g->conversation_history));
        free(bad);free(bytes);
    }
    /* Replace with a heap-owned flat world; history travels with the game,
     * independent of old world lifetime and transient encounter reset. */
    assert(pe_world_resize(world,96,32,64));world->seed=77;
    PeVoxel *voxels=pe_world_data(world);
    for(int z=0;z<64;z++)for(int y=0;y<4;y++)for(int x=0;x<96;x++)voxels[((size_t)z*32+y)*96+x]=(PeVoxel){255,PE_MAT_DIRT};
    assert(pe_game_set_world(g,world,(PeVec3){32,6,32})&&g->world.extended&&g->conversation_history.count==20);
    assert(pe_game_clone(copy,g)&&copy->world.extended!=g->world.extended&&!memcmp(&copy->conversation_history,&g->conversation_history,sizeof g->conversation_history));
    pe_game_move(loaded,copy);assert(!copy->world.extended&&!copy->conversation_history.count&&loaded->conversation_history.count==20);
    assert(pe_game_save(loaded,path,error,sizeof error)&&pe_game_load(copy,path,error,sizeof error));
    assert(copy->world.extended&&copy->conversation_history.count==20&&pe_world_hash(&copy->world)==pe_world_hash(&loaded->world));
    pe_conversation_history_clear(&copy->conversation_history);assert(pe_game_save(copy,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error)&&!loaded->conversation_history.count);
    pe_world_destroy(world);free(world);pe_game_destroy(g);pe_game_destroy(loaded);pe_game_destroy(copy);free(g);free(loaded);free(copy);free(snapshot);unlink(path);
    puts("conversation history saves: v21 roundtrip, literal19/20 migration, corruption rollback and owning-world lifecycle pass");return 0;
}
#ifdef PE_CONVERSATION_HISTORY_SAVE_TEST_MAIN
int main(void){return pe_test_conversation_history_save();}
#endif
