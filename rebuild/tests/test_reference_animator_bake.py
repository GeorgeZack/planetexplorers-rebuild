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

    def test_extended_catalog_preserves_default_and_selected_32_clip_bound(self):
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
        self.assertEqual(len(self.read(tuple(inventory.CLIPS)[:32])['clips']),32)
        with self.assertRaises(ValueError):self.read(tuple(inventory.CLIPS)[:33])
        for clip in self.data['genders']['female']['clips']:clip['length']=10
        self.save()
        with self.assertRaises(ValueError):self.read(tuple(inventory.CLIPS)[:32],60)

    def test_sword_selection_is_complete_bounded_and_not_a_normal_idle_alias(self):
        self.data['genders']['female']['clips']=[dict(name=name,path_id=pid,length=.5,
            frame_rate=30,loop=name in ('AttackIdle','SwordWF','SwordRF','SwordSprint','SwordRunFall'),
            humanoid_bindings=127) for name,pid in inventory.SWORD_CLIPS.items()]
        self.save();plan=self.read(tuple(inventory.SWORD_CLIPS))
        self.assertEqual(len(plan['clips']),10)
        self.assertEqual(plan['planned_samples'],160)
        self.assertEqual(plan['clips'][0]['name'],'AttackIdle')
        self.assertEqual(plan['clips'][0]['path_id'],90)
        self.assertFalse(next(c for c in plan['clips'] if c['name']=='SwordFall')['loop'])
        self.assertTrue(next(c for c in plan['clips'] if c['name']=='SwordRunFall')['loop'])
        with self.assertRaises(ValueError):self.read(('SwordIdle',))

    def test_sword_controller_branch_and_leaf_identity_guards(self):
        # Minimal structural fixture: full source graph is verified separately
        # by the offline inventory. Malformed style/ancestry/clip must fail.
        names=[];states=[];references=[]
        def node():return dict(m_ChildIndices=[],m_ClipID=4294967295,m_BlendType=0,
            m_BlendEventID=4294967295,m_BlendEventYID=4294967295,
            m_Blend1dData={'data':{'m_ChildThresholdArray':[]}},
            m_Blend2dData={'data':{'m_ChildPositionArray':[]}})
        names.extend([(1000,'MoveStyleH'),(1001,'MoveStyleV')])
        groups=[('Base Layer.GroundLocomotion.Move',
            [('AttackIdle',(0,1,41,42)),('SwordWF',(0,1,41,61,63)),('SwordRF',(0,1,41,43,45)),('SwordSprint',(0,1,41,74))])]
        for phase in ('Jump','Fall','Land'):
            groups.append(('Base Layer.GroundLocomotion.Jump.'+phase,
                [('Sword'+phase,(0,1,3)),('SwordRun'+phase,(0,17,19))]))
        for state_id,(path,leaves) in enumerate(groups):
            names.append((state_id,path));nodes=[{'data':node()} for _ in range(75)]
            for name,indices in leaves:
                for parent,child in zip(indices,indices[1:]):
                    n=nodes[parent]['data']
                    if child not in n['m_ChildIndices']:n['m_ChildIndices'].append(child)
                # Match the style point only at each branch preceding the leaf.
                style=nodes[indices[1]]['data'];style['m_BlendEventID']=1000;style['m_BlendEventYID']=1001
                style['m_Blend2dData']['data']['m_ChildPositionArray']=[dict(x=.94,y=.342) for _ in style['m_ChildIndices']]
                leaf=nodes[indices[-1]]['data'];leaf['m_ClipID']=len(references)
                references.append(dict(m_FileID=0,m_PathID=inventory.SWORD_CLIPS[name]))
            states.append({'data':dict(m_FullPathID=state_id,m_Speed=1,m_BlendTreeConstantArray=[{'data':{'m_NodeArray':nodes}}])})
        controller=dict(m_TOS=names,m_AnimationClips=references,m_Controller={'m_StateMachineArray':[{'data':{'m_StateConstantArray':states}}]})
        result=inventory.sword_states(controller)
        self.assertEqual(set(result),set(inventory.SWORD_CLIPS))
        self.assertEqual(result['AttackIdle']['node_path'],[0,1,41,42])
        for kind in ('clip','point','ancestry','external'):
            c=copy.deepcopy(controller);nodes=c['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'][0]['data']['m_BlendTreeConstantArray'][0]['data']['m_NodeArray']
            if kind=='clip':c['m_AnimationClips'][0]['m_PathID']=684
            if kind=='external':c['m_AnimationClips'][0]['m_FileID']=1
            if kind=='point':nodes[1]['data']['m_Blend2dData']['data']['m_ChildPositionArray'][0]['x']=0
            if kind=='ancestry':nodes[41]['data']['m_ChildIndices'].remove(42)
            with self.subTest(kind=kind),self.assertRaises(ValueError):inventory.sword_states(c)

    def test_directional_branch_parameters_points_and_leaf_identity(self):
        def node():return dict(m_ChildIndices=[],m_ClipID=4294967295,m_BlendType=0,
            m_BlendEventID=4294967295,m_BlendEventYID=4294967295,
            m_Blend1dData={'data':{'m_ChildThresholdArray':[]}},
            m_Blend2dData={'data':{'m_ChildPositionArray':[]}})
        names=[(0,'Base Layer.GroundLocomotion.Move')]+list(enumerate(
            ['BeatWeight','MoveStyleH','MoveStyleV','RightSpeed','ForwardSpeed','FirstPerson'],100))
        ids={v:k for k,v in names};nodes=[{'data':node()} for _ in range(37)];refs=[]
        for name,(path,point,variant) in inventory.DIRECTIONAL_BRANCHES.items():
            expected=[('BeatWeight',None,0),('MoveStyleH','MoveStyleV',(0,0)),('RightSpeed','ForwardSpeed',point)]
            if variant:expected.append((variant[0],None,variant[1]))
            for (a,b),(param,param_y,value) in zip(zip(path,path[1:]),expected):
                n=nodes[a]['data'];n['m_BlendEventID']=ids[param];n['m_BlendEventYID']=ids.get(param_y,4294967295)
                if b not in n['m_ChildIndices']:
                    n['m_ChildIndices'].append(b)
                    if isinstance(value,tuple):n['m_Blend2dData']['data']['m_ChildPositionArray'].append(dict(zip(('x','y'),value)))
                    else:n['m_Blend1dData']['data']['m_ChildThresholdArray'].append(value)
            nodes[path[-1]]['data']['m_ClipID']=len(refs);refs.append(dict(m_FileID=0,m_PathID=inventory.DIRECTIONAL_CLIPS[name]))
        state=dict(m_FullPathID=0,m_Speed=1,m_BlendTreeConstantArray=[{'data':{'m_NodeArray':nodes}}])
        c=dict(m_TOS=names,m_AnimationClips=refs,m_Controller={'m_StateMachineArray':[{'data':{'m_StateConstantArray':[{'data':state}]}}]})
        result=inventory.directional_states(c)
        self.assertEqual(set(result),set(inventory.DIRECTIONAL_CLIPS))
        self.assertEqual(result['NormalWR']['branches'][-1]['parameter'],'RightSpeed')
        self.assertEqual(result['NormalRL']['branches'][-1]['threshold'],1)
        self.assertEqual(result['NormalRunTurnLeft']['branches'][-1]['threshold'],0)
        for kind in ('parameter','threshold','point','leaf','external','ancestry'):
            bad=copy.deepcopy(c);ns=bad['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'][0]['data']['m_BlendTreeConstantArray'][0]['data']['m_NodeArray']
            if kind=='parameter':ns[30]['data']['m_BlendEventID']=ids['FirstPerson']
            if kind=='threshold':ns[34]['data']['m_Blend1dData']['data']['m_ChildThresholdArray'][0]=.5
            if kind=='point':ns[2]['data']['m_Blend2dData']['data']['m_ChildPositionArray'][0]['y']=5
            if kind=='leaf':bad['m_AnimationClips'][0]['m_PathID']=531
            if kind=='external':bad['m_AnimationClips'][0]['m_FileID']=1
            if kind=='ancestry':ns[2]['data']['m_ChildIndices'].remove(33)
            with self.subTest(kind=kind),self.assertRaises(ValueError):inventory.directional_states(bad)

    def test_sword_backward_source_branches_reject_normal_substitution(self):
        names=list(enumerate(['Base Layer.GroundLocomotion.Move','BeatWeight','MoveStyleH','MoveStyleV','RightSpeed','ForwardSpeed']))
        ids={name:i for i,name in names}
        def node():return dict(m_ChildIndices=[],m_ClipID=4294967295,m_BlendType=0,
            m_BlendEventID=4294967295,m_BlendEventYID=4294967295,
            m_Blend1dData={'data':{'m_ChildThresholdArray':[]}},
            m_Blend2dData={'data':{'m_ChildPositionArray':[]}})
        nodes=[{'data':node()} for _ in range(69)]
        for index,children,param,param_y,values in [(0,[1],'BeatWeight',None,[0]),
                (1,[41],'MoveStyleH','MoveStyleV',[(.94,.342)]),
                (41,[68,53],'RightSpeed','ForwardSpeed',[(0,-2),(0,-5)])]:
            n=nodes[index]['data'];n['m_ChildIndices']=children;n['m_BlendEventID']=ids[param];n['m_BlendEventYID']=ids.get(param_y,4294967295)
            if param_y:n['m_Blend2dData']['data']['m_ChildPositionArray']=[dict(x=x,y=y) for x,y in values]
            else:n['m_Blend1dData']['data']['m_ChildThresholdArray']=values
        nodes[68]['data']['m_ClipID']=0;nodes[53]['data']['m_ClipID']=1
        state=dict(m_FullPathID=0,m_Speed=1,m_BlendTreeConstantArray=[{'data':{'m_NodeArray':nodes}}])
        c=dict(m_TOS=names,m_AnimationClips=[dict(m_FileID=0,m_PathID=p) for p in (441,708)],
            m_Controller={'m_StateMachineArray':[{'data':{'m_StateConstantArray':[{'data':state}]}}]})
        result=inventory.sword_backward_states(c)
        self.assertEqual(result['SwordWB']['node_path'],[0,1,41,68]);self.assertEqual(result['SwordRB']['node_path'],[0,1,41,53])
        for change in ('normal','external','parameter','style','direction','threshold','ancestry','speed','duplicate'):
            bad=copy.deepcopy(c);st=bad['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'][0]['data'];ns=st['m_BlendTreeConstantArray'][0]['data']['m_NodeArray']
            if change=='normal':bad['m_AnimationClips'][0]['m_PathID']=547
            if change=='external':bad['m_AnimationClips'][1]['m_FileID']=1
            if change=='parameter':ns[41]['data']['m_BlendEventID']=ids['ForwardSpeed']
            if change=='style':ns[1]['data']['m_Blend2dData']['data']['m_ChildPositionArray'][0]['x']=0
            if change=='direction':ns[41]['data']['m_Blend2dData']['data']['m_ChildPositionArray'][0]['y']=2
            if change=='threshold':ns[0]['data']['m_Blend1dData']['data']['m_ChildThresholdArray'][0]=1
            if change=='ancestry':ns[41]['data']['m_ChildIndices'].remove(68)
            if change=='speed':st['m_Speed']=1.37
            if change=='duplicate':bad['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'].append(copy.deepcopy({'data':st}))
            with self.subTest(change=change),self.assertRaises(ValueError):inventory.sword_backward_states(bad)

    def test_sword_backward_preset_separate_exact_ids_clock_and_build_only(self):
        self.data['genders']['female']['clips']=[dict(name=n,path_id=p,length=.5,frame_rate=30,loop=True,humanoid_bindings=127) for n,p in inventory.CLIPS.items()]
        self.save();selected=tuple(inventory.SWORD_BACKWARD_CLIPS);p=self.read(selected)
        self.assertEqual(selected,('SwordWB','SwordRB'));self.assertEqual([c['path_id'] for c in p['clips']],[441,708])
        self.assertEqual(p['planned_samples'],32);self.assertEqual(p['spec'],'SwordWB|0.5|30|441|1;SwordRB|0.5|30|708|1')
        self.assertEqual(len(inventory.SWORD_CLIPS),10);self.assertEqual(len(tool.DEFAULT_CLIPS),10)
        self.assertTrue(set(selected).isdisjoint(inventory.SWORD_CLIPS))
        args=self.args()[:-2]+['--preset','sword-backward']
        with mock.patch.object(tool,'build',return_value=self.setup_mock_launch()), \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/original/client'],{})), \
             mock.patch.object(tool.launch_reference,'prepare_settings') as settings, \
             mock.patch.object(tool.subprocess,'run') as launch:
            self.assertEqual(tool.main(args),0);settings.assert_not_called();launch.assert_not_called()
        before=copy.deepcopy(self.data)
        for key,value in [('path_id',547),('length',float('nan')),('frame_rate',0),('loop',1)]:
            self.data=copy.deepcopy(before);c=next(c for c in self.data['genders']['female']['clips'] if c['name']=='SwordWB');c[key]=value;self.save()
            with self.subTest(key=key),self.assertRaises(ValueError):self.read(selected)

    def test_directional_preset_build_only_and_opt_in_render_flag(self):
        self.data['genders']['female']['clips']=[dict(name=n,path_id=p,length=.5,frame_rate=30,loop=True,humanoid_bindings=127) for n,p in inventory.CLIPS.items()]
        self.save();args=self.args()[:-2]+['--preset','normal-directional']
        with mock.patch.object(tool,'build',return_value=self.setup_mock_launch()), \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/original/client'],{})) as command, \
             mock.patch.object(tool.launch_reference,'prepare_settings') as settings, \
             mock.patch.object(tool.subprocess,'run') as launch:
            self.assertEqual(tool.main(args),0);self.assertFalse(command.call_args.args[2])
            self.assertEqual(tool.main(args+['--single-threaded-rendering']),0);self.assertTrue(command.call_args.args[2])
            settings.assert_not_called();launch.assert_not_called()
        selection=tool.DEFAULT_CLIPS+tuple(inventory.DIRECTIONAL_CLIPS)
        self.assertEqual(len(self.read(selection)['clips']),20)
        self.assertEqual(selection[:10],tool.DEFAULT_CLIPS)

    def reaction_controller(self):
        def node():return dict(m_ChildIndices=[],m_ClipID=4294967295,m_BlendType=0,
            m_BlendEventID=4294967295,m_BlendEventYID=4294967295,
            m_Blend1dData={'data':{'m_ChildThresholdArray':[]}},
            m_Blend2dData={'data':{'m_ChildPositionArray':[]}})
        paths=['Base Layer.GroundLocomotion.Move','FullBody.BeHit.Blend Tree']
        names=list(enumerate(paths+['BeatWeight','RightSpeed','ForwardSpeed','SheildBlockF','MoveStyle']))
        ids={name:i for i,name in names};trees={path:[{'data':node()}] for path in paths};refs=[]
        for name,(clip,path,steps) in inventory.REACTION_BRANCHES.items():
            nodes=trees[path];index=0
            for parameter,parameter_y,value in steps:
                n=nodes[index]['data'];n['m_BlendEventID']=ids[parameter];n['m_BlendEventYID']=ids.get(parameter_y,4294967295)
                if isinstance(value,tuple):
                    n['m_BlendType']=2;values=n['m_Blend2dData']['data']['m_ChildPositionArray'];key=dict(zip(('x','y'),value))
                else:values=n['m_Blend1dData']['data']['m_ChildThresholdArray'];key=value
                if key not in values:
                    values.append(key);n['m_ChildIndices'].append(len(nodes));nodes.append({'data':node()})
                index=n['m_ChildIndices'][values.index(key)]
            nodes[index]['data']['m_ClipID']=len(refs);refs.append(dict(m_FileID=0,m_PathID=clip))
        states=[{'data':dict(m_FullPathID=ids[path],m_Speed=1,m_BlendTreeConstantArray=[{'data':{'m_NodeArray':trees[path]}}])} for path in paths]
        return dict(m_TOS=names,m_AnimationClips=refs,m_Controller={'m_StateMachineArray':[
            {'data':{'m_StateConstantArray':[states[0]]}},{'data':{'m_StateConstantArray':[states[1]]}}]}),ids

    def test_reaction_branches_reject_locomotion_and_shield_substitution(self):
        c,ids=self.reaction_controller();result=inventory.reaction_states(c)
        self.assertEqual(set(result),set(inventory.REACTION_CLIPS));self.assertEqual(len(result),19)
        self.assertEqual([b['parameter'] for b in result['SwordBeHit']['branches']],['MoveStyle'])
        self.assertEqual(result['SwordBeHit']['branches'][0]['value'],1)
        self.assertEqual([b['parameter'] for b in result['BlockRepulsed']['branches']],['BeatWeight','RightSpeed','SheildBlockF'])
        self.assertEqual(result['BackFast']['node_path'][:-1],result['BlockRepulsed']['node_path'][:-1])
        self.assertEqual(result['BackFast']['branches'][-1]['value'],0)
        # No reaction leaf aliases a Normal or Sword locomotion clip already in the catalog.
        earlier={v for k,v in inventory.CLIPS.items() if k not in inventory.REACTION_CLIPS}
        self.assertTrue(earlier.isdisjoint(inventory.REACTION_CLIPS.values()))
        def machine(bad,i):return bad['m_Controller']['m_StateMachineArray'][i]['data']['m_StateConstantArray']
        for change in ('normal','external','beat','direction','shield','style','parameter','speed','duplicate','missing'):
            bad=copy.deepcopy(c);move=machine(bad,0)[0]['data'];hit=machine(bad,1)[0]['data']
            nodes=move['m_BlendTreeConstantArray'][0]['data']['m_NodeArray'];beat=nodes[nodes[0]['data']['m_ChildIndices'][0]]['data']
            if change=='normal':bad['m_AnimationClips'][2]['m_PathID']=531
            if change=='external':bad['m_AnimationClips'][0]['m_FileID']=1
            if change=='beat':nodes[0]['data']['m_Blend1dData']['data']['m_ChildThresholdArray'][0]=0
            if change=='direction':beat['m_Blend2dData']['data']['m_ChildPositionArray'][0]['y']=-5
            if change=='shield':
                back=nodes[result['BackFast']['node_path'][-2]]['data'];back['m_Blend1dData']['data']['m_ChildThresholdArray'].reverse()
            if change=='style':hit['m_BlendTreeConstantArray'][0]['data']['m_NodeArray'][0]['data']['m_Blend1dData']['data']['m_ChildThresholdArray']=[1,0]
            if change=='parameter':beat['m_BlendEventID']=ids['ForwardSpeed']
            if change=='speed':hit['m_Speed']=1.2
            if change=='duplicate':machine(bad,1).append(copy.deepcopy(machine(bad,1)[0]))
            if change=='missing':machine(bad,1).clear()
            with self.subTest(change=change),self.assertRaises(ValueError):inventory.reaction_states(bad)

    def test_getup_states_cross_named_clips_and_triggers(self):
        names=[(i,n) for i,n in enumerate(['GetUpBack','GetUpFront','GetUpBackMirror','GetUpFrontMirror','ragdollInWater']+
                                         [row[1] for row in inventory.GETUP_STATES.values()])]
        ids={n:i for i,n in names};states=[];anys=[];refs=[]
        for index,(clip,(path_id,path,trigger)) in enumerate(inventory.GETUP_STATES.items()):
            node=dict(m_ChildIndices=[],m_ClipID=index);refs.append(dict(m_FileID=0,m_PathID=path_id))
            states.append({'data':dict(m_FullPathID=ids[path],m_Speed=1,m_Mirror=False,m_BlendTreeConstantArray=[{'data':{'m_NodeArray':[{'data':node}]}}])})
            anys.append({'data':dict(m_DestinationState=index,m_ConditionConstantArray=[{'data':{'m_EventID':ids[trigger]}},{'data':{'m_EventID':ids['ragdollInWater']}}])})
        c=dict(m_TOS=names,m_AnimationClips=refs,m_Controller={'m_StateMachineArray':[{'data':{'m_StateConstantArray':states,'m_AnyStateTransitionConstantArray':anys}}]})
        result=inventory.getup_states(c)
        self.assertEqual(result['GetUpBackMirror']['path'],'Base Layer.Ragdoll.GetupBack') # state and clip names cross
        self.assertEqual(result['GetUpBack']['entry_conditions'][0],'GetUpBackMirror')
        for change in ('uncross','trigger','speed','mirror','external','missing','second-entry'):
            bad=copy.deepcopy(c);m=bad['m_Controller']['m_StateMachineArray'][0]['data']
            if change=='uncross':bad['m_AnimationClips'][0]['m_PathID']=311
            if change=='trigger':m['m_AnyStateTransitionConstantArray'][0]['data']['m_ConditionConstantArray'][0]['data']['m_EventID']=ids['GetUpFront']
            if change=='speed':m['m_StateConstantArray'][1]['data']['m_Speed']=1.5
            if change=='mirror':m['m_StateConstantArray'][2]['data']['m_Mirror']=True
            if change=='external':bad['m_AnimationClips'][3]['m_FileID']=1
            if change=='missing':m['m_StateConstantArray'].pop()
            if change=='second-entry':m['m_AnyStateTransitionConstantArray'].append(copy.deepcopy(m['m_AnyStateTransitionConstantArray'][0]))
            with self.subTest(change=change),self.assertRaises(ValueError):inventory.getup_states(bad)

    def test_hit_reaction_preset_is_separate_bounded_and_build_only(self):
        lengths={'IdleBeHit':.9333336353302002,'SwordBeHit':.9333419799804688,'BlockRepulsed':1.0000152587890625}
        self.data['genders']['female']['clips']=[dict(name=n,path_id=p,length=lengths.get(n,1.),frame_rate=30,
            loop=n not in lengths or n=='BlockRepulsed',humanoid_bindings=127) for n,p in inventory.CLIPS.items()]
        self.save();selection=tool.REACTION_SELECTION;p=self.read(selection)
        self.assertEqual(selection[0],'NormalIdle');self.assertEqual(selection[1:20],tuple(inventory.REACTION_CLIPS))
        self.assertEqual(selection[20:],('GetUpBackMirror','GetUpFrontMirror','GetUpBack','GetUpFront','SprintSwordAttack'))
        self.assertEqual(len(p['clips']),25);self.assertLessEqual(len(selection),tool.MAX_CLIPS)
        self.assertEqual([c['path_id'] for c in p['clips'][1:3]],[139,157])
        self.assertLessEqual(p['planned_samples'],3600)
        self.assertIn('IdleBeHit|0.933333635|30|139|0',p['spec'])
        self.assertEqual([c['path_id'] for c in p['clips'][20:]],[322,326,311,317,401])
        self.assertTrue(set(inventory.REACTION_CLIPS).isdisjoint(tool.DEFAULT_CLIPS+tuple(inventory.DIRECTIONAL_CLIPS)+tuple(inventory.SWORD_CLIPS)))
        self.assertEqual(len(tool.DEFAULT_CLIPS),10)
        args=self.args()[:-2]+['--preset','hit-reaction']
        with mock.patch.object(tool,'build',return_value=self.setup_mock_launch()), \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/original/client'],{})), \
             mock.patch.object(tool.launch_reference,'prepare_settings') as settings, \
             mock.patch.object(tool.subprocess,'run') as launch:
            self.assertEqual(tool.main(args),0);settings.assert_not_called();launch.assert_not_called()
            self.assertNotEqual(tool.main(args+['--clip','NormalIdle']),0) # a preset excludes explicit clips

    def gather_plan(self):
        # Installed FemaleAnimator2 lengths for clips684/267/273/280.
        lengths={'NormalIdle':2.0000152587890625,'HerbDown':.5333347320556641,'Herb':.8000011444091797,'HerbUp':.6333332062}
        clips=[]
        for name,pid in inventory.CLIPS.items():
            clip=dict(name=name,path_id=pid,length=lengths.get(name,1.),frame_rate=30,loop=name=='NormalIdle',humanoid_bindings=127)
            if name in inventory.GATHER_CLIPS:clip['controller_state']=dict(path='FullBody.Gather.'+name,clip_index=405,speed=1,transitions=[])
            clips.append(clip)
        self.data['genders']['female']['clips']=clips
        self.data['genders']['male']={'avatar':'MaleAvatar','controller':'MaleAnimator','clips':copy.deepcopy(clips)}
        self.save()

    def test_gather_preset_is_female_exact_leaf_bounded_and_build_only(self):
        self.gather_plan()
        self.assertEqual(tool.GATHER_SELECTION,('NormalIdle','HerbDown','Herb','HerbUp'))
        self.assertEqual(inventory.GATHER_CLIPS,{'HerbDown':267,'Herb':273,'HerbUp':280})
        p=self.read(tool.GATHER_SELECTION)
        self.assertEqual([c['path_id'] for c in p['clips']],[684,267,273,280])
        # ceil(length*30)+1 per clip: 62+18+26+20.
        self.assertEqual(p['planned_samples'],126)
        self.assertIn('HerbDown|0.533334732|30|267|0',p['spec'])
        self.assertTrue(set(inventory.GATHER_CLIPS).isdisjoint(tool.DEFAULT_CLIPS+tool.REACTION_SELECTION+tuple(inventory.SWORD_CLIPS)+tuple(inventory.DIRECTIONAL_CLIPS)))
        self.assertEqual(len(tool.DEFAULT_CLIPS),10)
        with self.assertRaises(ValueError):tool.read_plan(self.plan,self.game,'male',tool.GATHER_SELECTION,30)
        original=copy.deepcopy(self.data)
        for change in ('id','path','speed','missing'):
            self.data=copy.deepcopy(original)
            herb=next(c for c in self.data['genders']['female']['clips'] if c['name']=='Herb')
            if change=='id':herb['path_id']=231
            elif change=='path':herb['controller_state']['path']='FullBody.Attack.Herb'
            elif change=='speed':herb['controller_state']['speed']=2
            else:del herb['controller_state']
            self.save()
            with self.subTest(change=change),self.assertRaises(ValueError):self.read(tool.GATHER_SELECTION)
        self.data=original;self.save()
        args=self.args()[:-2]+['--preset','gather']
        with mock.patch.object(tool,'build',return_value=self.setup_mock_launch()), \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/original/client'],{})), \
             mock.patch.object(tool.launch_reference,'prepare_settings') as settings, \
             mock.patch.object(tool.subprocess,'run') as launch:
            self.assertEqual(tool.main(args),0);settings.assert_not_called();launch.assert_not_called()
            self.assertNotEqual(tool.main(args+['--clip','NormalIdle']),0)
            self.assertNotEqual(tool.main(args+['--sex','male']),0)

    def test_gather_states_require_exact_direct_speed_one_leaves(self):
        names=[];states=[];refs=[]
        for i,name in enumerate(('HerbDown','Herb','HerbUp')):
            names.extend([(i,name),(i+10,'FullBody.Gather.'+name)])
            refs.append(dict(m_FileID=0,m_PathID=inventory.GATHER_CLIPS[name]))
            states.append({'data':dict(m_NameID=i,m_FullPathID=i+10,m_Speed=1,
                m_TransitionConstantArray=[],m_BlendTreeConstantArray=[{'data':{'m_NodeArray':[{'data':dict(m_ChildIndices=[],m_ClipID=i)}]}}])})
        c=dict(m_TOS=names,m_AnimationClips=refs,m_Controller={'m_StateMachineArray':[{'data':{'m_StateConstantArray':states}}]})
        result=inventory.gather_states(c)
        self.assertEqual({n:r['clip_index'] for n,r in result.items()},{'HerbDown':0,'Herb':1,'HerbUp':2})
        for kind in ('clip','path','speed','branch','missing'):
            bad=copy.deepcopy(c);state=bad['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'][1]['data']
            if kind=='clip':bad['m_AnimationClips'][1]['m_PathID']=267
            elif kind=='path':bad['m_TOS'][3]=(11,'FullBody.Fell.Herb')
            elif kind=='speed':state['m_Speed']=2
            elif kind=='branch':state['m_BlendTreeConstantArray'][0]['data']['m_NodeArray'][0]['data']['m_ChildIndices']=[1]
            else:bad['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'].pop()
            with self.subTest(kind=kind),self.assertRaises(ValueError):inventory.gather_states(bad)

    def gather_graph(self):
        names=[(1,'FullBody'),(2,'Gather'),(10,'FullBody.DoNothing'),(11,'FullBody.Gather.HerbDown'),
               (12,'FullBody.Gather.Herb'),(13,'FullBody.Gather.HerbUp'),(20,'FullBody.Gather')]
        def move(destination,exit_time,duration,conditions=(),has_exit=True):
            return {'data':dict(m_DestinationState=destination,m_ExitTime=exit_time,m_TransitionDuration=duration,
                m_HasExitTime=has_exit,m_HasFixedDuration=False,m_TransitionOffset=0.0,
                m_ConditionConstantArray=[{'data':dict(m_EventID=e,m_ConditionMode=1)} for e in conditions])}
        def state(path,motion,moves):
            return {'data':dict(m_NameID=path+100,m_FullPathID=path,m_Speed=1.0,m_CycleOffset=0.0,m_SpeedParamID=0,
                m_CycleOffsetParamID=0,m_MirrorParamID=0,m_Mirror=False,m_BlendTreeConstantArray=[{}] if motion else [],
                m_TransitionConstantArray=moves)}
        states=[state(10,False,[move(30000,.9,.1,(2,),False)]),state(11,True,[move(2,.53,.47)]),
                state(12,True,[move(3,.69,.31)]),state(13,True,[move(30001,.61,.39)])]
        selectors=[{'data':dict(m_IsEntry=True,m_FullPathID=20,m_TransitionConstantArray=[{'data':dict(m_Destination=1,m_ConditionConstantArray=[])}])},
                   {'data':dict(m_IsEntry=False,m_FullPathID=20,m_TransitionConstantArray=[{'data':dict(m_Destination=0,m_ConditionConstantArray=[])}])}]
        return dict(m_TOS=names,m_Controller=dict(m_LayerArray=[{'data':dict(m_Binding=1,m_StateMachineIndex=0)}],
            m_StateMachineArray=[{'data':dict(m_StateConstantArray=states,m_SelectorStateConstantArray=selectors)}]))

    def test_gather_controller_binds_entry_chain_exit_and_clock_flags(self):
        result=inventory.gather_controller(self.gather_graph())
        self.assertEqual(result['entry']['duration'],.1);self.assertFalse(result['idle']['motion'])
        self.assertEqual([t['destination'] for t in result['transitions']],
                         ['FullBody.Gather.Herb','FullBody.Gather.HerbUp','FullBody.DoNothing'])
        self.assertEqual(result['states'][1]['cycle_offset'],0.0);self.assertIsNone(result['states'][1]['speed_parameter'])
        def states(c):return c['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray']
        def selectors(c):return c['m_Controller']['m_StateMachineArray'][0]['data']['m_SelectorStateConstantArray']
        changes={'cycle offset':lambda c:states(c)[2]['data'].update(m_CycleOffset=.25),
                 'speed parameter':lambda c:states(c)[1]['data'].update(m_SpeedParamID=7),
                 'idle speed':lambda c:states(c)[0]['data'].update(m_Speed=2.0),
                 'fixed':lambda c:states(c)[3]['data']['m_TransitionConstantArray'][0]['data'].update(m_HasFixedDuration=True),
                 'entry exit time':lambda c:states(c)[0]['data']['m_TransitionConstantArray'][0]['data'].update(m_HasExitTime=True),
                 'exit selector':lambda c:selectors(c)[1]['data'].update(m_FullPathID=21),
                 'entry target':lambda c:selectors(c)[0]['data']['m_TransitionConstantArray'][0]['data'].update(m_Destination=2),
                 'chain':lambda c:states(c)[1]['data']['m_TransitionConstantArray'][0]['data'].update(m_DestinationState=3)}
        for name,change in changes.items():
            c=self.gather_graph();change(c)
            with self.subTest(name=name),self.assertRaises(ValueError):inventory.gather_controller(c)

    def test_managed_configuration_accepts_exactly_inventory_catalog(self):
        # Independent managed guard caused a real pre-capture failure when only
        # Python's catalog grew. Keep both entry-point contracts synchronized.
        source=(TOOLS/'ReferenceAnimatorBake.cs').read_text()
        field=re.search(r'\bAllowed\s*=\s*\{([^}]+)\}',source)
        self.assertIsNotNone(field)
        names=re.findall(r'"([A-Za-z0-9]+)"',field.group(1))
        self.assertEqual(len(names),len(set(names)))
        self.assertEqual(set(names),set(inventory.CLIPS))
        self.assertRegex(source,r'\bMaxClips\s*=\s*32\b')

    def test_running_first_stage_optional_selection_keeps_stationary_defaults(self):
        self.data['genders']['female']['clips'].append(dict(name='RunSwordAttack',path_id=408,
            length=1.8333396911621094,frame_rate=30,loop=False,humanoid_bindings=127))
        self.save();plan=self.read(('RunSwordAttack',))
        self.assertEqual(plan['planned_samples'],57)
        self.assertEqual(plan['clips'][0]['path_id'],408)
        self.assertNotIn('RunSwordAttack',tool.DEFAULT_CLIPS)
        self.assertNotIn('RunSwordAttack',inventory.SWORD_CLIPS)
        self.assertEqual(len(tool.DEFAULT_CLIPS),10)

    def test_running_first_stage_has_exact_direct_controller_leaf(self):
        state=dict(m_NameID=1,m_FullPathID=2,m_Speed=1,
            m_BlendTreeConstantArray=[{'data':{'m_NodeArray':[{'data':{'m_ChildIndices':[], 'm_ClipID':0}}]}}],
            m_TransitionConstantArray=[])
        controller=dict(m_TOS=[(1,'RunSwordAttack'),(2,'FullBody.Attack.RunSwordAttack')],
            m_AnimationClips=[dict(m_FileID=0,m_PathID=408)],
            m_Controller={'m_StateMachineArray':[{'data':{'m_StateConstantArray':[{'data':state}]}}]})
        result=inventory.running_attack_states(controller)['RunSwordAttack']
        self.assertEqual(result['clip_index'],0);self.assertEqual(result['speed'],1)
        for kind in ('clip','path','branch'):
            c=copy.deepcopy(controller)
            if kind=='clip':c['m_AnimationClips'][0]['m_PathID']=231
            elif kind=='path':c['m_TOS'][1]=(2,'FullBody.Attack.Attack6')
            else:c['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'][0]['data']['m_BlendTreeConstantArray'][0]['data']['m_NodeArray'][0]['data']['m_ChildIndices']=[1]
            with self.subTest(kind=kind),self.assertRaises(ValueError):inventory.running_attack_states(c)

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

    def test_loaded_source_entity_is_bounded_and_provenance_recorded(self):
        with mock.patch.object(tool,'build',return_value=self.setup_mock_launch()), \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['/original/client'],{})), \
             mock.patch.object(tool.launch_reference,'prepare_settings'), \
             mock.patch.object(tool.launch_reference,'prepare_unity_preferences'), \
             mock.patch.object(tool.subprocess,'run',return_value=SimpleNamespace(returncode=0)) as launch:
            self.assertEqual(tool.main(self.args()+['--source-entity','0','--launch']),1)
            self.assertEqual(tool.main(self.args()+['--source-entity','1000001','--launch']),1)
            launch.assert_not_called()
            self.assertEqual(tool.main(self.args()+['--source-entity','20010','--launch']),0)
            self.assertEqual(launch.call_args.kwargs['env']['PE_BAKE_SOURCE_ENTITY'],'20010')
        path=next((self.root/'profile/pose-captures').glob('*.launch.json'))
        self.assertEqual(json.loads(path.read_text())['plan']['source_entity_id'],20010)

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
