#!/usr/bin/env python3
"""Hare loot source guards, inventory qualifications and deterministic output."""
from contextlib import closing
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_hunt_loot as e


class SourceGuards(unittest.TestCase):
    def test_current_sources_and_table_contract(self):
        report=e.recover()
        self.assertEqual(report['meat'],dict(item_id=229,min_count=22,max_count_exclusive=35))
        self.assertEqual([(v['item_id'],v['probability']) for v in report['entries']],e.ROLL)
        self.assertEqual(report['trials'],5)
        self.assertEqual([i['item_id'] for i in report['items']],[229,1011,1008,236,1026,1228,1742])
        self.assertTrue(all(i['max_stack']>1 and i['inventory_tab']==2 and not i['has_durability'] and not i['components'] for i in report['items']))
        self.assertEqual(report['pickup_distance'],4.)
        self.assertEqual(report['generate_distance'],48.)
        self.assertEqual(report['lifetime_game_seconds'],93600.)

    def test_active_source_decision_mutations(self):
        changes=[
          ('Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs','LootItemMgr.Instance.RequestCreateLootItem(entity);','return;'),
          ('Assets/PeWorld/PeEntityCreator.cs','common.ItemDropId = protoItem.dropItemId;','common.ItemDropId = 0;'),
          ('Assets/Scripts/Items/ItemDrop/ItemDropData.cs','r.Next(itemDropData._meatData.lower, itemDropData._meatData.upper)','r.Next(itemDropData._meatData.lower, itemDropData._meatData.upper + 1)'),
          ('Assets/Scripts/Items/ItemDrop/ItemDropData.cs','float perc = (float)r.NextDouble();','float perc = 0;'),
          ('Assets/Scripts/Items/ItemDrop/ItemDropData.cs','perc < dat.pro','perc <= dat.pro'),
          ('Assets/Scripts/Items/ItemDrop/ItemDropData.cs','i < itemDropData._cnt','i <= itemDropData._cnt'),
          ('Assets/Scripts/Items/ItemDrop/ItemDropData.cs','item.stackCount += 1;','item.stackCount = 1;'),
          ('Assets/Scripts/Items/ItemDrop/ItemDropData.cs','break;','continue;'),
          ('Assets/Scripts/LootItem/LootItemMgr.cs','sqrDis < SqrFetchRange && !MainPlayer.Instance.entity.IsDeath()','sqrDis <= SqrFetchRange'),
          ('Assets/Scripts/LootItem/LootItemMgr.cs','4f * 4f','8f * 8f'),
          ('Assets/Scripts/LootItem/LootItemMgr.cs','48f*48f','64f*64f'),
          ('Assets/Scripts/LootItem/LootItemMgr.cs','GameTime.Timer.Second - data.dropTime > RemoveTime','Time.time - data.dropTime >= RemoveTime'),
          ('Assets/Scripts/LootItem/LootItemMgr.cs','playerPackage.Add(m_Datas[id].itemObj.protoId, m_Datas[id].itemObj.stackCount)','playerPackage.Add(m_Datas[id].itemObj.protoId, 1)'),
          ('Assets/Scripts/LootItem/LootItemMgr.cs','ItemMgr.Instance.DestroyItem(m_Datas[id].itemObj.instanceId);','return false;'),
          ('Assets/Scripts/Items/ItemObj.cs','if (!dicItemType.ContainsKey(typeId))','if (false)'),
          ('Assets/Scripts/ZhangShunBo/GetSpecialItem.cs','col.m_Type == 1 || col.m_Type == 3','col.m_Type == 0'),
        ]
        with tempfile.TemporaryDirectory(prefix='hunt-loot-source-') as directory:
            root=Path(directory)
            for name in e.SOURCE_SHA256:
                dest=root/name;dest.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(e.ROOT/name,dest)
            for name,old,new in changes:
                with self.subTest(name=name,old=old):
                    dest=root/name
                    original=dest.read_bytes()
                    source=original.decode('utf-8-sig',errors='replace')
                    self.assertIn(old,source)
                    dest.write_text(source.replace(old,new,1))
                    with self.assertRaisesRegex(ValueError,'source contract changed'):
                        e.validate_sources(root)
                    dest.write_bytes(original)

    def changed_database(self,sql,params=()):
        with closing(e.connect_readonly(e.DEFAULT_DATABASE)) as source, closing(sqlite3.connect(':memory:')) as db:
            source.backup(db);db.row_factory=sqlite3.Row
            db.execute(sql,params)
            with self.assertRaises((ValueError,KeyError)):
                e.qualify_tables(db)

    def test_loot_and_monster_mutations(self):
        for table,key,identity,column,value in [
          ('PrototypeMonster','id','13','Loot','12'),
          ('PrototypeMonster','id','13','DeleteTime','60'),
          ('loot','id','11','meat','22;36'),
          ('loot','id','11','meat','35;22'),
          ('loot','id','11','loot','4;1011_0.1,1008_0.2,236_0.4,1026_0.44,1228_0.45,1742_0.55,0_1'),
          ('loot','id','11','loot','5;1008_0.2,1011_0.1,236_0.4,1026_0.44,1228_0.45,1742_0.55,0_1'),
          ('loot','id','11','loot','5;1011_0.11,1008_0.2,236_0.4,1026_0.44,1228_0.45,1742_0.55,0_1'),
          ('loot','id','11','loot','5;1011_0.1,1008_0.2,236_0.4,1026_0.44,1228_0.45,1742_0.55,0_0.5'),
        ]:
            with self.subTest(table=table,column=column,value=value):
                self.changed_database(f'UPDATE "{table}" SET "{column}"=? WHERE "{key}"=?',(value,identity))
        self.changed_database("DELETE FROM loot WHERE id='11'")
        self.changed_database("INSERT INTO loot SELECT * FROM loot WHERE id='11'")

    def test_item_ownership_mutations(self):
        for identity,_,_ in e.ITEMS:
            for column,value in [('stacking_num','1'),('_function','1'),('category','Quest Item'),('tab','0'),('is_formula','1'),('skillId','1')]:
                with self.subTest(identity=identity,column=column):
                    self.changed_database(f'UPDATE PrototypeItem SET "{column}"=? WHERE id=?',(value,str(identity)))
        # Even adding class0 without Durability changes the no-components proof.
        self.changed_database("INSERT INTO item_class(id,classname) VALUES('0','Durability')")
        self.changed_database("INSERT INTO item_class(id,classname) VALUES('0','Consume')")

    def test_prototype_durability_default_is_not_a_component(self):
        with closing(e.connect_readonly(e.DEFAULT_DATABASE)) as source, closing(sqlite3.connect(':memory:')) as db:
            source.backup(db);db.row_factory=sqlite3.Row
            db.execute("UPDATE PrototypeItem SET _durability='9876' WHERE _function='0'")
            self.assertTrue(all(not item['has_durability'] and not item['components'] for item in e.qualify_tables(db)))

    def test_supported_special_quest_mutations(self):
        self.changed_database("UPDATE AdRandomQuest_List SET TargetIDList='2531' WHERE ID='9126'")
        for column,value in [('Type','1'),('TargetItemID','13'),('Chance','100'),('MaxNum','1')]:
            with self.subTest(column=column):
                self.changed_database(f'UPDATE AdQuest_LootItem SET "{column}"=? WHERE TargetID=?',(value,'2531'))
        self.changed_database("UPDATE AdQuest_KillMonster SET MonsterID='13_5' WHERE TargetID='1518'")

    def test_prefab_field_mutations(self):
        original=dict(e.PRESENTATION,m_Enabled=1)
        e.qualify_prefab(original)
        for field in original:
            with self.subTest(field=field):
                changed=copy.deepcopy(original);changed[field]+=1
                with self.assertRaisesRegex(ValueError,'prefab changed'):
                    e.qualify_prefab(changed)
                del changed[field]
                with self.assertRaisesRegex(ValueError,'prefab changed'):
                    e.qualify_prefab(changed)

    def test_prefab_bytes_guard_without_unitypy(self):
        with tempfile.TemporaryDirectory(prefix='hunt-loot-prefab-') as directory:
            root=Path(directory)
            for name in e.SOURCE_SHA256:
                dest=root/name;dest.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(e.ROOT/name,dest)
            path=root/e.PREFAB;data=bytearray(path.read_bytes());data[-1]^=1;path.write_bytes(data)
            with self.assertRaisesRegex(ValueError,'source contract changed'):
                e.validate_sources(root)

    def test_generated_outputs_and_readonly_inputs(self):
        paths=[e.ROOT/p for p in e.SOURCE_SHA256]+[e.DEFAULT_DATABASE]
        before=[hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
        outputs=e.build_outputs()
        for name,text in outputs.items():
            self.assertEqual(text,(e.ROOT/'rebuild/generated'/name).read_text())
        with tempfile.TemporaryDirectory(prefix='hunt-loot-generated-') as directory:
            args=['export_hunt_loot.py','--output',directory]
            with patch.object(sys,'argv',args):self.assertEqual(e.main(),0)
            with patch.object(sys,'argv',args+['--check']):
                self.assertEqual(e.main(),0)
                header=Path(directory)/'pe_hunt_loot_source.h';header.write_text('stale')
                self.assertEqual(e.main(),1)
                header.unlink();self.assertEqual(e.main(),1)
        self.assertEqual(before,[hashlib.sha256(p.read_bytes()).hexdigest() for p in paths])

    def test_refuses_asset_output(self):
        with patch.object(sys,'argv',['export_hunt_loot.py','--output',str(e.ROOT/'Assets')]):
            self.assertEqual(e.main(),1)


@unittest.skipUnless(importlib.util.find_spec('UnityPy'),'optional offline UnityPy unavailable')
class SerializedPrefab(unittest.TestCase):
    def test_source_prefab_fields(self):
        e.validate_prefab(e.ROOT)
        self.assertEqual(e.recover(verify_prefab=True),e.recover())


if __name__=='__main__':unittest.main()
