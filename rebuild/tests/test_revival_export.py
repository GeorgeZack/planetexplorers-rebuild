#!/usr/bin/env python3
"""Revival metadata drift tests: source/database mutations use disposable copies."""
from contextlib import closing, redirect_stderr
import hashlib
import io
import json
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import export_revival as e


class RevivalExportTests(unittest.TestCase):
    def test_reproducible_and_preserves_inputs(self):
        paths=[e.DEFAULT_DATABASE,*(e.ROOT/p for p in e.SOURCE_FILES)]
        hashes=[hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
        result=e.build_outputs()
        self.assertEqual(result,e.build_outputs())
        for name,data in result.items():self.assertEqual(data,(e.ROOT/'rebuild/generated'/name).read_bytes())
        self.assertEqual(hashes,[hashlib.sha256(p.read_bytes()).hexdigest() for p in paths])

    def test_exact_rule_and_float_literal(self):
        outputs=e.build_outputs();j=json.loads(outputs['pe_revival_source.json']);r=j['rule']
        self.assertEqual((r['item_id'],r['skill_id'],r['comfort_event_id'],r['hp_event_id']),(937,20150031,53,59))
        self.assertEqual((r['comfort_cost'],r['hp_fraction'],r['minimum_hp'],r['injured_grace_seconds']),(25,.333,1,1.5))
        self.assertFalse(r['ordinary_request_allowed'])
        self.assertTrue(r['restore_stamina']);self.assertTrue(r['restore_oxygen'])
        self.assertEqual(j['semantics']['event_order'],[53,59])
        self.assertIn(b'.333f',outputs['pe_revival_source.h'])
        self.assertNotIn(b'.333333',outputs['pe_revival_source.h'])
        for src in [j['source'],*j['source_files']]:self.assertEqual(len(src['sha256']),64)
        self.assertEqual(j['source_component']['classname'],'Property,RecycleReplicate,Consume')

    def db_change(self,sql,args=()):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'changed.db'
            with closing(e.connect_readonly(e.DEFAULT_DATABASE)) as src, closing(sqlite3.connect(path)) as dst:
                src.backup(dst);dst.execute(sql,args);dst.commit()
            with self.assertRaises((ValueError,KeyError)):e.build_outputs(path)

    def test_item_identity_binding_and_components(self):
        for column,value in [('_function','56'),('skillId','20150030'),('buffId','30200001'),('stacking_num','1'),('learn','1'),('bundle','1')]:
            with self.subTest(column=column):self.db_change(f'UPDATE PrototypeItem SET "{column}"=? WHERE id=\'937\'',(value,))
        self.db_change("UPDATE item_class SET classname='Property,Consume' WHERE id='55'")

    def test_missing_or_duplicate_rows(self):
        for table,key,identity in [('PrototypeItem','id','937'),('item_class','id','55'),('skMain','_id','20150031'),('skTriggerEvent','_id','53'),('skTriggerEvent','_id','59'),('ActionRelation','ActionType','11')]:
            with self.subTest(table=table,identity=identity):
                self.db_change(f'DELETE FROM {table} WHERE {key}=?',(identity,))
                self.db_change(f'INSERT INTO {table} SELECT * FROM {table} WHERE {key}=?',(identity,))

    def test_skill_order_timing_cooldown_and_loop(self):
        for column,value in [('_triggerEvents','59,53'),('_cond','True'),('_coolingTime','1'),('_coolingTimeShared','1'),('_coolingTimeType','9'),('_interruptable','1'),('_timeOfMain','.1'),('_effPrepare','1')]:
            with self.subTest(column=column):self.db_change(f'UPDATE skMain SET "{column}"=? WHERE _id=\'20150031\'',(value,))

    def test_event_expression_and_target(self):
        for identity in (53,59):
            for column,value in [('_modsTarget',e.EVENTS[identity]+';target.raws[1]=500'),('_modsCaster','exp,1,caster.raws[1]=500'),('_cond','False'),('_force','1'),('_effOnHitTarget','1')]:
                with self.subTest(identity=identity,column=column):self.db_change(f'UPDATE skTriggerEvent SET "{column}"=? WHERE _id=?',(value,str(identity)))

    def test_action_relation_drift(self):
        for column,value in [('Death','0'),('Fall','0'),('Ride','0'),('EndImmediately','0'),('PauseAction','0'),('SwordAttack','2')]:
            with self.subTest(column=column):self.db_change(f'UPDATE ActionRelation SET "{column}"=? WHERE ActionType=\'11\'',(value,))

    def source_change(self,path,old,new):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for name in e.SOURCE_FILES:
                dest=root/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(e.ROOT/name,dest)
            dest=root/path;text=dest.read_text(encoding='utf-8-sig',errors='replace');self.assertIn(old,text);dest.write_text(text.replace(old,new))
            with self.assertRaises(ValueError):e.build_outputs(source_root=root)

    def test_item_request_and_consumption_order_guard(self):
        path='Assets/Scripts/PeEntity/Cmpt/UseItemCmpt.cs'
        self.source_change(path,'item.protoId == ReviveItemProtoId','item.protoId != ReviveItemProtoId')
        self.source_change(path,'mPkg.DestroyItem(item, 1);','mPkg.DestroyItem(item, 2);')
        self.source_change(path,'return Use(Obj);','return Request(Obj);')

    def test_action_attributes_grace_and_readiness_guard(self):
        path='Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs'
        for old,new in [('AttribType.Hp, 1f','AttribType.Hp, 500f'),('ActivateInjured(true, 1.5f)','ActivateInjured(true, 0f)'),('viewCmpt.IsReadyGetUp()','true'),('viewCmpt.IsRagdoll','false')]:
            with self.subTest(old=old):self.source_change(path,old,new)

    def test_ui_and_state_event_order_guard(self):
        self.source_change('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRevive.cs','if (DoRevive(false))','if (true)')
        self.source_change('Assets/Scripts/SkillSystem/SkAliveEntity.cs','DispatchReviveEvent();','isDead = false; DispatchReviveEvent();')
        self.source_change('Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs','if(active)','if(!active)')
        self.source_change('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs','CancelInvoke("DoActivateInjured");','')

    def test_skill_execution_and_attribute_write_guard(self):
        self.source_change('Assets/Scripts/SkillSystem/SkInst.cs','skEvent.Exec(this);','skEvent.Exec(this); skEvent.Exec(this);')
        self.source_change('Assets/Scripts/SkillSystem/SkEntity.cs','bool eventOff = true, bool bBoth = true','bool eventOff = false, bool bBoth = true')

    def test_cli_fresh_output_stale_and_missing_check(self):
        with tempfile.TemporaryDirectory() as tmp:
            with redirect_stderr(io.StringIO()):self.assertEqual(e.main(['--output',tmp,'--check']),1)
            self.assertEqual(e.main(['--output',tmp]),0)
            self.assertEqual(e.main(['--output',tmp,'--check']),0)
            (Path(tmp)/'pe_revival_source.h').write_bytes(b'stale')
            with redirect_stderr(io.StringIO()):self.assertEqual(e.main(['--output',tmp,'--check']),1)


if __name__=='__main__':unittest.main()
