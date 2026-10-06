#!/usr/bin/env python3
"""Source/table contract tests; all mutations occur in disposable copies."""
from contextlib import closing
import hashlib
import json
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_starter_outfit as exporter


class StarterOutfitExportTests(unittest.TestCase):
    def outputs(self):return exporter.build_outputs()

    def test_regeneration_and_readonly_inputs(self):
        before=hashlib.sha256(exporter.DEFAULT_DATABASE.read_bytes()).hexdigest()
        first=self.outputs();self.assertEqual(first,self.outputs())
        for name,data in first.items():self.assertEqual((exporter.ROOT/'rebuild/generated'/name).read_bytes(),data)
        self.assertEqual(before,hashlib.sha256(exporter.DEFAULT_DATABASE.read_bytes()).hexdigest())

    def test_both_sex_bindings_and_buff_properties_preserved(self):
        j=json.loads(self.outputs()['pe_starter_outfit_source.json'])
        for sex,ids,sex_id in [('female',[95,131,192],1),('male',[113,149,210],2)]:
            rows=j['outfits'][sex]
            self.assertEqual([r['item_id'] for r in rows],ids)
            self.assertEqual([r['defense'] for r in rows],[7,4,1])
            self.assertEqual([r['durability'] for r in rows],[2156,2156,862])
            self.assertEqual([r['equip_position'] for r in rows],[2,64,128])
            self.assertTrue(all(r['sex']==sex_id and r['wear_factor']==.1 for r in rows))
        self.assertEqual(j['initialization']['original_order'],[113,149,210,95,131,192])
        self.assertEqual(j['semantics']['buff_indices'],{'Def':27,'ComfortReducePercent':9,'ComfortRecoverSpeed':10})
        self.assertEqual(len(j['source_files']),8)
        for source in j['source_files']:self.assertEqual(len(source['sha256']),64)
        self.assertIn('fixed female',j['scope'])

    def altered_database(self,sql,args=()):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'changed.db'
            with closing(exporter.connect_readonly(exporter.DEFAULT_DATABASE)) as source,closing(sqlite3.connect(p)) as dest:
                source.backup(dest);dest.execute(sql,args);dest.commit()
            with self.assertRaises(ValueError):exporter.build_outputs(p)

    def test_wrong_item_stats_sex_slots_model_and_buff_rejected(self):
        for field,value in [('Def','12'),('_durability','2155'),('durDec','NaN'),('sex','2'),('_position','128'),
                            ('buffId','30200100'),('_modelPath','Model/PlayerModel/male01-torso'),('ComfortRecoverSpeed','1')]:
            with self.subTest(field=field):self.altered_database(f'UPDATE PrototypeItem SET "{field}"=? WHERE id=\'95\'',(value,))

    def test_defense_component_binding_cannot_silently_change(self):
        self.altered_database("UPDATE item_class SET classname='Property,Durability,EquipAttack' WHERE id='2'")

    def test_male_binding_drift_is_not_ignored(self):
        self.altered_database("UPDATE PrototypeItem SET Def='8' WHERE id='113'")

    def test_buff_semantics_and_lifetime_changes_rejected(self):
        for field,value in [('_mods','exp,27,target.buffPreAdd+=12;'),('_timeActive','60'),('_stackLimit','2'),('_type','65518')]:
            with self.subTest(field=field):self.altered_database(f'UPDATE skBuff SET "{field}"=? WHERE _id=\'30200097\'',(value,))

    def altered_source(self,name,old,new):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            for rel in exporter.SOURCE_FILES:
                dst=root/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(exporter.ROOT/rel,dst)
            p=root/name;b=p.read_bytes();self.assertIn(old,b);p.write_bytes(b.replace(old,new))
            with self.assertRaises(ValueError):exporter.build_outputs(source_root=root)

    def test_initializer_and_sex_filter_changes_rejected(self):
        self.altered_source(exporter.SOURCE_FILES[0],b'113, 149, 210, 95, 131, 192',b'113, 149, 210, 96, 131, 192')
        self.altered_source(exporter.SOURCE_FILES[0],b'!PeGender.IsMatch(itemProto.equipSex, entitySex)',b'PeGender.IsMatch(itemProto.equipSex, entitySex)')

    def test_wear_exhaustion_and_build_exclusion_changes_rejected(self):
        self.altered_source(exporter.SOURCE_FILES[4],b'hpChange * durabilityFactor',b'hpChange * durabilityFactor * 2')
        self.altered_source(exporter.SOURCE_FILES[4],b'if (DurabilityExhaust())',b'if (!DurabilityExhaust())')
        self.altered_source(exporter.SOURCE_FILES[5],b'if(PeGameMgr.IsBuild)',b'if(!PeGameMgr.IsBuild)')

    def test_initial_durability_and_damage_notification_changes_rejected(self):
        self.altered_source(exporter.SOURCE_FILES[4],b'float initLife = valueMax;',b'float initLife = 0;')
        self.altered_source(exporter.SOURCE_FILES[7],b'EMsg.Battle_BeAttacked, dmg, inst.Caster',b'EMsg.Battle_BeAttacked, 1, inst.Caster')

    def test_check_detects_stale_output_without_rewriting(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)
            for name,data in self.outputs().items():(p/name).write_bytes(data)
            self.assertEqual(exporter.main(['--output',str(p),'--check']),0)
            header=p/'pe_starter_outfit_source.h';header.write_bytes(b'stale')
            self.assertEqual(exporter.main(['--output',str(p),'--check']),1)
            self.assertEqual(header.read_bytes(),b'stale')


if __name__=='__main__':unittest.main()
