#!/usr/bin/env python3
"""Static qualification of the Whacked/Repulsed animation contract; no game launch."""
from contextlib import redirect_stderr
import copy
import importlib.util
import io
import json
from pathlib import Path
import shutil
import sqlite3
import struct
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import cil_disasm
import cil_evidence
import export_reaction_presentation as e
HAVE_UNITY=importlib.util.find_spec('UnityPy') is not None
INSTALLED=all((e.MANAGED/name).is_file() for name in e.ASSEMBLIES) and (e.GAME/'PE_Client_Data'/e.ASSET).is_file()
HEADER=struct.pack('<iqIiqI',0,0,1,1,e.SENDER_SCRIPT,0)
SCRIPT_NAMES={e.SENDER_SCRIPT:'AnimatorEventSender',e.FADER_SCRIPT:'AnimatorLayerWeightCtr'}


def text(value):
    data=value.encode(); return struct.pack('<I',len(data))+data+b'\0'*(-len(data)%4)


class SourceAndEvidence(unittest.TestCase):
    def test_active_sources(self):
        self.assertEqual(len(e.validate_sources(e.ROOT)),len(e.SOURCES))

    def mutate(self,path,old,new):
        with tempfile.TemporaryDirectory(prefix='reaction-source-') as directory:
            root=Path(directory)
            for name in e.SOURCES:
                target=root/name; target.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(e.ROOT/name,target)
            target=root/path; data=target.read_text(encoding='utf-8-sig',errors='replace')
            self.assertIn(old,data); target.write_text(data.replace(old,new,1))
            with self.assertRaises(ValueError): e.validate_sources(root)

    def test_source_mutations(self):
        self.mutate(e.BEAT_ACTION,'if(eventParam == "HitAnimEnd")','if(eventParam == "OnEndFullAnim")')
        self.mutate(e.BEAT_ACTION,'anim.SetTrigger("BeHit");','anim.SetTrigger("BeHit"); m_Timer = 1f;')
        self.mutate(e.SENDER,'SendMsg(animator, m_EixtEvent);','SendMsg(animator, m_EnterEvent);')
        self.mutate(e.FADER,'fadeoutSpeed = 5f','fadeoutSpeed = 1f')

    def test_shipped_readings(self):
        evidence,found=e.load_evidence()
        self.assertEqual(cil_evidence.strings(found['Pathea.Action_Whacked::EndImmediately']),['ResetFullBody','BeHit'])
        # Shipped and checked-in agree. The C# file is ISO-8859: read it tolerantly, never with a plain grep.
        source=(e.ROOT/'Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Move_Human.cs').read_text(encoding='utf-8-sig',errors='replace')
        self.assertIn('anim.SetFloat("MoveStyle", Mathf.Lerp(anim.GetFloat("MoveStyle"), (float)style, AnimLerpSpeed * Time.deltaTime));',source)
        self.assertIn('AnimLerpSpeed = 5f',source)
        self.assertEqual(cil_evidence.strings(found['Pathea.Motion_Move_Human::UpdateAnimState']).count('MoveStyle'),2)
        # Exit sends the exit event before any unsent timed events.
        calls=[m for _,m,_ in cil_disasm.decode(bytes.fromhex(found['AnimatorEventSender::OnStateExit']['raw_il_hex'])) if m=='call']
        self.assertEqual(len(calls),4)

    def staged(self,change):
        evidence=json.loads((e.ROOT/e.EVIDENCE).read_text())
        with tempfile.TemporaryDirectory(prefix='reaction-evidence-') as directory:
            root=Path(directory); target=root/e.EVIDENCE; target.parent.mkdir(parents=True)
            change(evidence); target.write_text(json.dumps(evidence))
            with self.assertRaises(ValueError): e.load_evidence(root)

    def test_changed_evidence_rejected(self):
        def method(data,name): return next(m for m in data['assemblies']['Assembly-CSharp.dll']['methods'] if m['name']==name)
        self.staged(lambda d:method(d,'OnAnimEvent')['strings'].update({k:'OnEndFullAnim' for k in method(d,'OnAnimEvent')['strings']}))
        self.staged(lambda d:method(d,'OnAnimEvent').pop('strings'))
        self.staged(lambda d:method(d,'get_ActionType').__setitem__('raw_il_hex','1c2a'))
        self.staged(lambda d:d['assemblies']['Assembly-CSharp.dll'].__setitem__('mvid','0'*36))
        self.staged(lambda d:d['assemblies']['Assembly-CSharp.dll']['methods'].pop())

    def test_string_literals_bound_to_installed_bytes(self):
        raw=bytearray(b'\0'*64); raw[10:10+1]=bytes([11]); raw[11:21]='BeHit'.encode('utf-16-le')
        self.assertEqual(cil_disasm.user_string(bytes(raw),0x7000000a,(0,64)),'BeHit')
        for token in (0x7000000a+60,0x0600000a,0x70000000):
            with self.subTest(token=token),self.assertRaises(ValueError): cil_disasm.user_string(bytes(raw),token,(0,64))
        with self.assertRaises(ValueError): cil_disasm.streams(b'MZ'+b'\0'*200)

    @unittest.skipUnless(INSTALLED,'installed game unavailable')
    def test_installed_bytes(self):
        evidence,_=e.load_evidence(); cil_evidence.qualify_installed(evidence,e.ASSEMBLIES,e.MANAGED)
        changed=copy.deepcopy(evidence)
        method=next(m for m in changed['assemblies']['Assembly-CSharp.dll']['methods'] if m['name']=='OnAnimEvent')
        method['strings']={k:'HitAnimEnd2' for k in method['strings']}
        with self.assertRaises(ValueError): cil_evidence.qualify_installed(changed,e.ASSEMBLIES,e.MANAGED)

    def test_relation(self):
        row=e.read_relation(e.DEFAULT_DATABASE)
        self.assertEqual(row['PauseAction'],'13,15,18,19,22,28,47'); self.assertEqual(row['Whacked'],'2'); self.assertEqual(row['InAir'],'0')
        for field,value in [('Whacked','0'),('InAir','2'),('PauseAction','13'),('EndAction','14'),('Name','Repulsed')]:
            with self.subTest(field=field),tempfile.TemporaryDirectory(prefix='reaction-db-') as directory:
                target=Path(directory)/'data'; shutil.copyfile(e.DEFAULT_DATABASE,target)
                with sqlite3.connect(target) as db:
                    self.assertEqual(db.execute(f'UPDATE ActionRelation SET {field}=? WHERE CAST(ActionType AS TEXT)=?',(value,'5')).rowcount,1)
                with self.assertRaises(ValueError): e.read_relation(target)

    def test_behaviour_decoding(self):
        hit=(HEADER+text('')+text('HitAnimEnd')+struct.pack('<I',0)).hex()
        self.assertEqual(e.behaviour(hit,SCRIPT_NAMES),dict(script='AnimatorEventSender',enter_event='',exit_event='HitAnimEnd',timed_events=0))
        fade=(struct.pack('<iqIiqI',0,0,1,1,e.FADER_SCRIPT,0)+struct.pack('<f',5.0)).hex()
        self.assertEqual(e.behaviour(fade,SCRIPT_NAMES)['fadeout_speed'],5.0)
        bad=[(HEADER+text('')+text('HitAnimEnd')+struct.pack('<I',1)).hex(),            # timed events present
             (HEADER+text('')+text('HitAnimEnd')).hex(),                                 # truncated
             (struct.pack('<iqIiqI',0,0,0,1,e.SENDER_SCRIPT,0)+text('')+text('x')+struct.pack('<I',0)).hex(), # disabled
             (struct.pack('<iqIiqI',0,0,1,1,999,0)+text('')+text('x')+struct.pack('<I',0)).hex(),             # other script
             (HEADER+struct.pack('<I',400)+b'abcd').hex(), fade+'00']
        for index,raw in enumerate(bad):
            with self.subTest(index=index),self.assertRaises(ValueError): e.behaviour(raw,SCRIPT_NAMES)


