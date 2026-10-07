/* Controlled camera/position fixtures for manual LMB-held action testing.
 * Inventory, equipment and quest grants use public APIs. No resource edits or
 * actions performed during validation are committed to the output saves. */
#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_sandbox_actions.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int item_slot(const PeGame *g,int item){for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==item)return i;return -1;}
static PeVec3 add(PeVec3 a,PeVec3 b){return(PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 scale(PeVec3 v,float s){return(PeVec3){v.x*s,v.y*s,v.z*s};}
static PeVec3 normalize(PeVec3 v){return scale(v,1/sqrtf(v.x*v.x+v.y*v.y+v.z*v.z));}
/* Recovered orgtree mesh6039 bounds. Conservative visibility checks only;
 * these boxes are neither resource targets nor gameplay colliders. */
static bool foliage_segment(PeVec3 tree,PeVec3 start,PeVec3 end,float padding){
    float lo[]={tree.x-2.65711046f-padding,tree.y-.24681623f-padding,tree.z-3.46337006f-padding};
    float hi[]={tree.x+2.15026356f+padding,tree.y+7.78633261f+padding,tree.z+2.08653378f+padding};
    float a[]={start.x,start.y,start.z},d[]={end.x-start.x,end.y-start.y,end.z-start.z};
    float entry=0,exit=1;
    for(int axis=0;axis<3;axis++){
        if(fabsf(d[axis])<1e-7f){if(a[axis]<lo[axis]||a[axis]>hi[axis])return false;continue;}
        float first=(lo[axis]-a[axis])/d[axis],last=(hi[axis]-a[axis])/d[axis];
        if(first>last){float swap=first;first=last;last=swap;}
        entry=fmaxf(entry,first);exit=fminf(exit,last);if(entry>exit)return false;
    }
    return true;
}
static bool visible_action(const PeGame *g,PeVec3 camera,int target){
    for(int i=0;i<g->tree_count;i++)if(g->trees[i].remaining>0){
        PeVec3 tree=g->trees[i].position;
        /* Keep the camera and body outside every canopy box, including target. */
        if(foliage_segment(tree,camera,camera,.35f)||foliage_segment(tree,g->player,g->player,.35f))return false;
        if(i==target)continue;
        if(foliage_segment(tree,camera,g->player,.5f)||
           foliage_segment(tree,camera,add(g->player,(PeVec3){0,1.8f,0}),.5f)||
           (target>=0&&foliage_segment(tree,camera,add(g->trees[target].position,(PeVec3){0,1,0}),.5f)))return false;
    }
    return true;
}
static float isolation(const PeGame *g,int tree){
    float nearest=INFINITY;
    for(int other=0;other<g->tree_count;other++)if(other!=tree&&g->trees[other].remaining>0){
        float distance=hypotf(g->trees[tree].position.x-g->trees[other].position.x,g->trees[tree].position.z-g->trees[other].position.z);
        nearest=fminf(nearest,distance);
    }
    return nearest;
}
static void camera_ray(const PeGame *g,const PeSandboxCollision *collision,PeVec3 *origin,PeVec3 *direction){
    PeVec3 view={sinf(g->yaw)*cosf(g->pitch),sinf(g->pitch),-cosf(g->yaw)*cosf(g->pitch)};
    PeVec3 eye=add(g->player,(PeVec3){0,1.58f,0}),target=add(eye,scale(view,2));
    PeVec3 delta=add(scale(view,-6.0f),(PeVec3){0,.5f,0});
    PeSceneHit hit=pe_sandbox_collision_sweep_geometry(collision,add(eye,(PeVec3){0,-.1f,0}),.1f,.2f,delta);
    float fraction=hit.hit?fmaxf(.05f,hit.fraction-.02f):1;
    *origin=add(eye,scale(delta,fraction));*direction=normalize(add(target,scale(*origin,-1)));
}
/* Fixture-only check of the exported tree11_grassland unit capsule, matching
 * root's source collider query. The production frontend loads PESC metadata.
 * Center(-.1,3,-.2), radius1, height7; source MeshFilter GUID matches orgtree. */
static int trunk(const PeGame *g,PeVec3 o,PeVec3 direction,float range){
    PeVec3 d=normalize(direction);double nearest=range;int index=-1;
    for(int i=0;i<g->tree_count;i++)if(g->trees[i].remaining>0){
        PeVec3 p=add(o,scale(add(g->trees[i].position,(PeVec3){-.1f,3,-.2f}),-1));
        double h=fmax(fabs(p.y)-2.5,0);if(p.x*p.x+h*h+p.z*p.z<1)continue;
        double candidate=INFINITY,a=d.x*d.x+d.z*d.z,b=p.x*d.x+p.z*d.z;
        if(a>0){double disc=b*b-a*(p.x*p.x+p.z*p.z-1);
            if(disc>=0){double t=(-b-sqrt(disc))/a,y=p.y+t*d.y;if(t>=0&&y>=-2.5&&y<=2.5)candidate=t;}}
        for(int end=-1;end<=1;end+=2){double y=p.y-end*2.5;
            double dot=p.x*d.x+y*d.y+p.z*d.z,disc=dot*dot-(p.x*p.x+y*y+p.z*p.z-1);
            if(disc>=0){double t=-dot-sqrt(disc);if(t>=0&&t<candidate)candidate=t;}}
        if(candidate<=nearest&&(index<0||candidate<nearest)){nearest=candidate;index=i;}
    }
    return index;
}
static bool initialize(PeGame *g,PeSandboxCollision *collision,int item){
    char error[200];
    return pe_game_init_mode(g,666,PE_GAME_ADVENTURE)&&pe_test_ack_welcome(g)&&pe_game_arrival_complete(g)&&pe_game_accept_supplies(g)&&
        pe_game_equip_slot(g,item_slot(g,item))&&pe_game_quickslot_assign(g,0,item_slot(g,item))&&
        pe_game_quickslot_assign(g,1,item_slot(g,916))&&pe_game_collision_update(g,collision,error,sizeof error);
}
static void report(const char *path,const PeGame *g){
    const PeSlot *tool=pe_game_equipped_tool(g);
    printf("%s mode=%d mission=%d supplies=%d tool=%d instance=%u wear_remaining=%.3f "
           "wood=%d dirt=%d stone=%d juice=%d gathered=%u cells=%d position=(%.6f,%.6f,%.6f) "
           "yaw=%.8f pitch=%.8f stamina=%.3f\n",path,g->mode,g->adventure_mission,g->supplies_claimed,
           tool?tool->item_id:0,tool?tool->instance_id:0,tool?tool->durability:0,
           pe_inventory_count(&g->inventory,279),pe_inventory_count(&g->inventory,268),
           pe_inventory_count(&g->inventory,280),pe_inventory_count(&g->inventory,916),g->gathered,g->cell_count,
           g->player.x,g->player.y,g->player.z,g->yaw,g->pitch,g->stamina);
    for(int i=0;i<g->tree_count;i++)printf("  tree%d remaining=%d health=%.3f position=(%.6f,%.6f,%.6f)\n",
        i,g->trees[i].remaining,g->trees[i].health,g->trees[i].position.x,g->trees[i].position.y,g->trees[i].position.z);
}
static bool verify_swing(const PeGame *g,PeSandboxActionInput input,int tree){
    PeGame *copy=malloc(sizeof *copy);if(!copy)return false;*copy=*g;
    PeSandboxActions action;pe_sandbox_actions_init(&action,false);PeSandboxActionUpdate event={0};
    bool ok=true;for(int tick=0;tick<180&&!event.swung;tick++)if(!pe_sandbox_actions_step(&action,copy,input,&event)){ok=false;break;}
    const PeSlot *before=pe_game_equipped_tool(g),*after=pe_game_equipped_tool(copy);
    ok=ok&&event.swung&&event.changed&&before&&after&&after->durability==before->durability-5&&copy->tool_cooldown==0;
    if(tree>=0)ok=ok&&event.tree_index==tree&&copy->trees[tree].health==g->trees[tree].health-46;
    else ok=ok&&copy->world.revision>g->world.revision;
    free(copy);return ok;
}
static bool save(const PeGame *g,PeGame *loaded,const char *path){
    char error[200];
    if(!pe_game_save(g,path,error,sizeof error)||!pe_game_load(loaded,path,error,sizeof error)){fprintf(stderr,"%s\n",error);return false;}
    bool ok=memcmp(&g->player,&loaded->player,sizeof g->player)==0&&memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory)==0&&
        g->yaw==loaded->yaw&&g->pitch==loaded->pitch&&g->equipped_instance_id==loaded->equipped_instance_id&&
        pe_world_hash(&g->world)==pe_world_hash(&loaded->world)&&g->mode==loaded->mode;
    if(ok)report(path,loaded);
    return ok;
}
static uint32_t bytes_hash(const unsigned char *data,size_t size){
    uint32_t hash=2166136261u;for(size_t i=0;i<size;i++){hash^=data[i];hash*=16777619u;}return hash;
}
static uint32_t read32(const unsigned char *p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void write32(unsigned char *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static void write_float(unsigned char *p,float f){uint32_t bits;memcpy(&bits,&f,4);write32(p,bits);}
static unsigned char *read_file(const char *path,size_t *size){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long length=ftell(f);
    if(length<16||length>400000){fclose(f);return NULL;}rewind(f);
    unsigned char *data=malloc((size_t)length);
    bool ok=data&&fread(data,1,(size_t)length,f)==(size_t)length;
    fclose(f);if(!ok){free(data);return NULL;}*size=(size_t)length;return data;
}
/* Explicit fixture-only v4 encoder: strip later append-only save extensions
 * with the shared historical fixture helper, then change player position and
 * checksum. The new position must be valid under the old geometry and
 * overlap a live source trunk. This is a compatibility fixture, not corruption. */
static bool migration_fixture(PeGame *g,PeGame *loaded,PeSandboxCollision *collision,const char *path){
    if(!path[0]||strlen(path)>3500||!initialize(g,collision,26))return false;
    int tree=0;for(int i=1;i<g->tree_count;i++)if(isolation(g,i)>isolation(g,tree))tree=i;
    *loaded=*g;loaded->player=add(g->trees[tree].position,(PeVec3){.7f,0,-.2f});
    loaded->player.y=pe_world_height(&g->world,loaded->player.x,loaded->player.z)+.1f;
    char error[200];
    if(!pe_sandbox_collision_update(collision,&g->world,NULL,0,NULL,0,error,sizeof error)||
       !pe_game_collision_settle(loaded,collision)||!pe_game_collision_update(g,collision,error,sizeof error)||
       pe_sandbox_collision_overlap_geometry(collision,loaded->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT)||
       !pe_sandbox_collision_overlap(collision,loaded->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT))return false;
    PeVec3 original=loaded->player;
    g->yaw=-1.57079632679f;g->pitch=0;
    char *temporary=malloc(strlen(path)+24);if(!temporary)return false;
    sprintf(temporary,"%s.fixture-XXXXXX",path);int fd=mkstemp(temporary);
    if(fd<0){free(temporary);return false;}close(fd);
    bool ok=pe_game_save(g,temporary,error,sizeof error)&&pe_test_save_pre_welcome(temporary,4);size_t size=0,after_size=0;
    unsigned char *data=ok?read_file(temporary,&size):NULL,*after=NULL;
    ok=data&&memcmp(data,"PECS",4)==0&&read32(data+4)==4&&read32(data+8)==size-16&&
        read32(data+12)==bytes_hash(data+16,size-16);
    if(ok){
        write_float(data+24,original.x);write_float(data+28,original.y);write_float(data+32,original.z);
        write32(data+12,bytes_hash(data+16,size-16));
        FILE *f=fopen(temporary,"wb");ok=f!=NULL;
        if(f){ok=fwrite(data,1,size,f)==size;if(fclose(f))ok=false;}
        if(ok)ok=rename(temporary,path)==0;
    }
    uint32_t original_hash=data?bytes_hash(data,size):0;
    if(ok)ok=pe_game_load(loaded,path,error,sizeof error);
    if(ok){
        after=read_file(path,&after_size);
        ok=after&&after_size==size&&memcmp(data,after,size)==0&&read32(after+4)==4&&
            read32(after+12)==bytes_hash(after+16,size-16)&&
            loaded->player.y==original.y&&hypotf(loaded->player.x-original.x,loaded->player.z-original.z)>0&&
            hypotf(loaded->player.x-original.x,loaded->player.z-original.z)<=3.001f&&
            !pe_sandbox_collision_overlap(collision,loaded->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT)&&
            memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory)==0&&
            pe_world_hash(&loaded->world)==pe_world_hash(&g->world)&&loaded->tree_count==g->tree_count&&
            memcmp(loaded->trees,g->trees,(size_t)g->tree_count*sizeof g->trees[0])==0&&
            strstr(loaded->message,"to clear a legacy tree overlap")!=NULL;
    }
    if(ok){
        printf("Controlled v4 migration fixture: original tree%d, intentionally positioned (%.6f,%.6f,%.6f).\n",tree,original.x,original.y,original.z);
        printf("Native load: %s Position (%.6f,%.6f,%.6f); original v4 file byte-identical, FNV32 %08x.\n",
            loaded->message,loaded->player.x,loaded->player.y,loaded->player.z,original_hash);
        /* Verify the current-version rewrite separately, retaining the v4 file. */
        ok=pe_game_save(loaded,temporary,error,sizeof error)&&pe_game_load(g,temporary,error,sizeof error)&&
            memcmp(&loaded->player,&g->player,sizeof g->player)==0&&
            memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory)==0;
        if(ok)report(path,loaded);
    }
    if(!ok)fprintf(stderr,"Migration fixture validation failed.\n");
    unlink(temporary);free(temporary);free(after);free(data);return ok;
}
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"Usage: %s DIG.pesave FELL.pesave\n       %s --inspect SAVE.pesave\n       %s --migration OUTPUT.v4.pesave\n       %s --mine MINE.pesave\n",argv[0],argv[0],argv[0],argv[0]);return 2;}
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);PeSandboxCollision *collision=pe_sandbox_collision_create();
    bool ok=false;if(!g||!loaded||!collision)goto end;
    if(strcmp(argv[1],"--migration")==0){ok=migration_fixture(g,loaded,collision,argv[2]);goto end;}
    if(strcmp(argv[1],"--inspect")==0){char error[200];ok=pe_game_load(g,argv[2],error,sizeof error);if(ok)report(argv[2],g);else fprintf(stderr,"%s\n",error);goto end;}
    if(strcmp(argv[1],argv[2])==0){fprintf(stderr,"Use distinct output files.\n");goto end;}
    bool mine=strcmp(argv[1],"--mine")==0;
    if(!initialize(g,collision,mine?19:12))goto end;
    PeSandboxActionInput use={.enabled=true,.held=true,.tool_drawn=true,.camera_tree=-1,.forward_tree=-1};
    bool aimed=false;
    for(int point=0;point<9&&!aimed;point++){
    g->player=(PeVec3){45+(point%3)*2,0,45+(point/3)*2};
    g->player.y=pe_world_height(&g->world,g->player.x,g->player.z)+.1f;
    if(!pe_game_collision_settle(g,collision))continue;
    for(int direction=0;direction<16&&!aimed;direction++)for(int angle=0;angle<5&&!aimed;angle++){
        g->yaw=direction*6.28318530718f/16;g->pitch=-.65f-angle*.1f;
        camera_ray(g,collision,&use.ray_origin,&use.ray_direction);
        if(!visible_action(g,use.ray_origin,-1))continue;
        PeHit hit=pe_world_dig_target(&g->world,use.ray_origin,use.ray_direction,g->player,3);
        if(hit.hit&&pe_world_get(&g->world,hit.x,hit.y,hit.z).volume&&verify_swing(g,use,-1)){
            printf("Dig fixture intentionally repositioned on open ground; sample (%d,%d,%d), camera/body clear of every padded foliage box.\n",hit.x,hit.y,hit.z);aimed=true;
        }
    }
    }
    if(mine){ok=aimed&&save(g,loaded,argv[2]);goto end;}
    if(!aimed||!save(g,loaded,argv[1])||!initialize(g,collision,26))goto end;
    aimed=false;int order[PE_MAX_TREES];
    for(int i=0;i<g->tree_count;i++)order[i]=i;
    for(int i=0;i<g->tree_count;i++)for(int j=i+1;j<g->tree_count;j++)if(isolation(g,order[j])>isolation(g,order[i])){
        int swap=order[i];order[i]=order[j];order[j]=swap;
    }
    for(int rank=0;rank<g->tree_count&&!aimed;rank++)for(int direction=0;direction<16&&!aimed;direction++){
        int tree=order[rank];
        PeVec3 center=add(g->trees[tree].position,(PeVec3){-.1f,0,-.2f});
        float angle=direction*6.28318530718f/16;
        g->player=add(center,(PeVec3){2.8f*cosf(angle),0,2.8f*sinf(angle)});
        g->player.y=pe_world_height(&g->world,g->player.x,g->player.z)+.1f;
        if(!pe_game_collision_settle(g,collision))continue;
        PeVec3 forward=normalize((PeVec3){center.x-g->player.x,0,center.z-g->player.z});
        g->yaw=atan2f(forward.x,-forward.z);g->pitch=0;
        camera_ray(g,collision,&use.ray_origin,&use.ray_direction);
        if(!visible_action(g,use.ray_origin,tree))continue;
        use.camera_tree=trunk(g,add(use.ray_origin,scale(use.ray_direction,-.5f)),use.ray_direction,100);
        use.forward_tree=trunk(g,add(g->player,(PeVec3){0,1,0}),forward,2);
        if(use.camera_tree==tree&&use.forward_tree==tree&&verify_swing(g,use,tree)){
            printf("Fell fixture intentionally repositioned to isolated tree%d (nearest neighbor %.3fm); dual source rays match, camera/body clear of padded foliage boxes.\n",tree,isolation(g,tree));aimed=true;
        }
    }
    ok=aimed&&save(g,loaded,argv[2]);
end:
    if(!ok)fprintf(stderr,"Sandbox action fixture failed; no graphics or input were used.\n");
    pe_sandbox_collision_free(collision);pe_game_destroy(loaded);pe_game_destroy(g);free(loaded);free(g);return ok?0:1;
}
