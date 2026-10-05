#!/usr/bin/env python3
"""Synthetic fixtures only; no claimed original-runtime capture or graphics."""
import copy
import json
import math
from pathlib import Path
import struct
import shutil
import subprocess
import sys
import tempfile
import unittest
import zlib

TOOLS=Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0,str(TOOLS))
import analyze_reference_pose as tool
import measure_reference_pose_clock as clock


def matrix(x=0,y=0,z=0):
    return [1,0,0,x,0,1,0,y,0,0,1,z,0,0,0,1]


def transform(identity,parent,path,local,world):
    return dict(id=identity,parentId=parent,path=path,activeInHierarchy=True,
                localPosition=[local[3],local[7],local[11]],localRotation=[0,0,0,1],
                localScale=[1,1,1],local=local,world=world)


def fixture():
    parent=transform(1,0,'Player',matrix(10),matrix(10))
    root=transform(2,1,'Player/Model',matrix(),matrix(10))
    bone=transform(3,2,'Player/Model/Bone',matrix(0,2),matrix(10,2))
    state=dict(fullPathHash=42,shortNameHash=7,normalizedTime=0.,length=1.,speed=1.,speedMultiplier=1.,loop=False)
    anim=dict(id=100,path='Player/Model',avatar='MaleAvatar',avatarId=90,controller='Test',enabled=True,
              activeInHierarchy=True,speed=1.,applyRootMotion=False,rootWorld=matrix(10),rootInverseWorld=matrix(-10),
              parameters={'FP':0.,'trigger':None},humanBones={'RightHand':3},
              layers=[dict(name='Base',weight=1.,inTransition=False,current=state,next=dict(state),
                           currentClips=[dict(name='Attack6',id=8,weight=1.,length=1.)],nextClips=[])])
    first=dict(type='frame',sample=0,frame=100,time=3.,realtime=5.,delta=.1,unscaledDelta=.1,fixedTime=3.,fixedDelta=.02,
               gender='Male',playerRoot=parent,modelRoot=root,transforms=[root,bone],animators=[anim],
               renderers=[dict(path='Player/Model',enabled=True,mesh='Combined',boneIds=[3])])
    second=copy.deepcopy(first);second.update(sample=1,frame=101,time=4.,realtime=6.,fixedTime=4.)
    second['transforms'][1]=transform(3,2,'Player/Model/Bone',matrix(1,2),matrix(11,2))
    second['animators'][0]['layers'][0]['current']['normalizedTime']=1.
    header=dict(type='header',schema=1,matrixOrder='row-major',phase='WaitForEndOfFrame',unity='synthetic-fixture',maxFrames=10,maxBytes=tool.MAX_BYTES)
    records=[header,first,second,dict(type='end',frames=2,reason='fixture')]
    inverse=[matrix(0,-2)[i:i+4] for i in range(0,16,4)]
    skin={**{f'weight[{i}]':1. if i==0 else 0. for i in range(4)},**{f'boneIndex[{i}]':0 for i in range(4)}}
    rig=dict(schema=1,sex='male',avatar={'m_Name':'MaleAvatar','m_TOS':[[0,''],[1,'Bone']],
        'm_Avatar':{'m_SkeletonNameIDArray':[0,zlib.crc32(b'Bone')],'m_AvatarSkeleton':{'data':{'m_ID':[0,1]}}}},
        parts=[dict(resource='synthetic',bone_names=['Bone'],inverse_binds=[inverse],positions=[[0,0,0],[1,0,0],[0,1,0]],
                    normals=[[0,0,1]]*3,uv=[[0,0],[1,0],[0,1]],skin=[dict(skin) for _ in range(3)],
                    triangles=[[[0,1,2]]],materials=[['fixture',1]])],materials=[dict(asset='fixture',path_id=1,name='test')])
    return records,rig


def write(path,records):
    path.write_text(''.join(json.dumps(row)+'\n' for row in records))


