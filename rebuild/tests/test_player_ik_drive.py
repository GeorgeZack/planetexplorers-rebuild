#!/usr/bin/env python3
"""Joint driver export on synthetic input, and the installed drivers and solver
poses through the real sandbox actor with a CPU draw boundary. No window and no
Unity execution."""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'rebuild/tools'))
import export_player_ik_drivers as e

LIBRARY=ROOT/'rebuild/build/obj/core/libpe_core.a'
INSTALLED=e.ASSETS/e.GROUP/e.FILE
POSES=e.ASSETS/'player-ik-pose/player-ik-pose.peik'
# The real sandbox actor over the installed assets. Every scene it draws is posed twice on the
# CPU, baked and driven, and the largest vertex displacement is reported per scenario.
HARNESS=r'''
#include "pe_sandbox_actor.h"
#include "pe_actor_scene_view.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned driven,baked,bound_views,unbound_views;static double worst;
static PeActorVertex plain[8192],moved[8192];
bool IsWindowReady(void){return true;}
bool FileExists(const char *path){return access(path,F_OK)==0;}
void BeginMode3D(Camera3D c){(void)c;}void EndMode3D(void){}
bool pe_actor_scene_view_prepare(PeActorSceneView*v,const char *path,char *err,size_t cap){
    v->manifest=pe_actor_scene_manifest_load(path,err,cap);if(!v->manifest)return false;
    v->count=pe_actor_scene_part_count(v->manifest);v->parts=calloc(v->count,sizeof *v->parts);assert(v->parts);
    for(size_t i=0;i<v->count;i++)v->parts[i].actor=(PeActor*)pe_actor_scene_actor(v->manifest,i);
    return true;
}
bool pe_actor_scene_view_upload(PeActorSceneView*v,char *e,size_t c){(void)e;(void)c;v->loaded=true;return true;}
void pe_actor_scene_view_unload(PeActorSceneView*v){pe_actor_scene_manifest_free(v->manifest);free(v->parts);memset(v,0,sizeof *v);}
size_t pe_actor_scene_view_clip_count(const PeActorSceneView*v){return pe_actor_scene_clip_count(v->manifest);}
size_t pe_actor_scene_view_find_clip(const PeActorSceneView*v,const char*n){return pe_actor_scene_find_clip(v->manifest,n);}
const PeActorClip *pe_actor_scene_view_clip_at(const PeActorSceneView*v,size_t i){return pe_actor_scene_clip_at(v->manifest,i);}
/* As pe_actor_scene_view.c: all parts resolve against their exact geometry, or none is bound. */
bool pe_actor_scene_view_bind_ik(PeActorSceneView*v,const PeIkDrive*d,const char*group){
    bool bound=d&&group&&v->loaded;
    for(size_t i=0;i<v->count;i++){v->parts[i].ik_driver=bound?pe_ik_drive_drivers(d,group,i,v->parts[i].actor):NULL;bound=bound&&v->parts[i].ik_driver;}
    if(!bound)for(size_t i=0;i<v->count;i++)v->parts[i].ik_driver=NULL;
    v->ik_bound=bound;v->ik_active=false;if(bound)bound_views++;else unbound_views++;
    return bound;
}
void pe_actor_scene_view_set_ik(PeActorSceneView*v,const PeIkDriveFrame*f){v->ik_active=f&&v->ik_bound;if(v->ik_active)v->ik=*f;}
bool pe_actor_scene_view_draw_blend(PeActorSceneView*v,const PeActorBlendSample*s,size_t n,Vector3 p,float yaw,float scale,Color color){
    (void)p;(void)yaw;(void)scale;(void)color;
    for(size_t part=0;part<v->count;part++){
        const PeActor *a=v->parts[part].actor;size_t vertices=pe_actor_stats(a).vertices;assert(vertices<=8192);
        if(!pe_actor_pose_blend(a,s,n,plain,8192))return false;
        if(!(v->ik_bound&&v->ik_active)){baked++;continue;}
        if(!pe_actor_pose_blend_driven(a,s,n,v->parts[part].ik_driver,v->ik.delta[0],moved,8192))return false;
        driven++;
        for(size_t i=0;i<vertices;i++){
            double d=0,length=0;
            for(int k=0;k<3;k++){double q=(double)moved[i].position[k]-plain[i].position[k];d+=q*q;length+=(double)moved[i].normal[k]*moved[i].normal[k];}
            assert(fabs(length-1)<1e-4);if(sqrt(d)>worst)worst=sqrt(d);
        }
    }
    return true;
}
bool pe_actor_scene_view_draw_clip(PeActorSceneView*v,size_t i,double t,Vector3 p,float yaw,float scale,Color color){
    PeActorBlendSample sample={i,t,1};return pe_actor_scene_view_draw_blend(v,&sample,1,p,yaw,scale,color);
}
static void report(const char *name){printf("%s %u %u %.9g\n",name,driven,baked,worst);driven=baked=0;worst=0;}
static void equip(PeGame *g,int item,uint32_t instance){
    g->inventory.slots[420]=(PeSlot){.item_id=item,.count=1,.instance_id=instance,.durability=100};g->equipped_instance_id=instance;
    const PeSlot *slot=pe_game_equipped_tool(g);assert(slot&&slot->item_id==item);
}
int main(int argc,char **argv){
    assert(argc==2);char error[256];PeSandboxActor *a=pe_sandbox_actor_load(argv[1],error,sizeof error);if(!a){puts(error);return 2;}
    printf("views %u %u\n",bound_views,unbound_views);
    PeGame *g=calloc(1,sizeof *g);assert(g);g->mode=PE_GAME_ADVENTURE;g->hp=500;g->grounded=true;g->yaw=PI;g->player=(PeVec3){20,10,20};
    g->inventory.next_instance_id=9;g->inventory.categorized=true;
    pe_sandbox_actor_reset_motion(a,g);pe_sandbox_actor_facing_mode(a,true);
    for(int i=0;i<60;i++){pe_sandbox_actor_update(a,1/30.f,g);pe_sandbox_actor_draw(a,g);}
    report("idle");
    for(int i=0;i<60;i++){g->player.x+=.05f;pe_sandbox_actor_update(a,1/30.f,g);pe_sandbox_actor_draw(a,g);}
    report("walk");
    for(int i=0;i<60;i++){g->player.x+=.2f;pe_sandbox_actor_update(a,1/30.f,g);pe_sandbox_actor_draw(a,g);}
    report("run");
    for(int i=0;i<60;i++){g->player.z-=.05f;pe_sandbox_actor_update(a,1/30.f,g);pe_sandbox_actor_draw(a,g);}
    report("turn");
    for(int i=0;i<30;i++){pe_sandbox_actor_update(a,1/30.f,g);pe_sandbox_actor_draw(a,g);}
    report("stop");
    g->hp=0;pe_sandbox_actor_draw(a,g);g->hp=500;report("dead");
    if(pe_sandbox_actor_hold_ready(a,33)){
        equip(g,33,3);assert(pe_sandbox_actor_set_melee(a,3,false,NULL));
        for(int i=0;i<30;i++){pe_sandbox_actor_update(a,1/30.f,g);pe_sandbox_actor_draw(a,g);}
        report("knife-stowed");
        assert(pe_sandbox_actor_set_melee(a,3,true,NULL));
        for(int i=0;i<30;i++){pe_sandbox_actor_update(a,1/30.f,g);pe_sandbox_actor_draw(a,g);}
        report("knife-held");
        PeCombat c={.active=true,.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,.variant=PE_COMBAT_STATIONARY};
        for(unsigned stage=0;stage<4;stage++)for(double t=0;t<.95;t+=1/30.){
            c.stage=stage;c.clip_time=t;assert(pe_sandbox_actor_set_melee(a,3,true,&c));pe_sandbox_actor_draw(a,g);
        }
        report("knife-attacks");
        if(pe_sandbox_actor_running_ready(a)){
            c.variant=PE_COMBAT_RUNNING;c.stage=0;
            for(double t=0;t<1.8;t+=1/30.){c.clip_time=t;assert(pe_sandbox_actor_set_melee(a,3,true,&c));pe_sandbox_actor_draw(a,g);}
            report("running-attack");
        }
        assert(pe_sandbox_actor_set_melee(a,3,true,NULL));
    }
    static const struct {int item;const char *name,*clip[3];double length[3];} tools[]={
        {12,"shovel",{"Spade1","Spade2","Spade3"},{.69,1.33,.83}},{1527,"axe",{"Felling1","Felling2","Felling3"},{.59,1.09,.59}},
        {19,"pickaxe",{"Mine1","Mine2","Mine3"},{.69,1.33,.83}}};
    for(size_t k=0;k<sizeof tools/sizeof tools[0];k++)if(pe_sandbox_actor_action_ready(a,tools[k].item)){
        equip(g,tools[k].item,4);PeSandboxActionPose pose={.item_id=tools[k].item,.instance_id=4,.held=true};
        assert(pe_sandbox_actor_set_action(a,&pose));pe_sandbox_actor_draw(a,g);
        for(int phase=0;phase<3;phase++)for(double t=0;t<tools[k].length[phase];t+=1/30.){
            pose.clip=tools[k].clip[phase];pose.seconds=t;assert(pe_sandbox_actor_set_action(a,&pose));pe_sandbox_actor_draw(a,g);
        }
        report(tools[k].name);
    }
    if(pe_sandbox_actor_hold_ready(a,10)){
        equip(g,10,5);PeSandboxActionPose pose={.item_id=10,.instance_id=5,.held=true};assert(pe_sandbox_actor_set_action(a,&pose));
        for(int i=0;i<30;i++){g->player.x+=.05f;pe_sandbox_actor_update(a,1/30.f,g);pe_sandbox_actor_draw(a,g);}
        PeVec3 light;assert(pe_sandbox_actor_torch_light(a,g,&light)&&isfinite(light.x));
        report("torch");
    }
    assert(pe_sandbox_actor_ok(a));pe_sandbox_actor_unload(a);free(g);return 0;
}
'''

