"""Source and staged generic pounce qualification; never starts a game."""
import copy
from contextlib import closing
import json
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_hare_pounce as e
import export_hare_pounce_assets as asset
from export_reference_actor import CLIPS,CONTROLLER_ID
class Obj:
    def __init__(self,t):self.tree=t
    def read_typetree(self):return self.tree

def objects():
    state={'m_FullPathID':e.STATE_ID,'m_Loop':False,'m_Speed':1,'m_BlendTreeConstantArray':[{'data':{'m_NodeArray':[{'data':{'m_ChildIndices':[],'m_ClipID':0}}]}}]}
    c={'m_TOS':[(e.STATE_ID,'Base Layer.Attack.attack5')],'m_Controller':{'m_StateMachineArray':[{'data':{'m_StateConstantArray':[{'data':state}]}}]},'m_AnimationClips':[{'m_PathID':e.CLIP_ID}],'m_StateMachineBehaviours':[{'m_PathID':id} for id in (1,2,3)],'m_StateMachineBehaviourVectorDescription':{'m_StateMachineBehaviourRanges':[({'m_StateID':e.STATE_ID,'m_LayerIndex':0},{'m_StartIndex':0,'m_Count':3})],'m_StateMachineBehaviourIndices':[0,1,2]}}
    clip={'m_Name':'attack5_pouncefront','m_Events':[],'m_MuscleClip':{'m_LoopTime':False,'m_StartTime':0,'m_StopTime':0.9666671752929688,'m_Clip':{'data':{'m_DenseClip':{'m_FrameCount':31,'m_CurveCount':302,'m_SampleRate':30},'m_StreamedClip':{'curveCount':0}}}}}
    rows={CONTROLLER_ID:c,e.CLIP_ID:clip,1:{'m_Script':{'m_PathID':11}},11:{'m_ClassName':'PEAnimatorRunning'},2:{'m_Script':{'m_PathID':12},'startTime':e.f32(.575),'endTime':e.f32(.802),'bones':['Bip01 L Forearm','Bip01 R Forearm']},12:{'m_ClassName':'PEAnimatorAttackTrigger'},3:{'m_Script':{'m_PathID':13},'triggers':[{'time':e.f32(.32),'effects':[{'effect':1,'data':'850'}]},{'time':e.f32(.66),'effects':[{'effect':1,'data':'891'}]}]},13:{'m_ClassName':'PEAnimatorTrigger'}}
    return {k:Obj(v) for k,v in rows.items()}
class PounceSourceTests(unittest.TestCase):
    def test_regeneration(self):
        for name,data in e.build_outputs().items():self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
    def test_source_active_action_and_skill(self):
        c=e.source_contract();self.assertEqual(c['xml']['startTime'],'0.267');self.assertEqual(c['xml']['endTime'],'0.6');self.assertEqual(c['skill']['_effPrepare'],'0');self.assertEqual(c['skill']['_timeOfMain'],'.967');self.assertEqual(c['trigger']['_cond'],'colinfo')
    def test_changed_skill_rejected(self):
        for field,value in (('_effPrepare','105'),('_cond','loopcnt,2'),('_timeOfMain','.6'),('_triggerEvents','10100'),('_coolingTime','1')):
            with tempfile.TemporaryDirectory() as tmp:
                path=Path(tmp)/'db';shutil.copyfile(e.DEFAULT_DATABASE,path)
                with closing(sqlite3.connect(path)) as db:
                    db.execute(f'UPDATE skMain SET {field}=? WHERE _id=?',(value,'30100053'));db.commit()
                with self.assertRaisesRegex(ValueError,'skill contract'):e.build_outputs(database=path)
    def test_changed_action_or_xml_rejected(self):
        for rel,old,new in ((e.BT,b'Time.time - m_Data.m_StartTime > m_Data.endTime',b'Time.time - m_Data.m_StartTime >= m_Data.endTime'),(e.XML,b'startTime="0.267"',b'startTime="0.3"')):
            with tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp)
                for name in (e.BT,e.XML,*e.PINS):
                    path=root/name;path.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(e.ROOT/name,path)
                path=root/rel;raw=path.read_bytes();self.assertIn(old,raw);path.write_bytes(raw.replace(old,new))
                with self.assertRaisesRegex(ValueError,'source changed|XML contract'):e.build_outputs(root=root)
    def test_controller_binding(self):
        c=e.installed_provenance(objects());self.assertIsNone(c['prepare_effect']);self.assertEqual(c['clip_path_id'],e.CLIP_ID);self.assertEqual(c['skill']['_id'],'30100053');self.assertTrue(c['attack_windows'][0]['end_inclusive'])
        self.assertEqual(len(CLIPS),8);self.assertEqual(CLIPS[-1],('walk','move_walk'))
    def test_changed_controller_clip_and_window_rejected(self):
        for mutate in (lambda o:o[e.CLIP_ID].tree.update(m_Name='attack4_normalattack1'),lambda o:o[e.CLIP_ID].tree['m_MuscleClip'].update(m_LoopTime=True),lambda o:o[e.CLIP_ID].tree['m_MuscleClip'].update(m_StopTime=.967),lambda o:o[2].tree.update(startTime=.575),lambda o:o[2].tree.update(bones=['Bip01']),lambda o:o[3].tree['triggers'][0]['effects'][0].update(data='999')):
            o=objects();mutate(o)
            with self.assertRaisesRegex(ValueError,'Pounce'):e.installed_provenance(o)
    def staged(self):
        root=e.ROOT/'rebuild/build/reference-assets';out=root/'hare-pounce-staged-20261007'
        if not (out/'hare.peac').exists():self.skipTest('Optional isolated staged pounce asset absent')
        return (root/'hare.peac').read_bytes(),(out/'hare.peac').read_bytes(),json.loads((out/'hare-manifest.json').read_text())
    def test_staged_prefix_and_metadata(self):
        old,new,report=self.staged();proof=asset.validate_prefix(old,new);self.assertEqual(proof['joints'],48);self.assertEqual(proof['palettes'],1488);asset.validate_report(report,new)
    def test_staged_corruption_rejected(self):
        old,new,report=self.staged();bad=bytearray(new);bad[30]^=1
        with self.assertRaises(ValueError):asset.validate_prefix(old,bad)
        with self.assertRaises(ValueError):asset.validate_prefix(old,new[:-1])
        bad=copy.deepcopy(report);bad['clips'][-1]['combat']['attack_windows'][0]['end_normalized']=.9
        with self.assertRaisesRegex(ValueError,'collision metadata'):asset.validate_report(bad,new)
        bad=bytearray(new);bad[-10]^=1
        with self.assertRaisesRegex(ValueError,'actor hash'):asset.validate_report(report,bad)
if __name__=='__main__':unittest.main()
