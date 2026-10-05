"""Synthetic closed isolated-Animator output; never invokes a Unity process."""
import copy
import json
from pathlib import Path
import struct
import shutil
import subprocess
import sys
import tempfile
import unittest

TOOLS=Path(__file__).resolve().parents[1]/'tools';sys.path.insert(0,str(TOOLS))
import analyze_reference_bake as bake
import compare_reference_bake as comparison
from test_reference_pose_analysis import fixture, matrix, write, transform


def bake_fixture():
    _,rig=fixture()
    rows=[dict(type='header',schema='pe-animator-bake-1',matrixOrder='row-major',coordinateSpace='isolated-animator-model-local',
               fireEvents=False,controllerAttached=False,applyRootMotion=False,sampleRate=2,maxSamples=3600,maxBytes=bake.pose.MAX_BYTES),
          dict(type='skeleton',gender='Male',avatar='MaleAvatar',plannedSamples=3,humanBones={'RightHand':1},nodes=[
              dict(path='',parent=-1,sourceId=1,cloneId=101,initialLocal=matrix()),
              dict(path='Bone',parent=0,sourceId=2,cloneId=102,initialLocal=matrix(0,2))]),
          dict(type='clip',index=0,name='Attack6',sourcePathId=179,runtimeId=8,length=.75,sourceFrameRate=30,samples=3,isLooping=False,events=[])]
    for i,t in enumerate((0,.5,.75)):
        rows.append(dict(type='pose',clip=0,sample=i,frame=100+i,realtime=20+i*.1,authoredTime=t,
                         rootWorld=matrix(),animatorRootPosition=[0,0,0],animatorRootRotation=[0,0,0,1],
                         deltaPosition=[0,0,0],deltaRotation=[0,0,0,1],
                         local=[matrix(),matrix(t,2)],world=[matrix(),matrix(t,2)]))
    rows.extend([dict(type='clipEnd',clip=0,samples=3),dict(type='end',reason='complete',samples=3)])
    return rows,rig


