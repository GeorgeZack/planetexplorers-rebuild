#define _POSIX_C_SOURCE 200809L
#include "pe_build_history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if(!(c)){fprintf(stderr,"build history lifecycle:%d %s\n",__LINE__,#c);failures++;} } while(0)
static unsigned char *read_bytes(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);rewind(f);if(n<=0){fclose(f);return NULL;}
    unsigned char *p=malloc((size_t)n);
    if(!p||fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return NULL;}
    fclose(f);*size=(size_t)n;return p;
}
int pe_test_build_history_save(void){
    failures=0;char path[1024],error[256];
    snprintf(path,sizeof path,"%s/pe-build-history-XXXXXX",getenv("TMPDIR")?getenv("TMPDIR"):"/tmp");
    int fd=mkstemp(path);if(fd>=0)close(fd);
    PeGame *g=calloc(1,sizeof *g),*copy=calloc(1,sizeof *copy),*moved=calloc(1,sizeof *moved);
    PeWorld *world=calloc(1,sizeof *world);unsigned char *before=NULL,*after=NULL;size_t n=0,m=0;
    CHECK(fd>=0&&g&&copy&&moved&&world);if(fd<0||!g||!copy||!moved||!world)goto done;
    CHECK(pe_game_init_mode(g,972,PE_GAME_ADVENTURE));
    CHECK(pe_world_resize(world,80,32,80));
    for(int z=0;z<80;z++)for(int y=0;y<=8;y++)for(int x=0;x<80;x++)
        CHECK(pe_world_set(world,x,y,z,(PeVoxel){255,PE_MAT_DIRT}));
    CHECK(pe_game_set_world(g,world,(PeVec3){32,10,32}));
    PeB45Cell cells[4]={{68,20,64,4,11},{69,20,64,4,11},{68,20,65,4,11},{69,20,65,4,11}};
    int indices[4]={0,1,2,3};
    CHECK(pe_game_build_cells(g,cells,4));CHECK(pe_game_remove_cells(g,indices,4));
    CHECK(pe_build_history_undo(g));
    CHECK(pe_build_history_can_undo(g)&&pe_build_history_can_redo(g)&&g->cell_count==4);
    PeBuildHistory history=g->build_history;PeInventory inventory=g->inventory;
    CHECK(pe_game_clone(copy,g));
    CHECK(!memcmp(&copy->build_history,&history,sizeof history));
    CHECK(copy->world.extended&&copy->world.extended!=g->world.extended);
    pe_game_move(moved,copy);CHECK(!memcmp(&moved->build_history,&history,sizeof history));
    CHECK(!pe_build_history_can_undo(copy)&&!pe_build_history_can_redo(copy));
    CHECK(pe_game_save(g,path,error,sizeof error));before=read_bytes(path,&n);CHECK(before&&n>16);
    if(!before||n<=16)goto done;
    CHECK(before[4]==18&&before[5]==0&&before[6]==0&&before[7]==0);
    CHECK(!memcmp(&g->build_history,&history,sizeof history)); /* Saving keeps the session. */
    char missing[1100];snprintf(missing,sizeof missing,"%s.missing",path);
    CHECK(!pe_game_load(g,missing,error,sizeof error));
    CHECK(!memcmp(&g->build_history,&history,sizeof history)&&!memcmp(&g->inventory,&inventory,sizeof inventory));
    /* An invalid transient journal cannot write over a valid checkpoint. */
    g->build_history.cursor=g->build_history.count+1;
    CHECK(!pe_game_save(g,path,error,sizeof error));g->build_history=history;
    after=read_bytes(path,&m);CHECK(after&&m==n);if(after&&m==n)CHECK(!memcmp(before,after,n));free(after);after=NULL;
    CHECK(pe_game_load(copy,path,error,sizeof error));
    CHECK(!pe_build_history_can_undo(copy)&&!pe_build_history_can_redo(copy)&&copy->cell_count==4);
    CHECK(!memcmp(&copy->inventory,&inventory,sizeof inventory));
    CHECK(pe_game_save(copy,path,error,sizeof error));after=read_bytes(path,&m);CHECK(after&&m==n);
    if(after&&m==n)CHECK(!memcmp(before,after,n));
    /* Independent live clone can still redo; loading into a populated live
     * game must clear both directions without restoring spent resources. */
    CHECK(pe_build_history_redo(moved)&&moved->cell_count==0);
    CHECK(!memcmp(&g->build_history,&history,sizeof history)&&g->cell_count==4);
    CHECK(pe_game_load(g,path,error,sizeof error));CHECK(!pe_build_history_can_undo(g)&&!pe_build_history_can_redo(g));
    CHECK(pe_game_build_cells(g,(PeB45Cell[]){{68,21,64,4,11}},1));
    history=g->build_history;
    CHECK(!pe_game_set_world(g,NULL,(PeVec3){32,10,32}));CHECK(!memcmp(&g->build_history,&history,sizeof history));
    CHECK(pe_game_set_world(g,world,(PeVec3){32,10,32}));
    CHECK(!g->cell_count&&!pe_build_history_can_undo(g)&&!pe_build_history_can_redo(g));
done:
    if(fd>=0)unlink(path);
    free(before);free(after);
    pe_world_destroy(world);free(world);
    pe_game_destroy(g);free(g);pe_game_destroy(copy);free(copy);pe_game_destroy(moved);free(moved);
    return failures;
}
