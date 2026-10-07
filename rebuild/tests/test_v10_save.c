#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_adventure_hunt.h"
#include "pe_adventure_medicine.h"
#include "pe_deployable.h"
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"v10-save:%d %s\n",__LINE__,#c);failures++;}}while(0)
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
static uint32_t add_torch(PeGame *g){
    if(!pe_inventory_add(&g->inventory,10,1))return 0;
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].item_id==10&&g->inventory.slots[i].count){
        g->inventory.slots[i].durability=1234.5f;return g->inventory.slots[i].instance_id;
    }
    return 0;
}
int pe_test_v10_save(void){
    failures=0;
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);PeWorld *w=calloc(1,sizeof *w);
    PeSandboxCollision *collision=pe_sandbox_collision_create();unsigned char *snapshot=malloc(sizeof *g),*original=NULL;
    char path[]="/tmp/pe-v10-save-XXXXXX",error[256];int fd=mkstemp(path);
    if(!g||!loaded||!w||!collision||!snapshot||fd<0){failures++;goto done;}close(fd);
    CHECK(pe_game_init_mode(g,77,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(g));
    CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));g->annot_complete=true;
    CHECK(pe_world_resize(w,80,32,80));
    for(int z=0;z<80;z++)for(int y=0;y<=8;y++)for(int x=0;x<80;x++)CHECK(pe_world_set(w,x,y,z,(PeVoxel){255,PE_MAT_DIRT}));
    CHECK(pe_game_set_world(g,w,(PeVec3){30,10,30}));CHECK(g->world.extended);
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};CHECK(pe_adventure_medicine_accept(g));
    CHECK(pe_inventory_add(&g->inventory,917,5));CHECK(pe_adventure_medicine_complete(g));
    CHECK(pe_game_collision_update(g,collision,error,sizeof error));
    uint32_t first=add_torch(g);CHECK(first);
    CHECK(pe_deployable_place(g,collision,first,(PeVec3){32,15,30},(PeVec3){0,-1,0}));
    CHECK(pe_deployable_turn(g,first));
    uint32_t second=add_torch(g);CHECK(second&&second!=first);
    CHECK(pe_deployable_place(g,collision,second,(PeVec3){31,15,30},(PeVec3){0,-1,0}));
    CHECK(pe_deployable_state_valid(g));
    /* Fixture progression tests persistence only. Without captured evidence we
     * cannot install a hunt, and never manufacture a destination for gameplay. */
    PeVec3 reference;bool hunt_configured=pe_adventure_hunt_reference(&reference);
    if(hunt_configured){g->hunt_state=PE_HUNT_TRAVEL;g->hunt_destination=reference;CHECK(pe_adventure_hunt_state_valid(g));}
    else fprintf(stderr,"v10-save: observed hunt destination absent; active hunt roundtrip not tested\n");
    bool saved=pe_game_save(g,path,error,sizeof error);if(!saved)fprintf(stderr,"v10-save: %s\n",error);CHECK(saved);if(!saved)goto done;
    CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->world.extended&&loaded->world.extended!=g->world.extended&&pe_world_hash(&loaded->world)==pe_world_hash(&g->world));
    CHECK(loaded->deployed_count==2&&loaded->deployed[0].instance_id==first&&loaded->deployed[1].instance_id==second);
    CHECK(loaded->deployed[0].quarter_turn==1&&loaded->deployed[0].durability==1234.5f);
    CHECK(loaded->hunt_state==g->hunt_state&&!memcmp(&loaded->hunt_destination,&g->hunt_destination,sizeof g->hunt_destination));
    CHECK(loaded->medicine_state==PE_MEDICINE_COMPLETE&&loaded->inventory.next_instance_id==g->inventory.next_instance_id);
    CHECK(!pe_inventory_count(&loaded->inventory,10));
    size_t size=0;original=read_save(path,&size);CHECK(original&&size>PE_SAVE_V10_APPEND);if(!original||size<=PE_SAVE_V10_APPEND)goto done;
    CHECK(get32(original+4)==18);size-=PE_SAVE_V18_APPEND+PE_SAVE_V17_APPEND+PE_SAVE_V16_APPEND+PE_SAVE_V15_APPEND+PE_SAVE_V14_APPEND+PE_SAVE_V13_APPEND+PE_SAVE_V11_APPEND;CHECK(write_save(path,original,size,10));size_t footer=size-PE_SAVE_V10_APPEND;CHECK(get32(original+footer+16)==2);
    /* A literal v9 prefix has no phantom deployed instances or quest progress.
     * Existing inventory, medicine, stocks and owned world remain intact. */
    CHECK(write_save(path,original,footer,9));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(!loaded->deployed_count&&!loaded->hunt_state&&!loaded->hunt_destination.x&&!loaded->hunt_destination.y&&!loaded->hunt_destination.z);
    CHECK(loaded->medicine_state==PE_MEDICINE_COMPLETE&&loaded->annot_complete&&loaded->inventory.next_instance_id==g->inventory.next_instance_id+PE_OUTFIT_SLOTS);
    CHECK(!memcmp(loaded->inventory.slots,g->inventory.slots,sizeof g->inventory.slots)&&pe_world_hash(&loaded->world)==pe_world_hash(&g->world));
    CHECK(pe_deployable_state_valid(loaded)&&pe_adventure_hunt_state_valid(loaded));
    memcpy(snapshot,loaded,sizeof *loaded);uint64_t world_hash=pe_world_hash(&loaded->world);
    unsigned char *bad=malloc(size+1);CHECK(bad);
    if(bad)for(int mode=0;mode<18;mode++){
        memcpy(bad,original,size);size_t output_size=size;unsigned char *tail=bad+footer,*record=tail+20;
        if(mode==0)put32(tail,3); /* unknown hunt state */
        if(mode==1){put32(tail,0);put32(tail+4,0x3f800000u);} /* destination without hunt */
        if(mode==2)put32(tail+8,0x7fc00000u); /* nonfinite hunt coordinate */
        if(mode==3)put32(tail+16,PE_MAX_DEPLOYABLES+1);
        if(mode==4)put32(record,11); /* unsupported deployment */
        if(mode==5)put32(record+4,0); /* missing identity */
        if(mode==6)put32(record+4,g->inventory.next_instance_id); /* unissued identity */
        if(mode==7)put32(record+8,0x7fc00000u); /* NaN durability */
        if(mode==8)put32(record+8,0xbf800000u); /* negative durability */
        if(mode==9)put32(record+12,0x42a00000u); /* x=80 outside saved world */
        if(mode==10)put32(record+16,0x7f800000u); /* infinite Y */
        if(mode==11)put32(record+24,4); /* invalid turn */
        if(mode==12)put32(record+28+4,first); /* duplicate deployed identity */
        if(mode==13)put32(record+56,10); /* nonzero unused record */
        if(mode==14)put32(tail+16,1); /* hidden second record */
        if(mode==15)put32(record+8,0x7f7fffffu); /* durability above source maximum */
        if(mode==16)output_size--;
        if(mode==17)bad[output_size++]=0;
        CHECK(write_save(path,bad,output_size,10));CHECK(!pe_game_load(loaded,path,error,sizeof error));
        CHECK(!memcmp(snapshot,loaded,sizeof *loaded)&&world_hash==pe_world_hash(&loaded->world));
    }
    free(bad);
    CHECK(write_save(path,original,size,10));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(pe_deployable_turn(loaded,first)&&loaded->deployed[0].quarter_turn==2);
    int charcoal=pe_inventory_count(&loaded->inventory,1001);CHECK(pe_deployable_get(loaded,first));
    CHECK(loaded->deployed_count==1&&loaded->deployed[0].instance_id==second&&pe_inventory_count(&loaded->inventory,1001)==charcoal+1);
    CHECK(pe_game_save(loaded,path,error,sizeof error));CHECK(pe_game_load(g,path,error,sizeof error));
    CHECK(!pe_deployable_get(g,first)&&g->deployed_count==1&&!pe_inventory_count(&g->inventory,10));
    CHECK(pe_inventory_count(&g->inventory,1001)==charcoal+1);
    if(hunt_configured){g->hunt_state=PE_HUNT_ENCOUNTER_PENDING;CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->hunt_state==PE_HUNT_ENCOUNTER_PENDING);}
    size_t before_size=0,after_size=0;unsigned char *before=read_save(path,&before_size);
    g->deployed[0].quarter_turn=4;CHECK(!pe_game_save(g,path,error,sizeof error));unsigned char *after=read_save(path,&after_size);
    CHECK(before&&after&&before_size==after_size&&!memcmp(before,after,before_size));free(before);free(after);
 done:
    if(fd>=0)unlink(path);
    pe_game_destroy(g);pe_game_destroy(loaded);pe_world_destroy(w);pe_sandbox_collision_free(collision);
    free(g);free(loaded);free(w);free(snapshot);free(original);return failures;
}