class Export(unittest.TestCase):
    def setUp(self):self.rig=json.loads(e.RIG.read_text())['rig'];self.bones={b['name']:b['index'] for b in self.rig['bones']}
    def test_a_joint_follows_itself_or_its_nearest_solver_ancestor(self):
        hand='Bip01/Bip01 Pelvis/Bip01 Spine1/Bip01 Spine2/Bip01 Spine3/Bip01 R Clavicle/Bip01 R UpperArm/Bip01 R Forearm/Bip01 R Hand'
        self.assertEqual(e.driver(hand,self.bones,'hand'),self.bones['Bip01 R Hand'])
        self.assertEqual(e.driver(hand+'/Bip01 R Finger0/Bip01 R Finger01',self.bones,'finger'),self.bones['Bip01 R Hand'])
        self.assertEqual(e.driver(hand+'/mountMain',self.bones,'mount'),self.bones['Bip01 R Hand'])
        self.assertEqual(e.driver('Bip01/Bip01 Pelvis/Bip01 Spine1/Bip01 Spine2/mountWaist',self.bones,'waist'),self.bones['Bip01 Spine2'])
        for bad in (None,'','CamTarget'):
            with self.assertRaises(ValueError):e.driver(bad,self.bones,'bad')
    def test_skeleton_row_gives_unique_paths_only(self):
        with tempfile.TemporaryDirectory() as directory:
            capture=Path(directory)/'bake.jsonl'
            capture.write_text(json.dumps(dict(type='header'))+'\n'+json.dumps(dict(type='skeleton',nodes=[dict(path=''),dict(path='A'),dict(path='A/B'),dict(path='A/C'),dict(path='A/C/B')]))+'\n')
            paths=e.skeleton_paths(capture);self.assertEqual(paths['C'],'A/C');self.assertIsNone(paths['B'])
            capture.write_text(json.dumps(dict(type='header'))+'\n')
            with self.assertRaises(ValueError):e.skeleton_paths(capture)
    def test_encoding_layout_and_bounds(self):
        data=e.encode(self.rig,[('player',0,0x1122334455667788,[4,5,9]),('player',1,7,[24])])
        count=len(self.rig['bones']);self.assertEqual(data[:4],b'PEID');self.assertEqual(struct.unpack_from('<II',data,4),(1,count))
        self.assertEqual(list(struct.unpack_from('<%dI'%count,data,12)),[b['id'] for b in self.rig['bones']])
        at=12+count*4;self.assertEqual(struct.unpack_from('<I',data,at)[0],2);at+=4
        self.assertEqual(data[at:at+48].rstrip(b'\0'),b'player');self.assertEqual(struct.unpack_from('<IQI',data,at+48),(0,0x1122334455667788,3))
        self.assertEqual(data[at+64:at+68],bytes([4,5,9,0]));self.assertEqual(len(data),at+68+68)
        for bad in ([],[('player',0,1,[4]),('player',0,2,[4])],[('player/x',0,1,[4])],[('player',0,1,[])],[('player',0,1,[count])],[('p'*48,0,1,[4])]):
            with self.assertRaises(ValueError):e.encode(self.rig,bad)
    def test_actor_header_and_hash(self):
        # The fixture of the C actor suite; the hash skips the version and clip count.
        data=bytearray(636);data[:4]=b'PEAC';struct.pack_into('<5I',data,4,1,3,3,2,1)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'a.peac';path.write_bytes(data);joints,first,binds=e.actor(path)
            self.assertEqual((joints,len(binds)),(2,24))
            struct.pack_into('<I',data,4,2);struct.pack_into('<I',data,20,5);path.write_bytes(data);self.assertEqual(e.actor(path)[1],first)
            data[30]^=1;path.write_bytes(data);self.assertNotEqual(e.actor(path)[1],first)
            path.write_bytes(b'PEAK'+bytes(40))
            with self.assertRaises(ValueError):e.actor(path)

