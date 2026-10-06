#!/usr/bin/env python3
"""Guard real source contracts; mutations affect disposable copies only."""
from contextlib import closing, redirect_stderr
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_starter_sword_shield as exporter

UNITY = importlib.util.find_spec('UnityPy') is not None


class SourceContracts(unittest.TestCase):
    def changed_database(self,table,column,identity_field,identity,value):
        with tempfile.TemporaryDirectory() as folder:
            target=Path(folder)/'changed.db'
            with closing(exporter.connect_readonly(exporter.DEFAULT_DATABASE)) as src,closing(sqlite3.connect(target)) as dst:
                src.backup(dst)
                dst.execute(f'UPDATE "{table}" SET "{column}"=? WHERE "{identity_field}"=?',(value,str(identity)));dst.commit()
            with self.assertRaises(ValueError):exporter.database_contract(target)

    def changed_source(self,key,old,new):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for rel in exporter.SCRIPTS.values():
                dst=root/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(exporter.ROOT/rel,dst)
            p=root/exporter.SCRIPTS[key];data=p.read_bytes();self.assertIn(old,data);p.write_bytes(data.replace(old,new))
            with self.assertRaises(ValueError):exporter.validate_sources(root)

    def test_numeric_pair_and_distinct_slot_ownership(self):
        exporter.validate_sources(exporter.ROOT)
        result=exporter.database_contract(exporter.DEFAULT_DATABASE);s=result['items'][1];b=result['items'][34]
        self.assertEqual((s['Atk'],s['durDec'],s['_durability']),('10','5','2369'))
        self.assertEqual((b['Def'],b['ShieldMeleeProtect'],b['durDec'],b['_durability']),('9','.22','.1','4312'))
        self.assertEqual(int(s['_position'])&int(b['_position']),0)
        self.assertEqual(result['player_defaults']['ShieldMultiple'],'1')
        self.assertEqual([r['_id'] for r in result['skills']],[str(x) for x in exporter.SKILLS.values()])

    def test_item_model_rate_related_stats_and_slots_cannot_drift(self):
        for identity,key,value in [(1,'Atk','9'),(1,'_durability','100'),(1,'durDec','0'),
                                   (1,'_modelPath','Prefab/Item/Equip/Weapon/Sword/knife'),
                                   (34,'_position','16'),(34,'Def','0'),(34,'ShieldMeleeProtect','1'),
                                   (34,'durDec','NaN'),(34,'_replacePos','16')]:
            with self.subTest(identity=identity,key=key):
                self.changed_database('PrototypeItem',key,'id',identity,value)

    def test_correct_equipment_wear_component_is_required(self):
        self.changed_database('item_class','classname','id',18,'Property,Durability,EquipAttack')

    def test_buff_formula_lifetime_and_multiplier_are_bound(self):
        for identity,key,value in [(30200093,'_mods','exp,27,target.buffPreAdd+=9;'),
                                   (30200087,'_stackLimit','2'),(30200177,'_timeActive','10')]:
            with self.subTest(identity=identity,key=key):self.changed_database('skBuff',key,'_id',identity,value)
        self.changed_database('initProp','ShieldMultiple','id',1,'2')

    def test_conditional_hare_mitigation_not_global_reduction(self):
        self.changed_database('skTriggerEvent','_modsTarget','_id',10100,exporter.HARE_MODS.replace('target.sums[35]==1','true'))
        self.changed_database('skTriggerEvent','_cond','_id',10100,'True')

    def test_attack_cancels_block_and_block_cannot_start_during_attack_or_sprint(self):
        for identity,key,value in [(14,'EndImmediately','1,2,3,31'),(15,'SwordAttack','0'),(15,'Sprint','0')]:
            with self.subTest(identity=identity,key=key):self.changed_database('ActionRelation',key,'ActionType',identity,value)

    def test_source_notifications_and_wear_are_not_per_swing_or_hp_clamped(self):
        self.changed_source('alive',b'EMsg.Battle_EquipAttack, m_MotionEquip.Weapon.ItemObj',b'EMsg.Battle_Attack, m_MotionEquip.Weapon.ItemObj')
        self.changed_source('item',b'ChangeDurability(skEntity, -durabilityFactor)',b'ChangeDurability(skEntity, -1)')
        self.changed_source('item',b'hpChange * durabilityFactor',b'hpChange * durabilityFactor * 2')
        self.changed_source('alive',b'EMsg.Battle_BeAttacked, dmg, inst.Caster',b'EMsg.Battle_BeAttacked, 1, inst.Caster')

    def test_hold_cost_release_attachment_and_rate_semantics_rejected_on_drift(self):
        self.changed_source('shield',b'"Bone L Forearm"',b'"mountMain"')
        self.changed_source('hold',b'AttribType.EnableShieldBlock, 0',b'AttribType.EnableShieldBlock, 1')
        self.changed_source('hold',b'anim.SetBool(PESheild.HoldSheildAnim, true);',
                            b'anim.SetBool(PESheild.HoldSheildAnim, true); skillCmpt.SetAttribute(AttribType.Stamina, 0);')
        self.changed_source('attack',b'animSpeed *= 0.9f;',b'animSpeed *= 0.8f;')
        self.changed_source('equipment',b'if(PeGameMgr.IsBuild)',b'if(!PeGameMgr.IsBuild)')


