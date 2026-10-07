#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include <math.h>
#include <string.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"v13:%d %s\n",__LINE__,#c);failures++;}}while(0)
static void put32(unsigned char *p,uint32_t n){for(int i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static unsigned char *read_bytes(const char *path,size_t *n){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long length=ftell(f);if(length<20){fclose(f);return NULL;}rewind(f);
    unsigned char *b=malloc((size_t)length+1);if(!b){fclose(f);return NULL;}
    bool ok=fread(b,1,(size_t)length,f)==(size_t)length;if(fclose(f))ok=false;
    if(!ok){free(b);return NULL;}*n=(size_t)length;return b;
}
static bool write_bytes(const char *path,unsigned char *b,size_t n,unsigned version){
    put32(b+4,version);put32(b+8,(uint32_t)n-16);uint32_t sum=2166136261u;
    for(size_t i=16;i<n;i++){sum^=b[i];sum*=16777619u;}put32(b+12,sum);
    FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(b,1,n,f)==n;return fclose(f)==0&&ok;
}
int pe_test_v13_save(void){
    failures=0;char path[]="/tmp/pe-v13-XXXXXX",error[256];int fd=mkstemp(path);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);
    unsigned char *bytes=NULL,*snapshot=NULL,*after=NULL;size_t n=0,after_n=0;
    if(fd<0||!g||!loaded){failures++;goto done;}close(fd);
    CHECK(pe_game_init_mode(g,93,PE_GAME_ADVENTURE));
    g->juice_cooldown=7.25;g->juice_buff_active=true;g->juice_buff_elapsed=1.25;g->juice_buff_tick=3;
    const float values[]={0,3.5f,10};
    for(size_t i=0;i<sizeof values/sizeof values[0];i++){
        g->medpack_cooldown=values[i];CHECK(pe_game_save(g,path,error,sizeof error));
        CHECK(pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->medpack_cooldown==values[i]);
        CHECK(loaded->juice_cooldown==7.25&&loaded->juice_buff_active&&loaded->juice_buff_elapsed==1.25&&loaded->juice_buff_tick==3);
    }
    bytes=read_bytes(path,&n);snapshot=malloc(sizeof *loaded);CHECK(bytes&&snapshot);
    if(!bytes||!snapshot)goto done;
    CHECK(bytes[4]==20&&PE_SAVE_V13_APPEND==4);
    if(n<=(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+PE_SAVE_V17_APPEND+2048){CHECK(false);goto done;}
    n-=(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+PE_SAVE_V17_APPEND; /* Test the literal V16 payload below, not a mislabeled V18 file. */
    CHECK(n>PE_SAVE_V13_APPEND+16);
    /* The independent clock precedes the fixed V14 outfit extension. */
    size_t clock_at=n-PE_SAVE_V16_APPEND-PE_SAVE_V15_APPEND-PE_SAVE_V14_APPEND-PE_SAVE_V13_APPEND;
    CHECK(bytes[clock_at]==0&&bytes[clock_at+1]==0&&bytes[clock_at+2]==0x20&&bytes[clock_at+3]==0x41);
    memcpy(snapshot,loaded,sizeof *loaded);uint64_t hash=pe_world_hash(&loaded->world);
    const uint32_t bad[]={0xbf800000u,0x41200001u,0x7fc00000u,0x7f800000u,0xff800000u};
    for(size_t i=0;i<sizeof bad/sizeof bad[0];i++){
        put32(bytes+clock_at,bad[i]);CHECK(write_bytes(path,bytes,n,16));
        CHECK(!pe_game_load(loaded,path,error,sizeof error));
        CHECK(!memcmp(snapshot,loaded,sizeof *loaded)&&pe_world_hash(&loaded->world)==hash);
    }
    put32(bytes+clock_at,0x41200000u);
    CHECK(write_bytes(path,bytes,n-1,16));CHECK(!pe_game_load(loaded,path,error,sizeof error));
    bytes[n]=0;CHECK(write_bytes(path,bytes,n+1,16));CHECK(!pe_game_load(loaded,path,error,sizeof error));
    CHECK(!memcmp(snapshot,loaded,sizeof *loaded)&&pe_world_hash(&loaded->world)==hash);
    /* Literal v12 loads without touching the existing juice cooldown/buff. */
    size_t old_n=n-PE_SAVE_V16_APPEND-PE_SAVE_V15_APPEND-PE_SAVE_V14_APPEND-PE_SAVE_V13_APPEND;CHECK(write_bytes(path,bytes,old_n,12));
    CHECK(pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->medpack_cooldown==0);
    CHECK(loaded->juice_cooldown==7.25&&loaded->juice_buff_active&&loaded->juice_buff_elapsed==1.25&&loaded->juice_buff_tick==3);
    CHECK(pe_game_save(loaded,path,error,sizeof error));after=read_bytes(path,&after_n);CHECK(after);
    put32(bytes+pe_test_save_counter_offset(loaded),loaded->inventory.next_instance_id);
    if(after){CHECK(after_n==n+(PE_SAVE_V20_APPEND+PE_SAVE_V19_APPEND+PE_SAVE_V18_APPEND)+PE_SAVE_V17_APPEND&&after[4]==20&&!memcmp(after+16,bytes+16,old_n-16));CHECK(!memcmp(after+old_n,"\0\0\0\0",4));}
    /* Invalid in-memory clocks cannot replace the last valid checkpoint. */
    const float invalid[]={-.01f,10.01f,NAN,INFINITY,-INFINITY};
    for(size_t i=0;i<sizeof invalid/sizeof invalid[0];i++){
        g->medpack_cooldown=invalid[i];CHECK(!pe_game_save(g,path,error,sizeof error));
        size_t current_n=0;unsigned char *current=read_bytes(path,&current_n);
        CHECK(current&&after&&current_n==after_n&&!memcmp(current,after,after_n));free(current);
    }
    /* Shared older-version fixture conversion must strip the new append too. */
    CHECK(pe_test_save_pre_welcome(path,7));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->medpack_cooldown==0&&loaded->juice_cooldown==7.25);
done:
    if(fd>=0)unlink(path);
    pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);free(bytes);free(snapshot);free(after);
    return failures;
}
