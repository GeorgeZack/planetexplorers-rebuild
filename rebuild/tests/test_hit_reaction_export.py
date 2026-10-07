#!/usr/bin/env python3
"""Offline source decisions and female model bindings; no graphics/game launch."""
from contextlib import redirect_stderr
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_hit_reaction as e
HAVE_UNITY=importlib.util.find_spec('UnityPy') is not None


class SourceGuards(unittest.TestCase):
    def test_active_source_chain(self):self.assertEqual(len(e.validate_sources(e.ROOT)),len(e.SOURCES))
    def mutate(self,path,old,new):
        with tempfile.TemporaryDirectory(prefix='hit-reaction-source-') as directory:
            root=Path(directory)
            for p in e.SOURCES:
                dst=root/p;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(e.ROOT/p,dst)
            dst=root/path;txt=dst.read_text(encoding='utf-8-sig',errors='replace');self.assertIn(old,txt)
            dst.write_text(txt.replace(old,new,1))
            with self.assertRaises(ValueError):e.validate_sources(root)
    def test_strict_threshold(self):self.mutate(e.BEAT,'if(forcePower <','if(forcePower <=')
    def test_air_multiplier_binding(self):self.mutate(e.BEAT,'m_Param.m_ThresholdScaleInAir : 1f','1f : 1f')
    def test_angle_force_before_selection(self):self.mutate(e.BEAT,'forcePower *= m_Param.m_AngleForceScale.Evaluate(angle);','forcePower = 0;')
    def test_knocked_dispatch_is_wentfly(self):self.mutate(e.BEAT,'m_MotionMgr.DoAction(PEActionType.Wentfly, paramVFNS);','m_MotionMgr.DoAction(PEActionType.Knocked, paramVFNS);')
    def test_force_permission_retained(self):self.mutate(e.ALIVE,'if(PETools.PEUtil.CanDamage (inst.Caster, inst.Target))','if(true)')
    def test_female_resource_binding(self):self.mutate(e.AVATAR,'Prefab/PlayerPrefab/FemaleModel','Prefab/PlayerPrefab/MaleModel')
    def test_current_database(self):
        player,shield,effect,events=e.read_rows(e.DEFAULT_DATABASE)
        self.assertEqual([player[k] for k in e.FIELDS],['250','400','750','1750'])
        self.assertEqual(shield['_mods'],e.SHIELD_MODS);self.assertEqual(effect['_spEff'],'True')
        self.assertEqual([(x['_id'],x['_force']) for x in events],[('10100','100'),('10400','400'),('10250','250')])
    def test_changed_database_rejected(self):
        import sqlite3
        for table,key,identity,field,value in [('initProp','id','1','ThresholdWhacked','251'),
                ('skBuff','_id','30200177','_mods','0'),('skTriggerEvent','_id','10400','_force','401'),
                ('skEffect','_id','57','_spEff','False')]:
            with self.subTest(table=table),tempfile.TemporaryDirectory(prefix='hit-reaction-db-') as directory:
                path=Path(directory)/'data';shutil.copyfile(e.DEFAULT_DATABASE,path)
                with sqlite3.connect(path) as db:db.execute(f'UPDATE {table} SET {field}=? WHERE {key}=?',(value,identity))
                with self.assertRaises(ValueError):e.read_rows(path)


@unittest.skipUnless(HAVE_UNITY,'optional UnityPy unavailable')
class PrefabGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model=e.read_prefab(e.ROOT/e.MODEL);cls.player=e.read_prefab(e.ROOT/e.PLAYER)
        cls.guids=dict(param=e.guid(e.ROOT,e.PARAM),view=e.guid(e.ROOT,e.VIEW),beat=e.guid(e.ROOT,e.BEAT))
    def changed(self,fn):
        model=copy.deepcopy(self.model);player=copy.deepcopy(self.player);fn(model,player)
        with self.assertRaises(ValueError):e.qualify(model,player,self.guids)
    def test_bound_prefab_not_class_defaults(self):
        d=e.qualify(self.model,self.player,self.guids)
        self.assertEqual(d['model_component'],11471154)
        self.assertEqual(d['curves']['m_AngleThresholdScale']['m_Curve'][1]['value'],.9850000739097595)
    def test_curve_mutation(self):self.changed(lambda m,p:m[11471154]['data']['m_AngleForceScale']['m_Curve'][1].__setitem__('value',1.))
    def test_tangent_mutation(self):self.changed(lambda m,p:m[11471154]['data']['m_AngleForceScale']['m_Curve'][1].__setitem__('outSlope',1.))
    def test_infinity_mutation(self):self.changed(lambda m,p:m[11471154]['data']['m_AngleForceScale'].__setitem__('m_PreInfinity',1))
    def test_air_mutation(self):self.changed(lambda m,p:m[11471154]['data'].__setitem__('m_ThresholdScaleInAir',1.))
    def test_disabled_component(self):self.changed(lambda m,p:p[11400038]['data'].__setitem__('m_Enabled',0))
    def test_view_binding(self):self.changed(lambda m,p:m[11424092]['data']['beatParam'].__setitem__('m_PathID',0))
    def test_wrong_script(self):self.changed(lambda m,p:m[11471154].__setitem__('script_guid','0'*32))
    def test_regeneration(self):
        before=[hashlib.sha256((e.ROOT/p).read_bytes()).hexdigest() for p in e.SOURCES]
        outputs=e.build_outputs();self.assertEqual(outputs,e.build_outputs())
        for name,data in outputs.items():self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
        self.assertEqual(before,[hashlib.sha256((e.ROOT/p).read_bytes()).hexdigest() for p in e.SOURCES])
        metadata=json.loads(outputs['pe_hit_reaction_source.json'])
        self.assertIn('Disconnected',metadata['scope']);self.assertIn('Caller supplies',metadata['threshold_owner'])
    def test_check_missing_and_stale(self):
        with tempfile.TemporaryDirectory(prefix='hit-reaction-output-') as directory:
            with redirect_stderr(io.StringIO()):self.assertEqual(e.main(['--output',directory,'--check']),1)
            self.assertEqual(e.main(['--output',directory]),0)
            self.assertEqual(e.main(['--output',directory,'--check']),0)
            (Path(directory)/'pe_hit_reaction_source.h').write_text('stale')
            with redirect_stderr(io.StringIO()):self.assertEqual(e.main(['--output',directory,'--check']),1)


if __name__=='__main__':unittest.main()
