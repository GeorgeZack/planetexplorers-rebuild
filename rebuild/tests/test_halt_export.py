#!/usr/bin/env python3
"""Source/prefab qualification of the Halt stop effect; no game or GPU launch."""
from contextlib import redirect_stderr
import copy
import importlib.util
import io
import json
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_halt_effect as e
HAVE_UNITY=importlib.util.find_spec('UnityPy') is not None


class SourceGuards(unittest.TestCase):
    def test_active_sources(self):
        self.assertEqual(len(e.validate_sources(e.ROOT)),len(e.SOURCES))

    def mutate(self,path,old,new):
        with tempfile.TemporaryDirectory(prefix='halt-source-') as directory:
            root=Path(directory)
            for name in set(e.SOURCES)|{r[0] for r in e.REQUESTS}:
                target=root/name; target.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(e.ROOT/name,target)
            target=root/path; data=e.read_text(target)
            self.assertIn(old,data); target.write_text(data.replace(old,new,1))
            with self.assertRaises(ValueError): e.validate_sources(root)

    def test_source_mutations(self):
        # The sources are ISO-8859; read them tolerantly, as the exporter does.
        self.mutate(e.MOVE,'animEffect.StopMove(phyMotor.velocity);','animEffect.StartMove(phyMotor.velocity);')
        self.mutate(e.MOVE,'if(null != animEffect && null != phyMotor && animEffect.moveEffectRunning)','if(null != animEffect && animEffect.moveEffectRunning)')
        self.mutate(e.MOVE_EFFECT,'return Vector3.Angle(rFootToLFoot, moveDir) > 90f;','return Vector3.Angle(rFootToLFoot, moveDir) < 90f;')
        self.mutate(e.EFFECT,'m_RemainingTime -= deltaTime;','m_RemainingTime -= 2f * deltaTime;')
        self.mutate(e.EFFECT,'float crossFadeWeight = Mathf.Clamp01(elapseTime / m_FadeTime);','float crossFadeWeight = 1f;')
        self.mutate(e.CONTROL,'isActiveAndEnabled && m_Weight >0 && m_MoveEffect.isRunning','m_MoveEffect.isRunning')
        self.mutate(e.MODIFIER,'if (m_Weight <= 0f) return;','')
        self.mutate(e.MOVE,'motionMgr.DoAction(PEActionType.Halt);','motionMgr.DoAction(PEActionType.Rotate);')

    def test_relation(self):
        data=e.read_relation(e.DEFAULT_DATABASE)
        self.assertEqual(data['blocked_by_mask'][:2],['Move','Jump'])
        self.assertEqual(data['row']['EndImmediately'],'1,2,3')
        for action,field,value in [('50','InAir','0'),('50','EndImmediately','1'),('50','PauseAction','13'),('1','Halt','0'),('14','Halt','2')]:
            with self.subTest(action=action,field=field),tempfile.TemporaryDirectory(prefix='halt-db-') as directory:
                target=Path(directory)/'data'; shutil.copyfile(e.DEFAULT_DATABASE,target)
                with sqlite3.connect(target) as db:
                    self.assertEqual(db.execute(f'UPDATE ActionRelation SET {field}=? WHERE CAST(ActionType AS TEXT)=?',(value,action)).rowcount,1)
                with self.assertRaises(ValueError): e.read_relation(target)


@unittest.skipUnless(HAVE_UNITY,'optional UnityPy unavailable')
class PrefabGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.model=e.read_objects(e.ROOT/e.MODEL)

    def changed(self,change):
        model=copy.deepcopy(self.model); change(model)
        with self.assertRaises(ValueError): e.qualify(model)

    def test_binding(self):
        data=e.qualify(self.model)
        self.assertEqual(data['speed_to_weight'][1]['time'],4.5); self.assertEqual(data['speed_to_weight'][1]['value'],0.0)
        self.assertEqual([k['value'] for k in data['speed_to_time']],[0.20000004768371582,0.5000000596046448,0.8999999761581421])
        self.assertEqual(data['start_effect_links'],0)   # StartMove is an empty effect
        self.assertEqual([l['effector'] for l in data['effects']['right']['links']],[7,8,0,1,2,5,6])
        # Serialized step time/weight/direction are leftovers that DoEffect overwrites.
        self.assertNotEqual(data['effects']['right']['stale_serialized']['step_time'],data['speed_to_time'][-1]['value'])

    def test_mutations(self):
        move=lambda m:m[e.COMPONENT]['data']['m_MoveEffect']
        self.changed(lambda m:move(m)['m_SpeedToWeight']['m_Curve'][1].__setitem__('time',3.0))
        self.changed(lambda m:move(m)['m_SpeedToTime'].__setitem__('m_PostInfinity',1))
        self.changed(lambda m:move(m)['m_RFootStopEffect'].__setitem__('m_FadeTime',.1))
        self.changed(lambda m:move(m)['m_RFootStopEffect']['m_Effects'][1]['m_ForceDir']['m_Curve'][2].__setitem__('value',.1))
        self.changed(lambda m:move(m)['m_LFootStopEffect']['m_Effects'][0].__setitem__('m_Effector',8))
        self.changed(lambda m:move(m)['m_LFootStopEffect']['m_Effects'][0].__setitem__('m_PinWordPos',1))
        self.changed(lambda m:move(m)['m_LFootStopEffect']['m_Effects'].pop())
        self.changed(lambda m:m[e.COMPONENT]['data'].__setitem__('m_Weight',.5))
        self.changed(lambda m:m[e.COMPONENT]['data'].__setitem__('m_Enabled',0))
        self.changed(lambda m:m.__setitem__(123456,copy.deepcopy(m[e.COMPONENT])))

    def test_regeneration_check_and_portable_labels(self):
        outputs=e.build_outputs(); self.assertEqual(outputs,e.build_outputs())
        for name,data in outputs.items(): self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
        data=json.loads(outputs['pe_halt_source.json'])
        self.assertIn('Disconnected',data['scope']); self.assertIn('FinalIK',data['engine'])
        self.assertTrue(all(not Path(s['path']).is_absolute() for s in data['source_files']))
        with tempfile.TemporaryDirectory(prefix='halt-output-') as directory:
            with redirect_stderr(io.StringIO()): self.assertEqual(e.main(['--output',directory,'--check']),1)
            self.assertEqual(e.main(['--output',directory]),0); self.assertEqual(e.main(['--output',directory,'--check']),0)
            (Path(directory)/'pe_halt_source.h').write_text('stale')
            with redirect_stderr(io.StringIO()): self.assertEqual(e.main(['--output',directory,'--check']),1)


if __name__=='__main__': unittest.main()
