#!/usr/bin/env python3
"""Source-data and exporter regression tests; no third-party Python packages."""
from contextlib import closing
import importlib.util
import json
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/export_content.py'
spec = importlib.util.spec_from_file_location('export_content', SCRIPT)
export = importlib.util.module_from_spec(spec)
spec.loader.exec_module(export)


class ContentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.outputs = export.build_outputs(export.DEFAULT_DATABASE, export.DEFAULT_LOCALIZATION)
        cls.manifest = json.loads(cls.outputs['content_manifest.json'])

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def clone_database(self):
        path = self.directory / 'content.sqlite'
        shutil.copyfile(export.DEFAULT_DATABASE, path)
        return path

    def mutate(self, path, sql, arguments=()):
        with closing(sqlite3.connect(path)) as db:
            db.execute(sql, arguments)
            db.commit()

    def run_export(self, *arguments):
        return subprocess.run([sys.executable, str(SCRIPT), *map(str, arguments)],
                              capture_output=True, text=True, cwd=self.directory)

    def test_original_recipe_and_item_invariants(self):
        items = {row['id']: row for row in self.manifest['items']}
        recipes = {row['id']: row for row in self.manifest['recipes']}
        self.assertEqual(len(items), 320)
        self.assertEqual(len(recipes), 95)
        self.assertEqual({k: items[279][k] for k in ('id', 'name', 'stack_limit')},
                         {'id': 279, 'name': 'Wood', 'stack_limit': 9999})
        self.assertEqual(items[20]['stack_limit'], 1)
        self.assertEqual(recipes[8]['ingredients'], [{'item_id': 280, 'count': 6},
                                                     {'item_id': 279, 'count': 6}])
        self.assertEqual((recipes[8]['output_id'], recipes[8]['seconds']), (20, 3.0))
        self.assertEqual((recipes[152]['output_id'], recipes[152]['seconds']), (308, 5.0))
        self.assertEqual(self.manifest['source_counts']['valid_items'], 1743)
        self.assertEqual(self.manifest['source_counts']['skipped_item_metadata'], 1)
        for recipe in recipes.values():
            self.assertIn(recipe['output_id'], items)
            for ingredient in recipe['ingredients']:
                self.assertIn(ingredient['item_id'], items)
        self.assertEqual(self.manifest['selected_item_ids'], sorted(items))

    def test_adventure_grants_books_and_icons(self):
        items = {row['id']: row for row in self.manifest['items']}
        starter = {v['item_id']: v['count'] for v in self.manifest['adventure_starter']}
        supplies = {v['item_id']: v['count'] for v in self.manifest['adventure_supplies']}
        self.assertEqual(starter, {33:1,1527:1,73:1,70:1,973:1,974:1,975:1,1579:1,
                                  916:10,228:1,937:10,229:400,279:200})
        self.assertEqual(len(supplies), 18)
        self.assertEqual(supplies[406], 1)
        self.assertEqual(items[406]['learn_ids'], [1])
        self.assertEqual(items[519]['learn_ids'], [114,42])
        self.assertEqual(items[973]['learn_kind'], 'PE_LEARN_SCAN')
        self.assertEqual(items[973]['learn_ids'], [2,3])
        self.assertEqual(items[406]['icons'], ['shovel_wood','','leftdown_script'])
        self.assertEqual(items[279]['tab'], 2)
        self.assertEqual(items[33]['equip_position'], 16)
        self.assertEqual(items[12]['stamina_cost'], 4)
        self.assertEqual(items[26]['stamina_cost'], 4)
        self.assertEqual(items[1527]['stamina_cost'], 4)
        self.assertEqual(self.manifest['player_defaults']['hp_max'], 500)
        recipes = set(self.manifest['selected_recipe_ids'])
        for item in items.values():
            if item['learn_kind'] == 'PE_LEARN_RECIPE':
                self.assertTrue(set(item['learn_ids']) <= recipes)
        self.assertEqual(self.manifest['selected_recipe_ids'][:8], list(export.RECIPE_IDS))

    def test_starter_outfit_presentation_and_component_provenance(self):
        items = {row['id']: row for row in self.manifest['items']}
        expected = [(95, 'Issued Overalls♀', 'equip_w_torso_01', 19, 2, 2),
                    (131, 'Issued Pants♀', 'equip_w_legs_01', 20, 64, 15),
                    (192, 'Issued Shoes♀', 'equip_w_feet_01', 22, 128, 16)]
        bindings = {row['item_id']: row for row in self.manifest['starter_outfit_catalog']}
        outfit = json.loads((export.ROOT/'rebuild/generated/pe_starter_outfit_source.json').read_text())
        source = {row['item_id']: row for row in outfit['outfits']['female']}
        for identity, name, icon, item_type, position, component in expected:
            row = items[identity]
            self.assertEqual((row['name'], row['icons']), (name, [icon, 'leftup_sex_f', '']))
            self.assertEqual((row['item_type'], row['tab'], row['equip_position']), (item_type, 1, position))
            self.assertEqual((row['tool_kind'], row['learn_kind'], row['skill_id']), ('PE_TOOL_NONE', 'PE_LEARN_NONE', 0))
            self.assertEqual((row['durability_max'], row['durability_cost']),
                             (source[identity]['durability'], source[identity]['wear_factor']))
            self.assertEqual(bindings[identity]['component_class'], component)
            self.assertIn('EquipDefence', bindings[identity]['components'])
        # Worn instances belong to the separate outfit owner, not duplicate bag grants.
        self.assertTrue(set(source).isdisjoint(v['item_id'] for v in self.manifest['adventure_starter']))

    def test_medicine_and_annot_closure(self):
        items = {row['id']: row for row in self.manifest['items']}
        recipes = {row['id']: row for row in self.manifest['recipes']}
        self.assertEqual(items[877]['learn_kind'], 'PE_LEARN_RECIPE')
        self.assertEqual(items[877]['item_type'], 0)
        self.assertEqual(items[877]['learn_ids'], [337,388,390])
        self.assertEqual(items[554]['learn_ids'], [149])
        for identity in (337,388,390):
            self.assertEqual((recipes[identity]['output_id'],recipes[identity]['seconds']), (917,1))
        self.assertEqual(recipes[337]['ingredients'], [{'item_id':239,'count':1},{'item_id':235,'count':2},
            {'item_id':241,'count':2},{'item_id':238,'count':1},{'item_id':233,'count':1}])
        self.assertEqual(self.manifest['medicine_grants']['RewardItem'], [{'item_id':925,'count':10},{'item_id':554,'count':1}])
        self.assertTrue(set(self.manifest['annot_vendor_item_ids']) <= set(items))

    def test_hunt_reward_formula_closure(self):
        items = {row['id']: row for row in self.manifest['items']}
        recipes = {row['id']: row for row in self.manifest['recipes']}
        self.assertEqual(self.manifest['hunt_return_rewards'],
                         [{'item_id': identity, 'count': 1} for identity in (592,593,594,595)])
        for identity, formulas in ((592,[187,42,41,51]),(593,[188]),(594,[189]),(595,[190])):
            self.assertEqual(items[identity]['learn_kind'], 'PE_LEARN_RECIPE')
            self.assertEqual(items[identity]['learn_ids'], formulas)
            for formula in formulas:
                self.assertIn(recipes[formula]['output_id'], items)
                for ingredient in recipes[formula]['ingredients']:
                    self.assertIn(ingredient['item_id'], items)

    def test_changed_hunt_rewards_rejected(self):
        database = self.clone_database()
        self.mutate(database, "UPDATE AdRandomQuest_List SET RewardItem='592_2,593_1,594_1,595_1' WHERE ID='9127'")
        with self.assertRaisesRegex(ValueError, 'Hunt9127 reward contract'):
            export.build_outputs(database, None)

    def test_medpack_heal_and_independent_cooldown_source(self):
        m=self.manifest['portable_medpack']
        self.assertEqual((m['item_id'],m['skill_id'],m['trigger_id']), (925,20150028,2))
        self.assertEqual((m['heal'],m['cooldown_seconds'],m['cooldown_group'],m['juice_cooldown_group']), (350,10,9,11))
        for sql in ["UPDATE skTriggerEvent SET _modsTarget='mad,1,1,300' WHERE _id='2'",
                    "UPDATE skMain SET _coolingTimeType='11' WHERE _id='20150028'",
                    "UPDATE skMain SET _pretimeOfMain='1' WHERE _id='20150028'"]:
            database=self.clone_database();self.mutate(database,sql)
            with self.assertRaisesRegex(ValueError,'Portable Medpack925'):
                export.build_outputs(database,None)

    def test_invalid_adventure_grants_rejected(self):
        database = self.clone_database()
        self.mutate(database, "UPDATE PrototypeItem SET bundle='1;33,1' WHERE id='1289'")
        with self.assertRaisesRegex(ValueError, 'fixed bundle'):
            export.build_outputs(database, None)
        self.mutate(database, "UPDATE PrototypeItem SET bundle='0;33,1,33,2' WHERE id='1289'")
        with self.assertRaisesRegex(ValueError, 'duplicate grant'):
            export.build_outputs(database, None)

    def test_source_backed_equipment_player_and_materials(self):
        items = {row['id']: row for row in self.manifest['items']}
        pickaxe = items[19]
        self.assertEqual((pickaxe['tool_kind'], pickaxe['item_type']), ('PE_TOOL_PICKAXE', 12))
        self.assertEqual((pickaxe['durability_max'], pickaxe['durability_cost']), (3553, 5.0))
        self.assertEqual((pickaxe['attack'], pickaxe['resource_damage'], pickaxe['resource_range']), (65.0, 0.0, 0.0))
        self.assertEqual(pickaxe['stamina_cost'], 4.0)
        self.assertIn('wood_hoe.prefab', self.manifest['source_tool_overrides']['19']['source']['path'])
        self.assertEqual(items[26]['cut_damage'], 46.0)
        self.assertEqual(items[26]['cut_bonus'], 0.01)
        self.assertEqual(items[1]['tool_kind'], 'PE_TOOL_NONE')
        player = self.manifest['player_defaults']
        self.assertEqual((player['stamina_max'], player['stamina_recover_speed']), (100.0, 40.0))
        self.assertEqual((player['resource_range'], player['attack']), (0.49, 10.0))
        mapping = self.manifest['terrain_material_items']
        durations = self.manifest['terrain_material_durability']
        self.assertEqual((len(mapping), len(durations)), (256, 256))
        self.assertEqual(sum(item != -1 for item in mapping), 52)
        self.assertEqual((mapping[1], mapping[17], mapping[10], mapping[20]), (268, 280, 278, 269))
        self.assertEqual(mapping[255], -1)
        self.assertEqual(self.manifest['source_counts']['construction_materials'], 52)
        self.assertEqual(self.manifest['source_counts']['terrain_resources'], 128)
        resources = self.manifest['terrain_resources']
        self.assertEqual(resources['57']['drops'], [{'item_id': 989, 'threshold': 70.0},
                                                   {'item_id': 271, 'threshold': 100.0}])
        self.assertEqual(resources['57']['self_count'], 4.0)
        self.assertEqual(resources['55']['drops'], [])
        self.assertEqual(resources['2']['self_count'], 2.0)
        self.assertEqual(resources['17']['drops'], [{'item_id': 280, 'threshold': 100.0}])
        for resource in resources.values():
            for drop in resource['drops']:
                self.assertIn(drop['item_id'], items)
        self.assertEqual((player['cut_damage'], player['cut_bonus']), (0.0, 0.0))
        self.assertAlmostEqual(durations[17], 0.15875)
        self.assertEqual(durations[0], 0.0)
        for item in mapping:
            if item != -1:
                self.assertIn(item, items)
        recipes = {row['id']: row for row in self.manifest['recipes']}
        self.assertEqual(recipes[13]['output_id'], 26)
        self.assertEqual(recipes[13]['ingredients'], [{'item_id': 279, 'count': 10}])

    def test_invalid_material_and_stat_fail_without_coercion(self):
        database = self.clone_database()
        self.mutate(database, "UPDATE buildblock SET matID='256' WHERE matID='1'")
        with self.assertRaisesRegex(ValueError, 'exceeds byte range'):
            export.build_outputs(database, None)
        shutil.copyfile(export.DEFAULT_DATABASE, database)
        self.mutate(database, "UPDATE resource SET production='989,80,271,70;;' WHERE ID='57'")
        with self.assertRaisesRegex(ValueError, 'invalid cumulative drop threshold'):
            export.build_outputs(database, None)
        shutil.copyfile(export.DEFAULT_DATABASE, database)
        self.mutate(database, "UPDATE PrototypeItem SET durDec='NaN' WHERE id='19'")
        with self.assertRaisesRegex(ValueError, 'invalid nonnegative float'):
            export.build_outputs(database, None)

    def test_determinism_and_checked_in_freshness(self):
        second = export.build_outputs(export.DEFAULT_DATABASE, export.DEFAULT_LOCALIZATION)
        self.assertEqual(self.outputs, second)
        for name, data in self.outputs.items():
            self.assertEqual((export.DEFAULT_OUTPUT / name).read_bytes(), data)
        self.assertEqual(self.run_export('--check').returncode, 0)

    def test_check_does_not_modify_stale_files(self):
        output = self.directory / 'generated'
        output.mkdir()
        stale = output / 'pe_content.c'
        stale.write_bytes(b'stale artifact\n')
        result = self.run_export('--check', '--output-dir', output)
        self.assertEqual(result.returncode, 1)
        self.assertIn('stale or missing', result.stderr)
        self.assertEqual(stale.read_bytes(), b'stale artifact\n')
        self.assertFalse((output / 'content_manifest.json').exists())

    def test_actual_fourth_column_and_fractional_duration(self):
        database = self.clone_database()
        self.mutate(database, "UPDATE synthesis_skill SET materia4='273', num4='2', need_time='.5' WHERE ID='1'")
        output = export.build_outputs(database, None)
        recipe = json.loads(output['content_manifest.json'])['recipes'][0]
        self.assertEqual(recipe['seconds'], 0.5)
        self.assertEqual(recipe['ingredients'][-1], {'item_id': 273, 'count': 2})
        self.assertIn(b'0.5f', output['pe_content.c'])

    def test_external_localization_precedence_and_fallback(self):
        database = self.clone_database()
        localization = self.directory / 'locale.db'
        with closing(sqlite3.connect(localization)) as db:
            db.execute('CREATE TABLE Translation(ID, English, Chinese)')
            db.execute("INSERT INTO Translation VALUES('2111466', 'Wood override', '')")
            db.execute("INSERT INTO Translation VALUES('2111466', 'Wood final', '')")
            db.execute("INSERT INTO Translation VALUES('2111467', '', '')")
            db.commit()
        output = export.build_outputs(database, localization)
        names = {row['id']: row['name'] for row in json.loads(output['content_manifest.json'])['items']}
        self.assertEqual(names[279], 'Wood final')
        self.assertEqual(names[280], 'Stone')
        self.mutate(database, "DELETE FROM Translation WHERE ID='2111466'")
        output = export.build_outputs(database, self.directory / 'missing-optional-locale.db')
        names = {row['id']: row['name'] for row in json.loads(output['content_manifest.json'])['items']}
        self.assertEqual(names[279], 'wood')  # PrototypeItem._engName fallback

    def test_missing_and_corrupt_database_do_not_create_outputs(self):
        missing = self.directory / 'missing.db'
        output = self.directory / 'generated'
        result = self.run_export('--database', missing, '--output-dir', output)
        self.assertEqual(result.returncode, 1)
        self.assertIn('Content export failed', result.stderr)
        self.assertFalse(missing.exists())
        self.assertFalse(output.exists())
        bad = self.directory / 'corrupt.db'
        bad.write_bytes(b'not a SQLite database')
        result = self.run_export('--database', bad, '--output-dir', output)
        self.assertEqual(result.returncode, 1)
        self.assertFalse(output.exists())

    def test_missing_material_and_invalid_recipe_fail(self):
        database = self.clone_database()
        self.mutate(database, "DELETE FROM PrototypeItem WHERE id='279'")
        with self.assertRaisesRegex(ValueError, 'missing required item 279'):
            export.build_outputs(database, None)
        shutil.copyfile(export.DEFAULT_DATABASE, database)
        self.mutate(database, "UPDATE synthesis_skill SET need_time='NaN' WHERE ID='1'")
        with self.assertRaisesRegex(ValueError, 'invalid craft duration'):
            export.build_outputs(database, None)
        self.mutate(database, "UPDATE synthesis_skill SET need_time='3', num4='2' WHERE ID='1'")
        with self.assertRaisesRegex(ValueError, 'incomplete ingredient 4'):
            export.build_outputs(database, None)

    def test_source_connection_is_readonly_and_c_escaping_is_unambiguous(self):
        db = export.connect_readonly(export.DEFAULT_DATABASE)
        try:
            with self.assertRaises(sqlite3.OperationalError):
                db.execute('CREATE TABLE unintended_write(x)')
        finally:
            db.close()
        self.assertEqual(export.c_string('a"\\\n9'), '"a\\042\\134\\0129"')


if __name__ == '__main__':
    unittest.main()
