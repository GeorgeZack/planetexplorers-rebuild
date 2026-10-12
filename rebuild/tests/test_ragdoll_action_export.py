#!/usr/bin/env python3
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_ragdoll_actions as e
class Sources(unittest.TestCase):
    def test_sources(self): self.assertEqual(len(e.validate_sources(e.ROOT)),len(e.SOURCES))
    def test_mutations(self):
        mutations=[(e.ACTION,'m_AutoGetUpTime <= 0','m_AutoGetUpTime < 0'),
                   (e.ACTION,'motionMgr.DoActionImmediately(PEActionType.GetUp);','motionMgr.DoAction(PEActionType.GetUp);'),
                   (e.ACTION,'viewCmpt.DeactivateRagdoll(true);','viewCmpt.DeactivateRagdoll(false);'),
                   (e.MANAGER,'mFreezePhyRequestType.Contains(type)','false'),
                   (e.RAGDOLL,'velocity.magnitude < settledSpeed','velocity.magnitude <= settledSpeed'),
                   (e.RAGDOLL,'hitInfo.hitForce * 100','hitInfo.hitForce'),
                   (e.VIEW,'CancelInvoke("DoActivateInjured");',''),
                   (e.BASE,'public virtual void ResetAction(PEActionParam para) { }','public virtual void ResetAction(PEActionParam para) { DoAction(para); }')]
        for path,old,new in mutations:
            with self.subTest(path=path,old=old),tempfile.TemporaryDirectory() as d:
                root=Path(d)
                for name in e.SOURCES:
                    target=root/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(e.ROOT/name,target)
                target=root/path;s=target.read_text(encoding='utf-8-sig',errors='replace');self.assertIn(old,s);target.write_text(s.replace(old,new,1))
                with self.assertRaises(ValueError): e.validate_sources(root)
    def test_relations(self):
        r=e.read_relations(e.DEFAULT_DATABASE);self.assertEqual(r[2]['required'],(1<<8)|(1<<44));self.assertEqual(r[2]['immediate'],(1<<7)|(1<<8))
        for action,field,value in [('10','Knocked','0'),('10','Lie','0'),('10','EndImmediately','7'),('8','PauseAction','0'),('7','Knocked','0')]:
            with self.subTest(field=field),tempfile.TemporaryDirectory() as d:
                db=Path(d)/'data';shutil.copyfile(e.DEFAULT_DATABASE,db)
                with sqlite3.connect(db) as connection:connection.execute(f'UPDATE ActionRelation SET {field}=? WHERE ActionType=?',(value,action))
                with self.assertRaises(ValueError):e.read_relations(db)
@unittest.skipUnless(importlib.util.find_spec('UnityPy'),'optional UnityPy unavailable')
class Prefab(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.player=e.read_objects(e.ROOT/e.PLAYER);cls.guid=e.guid(e.ROOT,e.BEAT)
    def test_player_time(self):self.assertEqual(e.qualify(self.player,self.guid)['knocked_get_up_time'],2.0)
    def test_mutations(self):
        for field in ('time','enabled','duplicate'):
            with self.subTest(field=field):
                m=copy.deepcopy(self.player)
                if field=='time':m[11400038]['data']['m_Knocked']['m_GetUpTime']=3
                elif field=='enabled':m[11400038]['data']['m_Enabled']=0
                else:m[123456]=copy.deepcopy(m[11400038])
                with self.assertRaises(ValueError):e.qualify(m,self.guid)
    def test_regeneration(self):
        outputs=e.build_outputs();self.assertEqual(outputs,e.build_outputs())
        for name,data in outputs.items():self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
        data=json.loads(outputs['pe_ragdoll_action_source.json']);self.assertTrue(all(not Path(s['path']).is_absolute() for s in data['source_files']))
if __name__=='__main__':unittest.main()