class BakeAnalysisTests(unittest.TestCase):
    def run_analysis(self,rows,rig,root,retain=False):
        path=root/'bake.jsonl';write(path,rows);return bake.analyze(path,rig,retain)

    def test_authored_clock_hierarchy_mapping_and_motion(self):
        rows,rig=bake_fixture()
        with tempfile.TemporaryDirectory() as d:
            report,clips=self.run_analysis(rows,rig,Path(d),True)
            self.assertEqual(report['samples'],3);self.assertTrue(report['runtime_motion_detected'])
            self.assertEqual(report['maximum_hierarchy_matrix_error'],0)
            self.assertEqual(report['clips'][0]['maximum_bone_position_change_m'],.75)
            self.assertEqual([s['realtime'] for s in clips['Attack6']],[0,.5,.75])
            self.assertFalse(report['root_motion_validated'])

    def test_rejects_incomplete_clock_identity_and_hierarchy_errors(self):
        original,rig=bake_fixture();cases=[]
        x=copy.deepcopy(original);x[-1]['reason']='stop-file';cases.append(x)
        cases.append(original[:-1])
        x=copy.deepcopy(original);x[1]['avatar']='FemaleAvatar';cases.append(x)
        x=copy.deepcopy(original);x[4]['authoredTime']=.4;cases.append(x)
        x=copy.deepcopy(original);x[4]['world'][1][3]=7;cases.append(x)
        x=copy.deepcopy(original);x[4]['frame']=100;cases.append(x)
        x=copy.deepcopy(original);x[3]['rootWorld'][3]=1;cases.append(x)
        x=copy.deepcopy(original);x[1]['nodes'][1]['parent']=1;cases.append(x)
        x=copy.deepcopy(original);x[0]['fireEvents']=True;cases.append(x)
        x=copy.deepcopy(original);x[4]['local'][1][0]=float('nan');cases.append(x)
        x=copy.deepcopy(original);x[2]['samples']=4;cases.append(x)
        with tempfile.TemporaryDirectory() as d:
            for i,rows in enumerate(cases):
                with self.subTest(case=i),self.assertRaises(ValueError):self.run_analysis(rows,rig,Path(d))

    def test_explicit_npc_skin_paths_disambiguate_same_named_siblings(self):
        rows,rig=bake_fixture();rig['parts'][0]['bone_paths']=['Bone#0']
        skeleton=copy.deepcopy(rows[1]);skeleton['nodes'][1]['path']='Bone#0'
        extra=copy.deepcopy(skeleton['nodes'][1]);extra.update(path='Bone#1',sourceId=3,cloneId=103)
        skeleton['nodes'].append(extra)
        mapped=bake.mapping_for(rig,skeleton)
        self.assertEqual(mapped['bones']['Bone']['index'],1);self.assertEqual(mapped['missing_bones'],[])
        rig['parts'][0]['bone_paths']=[]
        with self.assertRaises(ValueError):bake.mapping_for(rig,skeleton)

    def test_signed_bundle_clip_ids_remain_exact_and_bounded(self):
        rows,rig=bake_fixture()
        with tempfile.TemporaryDirectory() as d:
            rows[2]['sourcePathId']=-4333529506565661133
            report,_=self.run_analysis(rows,rig,Path(d))
            self.assertEqual(report['clips'][0]['sourcePathId'],-4333529506565661133)
            for invalid in (0,1<<63,-(1<<63)-1,True,1.5):
                rows[2]['sourcePathId']=invalid
                with self.subTest(invalid=invalid),self.assertRaises(ValueError):self.run_analysis(rows,rig,Path(d))

    def test_frozen_bake_is_reported_without_claiming_successful_motion(self):
        rows,rig=bake_fixture()
        for row in rows:
            if row['type']=='pose':row['local'][1]=matrix(0,2);row['world'][1]=matrix(0,2)
        with tempfile.TemporaryDirectory() as d:
            report,_=self.run_analysis(rows,rig,Path(d))
            self.assertFalse(report['runtime_motion_detected']);self.assertEqual(report['clips'][0]['changing_bones'],0)

    def test_explicit_new_terminal_policy_and_legacy_extended_precision(self):
        rows,rig=bake_fixture();length=bake.f32(2/3);rows[0]['sampleRate']=30;rows[1]['plannedSamples']=22
        rows[2].update(length=length,samples=22)
        sample=rows[3];new=rows[:3]
        for i in range(22):
            row=copy.deepcopy(sample);t=min(i/30,length)
            row.update(sample=i,frame=100+i,realtime=20+i*.1,authoredTime=t,local=[matrix(),matrix(t,2)],world=[matrix(),matrix(t,2)])
            new.append(row)
        new.extend([dict(type='clipEnd',clip=0,samples=22),dict(type='end',reason='complete',samples=22)])
        with tempfile.TemporaryDirectory() as d:
            report,_=self.run_analysis(new,rig,Path(d))
            self.assertEqual(report['clips'][0]['sample_count_arithmetic'],'extended product precision')
            new[0]['sampleCountPolicy']='ceil-f64-product-terminal-length'
            self.run_analysis(new,rig,Path(d))
            new[2]['samples']=21
            with self.assertRaises(ValueError):self.run_analysis(new,rig,Path(d))

    def test_retained_per_clip_hierarchies_require_distinct_instance_ids(self):
        rows,rig=bake_fixture()
        rows[0].update(cleanupPolicy='retain-inactive-per-clip-until-process-exit',maxRetainedGraphs=10)
        rows[1]['plannedSamples']=6;rows[2]['cloneIds']=[101,102]
        second=copy.deepcopy(rows[2:-1]);second[0].update(index=1,name='Attack7',cloneIds=[201,202])
        for row in second[1:]:
            row['clip']=1
            if row['type']=='pose':row['frame']+=100;row['realtime']+=10
        rows=rows[:-1]+second+[dict(type='end',reason='complete',samples=6)]
        with tempfile.TemporaryDirectory() as d:
            report,_=self.run_analysis(rows,rig,Path(d));self.assertEqual(len(report['clips']),2)
            next(r for r in rows if r['type']=='clip' and r['index']==1)['cloneIds']=[101,102]
            with self.assertRaises(ValueError):self.run_analysis(rows,rig,Path(d))

    def test_observed_pose_comparison_preserves_errors_and_filters_transitions(self):
        rows,rig=bake_fixture();observed,_=fixture()
        for row in observed:
            if row['type']=='frame':row['animators'][0]['layers'][0]['currentClips'][0]['length']=.75
        observed[2]['transforms'][1]=transform(3,2,'Player/Model/Bone',matrix(.75,2),matrix(10.75,2))
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);bp=root/'b.jsonl';cp=root/'c.jsonl';write(bp,rows);write(cp,observed)
            result=comparison.compare(bp,cp,rig,'Attack6',layer='Base')
            self.assertEqual(result['compared_frames'],2);self.assertEqual(result['maximum_position_error_m'],0)
            observed[2]['transforms'][1]=transform(3,2,'Player/Model/Bone',matrix(1,2),matrix(11,2));write(cp,observed)
            self.assertEqual(comparison.compare(bp,cp,rig,'Attack6',layer='Base')['maximum_position_error_m'],.25)
            observed[2]['animators'][0]['layers'][0]['inTransition']=True;write(cp,observed)
            self.assertEqual(comparison.compare(bp,cp,rig,'Attack6',layer='Base')['compared_frames'],1)

    def test_export_preserves_authored_name_timeline_skin_and_terminal_pose(self):
        rows,rig=bake_fixture()
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);report,samples=self.run_analysis(rows,rig,root,True)
            out=root/'out';out.mkdir();clip=report['clips'][0]
            manifest=bake.export_clip(out,rig,root/'rig.json',samples['Attack6'],report,clip)
            data=(out/manifest['parts'][0]['file']).read_bytes()
            self.assertEqual(struct.unpack_from('<4s5I',data),(b'PEAC',2,3,3,1,1))
            name,fps,count,duration,loop=struct.unpack_from('<32sfIfI',data,252)
            self.assertEqual((name.rstrip(b'\0'),fps,count,duration,loop),(b'Attack6',2.,3,.75,0))
            self.assertEqual([struct.unpack_from('<12f',data,300+i*48)[3] for i in range(3)],[0,.5,.75])
            self.assertIn('authored',manifest['timebase']);self.assertNotIn('first_capture_sample',manifest)
            bake.export_clip(out,rig,root/'rig.json',samples['Attack6'],report,clip)
            self.assertEqual(data,(out/manifest['parts'][0]['file']).read_bytes())

    def test_combines_clip_blocks_without_repeating_or_changing_geometry(self):
        rows,rig=bake_fixture()
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);report,samples=self.run_analysis(rows,rig,root,True)
            sources=[]
            for name,loop in [('Attack6',False),('NormalIdle',True)]:
                out=root/name;out.mkdir();clip=dict(report['clips'][0],name=name,isLooping=loop)
                sources.append((out,bake.export_clip(out,rig,root/'rig.json',samples['Attack6'],report,clip)))
            manifest=bake.combine_clips(root/'all',sources)
            data=(root/'all'/manifest['parts'][0]['file']).read_bytes()
            self.assertEqual(struct.unpack_from('<I',data,20)[0],2)
            offset=252
            self.assertEqual(struct.unpack_from('<32s',data,offset)[0].rstrip(b'\0'),b'Attack6')
            offset+=48+3*48
            self.assertEqual(struct.unpack_from('<32s',data,offset)[0].rstrip(b'\0'),b'NormalIdle')
            self.assertEqual(struct.unpack_from('<I',data,offset+44)[0],1)
            self.assertEqual(len(data),252+2*(48+3*48))
            part=sources[1][0]/sources[1][1]['parts'][0]['file'];bad=bytearray(part.read_bytes());bad[24]^=1;part.write_bytes(bad)
            with self.assertRaises(ValueError):bake.combine_clips(root/'bad',sources)

    @unittest.skipUnless(shutil.which('cc'),'optional native C compiler')
    def test_combined_output_loads_through_native_multiclip_scene_parser(self):
        rows,rig=bake_fixture()
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);report,samples=self.run_analysis(rows,rig,root,True);sources=[]
            for name in ('Attack6','NormalIdle'):
                out=root/name;out.mkdir();clip=dict(report['clips'][0],name=name)
                sources.append((out,bake.export_clip(out,rig,root/'rig.json',samples['Attack6'],report,clip)))
            bake.combine_clips(root/'all',sources)
            source=root/'check.c';source.write_text('''#include "pe_actor_scene.h"
#include <string.h>
int main(int argc,char **argv){
 char error[256];if(argc!=2)return 1;
 PeActorSceneManifest *s=pe_actor_scene_manifest_load(argv[1],error,sizeof error);if(!s)return 2;
 int ok=pe_actor_scene_clip_count(s)==2 && !strcmp(pe_actor_scene_clip_at(s,1)->name,"NormalIdle");
 pe_actor_scene_manifest_free(s);return ok?0:3;
}
''')
            src=TOOLS.parent/'src';exe=root/'check'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(src),str(source),str(src/'pe_actor.c'),
                            str(src/'pe_actor_scene_manifest.c'),'-lm','-o',str(exe)],check=True,capture_output=True)
            subprocess.run([str(exe),str(root/'all/recording.peas')],check=True,capture_output=True)


if __name__=='__main__':unittest.main()
