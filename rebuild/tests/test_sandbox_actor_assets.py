"""Actual optional Sword actor loading with a CPU draw boundary, no window."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]


class ActorAssets(unittest.TestCase):
    @unittest.skipUnless(shutil.which('cc'),'C compiler required')
    def test_installed21_staged33_and_mixed_attachment_assets(self):
        stage=ROOT/'rebuild/build/starter-pair-unified-03';assets=ROOT/'rebuild/build/reference-assets'
        if not (stage/'unified-manifest.json').is_file() or not (assets/'player/recording.peas').is_file():
            self.skipTest('Actual21 installed and33 staged assets required')
        src=ROOT/'rebuild/src'
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);code=folder/'actor.c';binary=folder/'actor'
            code.write_text(r'''#include "pe_sandbox_actor.h"
#include "pe_actor_scene_view.h"
#include "pe_shield.h"
#include "pe_hitbox.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const char *base,*stage;static PeSandboxBodyPose expected;static unsigned draws;
static void resolve(const char *path,char out[4096]){
const char *part=strchr(path,'/');assert(part);part++;
if(!strncmp(path,"original/",9))snprintf(out,4096,"%s/%s",base,part);
else if(!strncmp(path,"stage/",6))snprintf(out,4096,"%s/%s",stage,part);
else{snprintf(out,4096,"%s/%s",stage,part);if(access(out,F_OK))snprintf(out,4096,"%s/%s",base,part);}}
bool IsWindowReady(void){return true;}
bool FileExists(const char *path){char out[4096];resolve(path,out);return access(out,F_OK)==0;}
void BeginMode3D(Camera3D c){(void)c;}void EndMode3D(void){}
bool pe_actor_scene_view_prepare(PeActorSceneView*v,const char *path,char *err,size_t cap){char out[4096];resolve(path,out);
v->manifest=pe_actor_scene_manifest_load(out,err,cap);if(!v->manifest)return false;
v->count=pe_actor_scene_part_count(v->manifest);v->parts=calloc(v->count,sizeof *v->parts);assert(v->parts);
for(size_t i=0;i<v->count;i++){v->parts[i].actor=(PeActor*)pe_actor_scene_actor(v->manifest,i);}return true;}
bool pe_actor_scene_view_upload(PeActorSceneView*v,char *e,size_t c){(void)e;(void)c;v->loaded=true;return true;}
void pe_actor_scene_view_unload(PeActorSceneView*v){pe_actor_scene_manifest_free(v->manifest);free(v->parts);memset(v,0,sizeof *v);}
size_t pe_actor_scene_view_clip_count(const PeActorSceneView*v){return pe_actor_scene_clip_count(v->manifest);}
size_t pe_actor_scene_view_find_clip(const PeActorSceneView*v,const char*n){return pe_actor_scene_find_clip(v->manifest,n);}
const PeActorClip *pe_actor_scene_view_clip_at(const PeActorSceneView*v,size_t i){return pe_actor_scene_clip_at(v->manifest,i);}
bool pe_actor_scene_view_draw_clip(PeActorSceneView*v,size_t i,double t,Vector3 p,float yaw,float scale,Color color){
(void)p;(void)yaw;(void)scale;(void)color;const PeActorClip*c=pe_actor_scene_view_clip_at(v,i);
assert(c&&!expected.blend_count&&!strcmp(c->name,expected.clip)&&t==expected.seconds);
for(size_t part=0;part<v->count;part++){PeActorVertex vertex;assert(pe_actor_pose_vertex(v->parts[part].actor,i,t,0,&vertex));}draws++;return true;}
bool pe_actor_scene_view_draw_blend(PeActorSceneView*v,const PeActorBlendSample*s,size_t n,Vector3 p,float yaw,float scale,Color color){
(void)p;(void)yaw;(void)scale;(void)color;assert(n==expected.blend_count);
for(size_t i=0;i<n;i++){const PeActorClip*c=pe_actor_scene_view_clip_at(v,s[i].clip);assert(c&&!strcmp(c->name,expected.blend[i].clip)&&s[i].seconds==expected.blend[i].seconds&&s[i].weight==expected.blend[i].weight);}
for(size_t part=0;part<v->count;part++){PeActorVertex vertex;assert(pe_actor_pose_vertex_blend(v->parts[part].actor,s,n,0,&vertex));}draws++;return true;}
const PeSlot*pe_game_equipped_tool(const PeGame*g){for(unsigned i=0;i<PE_INVENTORY_SLOTS;i++)if(g->equipped_instance_id&&g->inventory.slots[i].instance_id==g->equipped_instance_id&&g->inventory.slots[i].count==1)return &g->inventory.slots[i];return NULL;}
static void check_draw(PeSandboxActor*a,PeGame*g){assert(pe_sandbox_actor_body_pose(a,g,&expected));draws=0;pe_sandbox_actor_draw(a,g);assert(pe_sandbox_actor_ok(a)&&draws==3);}
int main(int argc,char **argv){assert(argc==3);base=argv[1];stage=argv[2];char error[256];
PeSandboxActor*a=pe_sandbox_actor_load("original",error,sizeof error);if(!a){puts(error);return 2;}assert(pe_sandbox_actor_hold_ready(a,33)&&!pe_sandbox_actor_hold_ready(a,1));pe_sandbox_actor_unload(a);
for(int mode=0;mode<2;mode++){
a=pe_sandbox_actor_load(mode?"overlay":"stage",error,sizeof error);if(!a){puts(error);return 3;}
assert(pe_sandbox_actor_hold_ready(a,1)&&pe_sandbox_actor_running_ready_for_item(a,1)&&pe_sandbox_actor_shield_ready(a));
assert(pe_sandbox_actor_hold_ready(a,33)==(mode!=0));assert(pe_sandbox_actor_action_ready(a,19)==(mode!=0));
PeGame*g=calloc(1,sizeof *g);assert(g);g->mode=PE_GAME_ADVENTURE;g->hp=500;g->grounded=true;g->yaw=PI;g->inventory.next_instance_id=3;
g->inventory.slots[0]=(PeSlot){.item_id=1,.count=1,.instance_id=1,.durability=2369};g->equipped_instance_id=1;
g->inventory.slots[1]=(PeSlot){.item_id=34,.count=1,.instance_id=2,.durability=4312};g->shield_instance_id=2;
pe_sandbox_actor_reset_motion(a,g);assert(pe_sandbox_actor_set_melee_item(a,1,1,true,NULL));assert(pe_sandbox_actor_set_shield(a,2));
check_draw(a,g);assert(!strcmp(expected.clip,"AttackIdle"));
g->player.z-=.2;pe_sandbox_actor_update(a,.1,g);check_draw(a,g);assert(!strcmp(expected.clip,"SwordWB"));
g->player.z-=.5;pe_sandbox_actor_update(a,.1,g);check_draw(a,g);assert(!strcmp(expected.clip,"SwordRB"));
const PeActor*body,*weapon;assert(pe_sandbox_actor_melee_assets_for_item(a,1,&body,&weapon));assert(pe_actor_stats(body).clips==33&&pe_actor_stats(weapon).clips==33);
char path[4096];snprintf(path,sizeof path,"%s/player-wood-sword-drawn/wood-sword-drawn.pehb",stage);PeHitboxes*h=pe_hitboxes_load(path,weapon,error,sizeof error);assert(h&&pe_hitboxes_count(h)==1);pe_hitboxes_free(h);
PeCombat c={.active=true,.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,.variant=PE_COMBAT_STATIONARY,.clip_time=.125};
for(unsigned i=0;i<4;i++){c.stage=i;assert(pe_sandbox_actor_set_melee_item(a,1,1,true,&c));check_draw(a,g);assert(!expected.blend_count);}
c.variant=PE_COMBAT_RUNNING;c.stage=0;assert(pe_sandbox_actor_set_melee_item(a,1,1,true,&c));check_draw(a,g);assert(!strcmp(expected.clip,"RunSwordAttack"));
pe_sandbox_actor_unload(a);free(g);}
return 0;}
''')
            command=['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(src),str(code),*[str(src/f) for f in ('pe_sandbox_actor.c','pe_player_locomotion.c','pe_combat.c','pe_actor.c','pe_actor_scene_manifest.c','pe_hitbox.c','pe_shield.c')],'-lm','-o',str(binary)]
            if os.environ.get('PE_TEST_SANITIZE'):command[1:1]=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
            subprocess.run(command,check=True)
            subprocess.run([str(binary),str(assets),str(stage)],check=True)


if __name__=='__main__':unittest.main()
