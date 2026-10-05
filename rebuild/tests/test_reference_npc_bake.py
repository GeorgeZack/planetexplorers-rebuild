"""NPC sampler identity/bounds and build-only/launch contracts; no game launched."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import reference_npc_bake as tool

class NpcBakeTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.game=self.root/'game';self.game.mkdir()
        self.asset=self.game/'bundle';self.asset.write_bytes(b'original bundle')
        engine=self.game/'PE_Client_Data/Managed/UnityEngine.dll';engine.parent.mkdir(parents=True);engine.write_bytes(b'engine')
        self.plan=self.root/'plan.json';self.data={'schema':1,'npcs':[]}
        for npc,model in tool.ROSTER.items():
            female=npc in(9004,9006,9031)
            self.data['npcs'].append(dict(npc_id=npc,model=model,avatar=model+'Avatar',gender='Female' if female else 'Male',
                controller='FemaleAnimator2' if female else 'AllenCarterAnimator' if npc==9009 else 'MaleAnimator',asset=str(self.asset),asset_sha256=tool.digest(self.asset),
                clip=dict(name='NormalIdle',path_id=7422444173399996218 if female else -4333529506565661133,length=2 if female else 2.266666889,frame_rate=30,loop=True,humanoid_bindings=127)))
        self.write()
    def tearDown(self):self.temp.cleanup()
    def write(self):self.plan.write_text(json.dumps(self.data))
    def read(self):return tool.read_plan(self.plan,self.game,30)
    def args(self):return ['--game-dir',str(self.game),'--user-data',str(self.root/'profile'),'--plan',str(self.plan),'--output',str(self.root/'profile/poses/npcs.jsonl')]
    def artifacts(self):
        files=[self.root/n for n in('mono.so','shim.so','sampler.dll')]
        for f in files:f.write_bytes(b'test build')
        return files
    def test_exact_nine_source_identity_and_signed64_clip(self):
        rows,spec=self.read();self.assertEqual(len(rows),9);self.assertIn('-4333529506565661133',spec)
        self.assertEqual(rows[1]['clips'][0]['path_id'],-4333529506565661133)
        self.assertEqual(len(spec.split(';')),9)
        self.assertEqual(len(set(tool.outputs_for(Path('/profile/npcs.jsonl'),rows))),9)
    def test_malformed_identity_and_timing_rejected(self):
        original=copy.deepcopy(self.data)
        for field,value in [('npc_id',9999),('model','Different'),('avatar','MaleAvatar'),('controller','Menu'),('gender','Male')]:
            self.data=copy.deepcopy(original);self.data['npcs'][0][field]=value;self.write()
            with self.subTest(field=field),self.assertRaises(ValueError):self.read()
        for field,value in [('path_id',0),('path_id',1<<63),('path_id',-(1<<63)-1),('path_id',True),('length',float('nan')),('frame_rate',0),('loop','yes'),('humanoid_bindings',0)]:
            self.data=copy.deepcopy(original);self.data['npcs'][0]['clip'][field]=value;self.write()
            with self.subTest(field=field),self.assertRaises(ValueError):self.read()
    def test_asset_hash_scope_duplicate_plan_and_count_rejected(self):
        self.asset.write_bytes(b'changed')
        with self.assertRaises(ValueError):self.read()
        self.asset.write_bytes(b'original bundle');outside=self.root/'outside';outside.write_bytes(self.asset.read_bytes());self.data['npcs'][0]['asset']=str(outside);self.write()
        with self.assertRaises(ValueError):self.read()
        self.data['npcs'][0]=self.data['npcs'][1];self.write()
        with self.assertRaises(ValueError):self.read()
        self.plan.write_text('{"schema":1,"schema":1,"npcs":[]}')
        with self.assertRaises(ValueError):self.read()
    def test_build_only_never_launches_or_creates_profile(self):
        with mock.patch.object(tool,'build',return_value=self.artifacts()),mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/client'],{})),mock.patch.object(tool.launch_reference,'prepare_settings')as settings,mock.patch.object(tool.subprocess,'run')as launch:
            self.assertEqual(tool.main(self.args()),0);settings.assert_not_called();launch.assert_not_called()
        self.assertFalse((self.root/'profile').exists())
    def test_launch_separate_nine_compatible_provenance_files_and_raw_exit(self):
        with mock.patch.object(tool,'build',return_value=self.artifacts()),mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/client'],{'XDG_CONFIG_HOME':str(self.root/'profile/engine-config')})),mock.patch.object(tool.launch_reference,'prepare_settings'),mock.patch.object(tool.launch_reference,'prepare_unity_preferences'),mock.patch.object(tool.subprocess,'run',return_value=SimpleNamespace(returncode=-11))as launch:
            self.assertEqual(tool.main(self.args()+['--launch']),139)
            env=launch.call_args.kwargs['env'];self.assertEqual(len(env['PE_NPC_PLAN'].split(';')),9)
            self.assertEqual(env['PE_POSE_ASSEMBLY'],str(self.root/'sampler.dll'))
        directory=self.root/'profile/poses'
        sidecars=list(directory.glob('npcs.npc-*.launch.json'));self.assertEqual(len(sidecars),9)
        for path in sidecars:
            data=json.loads(path.read_text());self.assertEqual(data['schema'],'pe-animator-bake-launch-1');self.assertEqual(len(data['plan']['clips']),1)
        record=json.loads((directory/'npcs.exit.json').read_text());self.assertEqual((record['returncode'],record['signal']),(-11,11))
    def test_existing_npc_output_rejects_before_build(self):
        output=self.root/'profile/poses/npcs.npc-9005.jsonl';output.parent.mkdir(parents=True);output.write_text('keep')
        with mock.patch.object(tool,'build')as build:
            self.assertEqual(tool.main(self.args()+['--launch']),1);build.assert_not_called()
        self.assertEqual(output.read_text(),'keep')
    def test_managed_probe_has_nonmutating_retained_graph_contract(self):
        source=(tool.TOOLS/'ReferenceNpcBake.cs').read_text()
        self.assertIn('Int64.TryParse(fields[3]',source)
        self.assertIn('candidate.runtimeAnimatorController.name!=expectedController',source)
        self.assertIn('ancestor.name==expectedModel',source)
        self.assertIn('animator.runtimeAnimatorController=null;animator.fireEvents=false;animator.applyRootMotion=false',source)
        self.assertIn('GC.SuppressFinalize(playable)',source)
        self.assertIn('alive<120',source)
        self.assertNotIn('Instantiate(original',source)
        self.assertNotIn('playerField',source)
if __name__=='__main__':unittest.main()