@unittest.skipUnless(shutil.which('cc') and LIBRARY.is_file(),'C compiler or built core library unavailable')
@unittest.skipUnless(INSTALLED.is_file() and POSES.is_file() and (e.ASSETS/'player/recording.peas').is_file(),'installed player actors, IK poses or IK drivers unavailable')
class Installed(unittest.TestCase):
    def test_manifest_matches_the_installed_groups(self):
        manifest=json.loads((e.ASSETS/e.GROUP/e.MANIFEST).read_text());self.assertEqual(e.digest(INSTALLED),manifest['sha256'])
        for entry in manifest['entries']:
            self.assertEqual(e.digest(e.ASSETS/entry['group']/entry['file']),entry['sha256'],entry['file'])
            self.assertEqual(len(entry['joints']),len(entry['drivers']))
            for joint,bone in zip(entry['joints'],entry['drivers']):
                if joint in manifest['bones']:self.assertEqual(joint,bone)
    def test_sandbox_actor_draws_every_installed_pose_through_the_solve(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'rebuild/build') as directory:
            source=Path(directory)/'drive.c';binary=Path(directory)/'drive';source.write_text(HARNESS)
            command=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT/'rebuild/src'),str(source),str(ROOT/'rebuild/src/pe_sandbox_actor.c'),str(LIBRARY),'-llz4','-lm','-o',str(binary)]
            if os.environ.get('PE_TEST_SANITIZE'):command[1:1]=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
            subprocess.run(command,check=True)
            result=subprocess.run([str(binary),str(e.ASSETS)],capture_output=True,text=True,check=True)
        rows={line.split()[0]:[float(v) for v in line.split()[1:]] for line in result.stdout.splitlines()}
        if os.environ.get('PE_TEST_VERBOSE'):print(result.stdout,file=sys.stderr)
        bound,unbound=rows.pop('views');self.assertGreaterEqual(bound,11);self.assertEqual(unbound,0)
        dead=rows.pop('dead');self.assertEqual(dead[0],0);self.assertGreater(dead[1],0)
        self.assertLessEqual({'idle','walk','run','turn','stop'},set(rows))
        for name,(driven,baked,worst) in rows.items():
            # Every part of every drawn scene carries the solve, and it stays a small correction.
            self.assertGreater(driven,0,name);self.assertEqual(baked,0,name);self.assertLess(worst,.04,name)
        self.assertGreater(max(row[2] for row in rows.values()),1e-3)
if __name__=='__main__':unittest.main()