@unittest.skipUnless(UNITY,'Optional UnityPy required for source-prefab validation')
class PrefabContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sword=exporter.read_prefab(exporter.ROOT/exporter.SWORD)
        cls.shield=exporter.read_prefab(exporter.ROOT/exporter.SHIELD)
        cls.outputs=exporter.build_outputs()

    def reject(self,key,edit):
        s=copy.deepcopy(self.sword);b=copy.deepcopy(self.shield)
        edit(s if key=='sword' else b)
        with self.assertRaises((ValueError,KeyError)):exporter.validate_prefabs(s,b)

    def test_prefabs_have_exact_modern_bindings_and_clocks(self):
        p=exporter.validate_prefabs(self.sword,self.shield)
        self.assertEqual(p['bindings']['sword']['guid'],'563fc98ffb60d2e438cc41c4ab5f0320')
        self.assertEqual(p['bindings']['shield']['guid'],'5b0b18af79a95f7448b1c8d92e0c9256')
        self.assertEqual(p['sword_component']['m_AnimSpeed'],1)
        self.assertEqual(p['sword_component']['m_StaminaCost'],[5])
        self.assertEqual(p['capsule']['heigh'],exporter.f32(1.8))

    def test_regeneration_and_input_hashes_remain_unchanged(self):
        paths=[exporter.DEFAULT_DATABASE,*[exporter.ROOT/p for p in exporter.SOURCE_FILES]]
        before={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
        self.assertEqual(self.outputs,exporter.build_outputs())
        for name,data in self.outputs.items():self.assertEqual((exporter.ROOT/'rebuild/generated'/name).read_bytes(),data)
        self.assertEqual(before,{p:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
        doc=json.loads(self.outputs['pe_starter_sword_shield_source.json'])
        self.assertEqual(len(doc['sources']),len(exporter.SOURCE_FILES))
        self.assertTrue(all(len(s['sha256'])==64 for s in doc['sources']))
        self.assertIn('no runtime',doc['scope'])

    def test_knife_speed_legacy_script_or_wrong_skill_cannot_be_substituted(self):
        self.reject('sword',lambda p:p['objects'][11400004]['data'].__setitem__('m_AnimSpeed',1.37))
        self.reject('sword',lambda p:p['objects'][11400004]['data'].__setitem__('m_StaminaCost',[0]))
        self.reject('sword',lambda p:p['objects'][11400004]['data']['m_AttackSkill'][0].__setitem__('m_SkillID',20011))
        self.reject('sword',lambda p:p['external_guids'].__setitem__(0,'0'*32))

    def test_attachment_capsule_broadphase_and_event_binding_drift_rejected(self):
        self.reject('sword',lambda p:p['objects'][11400004]['data']['m_HandChangeAttr'].__setitem__('m_PutOffBone','mountWaist'))
        self.reject('sword',lambda p:p['objects'][11412292]['data']['attackParts'][0]['capsule'].__setitem__('radius',.3))
        self.reject('sword',lambda p:p['objects'][6500000]['data'].__setitem__('m_Enabled',1))
        self.reject('sword',lambda p:p['objects'][100004]['data'].__setitem__('m_Name','Knife'))
        self.reject('sword',lambda p:p['objects'][400004]['data']['m_LocalScale'].__setitem__('z',2.))

    def test_no_invented_shield_collider_or_attachment_offset(self):
        self.reject('shield',lambda p:p['objects'].__setitem__(65000,dict(type='BoxCollider',data={})))
        self.reject('shield',lambda p:p['objects'][400002]['data']['m_LocalPosition'].__setitem__('x',0.))
        self.reject('shield',lambda p:p['objects'][400002]['data']['m_LocalRotation'].__setitem__('w',1.))

    def test_check_detects_stale_without_rewriting_or_partial_generation(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            for name,data in self.outputs.items():(root/name).write_bytes(data)
            self.assertEqual(exporter.main(['--output',str(root),'--check']),0)
            header=root/'pe_starter_sword_shield_source.h';header.write_bytes(b'stale')
            with redirect_stderr(io.StringIO()):self.assertEqual(exporter.main(['--output',str(root),'--check']),1)
            self.assertEqual(header.read_bytes(),b'stale')

    @unittest.skipUnless(shutil.which('cc'),'C compiler required for standalone header validation')
    def test_header_is_standalone_c11_with_correct_source_numeric_contract(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'pair.h').write_bytes(self.outputs['pe_starter_sword_shield_source.h'])
            source=root/'check.c';source.write_text('''#include "pair.h"
int main(void) {
 return !(pe_starter_sword_source.item_id==1 && pe_starter_sword_source.animation_speed==1.0f &&
   pe_starter_sword_source.wear_per_damage_notification==5.0f &&
   pe_starter_shield_source.item_id==34 && pe_starter_shield_source.defense==9.0f &&
   pe_starter_shield_source.melee_protect==0.22f && pe_starter_sword_capsule_source.height==1.8f &&
   !(pe_starter_sword_source.equip_mask & pe_starter_shield_source.equip_mask));
}
''')
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(source),'-o',str(root/'check')],check=True,capture_output=True)
            subprocess.run([str(root/'check')],check=True,capture_output=True)


if __name__=='__main__':unittest.main()