class CaptureTests(unittest.TestCase):
    def test_valid_hierarchy_mapping_and_clip_span(self):
        records,rig=fixture()
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'capture.jsonl';write(path,records)
            report,samples=tool.analyze(path,rig,retain=True)
            self.assertTrue(report['complete']);self.assertEqual(report['selected_frames'],2)
            self.assertEqual(report['maximum_trs_hierarchy_matrix_error'],0.)
            self.assertEqual(report['rig_mapping']['bones']['Bone']['id'],3)
            self.assertTrue(report['rig_mapping']['bones']['Bone']['referenced_by_enabled_skin'])
            self.assertEqual(samples[0]['bones']['Bone'][0][3],0.)
            self.assertEqual(samples[1]['bones']['Bone'][0][3],1.)
            self.assertEqual(report['animation_spans'][0]['normalized_time'],[0.,1.])
            self.assertFalse(report['general_retargeting_validated'])

    def test_malformed_frames_are_rejected(self):
        cases=[]
        rows,_=fixture();rows[2]['frame']=100;cases.append(rows)
        rows,_=fixture();rows[2]['sample']=3;cases.append(rows)
        rows,_=fixture();rows[1]['transforms'][1]['world'][3]+=1;cases.append(rows)
        rows,_=fixture();rows[1]['transforms'][1]['localPosition'][0]=4;cases.append(rows)
        rows,_=fixture();rows[1]['transforms'][1]['parentId']=3;cases.append(rows)
        rows,_=fixture();rows[1]['transforms'].append(copy.deepcopy(rows[1]['transforms'][1]));cases.append(rows)
        rows,_=fixture();rows[1]['transforms'][1]['localRotation'][3]=0;cases.append(rows)
        rows,_=fixture();rows[1]['transforms'][1]['world'][0]=float('nan');cases.append(rows)
        rows,_=fixture();rows[1]['animators'][0]['rootInverseWorld'][3]=0;cases.append(rows)
        rows,_=fixture();rows[1]['gender']='Female';cases.append(rows)
        rows,_=fixture();rows[-1]['frames']=3;cases.append(rows)
        rows,_=fixture();rows.append(rows[1]);cases.append(rows)
        _,rig=fixture()
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'capture.jsonl'
            for index,rows in enumerate(cases):
                with self.subTest(case=index):
                    write(path,rows)
                    with self.assertRaises(ValueError):tool.analyze(path,rig)

    def test_incomplete_analysis_cannot_export(self):
        records,rig=fixture()
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'capture.jsonl';write(path,records[:-1])
            with self.assertRaises(ValueError):tool.analyze(path,rig)
            report,samples=tool.analyze(path,rig,allow_incomplete=True,retain=True)
            self.assertFalse(report['complete'])
            with self.assertRaises(ValueError):tool.export_recording(Path(d),rig,Path(d)/'rig.json',samples,report,30.,[0])
            path.write_text(path.read_text().rstrip('\n'))
            with self.assertRaises(ValueError):tool.analyze(path,rig,allow_incomplete=True)

    def test_explicit_hypothesis_error_and_frame_alignment(self):
        records,rig=fixture()
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'capture.jsonl';write(path,records)
            hyp=Path(d)/'hyp.json'
            value=dict(schema=1,coordinate_space='model-local',frames=[dict(sample=1,frame=101,bone_matrices={'Bone':matrix(1.25,2)})])
            hyp.write_text(json.dumps(value))
            report,_=tool.analyze(path,rig,hypothesis=hyp)
            result=report['hypothesis_comparison'];self.assertEqual(result['bone_comparisons'],1)
            self.assertAlmostEqual(result['translation_rms_m'],.25)
            self.assertAlmostEqual(result['max_absolute_matrix_error'],.25)
            value['frames'][0]['frame']=102;hyp.write_text(json.dumps(value))
            with self.assertRaises(ValueError):tool.analyze(path,rig,hypothesis=hyp)

    def test_recording_matrix_order_skinning_timing_and_determinism(self):
        records,rig=fixture()
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);path=root/'capture.jsonl';write(path,records)
            report,samples=tool.analyze(path,rig,retain=True)
            manifest=tool.export_recording(root,rig,root/'rig.json',samples,report,2.,[0])
            self.assertEqual((root/'recording.peas').read_text(),'PEAS1\nplayer-part-0-material-0.peac player-part-0-material-0-solid.png\n')
            self.assertTrue((root/manifest['parts'][0]['texture']).read_bytes().startswith(b'\x89PNG\r\n\x1a\n'))
            self.assertTrue(manifest['parts'][0]['generated_solid_diffuse'])
            binary=(root/manifest['parts'][0]['file']).read_bytes()
            self.assertEqual(struct.unpack_from('<4s5I',binary),(b'PEAC',2,3,3,1,1))
            offset=24+3*56+3*4+48
            name,fps,count,duration,loop=struct.unpack_from('<32sfIfI',binary,offset)
            self.assertEqual((fps,count,duration,loop),(2.,3,1.,0))
            palettes=[struct.unpack_from('<12f',binary,offset+48+i*48) for i in range(3)]
            self.assertEqual([p[3] for p in palettes],[0.,.5,1.])
            self.assertEqual([p[7] for p in palettes],[0.,0.,0.])
            self.assertEqual(manifest['timebase'],'recorded realtimeSinceStartup; includes pauses and speed changes')
            tool.export_recording(root,rig,root/'rig.json',samples,report,2.,[0])
            self.assertEqual(binary,(root/manifest['parts'][0]['file']).read_bytes())

    def test_v2_non_grid_terminal_and_rounded_duration(self):
        for duration,expected_frames in ((.75,3),(1.00000001,3)):
            records,rig=fixture();records[2]['realtime']=5.+duration
            with tempfile.TemporaryDirectory() as directory:
                root=Path(directory);path=root/'capture.jsonl';write(path,records)
                report,samples=tool.analyze(path,rig,retain=True)
                manifest=tool.export_recording(root,rig,root/'rig.json',samples,report,2.,[0])
                binary=(root/manifest['parts'][0]['file']).read_bytes();offset=252
                self.assertEqual(struct.unpack_from('<I',binary,4)[0],2)
                self.assertEqual(manifest['frames'],expected_frames)
                self.assertTrue(manifest['terminal_at_duration'])
                self.assertEqual(struct.unpack_from('<12f',binary,offset+48+(expected_frames-1)*48)[3],1.)
                if duration==.75:
                    self.assertAlmostEqual(struct.unpack_from('<12f',binary,offset+96)[3],2/3,places=6)

    def test_cli_failure_preserves_existing_report(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);report=root/'report.json';report.write_text('keep')
            result=subprocess.run([sys.executable,str(TOOLS/'analyze_reference_pose.py'),str(root/'missing'),
                '--rig',str(root/'missingrig'),'--report',str(report)],capture_output=True)
            self.assertNotEqual(result.returncode,0);self.assertEqual(report.read_text(),'keep')

    def test_renderer_visibility_change_keeps_mapping(self):
        records,rig=fixture();records[2]['renderers'][0]['enabled']=False
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'capture.jsonl';write(path,records)
            report,_=tool.analyze(path,rig)
            self.assertEqual(report['selected_frames'],2)

    @unittest.skipUnless(shutil.which('cc'),'optional native C compiler')
    def test_recording_loads_and_skins_in_native_actor_module(self):
        records,rig=fixture()
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);path=root/'capture.jsonl';write(path,records)
            report,samples=tool.analyze(path,rig,retain=True)
            manifest=tool.export_recording(root,rig,root/'rig.json',samples,report,2.,[0])
            source=root/'test.c'
            source.write_text('''#include "pe_actor.h"
#include <math.h>
int main(int argc,char **argv){
 char error[256]; if(argc!=2)return 1;
 PeActor *actor=pe_actor_load(argv[1],error,sizeof error);if(!actor)return 2;
 PeActorVertex vertex; if(!pe_actor_pose_vertex(actor,0,.5,0,&vertex))return 3;
 int result=fabsf(vertex.position[0]-.5f)>.00001f || fabsf(vertex.position[1])>.00001f;
 pe_actor_free(actor);return result?4:0;
}
''')
            src=TOOLS.parent/'src';binary=root/'check'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(src),str(source),
                            str(src/'pe_actor.c'),'-lm','-o',str(binary)],check=True,capture_output=True)
            subprocess.run([str(binary),str(root/manifest['parts'][0]['file'])],check=True,capture_output=True)

    def test_measured_clock_and_estimated_event_alignment(self):
        records,rig=fixture()
        mid=copy.deepcopy(records[1]);mid.update(sample=1,frame=101,time=3.5,realtime=5.5,fixedTime=3.5)
        mid['animators'][0]['layers'][0]['current']['normalizedTime']=.5
        records[2].update(sample=2,frame=102);records.insert(2,mid);records[-1]['frames']=3
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'capture.jsonl';write(path,records)
            rows=clock.compact_frames(path,100,0,None,'Base')
            source={'m_Name':'Attack6','m_Events':[{'time':.25,'functionName':'AnimEvent','data':'test-event'}]}
            result=clock.measure(rows,source)
            self.assertAlmostEqual(result['clip_clocks'][0]['clip_seconds_per_scaled_second'],1.)
            event=result['event_alignment']['events'][0]
            self.assertAlmostEqual(event['recording_alignment']['recording_seconds'],.25)
            self.assertFalse(result['activation_observed'])
            rows[1]['layer']['current']['normalizedTime']=-.1
            with self.assertRaises(ValueError):clock.measure(rows,source)

    def test_duplicate_json_keys_rejected(self):
        with self.assertRaises(ValueError):tool.decode('{"schema":1,"schema":2}')

if __name__=='__main__':unittest.main()
