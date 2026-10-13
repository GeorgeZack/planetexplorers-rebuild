#!/usr/bin/env python3
"""Solver pose export and the C loader on synthetic poses, and the staged real
poses through pe_fbbik when they are installed. No Unity execution."""
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'rebuild/tools'))
import export_player_ik_pose as e

LIBRARY=ROOT/'rebuild/build/obj/core/libpe_core.a'
STAGED=e.ASSETS/e.GROUP/e.FILE
# With a time list: world position and rotation of every bone at each time of clip 0.
# Without: zero-offset and body-offset solves over every frame of every clip.
HARNESS=r'''
#include "pe_ik_pose.h"
#include <math.h>
#include <stdio.h>
static float gap(PeVec3 a,PeVec3 b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return sqrtf(x*x+y*y+z*z);}
int main(int argc,char **argv){
    const PeFbbikRig *rig=pe_fbbik_female_rig();char error[64]="";PeIkPose *pose=argc>1?pe_ik_pose_load(argv[1],rig,error,sizeof error):0;
    if(!pose){fprintf(stderr,"%s\n",error);return 2;}
    PeFbbikTransform rest[PE_FBBIK_MAX_BONES],local[PE_FBBIK_MAX_BONES];PeFbbik state;
    if(!pe_fbbik_female_rest(rest)||!pe_fbbik_init(&state,rig,rest))return 3;
    double t;int timed=0;
    while(scanf("%lf",&t)==1){
        timed=1;if(!pe_ik_pose_sample(pose,0,t,local))return 4;
        for(unsigned b=0;b<rig->bone_count;b++){PeFbbikTransform w;pe_fbbik_world(rig,local,b,&w);
            printf("%.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",w.position.x,w.position.y,w.position.z,w.rotation.x,w.rotation.y,w.rotation.z,w.rotation.w);}
    }
    if(timed){pe_ik_pose_free(pose);return 0;}
    float worst=0,length=0,moved=0;unsigned frames=0,failed=0,exact=0;
    for(size_t c=0;c<pe_ik_pose_clips(pose);c++){
        const PeActorClip *clip=pe_ik_pose_clip(pose,c);float clip_worst=0;
        for(unsigned f=0;f<clip->frames;f++){
            PeFbbikInput in;PeFbbikOutput out;
            if(!pe_ik_pose_sample(pose,c,f+1<clip->frames?f/(double)clip->fps:clip->duration,local))return 4;
            if(!pe_fbbik_input(&state,local,&in)||!pe_fbbik_solve(&state,&in,&out)){failed++;continue;}
            frames++;
            for(unsigned b=0;b<rig->bone_count;b++){PeFbbikTransform a,s;pe_fbbik_world(rig,local,b,&a);pe_fbbik_world(rig,out.local,b,&s);
                float d=gap(a.position,s.position);if(d>clip_worst)clip_worst=d;}
            for(unsigned k=1;k<PE_FBBIK_CHAINS;k++)for(unsigned n=0;n<2;n++){
                PeFbbikTransform a,b,ra,rb;unsigned i=rig->chain_bone[k][n],j=rig->chain_bone[k][n+1];
                pe_fbbik_world(rig,local,i,&a);pe_fbbik_world(rig,local,j,&b);pe_fbbik_world(rig,rest,i,&ra);pe_fbbik_world(rig,rest,j,&rb);
                float d=fabsf(gap(a.position,b.position)-gap(ra.position,rb.position));if(d>length)length=d;}
            in.effector[0].position_offset=(PeVec3){0,-.2f,0};
            if(!pe_fbbik_solve(&state,&in,&out))failed++;
            else{PeFbbikTransform a,s;unsigned b=rig->chain_bone[0][0];pe_fbbik_world(rig,local,b,&a);pe_fbbik_world(rig,out.local,b,&s);
                float d=gap(a.position,s.position);if(d>moved)moved=d;}
        }
        if(clip_worst>worst)worst=clip_worst;
        if(clip_worst<1e-5f)exact++;
    }
    printf("%zu %u %u %u %.9g %.9g %.9g\n",pe_ik_pose_clips(pose),frames,failed,exact,worst,length,moved);
    pe_ik_pose_free(pose);return 0;
}
'''
# Arguments: actor, poses, then joint:bone pairs. For each pair, how far a point skinned to the
# actor joint strays from a fixed place in the solver bone's animated frame, over every sample.
SKIN_HARNESS=r'''
#include "pe_actor.h"
#include "pe_ik_pose.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static PeVec3 into(PeFbbikTransform t,const float p[3]){
    PeVec3 d={p[0]-t.position.x,p[1]-t.position.y,p[2]-t.position.z};PeFbbikQuat q={-t.rotation.x,-t.rotation.y,-t.rotation.z,t.rotation.w};
    float x=q.x*2,y=q.y*2,z=q.z*2,xx=q.x*x,yy=q.y*y,zz=q.z*z,xy=q.x*y,xz=q.x*z,yz=q.y*z,wx=q.w*x,wy=q.w*y,wz=q.w*z;
    return (PeVec3){(1-(yy+zz))*d.x+(xy-wz)*d.y+(xz+wy)*d.z,(xy+wz)*d.x+(1-(xx+zz))*d.y+(yz-wx)*d.z,(xz-wy)*d.x+(yz+wx)*d.y+(1-(xx+yy))*d.z};
}
int main(int argc,char **argv){
    char error[128]="";const PeFbbikRig *rig=pe_fbbik_female_rig();
    PeActor *actor=argc>3?pe_actor_load(argv[1],error,sizeof error):0;PeIkPose *pose=actor?pe_ik_pose_load(argv[2],rig,error,sizeof error):0;
    if(!pose){fprintf(stderr,"%s\n",error);return 2;}
    static const float points[3][3]={{0,0,1},{.1f,0,1.2f},{0,.1f,.8f}};
    for(int a=3;a<argc;a++){
        unsigned joint,bone;if(sscanf(argv[a],"%u:%u",&joint,&bone)!=2)return 3;
        PeVec3 first[3];int have=0;float worst=0;unsigned samples=0;
        for(size_t c=0;c<pe_ik_pose_clips(pose);c++){
            const PeActorClip *clip=pe_ik_pose_clip(pose,c);size_t other=pe_actor_find_clip(actor,clip->name);
            if(other==PE_ACTOR_BIND_POSE)return 4;
            for(unsigned f=0;f<clip->frames;f++){
                double t=f+1<clip->frames?f/(double)clip->fps:clip->duration;PeFbbikTransform local[PE_FBBIK_MAX_BONES],world;
                if(!pe_ik_pose_sample(pose,c,t,local)||!pe_fbbik_world(rig,local,bone,&world))return 5;
                for(int k=0;k<3;k++){
                    float skinned[3];if(!pe_actor_pose_joint_point(actor,other,t,joint,points[k],skinned))return 6;
                    PeVec3 inside=into(world,skinned);
                    if(!have)first[k]=inside;
                    else{float x=inside.x-first[k].x,y=inside.y-first[k].y,z=inside.z-first[k].z,d=sqrtf(x*x+y*y+z*z);if(d>worst)worst=d;}
                }
                have=1;samples++;
            }
        }
        printf("%u %u %u %.9g\n",joint,bone,samples,worst);
    }
    pe_ik_pose_free(pose);pe_actor_free(actor);return 0;
}
'''
def identity():return [[1.,0.,0.,0.],[0.,1.,0.,0.],[0.,0.,1.,0.],[0.,0.,0.,1.]]
def from_trs(p,q):
    x,y,z,w=q
    return [[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),p[0]],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),p[1]],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),p[2]],[0.,0.,0.,1.]]