@unittest.skipUnless(HAVE_UNITY and INSTALLED,'optional UnityPy or installed game unavailable')
class ControllerGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.installed=e.read_installed()

    def changed(self,change):
        data=copy.deepcopy(self.installed); change(data)
        with self.assertRaises(ValueError): e.qualify_controller(data)

    def graph(self,data): return data['controller']['m_Controller']
    def full_body(self,data): return self.graph(data)['m_StateMachineArray'][1]['data']
    def state(self,data,machine,name):
        names=dict(data['controller']['m_TOS'])
        return next(s['data'] for s in self.graph(data)['m_StateMachineArray'][machine]['data']['m_StateConstantArray']
                    if names.get(s['data']['m_FullPathID'])==name)

    def test_contract(self):
        data=e.qualify_controller(self.installed); hit=data['be_hit']
        self.assertEqual([l['path_id'] for l in hit['leaves']][:2],[139,157])
        self.assertEqual(hit['exit']['exit_time'],0.8999999761581421); self.assertEqual(hit['exit']['duration'],0.10000000149011612)
        self.assertEqual(hit['behaviours'][0]['exit_event'],'HitAnimEnd')
        self.assertEqual(len(data['repulsed']['ground']['leaves']),17); self.assertEqual(len(data['repulsed']['water']['leaves']),5)
        blocked=[l for l in data['repulsed']['ground']['leaves'] if 'shield_block_clip' in l]
        self.assertEqual([(l['right_speed'],l['forward_speed'],l['shield_block_clip']['path_id']) for l in blocked],[(0.0,-5.0,691),(0.0,-3.0,691)])
        self.assertEqual(data['controller']['male_clip_overrides'],{'684':688,'210':110})
        self.assertTrue(all(c['events']==0 for c in data['clips']))  # the end signal is not a clip event

    def test_hit_state_mutations(self):
        hit=lambda d:self.state(d,1,e.BE_HIT)
        self.changed(lambda d:hit(d)['m_TransitionConstantArray'][0]['data'].__setitem__('m_HasExitTime',False))
        self.changed(lambda d:hit(d)['m_TransitionConstantArray'][0]['data'].__setitem__('m_HasFixedDuration',True))
        self.changed(lambda d:hit(d)['m_TransitionConstantArray'].append(copy.deepcopy(hit(d)['m_TransitionConstantArray'][0])))
        self.changed(lambda d:hit(d).__setitem__('m_Speed',1.5))
        self.changed(lambda d:hit(d).__setitem__('m_Loop',True))
        nodes=lambda d:hit(d)['m_BlendTreeConstantArray'][0]['data']['m_NodeArray']
        self.changed(lambda d:nodes(d)[0]['data']['m_Blend1dData']['data']['m_ChildThresholdArray'].__setitem__(1,1.5))
        self.changed(lambda d:nodes(d)[2]['data'].__setitem__('m_ClipID',nodes(d)[1]['data']['m_ClipID']))
        self.changed(lambda d:nodes(d)[2]['data'].__setitem__('m_Mirror',True))

    def test_entry_layer_and_behaviour_mutations(self):
        any_state=lambda d:self.full_body(d)['m_AnyStateTransitionConstantArray']
        def be_hit(d):
            names=dict(d['controller']['m_TOS'])
            return next(t['data'] for t in any_state(d) if [names.get(c['data']['m_EventID']) for c in t['data']['m_ConditionConstantArray']]==['BeHit'])
        self.changed(lambda d:be_hit(d).__setitem__('m_TransitionDuration',.25))
        self.changed(lambda d:be_hit(d).__setitem__('m_CanTransitionToSelf',False))
        self.changed(lambda d:any_state(d).append({'data':copy.deepcopy(be_hit(d))}))
        layer=lambda d:self.graph(d)['m_LayerArray'][1]['data']
        self.changed(lambda d:layer(d).__setitem__('m_DefaultWeight',0.0))
        self.changed(lambda d:layer(d)['m_BodyMask'].__setitem__('word0',1))
        self.changed(lambda d:layer(d).__setitem__('(int&)m_LayerBlendingMode',1))
        sender=next(k for k,v in self.installed['behaviours'].items() if '486974416e696d456e64' in v)
        self.changed(lambda d:d['behaviours'].__setitem__(sender,d['behaviours'][sender].replace('486974416e696d456e64','4f6e456e6446756c6c41')))
        self.changed(lambda d:d['scripts'].__setitem__(e.SENDER_SCRIPT,'AnimatorLayerWeightCtr'))

    def test_repulsed_branch_and_clip_mutations(self):
        nodes=lambda d:self.state(d,0,'Base Layer.GroundLocomotion.Move')['m_BlendTreeConstantArray'][0]['data']['m_NodeArray']
        self.changed(lambda d:nodes(d)[0]['data']['m_ChildIndices'].__setitem__(1,1))
        self.changed(lambda d:nodes(d)[193]['data'].__setitem__('m_BlendType',1))
        self.changed(lambda d:nodes(d)[193]['data']['m_Blend2dData']['data']['m_ChildPositionArray'][5].__setitem__('y',-4.0))
        self.changed(lambda d:nodes(d)[199]['data']['m_Blend1dData']['data']['m_ChildThresholdArray'].reverse())
        self.changed(lambda d:nodes(d)[201]['data'].__setitem__('m_ClipID',nodes(d)[200]['data']['m_ClipID']))
        self.changed(lambda d:d['clips'][139].__setitem__('events',1))
        self.changed(lambda d:d['clips'][235].__setitem__('mirror',False))
        self.changed(lambda d:d['clips'][691].__setitem__('loop',False))
        self.changed(lambda d:d['overridden'].pop(210))
        self.changed(lambda d:d['overridden'].__setitem__(139,140))

    def test_regeneration_check_and_portable_labels(self):
        outputs=e.build_outputs(); self.assertEqual(outputs,e.build_outputs())
        for name,data in outputs.items(): self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
        data=json.loads(outputs['pe_reaction_presentation_source.json'])
        self.assertIn('Disconnected',data['scope']); self.assertIn('engine bake',data['engine'])
        self.assertTrue(all(not Path(s['path']).is_absolute() for s in data['source_files']))
        for blob in (outputs['pe_reaction_presentation_source.json'].decode(),(e.ROOT/e.EVIDENCE).read_text()):
            self.assertNotIn(str(Path.home()),blob)
        with tempfile.TemporaryDirectory(prefix='reaction-output-') as directory:
            with redirect_stderr(io.StringIO()): self.assertEqual(e.main(['--output',directory,'--check']),1)
            self.assertEqual(e.main(['--output',directory]),0); self.assertEqual(e.main(['--output',directory,'--check']),0)
            (Path(directory)/'pe_reaction_presentation_source.h').write_text('stale')
            with redirect_stderr(io.StringIO()): self.assertEqual(e.main(['--output',directory,'--check']),1)

    def test_unqualified_assets_rejected(self):
        with tempfile.TemporaryDirectory(prefix='reaction-game-') as directory:
            data=Path(directory)/'PE_Client_Data'; data.mkdir()
            for name in (e.ASSET,e.SCRIPTS): (data/name).write_bytes(b'unsupported')
            with self.assertRaises(ValueError): e.read_installed(Path(directory))


if __name__=='__main__': unittest.main()
