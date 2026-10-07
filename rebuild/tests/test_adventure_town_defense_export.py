"""Offline source-contract checks; never mutate original databases or saves."""
from contextlib import closing
import json
from pathlib import Path
import sqlite3
import shutil
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_adventure_town_defense as exporter


class TownDefenseExportTests(unittest.TestCase):
    def manifest(self, database=exporter.DEFAULT_DATABASE):
        return json.loads(exporter.build_outputs(database)['pe_adventure_town_defense_source.json'])

    def test_regeneration(self):
        for name,data in exporter.build_outputs().items():
            self.assertEqual((exporter.DEFAULT_OUTPUT/name).read_bytes(),data)

    def test_actual_schedule_not_quest_goal_or_legacy_schedule(self):
        m=self.manifest()
        self.assertEqual(m['kill_goal'],15)
        self.assertEqual(m['total_possible_spawns'],dict(minimum=20,maximum=22))
        self.assertEqual(len(m['waves']),2)
        self.assertEqual([w['delay_seconds'] for w in m['waves']],[0,20])
        self.assertEqual([[g['prototype'] for g in w['groups']] for w in m['waves']],[[13,4],[7,4]])
        self.assertEqual(m['contract']['waves']['cd_time'],'10')

    def test_unobserved_positions_fail_closed(self):
        m=self.manifest()
        self.assertEqual(m['reference'],dict(available=False,beacon_position=None,agents=None))
        self.assertEqual(m['position_rule']['town_id'],4)
        self.assertIn(b'PE_TOWN_DEFENSE_COHORT_AVAILABLE 0',exporter.build_outputs()['pe_adventure_town_defense_source.h'])
        self.assertEqual([r['role'] for r in m['dialogue'][:4]],[4,3,4,3])

    def test_all_species_dependencies_are_preserved(self):
        m=self.manifest()
        self.assertEqual([int(r['id']) for r in m['species']],[13,4,7])
        self.assertTrue(all(r['assetbundle_path'] and r['behave_path'] for r in m['species']))
        self.assertEqual(m['contract']['missions']['9077']['NextQuest'],'0')
        self.assertEqual(m['contract']['missions']['9078']['PreLimit'],'1:9077')

    def test_changed_modern_contract_rejected_legacy_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'copy.db'
            with closing(exporter.connect_readonly(exporter.DEFAULT_DATABASE)) as source,closing(sqlite3.connect(p)) as changed:
                source.backup(changed)
                changed.execute("UPDATE MonsterSiege_SpawnId SET Sp_type='not the active path' WHERE Sp_id='520'");changed.commit()
                self.assertEqual(self.manifest(p)['total_possible_spawns'],dict(minimum=20,maximum=22))
                for sql in ("UPDATE MonsterBesiege SET start_time='31' WHERE dif_coef='520'",
                            "UPDATE MonsterBesiegeSpawn SET land='0_13' WHERE sp_type='503'",
                            "UPDATE AdQuest_TowerDefence SET Count='20' WHERE TargetID='7507'",
                            "UPDATE Scenario SET npc_id='-9898' WHERE talk_id='4107'"):
                    changed.execute('SAVEPOINT bad');changed.execute(sql)
                    # A readonly connection sees committed changes, then restore from source.
                    changed.commit()
                    with self.assertRaises(ValueError):self.manifest(p)
                    source.backup(changed)

    def test_planner_source_contract(self):
        m=self.manifest()
        self.assertEqual(len(m['guarded_spawn_methods']),6)
        self.assertEqual(m['planner_rule']['created_permissions'],dict(default_player_id=8,camp_id=26))
        self.assertEqual(m['planner_rule']['integer_maximum'],'exclusive')
        self.assertEqual(m['planner_rule']['real_endpoints'],'inclusive')
        self.assertEqual(m['planner_rule']['deferred_y_value'],0)
        self.assertFalse(m['planner_rule']['can_ride'])

    def test_changed_source_algorithm_rejected(self):
        changes=(
            ('Assets/Scripts/Scene/EntityMonsterBeacon.cs','go.transform.rotation = Quaternion.identity;','go.transform.rotation = targetTrans.rotation;'),
            ('Assets/Scripts/Scene/EntityMonsterBeacon.cs','AiUtil.GetRandomPosition(center, 20, 80, dir, minAngle, maxAngle)','AiUtil.GetRandomPosition(center, 10, 80, dir, minAngle, maxAngle)'),
            ('Assets/Scripts/Scene/EntityMonsterBeacon.cs','const int MonsterBeaconCampID = 26;','const int MonsterBeaconCampID = 27;'),
            ('Assets/Scripts/Scene/EntityMonsterBeacon.cs','sae.deathEvent += (t, c) => OnMonsterDeath(e);','sae.deathEvent += (t, c) => OnMonsterDeath(null);'),
            ('Assets/Scripts/AiScripts/AiHelper/AiUtil.cs','direction.y = 0.0f;','direction.y = 1.0f;'),
            ('Assets/Scripts/Scene/SceneEntityMissionPointTowerDefense.cs','num++;','num += 2;'),
            ('Assets/Scripts/Scene/SceneEntityPosAgent.cs','public const float PosYTBD = 0.0f;','public const float PosYTBD = 1.0f;'),
            ('Assets/Scripts/Scene/SceneEntityPosAgent.cs','if(CheckSetAvailablePos())','if(true)'),
        )
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for path,_,_ in exporter.SPAWN_METHODS:
                target=root/path;target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(exporter.ROOT/path,target)
            self.assertEqual(len(exporter.validate_spawn_sources(root)),6)
            for path,old,new in changes:
                with self.subTest(change=old):
                    target=root/path;original=target.read_text(encoding='utf-8-sig')
                    self.assertIn(old,original)
                    target.write_text(original.replace(old,new),encoding='utf-8')
                    with self.assertRaises(ValueError):exporter.validate_spawn_sources(root)
                    target.write_text(original,encoding='utf-8')

    def test_parser_rejects_malformed_or_invalid_ranges(self):
        for bad in ('0_501_8_10_0','0_501_8_8_0_360','-1_501_8_10_0_360','0_501_8_10_360_0'):
            with self.subTest(value=bad),self.assertRaises(ValueError):exporter.parse_waves(bad)


if __name__=='__main__':unittest.main()