def worlds(rig,turn):
    """Model-space matrices of the rest pose with the left forearm turned about its local z."""
    out=[]
    for bone in rig['bones']:
        q=bone['local_rotation']
        if bone['name']=='Bip01 L Forearm':
            s,c=math.sin(turn/2),math.cos(turn/2);x,y,z,w=q;q=[x*c+y*s,y*c-x*s,z*c+w*s,w*c-z*s]
        local=from_trs(bone['local_position'],q);out.append(local if bone['parent']<0 else e.multiply(out[bone['parent']],local))
    return out
def samples(rig,turns):
    return [dict(bones={b['name']:m for b,m in zip(rig['bones'],worlds(rig,t)) if b['name'] not in e.STATIC}) for t in turns]

class Export(unittest.TestCase):
    def setUp(self):self.rig=json.loads(e.RIG.read_text())['rig']
    def test_decompose_recovers_rest_locals_and_refuses_scale(self):
        frames=e.local_frames(self.rig,samples(self.rig,[0.]))
        for bone,values in zip(self.rig['bones'],frames[0]):
            expected=bone['local_position']+bone['local_rotation']
            if sum(a*b for a,b in zip(values[3:],expected[3:]))<0:values=values[:3]+[-x for x in values[3:]]
            for a,b in zip(values,expected):self.assertAlmostEqual(a,b,places=5)
        scaled=samples(self.rig,[0.]);m=scaled[0]['bones']['Bip01 L Calf']
        for row in m[:3]:row[0]*=1.01
        with self.assertRaises(ValueError):e.local_frames(self.rig,scaled)
    def test_encoding_bounds(self):
        frames=e.local_frames(self.rig,samples(self.rig,[0.,.2,.4]))
        data=e.encode(self.rig,[('Bend',2.,.75,False,frames)]);self.assertEqual(data[:4],b'PEIK')
        self.assertEqual(len(data),16+28+48+3*25*28)
        for bad in ([('Bend',2.,.75,False,frames),('Bend',2.,.75,False,frames)],[('Bad name',2.,.75,False,frames)],[('Bend',2.,.4,False,frames)],
                    [('Bend',2.,1.2,False,frames)],[('Bend',2.,.75,False,frames[:1])],[]):
            with self.assertRaises(ValueError):e.encode(self.rig,bad)
    def test_manifest_names_the_animated_bones(self):
        names=e.animated_names(self.rig);self.assertEqual(len(names),22);self.assertNotIn('IK',names);self.assertIn('Bip01',names)
        starter=dict(schema=1,sex='female',avatar={},parts=[dict(bone_names=['x'])]);self.assertEqual(e.bake_rig(starter,self.rig)['parts'],[dict(bone_names=names)])
        self.assertEqual(starter['parts'][0]['bone_names'],['x'])

