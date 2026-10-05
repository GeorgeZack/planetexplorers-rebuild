"""Headless launch/identity/bounds tests. No original process or input is used."""
import copy
import hashlib
import json
import re
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0, str(TOOLS))
import reference_animator_bake as tool
import inventory_reference_bake as inventory


class BakeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.game = self.root/'game'; self.asset = self.game/'PE_Client_Data/sharedassets1.assets'
        self.asset.parent.mkdir(parents=True); self.asset.write_bytes(b'synthetic asset identity')
        managed = self.game/'PE_Client_Data/Managed'; managed.mkdir()
        (managed/'UnityEngine.dll').write_bytes(b'fake engine for hashing only')
        self.plan = self.root/'plan.json'
        self.data = {'schema': 1, 'asset': str(self.asset), 'asset_sha256': hashlib.sha256(self.asset.read_bytes()).hexdigest(),
                     'genders': {'female': {'avatar': 'FemaleAvatar', 'controller': 'FemaleAnimator2', 'clips': [
                         {'name': 'NormalIdle', 'path_id': 684, 'length': 2., 'frame_rate': 30, 'loop': True, 'humanoid_bindings': 127},
                         {'name': 'Attack6', 'path_id': 231, 'length': 1.1, 'frame_rate': 30, 'loop': False, 'humanoid_bindings': 127}]}}}
        self.save()

    def tearDown(self):
        self.tmp.cleanup()

    def save(self):
        self.plan.write_text(json.dumps(self.data))

    def read(self, selected=('NormalIdle', 'Attack6'), rate=30):
        return tool.read_plan(self.plan, self.game, 'female', selected, rate)

    def test_exact_identity_and_ordered_bounded_plan(self):
        p = self.read()
        self.assertEqual(p['planned_samples'], 95)
        self.assertEqual(p['spec'], 'NormalIdle|2|30|684|1;Attack6|1.1|30|231|0')
        self.assertEqual(self.read(('Attack6',))['clips'][0]['path_id'], 231)

    def test_rejects_changed_original_asset_and_other_install(self):
        self.asset.write_bytes(b'changed')
        with self.assertRaises(ValueError): self.read()
        self.data['asset_sha256'] = tool.digest(self.asset); self.data['asset'] = str(self.root/'other')
        self.save()
        with self.assertRaises(ValueError): self.read()

    def test_rejects_wrong_identity_duplicates_nan_and_malformed_timing(self):
        original = copy.deepcopy(self.data)
        for key, value in [('avatar', 'MaleAvatar'), ('controller', 'MenuAnimator')]:
            self.data = copy.deepcopy(original); self.data['genders']['female'][key] = value; self.save()
            with self.assertRaises(ValueError): self.read()
        for key, value in [('length', float('nan')), ('length', 11), ('frame_rate', 0), ('path_id', True), ('humanoid_bindings',0), ('loop','true')]:
            self.data = copy.deepcopy(original); self.data['genders']['female']['clips'][0][key] = value; self.save()
            with self.assertRaises(ValueError): self.read()
        self.data = original; self.data['genders']['female']['clips'].append(self.data['genders']['female']['clips'][0]); self.save()
        with self.assertRaises(ValueError): self.read()

    def test_selection_is_allowlisted_unique_and_rate_bounded(self):
        for names, rate in [((),30), (('NormalIdle','NormalIdle'),30), (('NormalSprint',),30), (('NormalIdle',),0), (('NormalIdle',),61)]:
            with self.subTest(names=names, rate=rate), self.assertRaises(ValueError): self.read(names,rate)
        self.plan.write_text('{"schema":1,"schema":1}')
        with self.assertRaises(ValueError): self.read()
        for malformed in ('[]', 'null'):
            self.plan.write_text(malformed)
            with self.assertRaises(ValueError): self.read()

    def test_extended_catalog_preserves_default_and_selected_ten_clip_bound(self):
        self.data['genders']['female']['clips']=[dict(name=name,path_id=pid,length=.6,frame_rate=30,loop=name=='Felling2',humanoid_bindings=127) for name,pid in inventory.CLIPS.items()]
        self.save()
        default=self.read(tool.DEFAULT_CLIPS)
        self.assertEqual(len(default['clips']),10)
        self.assertFalse(any(c['name'].startswith('Felling') for c in default['clips']))
        selected=self.read(('Felling1','Felling2','Felling3'))
        self.assertEqual([c['path_id'] for c in selected['clips']],[244,252,259])
        self.assertEqual(selected['planned_samples'],57)
        spade=self.read(('Spade1','Spade2','Spade3'))
        self.assertEqual([c['path_id'] for c in spade['clips']],[189,201,211])
        mine=self.read(('Mine1','Mine2','Mine3'))
        self.assertEqual([c['path_id'] for c in mine['clips']],[221,228,237])
        self.assertFalse(any(c['name'].startswith('Mine') for c in default['clips']))
        with self.assertRaises(ValueError):self.read(tuple(inventory.CLIPS)[:11])

    def test_managed_configuration_accepts_exactly_inventory_catalog(self):
        # Independent managed guard caused a real pre-capture failure when only
        # Python's catalog grew. Keep both entry-point contracts synchronized.
        source=(TOOLS/'ReferenceAnimatorBake.cs').read_text()
        field=re.search(r'\bAllowed\s*=\s*\{([^}]+)\}',source)
        self.assertIsNotNone(field)
        names=re.findall(r'"([A-Za-z0-9]+)"',field.group(1))
        self.assertEqual(len(names),len(set(names)))
        self.assertEqual(set(names),set(inventory.CLIPS))
        self.assertRegex(source,r'\bMaxClips\s*=\s*10\b')

    def test_felling_state_leaf_mapping_rejects_wrong_clip_and_path(self):
        states=[];names=[];references=[]
        for index,name in enumerate(('Felling1','Felling2','Felling3')):
            names += [(index,name),(index+10,'FullBody.Fell.'+name)]
            references.append(dict(m_FileID=0,m_PathID=inventory.CLIPS[name]))
            states.append({'data':dict(m_NameID=index,m_FullPathID=index+10,m_Speed=1,
                m_TransitionConstantArray=[],m_BlendTreeConstantArray=[{'data':{'m_NodeArray':[{'data':dict(m_ChildIndices=[],m_ClipID=index)}]}}])})
        controller=dict(m_TOS=names,m_AnimationClips=references,m_Controller={'m_StateMachineArray':[{'data':{'m_StateConstantArray':states}}]})
        self.assertEqual(inventory.felling_states(controller)['Felling2']['clip_index'],1)
        references[1]['m_PathID']=999
        with self.assertRaises(ValueError):inventory.felling_states(controller)
        references[1]['m_PathID']=252;names[1]=(10,'Other.Felling1')
        with self.assertRaises(ValueError):inventory.felling_states(controller)

    def test_spade_state_speed_retained_without_changing_authored_time(self):
        names=[];states=[];refs=[]
        for i,name in enumerate(('Spade1','Spade2','Spade3')):
            names.extend([(i,name),(i+10,'FullBody.Dig.'+name)])
            refs.append(dict(m_FileID=0,m_PathID=inventory.CLIPS[name]))
            states.append({'data':dict(m_NameID=i,m_FullPathID=i+10,m_Speed=2 if i==0 else 1,
                m_TransitionConstantArray=[],m_BlendTreeConstantArray=[{'data':{'m_NodeArray':[{'data':dict(m_ChildIndices=[],m_ClipID=i)}]}}])})
        c=dict(m_TOS=names,m_AnimationClips=refs,m_Controller={'m_StateMachineArray':[{'data':{'m_StateConstantArray':states}}]})
        result=inventory.spade_states(c)
        self.assertEqual([result[n]['speed'] for n in ('Spade1','Spade2','Spade3')],[2,1,1])
        refs[0]['m_PathID']=244
        with self.assertRaises(ValueError):inventory.spade_states(c)

    def test_mine_plan_uses_distinct_source_clips_and_authored_times(self):
        names=[];states=[];refs=[]
        for i,name in enumerate(('Mine1','Mine2','Mine3')):
            names.extend([(i,name),(i+10,'FullBody.Dig.'+name)])
            refs.append(dict(m_FileID=0,m_PathID=(221,228,237)[i]))
            transition=dict(m_ExitTime=(.6,.8,.9)[i],m_HasFixedDuration=i<2,
                            m_TransitionDuration=(.4,.2,.1)[i])
            states.append({'data':dict(m_NameID=i,m_FullPathID=i+10,m_Speed=1.5 if i==0 else 1,
                m_TransitionConstantArray=[{'data':transition}],
                m_BlendTreeConstantArray=[{'data':{'m_NodeArray':[{'data':dict(m_ChildIndices=[],m_ClipID=i)}]}}])})
        c=dict(m_TOS=names,m_AnimationClips=refs,m_Controller={'m_StateMachineArray':[{'data':{'m_StateConstantArray':states}}]})
        result=inventory.mine_states(c)
        self.assertEqual([result[n]['speed'] for n in ('Mine1','Mine2','Mine3')],[1.5,1,1])
        self.assertFalse(result['Mine3']['transitions'][0]['m_HasFixedDuration'])
        refs[1]['m_PathID']=201 # A shovel loop must never satisfy Mine.
        with self.assertRaises(ValueError):inventory.mine_states(c)
        refs[1]['m_PathID']=228
        states[0]['data']['m_BlendTreeConstantArray'][0]['data']['m_NodeArray'][0]['data']['m_ChildIndices']=[0]
        with self.assertRaises(ValueError):inventory.mine_states(c)
        clips=[dict(name=n,path_id=p,length=t,frame_rate=30,loop=n=='Mine2',humanoid_bindings=127)
               for n,p,t in zip(('Mine1','Mine2','Mine3'),(221,228,237),
                                (.6999969482421875,1.3333358764648438,.8333358764648438))]
        self.data['genders']['female']['clips']=clips;self.save()
        plan=self.read(('Mine1','Mine2','Mine3'))
        self.assertEqual(plan['planned_samples'],91)
        self.assertEqual(plan['clips'][0]['length'],.6999969482421875)
        male=copy.deepcopy(self.data['genders']['female'])
        male.update(avatar='MaleAvatar',controller='MaleAnimator')
        male['clips'][1].update(path_id=234,length=1.6000022888183594)
        self.data['genders']['male']=male;self.save()
        plan=tool.read_plan(self.plan,self.game,'male',('Mine1','Mine2','Mine3'),30)
        self.assertEqual(plan['planned_samples'],99)
        self.assertEqual(plan['clips'][1]['path_id'],234)

    def setup_mock_launch(self):
        mono, shim, dll = [self.root/n for n in ('mono.so','bake-shim.so','bake.dll')]
        for p in (mono,shim,dll): p.write_bytes(b'synthetic build output')
        return mono,shim,dll

    def args(self):
        return ['--game-dir',str(self.game),'--user-data',str(self.root/'profile'), '--plan',str(self.plan),'--clip','NormalIdle']

    def test_build_only_does_not_create_profile_or_launch(self):
        with mock.patch.object(tool, 'build', return_value=self.setup_mock_launch()), \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/original/client'],{})), \
             mock.patch.object(tool.launch_reference,'prepare_settings') as settings, \
             mock.patch.object(tool.launch_reference,'prepare_unity_preferences') as prefs, \
             mock.patch.object(tool.subprocess,'run') as launch:
            self.assertEqual(tool.main(self.args()),0)
            launch.assert_not_called(); settings.assert_not_called(); prefs.assert_not_called()
        self.assertFalse((self.root/'profile').exists())

    def test_explicit_launch_uses_isolated_profile_bake_dll_and_bounded_env(self):
        build = self.setup_mock_launch(); profile = self.root/'profile'; before = self.asset.read_bytes()
        with mock.patch.object(tool,'build',return_value=build), \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/original/client'], {'XDG_CONFIG_HOME':str(profile/'engine-config')})), \
             mock.patch.object(tool.launch_reference,'prepare_settings') as settings, \
             mock.patch.object(tool.launch_reference,'prepare_unity_preferences') as prefs, \
             mock.patch.object(tool.subprocess,'run',return_value=SimpleNamespace(returncode=0)) as launch:
            self.assertEqual(tool.main(self.args()+['--launch']),0)
            settings.assert_called_once_with(profile); prefs.assert_called_once_with(profile/'engine-config')
            env = launch.call_args.kwargs['env']
            self.assertEqual(env['PE_POSE_ASSEMBLY'],str(build[2]))
            self.assertEqual(env['PE_BAKE_CLIPS'],'NormalIdle|2|30|684|1')
            self.assertEqual(env['PE_BAKE_SECONDS'],'180'); self.assertEqual(env['PE_BAKE_RATE'],'30')
            self.assertTrue(Path(env['PE_POSE_OUTPUT']).is_relative_to(profile))
            self.assertEqual(env['XDG_CONFIG_HOME'],str(profile/'engine-config'))
        self.assertEqual(self.asset.read_bytes(),before)
        manifests = list(profile.glob('pose-captures/*.launch.json')); self.assertEqual(len(manifests),1)
        manifest = json.loads(manifests[0].read_text()); self.assertEqual(manifest['plan']['planned_samples'],61)
        result = json.loads(manifests[0].with_suffix('').with_suffix('.exit.json').read_text())
        self.assertEqual(result['returncode'],0); self.assertIsNone(result['signal'])
        self.assertTrue(result['observed_utc'].endswith('+00:00'))

    def test_exit_record_preserves_signal_and_existing_record_refused(self):
        profile=self.root/'profile'; output=profile/'pose-captures/test.jsonl'
        args=self.args()+['--launch','--output',str(output)]
        with mock.patch.object(tool,'build',return_value=self.setup_mock_launch()), \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/original/client'],{})), \
             mock.patch.object(tool.launch_reference,'prepare_settings'), \
             mock.patch.object(tool.launch_reference,'prepare_unity_preferences'), \
             mock.patch.object(tool.subprocess,'run',return_value=SimpleNamespace(returncode=-6)) as launch:
            self.assertEqual(tool.main(args),134)
            result=json.loads(output.with_suffix('.exit.json').read_text())
            self.assertEqual(result['returncode'],-6); self.assertEqual(result['signal'],6)
            output.with_suffix('.launch.json').unlink()
            launch.reset_mock()
            self.assertEqual(tool.main(args),1)
            launch.assert_not_called()

    def test_existing_preload_rejected_before_settings_or_launch(self):
        with mock.patch.object(tool,'build',return_value=self.setup_mock_launch()), \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/original/client'], {'LD_PRELOAD':'foreign.so'})), \
             mock.patch.object(tool.launch_reference,'prepare_settings') as settings, \
             mock.patch.object(tool.subprocess,'run') as launch:
            self.assertEqual(tool.main(self.args()+['--launch']),1)
            settings.assert_not_called(); launch.assert_not_called()


if __name__ == '__main__':
    unittest.main()
