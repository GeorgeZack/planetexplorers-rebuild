#!/usr/bin/env python3
"""Source/prefab qualification of admitted Repulsed; no game or GPU launch."""
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
import export_repulsed_action as e
HAVE_UNITY=importlib.util.find_spec('UnityPy') is not None


class SourceGuards(unittest.TestCase):
    def test_active_methods(self):
        self.assertEqual(len(e.validate_sources(e.ROOT)),len(e.SOURCES))

    def mutate(self,path,old,new):
        with tempfile.TemporaryDirectory(prefix='repulsed-source-') as directory:
            root=Path(directory)
            for name in e.SOURCES:
                target=root/name; target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(e.ROOT/name,target)
            target=root/path; data=target.read_text(encoding='utf-8-sig',errors='replace')
            self.assertIn(old,data); target.write_text(data.replace(old,new,1))
            with self.assertRaises(ValueError): e.validate_sources(root)

    def test_previous_direction_shield_order(self):
        self.mutate(e.BEAT_ACTION,'Vector3.Angle(-entity.forward, m_MoveDir)',
                    'Vector3.Angle(-entity.forward, PEActionParamVVF.param.vec2)')

    def test_halt_strict_boundary(self):
        self.mutate(e.BEAT_ACTION,'m_ElapseTime > m_MoveStopTime','m_ElapseTime >= m_MoveStopTime')

    def test_wentfly_strict_boundary(self):
        self.mutate(e.BEAT_ACTION,'m_ElapseTime > m_WentflyTime','m_ElapseTime >= m_WentflyTime')

    def test_repeat_uses_noop_reset(self):
        self.mutate(e.MANAGER,'m_ActionDic[(int)type].ResetAction(para);',
                    'm_ActionDic[(int)type].DoAction(para);')

    def test_live_param_binding(self):
        self.mutate(e.reaction.BEAT,'m_Repulsed.m_Param = m_Param;',
                    'm_Repulsed.m_Param = null;')

    def test_relations(self):
        row=e.read_relation(e.DEFAULT_DATABASE)
        self.assertEqual(row['EndAction'],'0')
        self.assertEqual(row['PauseAction'],'13,15,18,19,22,24')
        self.assertEqual(row['Whacked'],'0')

    @unittest.skipUnless(e.DEFAULT_UNITY.is_file(),'installed UnityEngine unavailable')
    def test_static_unity_zero_angle(self):
        data=e.qualify_unity()
        self.assertEqual(data['zero_direction_angle_degrees'],90)
        self.assertEqual(data['sha256'],e.UNITY_SHA256)

    def test_unqualified_engine_rejected(self):
        with tempfile.TemporaryDirectory(prefix='repulsed-engine-') as directory:
            path=Path(directory)/'UnityEngine.dll'; path.write_bytes(b'unsupported')
            with self.assertRaises(ValueError): e.qualify_unity(path)

    def test_changed_relation_rejected(self):
        for field,value in [('InAir','0'),('Whacked','2'),('PauseAction','0'),('EndAction','14')]:
            with self.subTest(field=field),tempfile.TemporaryDirectory(prefix='repulsed-db-') as directory:
                target=Path(directory)/'data'; shutil.copyfile(e.DEFAULT_DATABASE,target)
                with sqlite3.connect(target) as db:
                    # Source spreadsheet cells are BLOBs; compare their text value.
                    result=db.execute(f'UPDATE ActionRelation SET {field}=? WHERE CAST(ActionType AS TEXT)=?',(value,'6'))
                    self.assertEqual(result.rowcount,1)
                with self.assertRaises(ValueError): e.read_relation(target)


@unittest.skipUnless(HAVE_UNITY,'optional UnityPy unavailable')
class PrefabGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model=e.reaction.read_prefab(e.ROOT/e.reaction.MODEL)
        cls.player=e.reaction.read_prefab(e.ROOT/e.reaction.PLAYER)

    def changed(self,change):
        model=copy.deepcopy(self.model); player=copy.deepcopy(self.player)
        change(model,player)
        with self.assertRaises(ValueError): e.qualify(model,player)

    def test_active_model_over_unused_entity(self):
        data=e.qualify(self.model,self.player)
        self.assertFalse(data['generic_motor_present'])
        self.assertEqual(data['human_motion_component'],11400024)
        self.assertEqual(data['wentfly_bone_name'],'Bip01 Spine2')
        for name in ('m_ForceToVelocity','m_ForceToMoveTime','m_ApplyMoveStopTime'):
            self.assertNotEqual(data['curves'][name],data['unused_entity_curves'][name])

    def test_curve_and_wrap_mutations(self):
        for name in e.CURVES:
            with self.subTest(field=name):
                self.changed(lambda m,p:m[11471154]['data'][name]['m_Curve'][0].__setitem__('value',0.0))
                self.changed(lambda m,p:m[11471154]['data'][name].__setitem__('m_PostInfinity',1))

    def test_force_bone_and_ancestry_mutations(self):
        self.changed(lambda m,p:m[11471154]['data'].__setitem__('m_ToWentflyForceScale',.5))
        self.changed(lambda m,p:m[11471154]['data']['m_ApplyWentflyBone'].__setitem__('m_PathID',0))
        self.changed(lambda m,p:m[400106]['data']['m_Father'].__setitem__('m_PathID',0))
        self.changed(lambda m,p:m[400106]['data']['m_Father'].__setitem__('m_PathID',400106))

    def test_human_owner_mutation(self):
        self.changed(lambda m,p:p[11400024]['data'].__setitem__('m_Enabled',0))

    def test_unexpected_motor_rejected(self):
        def add_motor(model,player):
            component=copy.deepcopy(player[11400024])
            component['script_guid']=e.reaction.guid(e.ROOT,e.MOTOR)
            player[123456]=component
        self.changed(add_motor)

    def test_regeneration_and_portable_labels(self):
        outputs=e.build_outputs(); self.assertEqual(outputs,e.build_outputs())
        for name,data in outputs.items():
            self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
        data=json.loads(outputs['pe_repulsed_source.json'])
        self.assertIn('Disconnected',data['scope'])
        self.assertTrue(all(not Path(s['path']).is_absolute() for s in data['source_files']))

    def test_check_missing_and_stale(self):
        with tempfile.TemporaryDirectory(prefix='repulsed-output-') as directory:
            with redirect_stderr(io.StringIO()): self.assertEqual(e.main(['--output',directory,'--check']),1)
            self.assertEqual(e.main(['--output',directory]),0)
            self.assertEqual(e.main(['--output',directory,'--check']),0)
            (Path(directory)/'pe_repulsed_source.h').write_text('stale')
            with redirect_stderr(io.StringIO()): self.assertEqual(e.main(['--output',directory,'--check']),1)


if __name__=='__main__': unittest.main()