@unittest.skipUnless(shutil.which('cc') and LIBRARY.is_file(),'C compiler or built core library unavailable')
class Loader(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder=tempfile.TemporaryDirectory(dir=ROOT/'rebuild/build');folder=Path(cls.folder.name)
        source=folder/'harness.c';cls.binary=folder/'harness';source.write_text(HARNESS)
        subprocess.run(['cc','-std=c11','-O2','-I',str(ROOT/'rebuild/src'),str(source),str(LIBRARY),'-lm','-o',str(cls.binary)],check=True)
    @classmethod
    def tearDownClass(cls):cls.folder.cleanup()
    def test_exported_poses_load_and_sample_in_c(self):
        rig=json.loads(e.RIG.read_text())['rig'];turns=[0.,.3,.9];frames=e.local_frames(rig,samples(rig,turns))
        path=Path(self.folder.name)/'synthetic.peik';path.write_bytes(e.encode(rig,[('Bend',2.,.75,False,frames)]))
        result=subprocess.run([str(self.binary),str(path)],input='0 0.5 0.75\n',capture_output=True,text=True,check=True)
        rows=[[float(v) for v in line.split()] for line in result.stdout.splitlines()];self.assertEqual(len(rows),3*25)
        for index,turn in enumerate(turns):
            for row,matrix in zip(rows[index*25:(index+1)*25],worlds(rig,turn)):
                for axis in range(3):self.assertAlmostEqual(row[axis],matrix[axis][3],places=5)
                back=from_trs(row[:3],row[3:])
                for i in range(3):
                    for j in range(3):self.assertAlmostEqual(back[i][j],matrix[i][j],places=5)
    @unittest.skipUnless(STAGED.is_file(),'staged player IK poses unavailable')
    def test_staged_poses_through_the_solver(self):
        manifest=json.loads((e.ASSETS/e.GROUP/e.MANIFEST).read_text());self.assertEqual(e.digest(STAGED),manifest['sha256'])
        result=subprocess.run([str(self.binary),str(STAGED)],input='',capture_output=True,text=True,check=True)
        clips,frames,failed,exact,worst,length,moved=(float(v) for v in result.stdout.split())
        self.assertEqual((clips,frames,failed),(len(manifest['clips']),sum(c['frames'] for c in manifest['clips']),0))
        # Baked limbs keep the prefab rest lengths. With no offsets the solve is exact on most clips
        # and bends nearly straight limbs by up to about two centimetres on the rest.
        self.assertLess(length,1e-5);self.assertLess(worst,.025);self.assertGreaterEqual(exact,15);self.assertGreater(moved,.1)
    @unittest.skipUnless(STAGED.is_file() and (e.ASSETS/'player-defence/player-defence.peac').is_file(),'staged player IK poses or defence actor unavailable')
    def test_staged_poses_match_the_staged_skin_palettes(self):
        # A point skinned to an actor joint must sit still in that bone's animated frame; otherwise
        # the poses and the palettes are not the same motion in the same space.
        rig=json.loads(e.RIG.read_text())['rig'];joints=json.loads((e.ASSETS/'player-defence/player-defence.json').read_text())['joint_names']
        bones={b['name']:b['index'] for b in rig['bones']};pairs=[(j,bones[name]) for j,name in enumerate(joints) if name in bones]
        self.assertGreaterEqual(len(pairs),19)
        source=Path(self.folder.name)/'skin.c';binary=Path(self.folder.name)/'skin';source.write_text(SKIN_HARNESS)
        subprocess.run(['cc','-std=c11','-O2','-I',str(ROOT/'rebuild/src'),str(source),str(LIBRARY),'-lm','-o',str(binary)],check=True)
        result=subprocess.run([str(binary),str(e.ASSETS/'player-defence/player-defence.peac'),str(STAGED)]+['%d:%d'%p for p in pairs],capture_output=True,text=True,check=True)
        rows=[line.split() for line in result.stdout.splitlines()];self.assertEqual(len(rows),len(pairs))
        for joint,bone,count,worst in rows:
            self.assertEqual(int(count),991);self.assertLess(float(worst),1e-4,joints[int(joint)])
if __name__=='__main__':unittest.main()
