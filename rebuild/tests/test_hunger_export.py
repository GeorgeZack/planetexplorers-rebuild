#!/usr/bin/env python3
"""Source/table drift tests for player hunger; only disposable copies change."""
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

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import export_hunger as exporter


class HungerExportTests(unittest.TestCase):
    def test_regeneration_is_deterministic_and_originals_unchanged(self):
        paths = [exporter.DEFAULT_DATABASE, *(exporter.ROOT / p for p in exporter.SOURCE_FILES)]
        before = [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
        first = exporter.build_outputs()
        self.assertEqual(first, exporter.build_outputs())
        for name, data in first.items():
            self.assertEqual((exporter.ROOT / 'rebuild/generated' / name).read_bytes(), data)
        self.assertEqual(before, [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths])

    def test_source_rule_is_preserved(self):
        j = json.loads(exporter.build_outputs()['pe_hunger_source.json'])
        r = j['rule']
        self.assertEqual((r['hunger_max'], r['initial_hunger'], r['recovery_speed'], r['reduction_percent']), (500, 300, -.0694, 1))
        self.assertEqual((r['drain_interval_seconds'], r['start_interval_seconds'], r['end_interval_seconds']), (5, 1, .1))
        self.assertEqual((r['start_minimum'], r['start_maximum'], r['end_minimum'], r['end_maximum']), (0, 50, 51, 1000))
        self.assertEqual([m['pre_add'] for m in j['starvation_pre_adds']], [1, -10, 2, 1, 1])
        self.assertEqual([f['item_id'] for f in j['foods']], list(exporter.FOODS))
        self.assertEqual({f['item_id']: f['hunger'] for f in j['foods']}[386], 80)
        for source in [j['source'], *j['source_files']]:
            self.assertEqual(len(source['sha256']), 64)

    def mutate_database(self, sql, args=()):
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / 'changed.db'
            with closing(exporter.connect_readonly(exporter.DEFAULT_DATABASE)) as source, closing(sqlite3.connect(database)) as dest:
                source.backup(dest); dest.execute(sql, args); dest.commit()
            with self.assertRaises((ValueError, KeyError)):
                exporter.build_outputs(database)

    def test_player_drain_and_starvation_changes_rejected(self):
        for field, value in [('HungerMax', '400'), ('Hunger', '500'), ('HungerRecoverSpeed', '-.1'), ('HungerReducePercent', '2')]:
            with self.subTest(field=field):
                self.mutate_database(f'UPDATE initProp SET "{field}"=? WHERE id=\'1\'', (value,))
        self.mutate_database("UPDATE skBuff SET _timeInterval='4' WHERE _id='30200050'")
        self.mutate_database("UPDATE skBuff SET _mods='exp,4,target.buffPreAdd+=2' WHERE _id='30200113'")
        for field, value in [('Hit_Attr', '16,0,60,1,1'), ('RH_Attr', '16,40,1000,1,1'), ('Trigger_Time', '2'),
                             ('RT_Time', '1'), ('IsDeathRemove', '1'), ('Eff_BuffAdd', '30200114')]:
            with self.subTest(field=field):
                self.mutate_database(f'UPDATE AbnormalType SET "{field}"=? WHERE AbnormalId=\'8\'', (value,))

    def test_food_changes_rejected(self):
        self.mutate_database("UPDATE skMain SET _coolingTime='5' WHERE _id='20150003'")
        self.mutate_database("UPDATE skMain SET _triggerEvents='457' WHERE _id='20150003'")
        self.mutate_database("UPDATE skTriggerEvent SET _modsTarget=replace(_modsTarget,'+=80','+=81') WHERE _id='457'")
        self.mutate_database("UPDATE skTriggerEvent SET _modsTarget='rnd,1,1,30200105,0.02' WHERE _id='489'")

    def mutate_source(self, relative, old, new):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in exporter.SOURCE_FILES:
                path = root / name; path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(exporter.ROOT / name, path)
            path = root / relative; data = path.read_bytes(); self.assertIn(old, data)
            path.write_bytes(data.replace(old, new, 1))
            with self.assertRaises(ValueError):
                exporter.build_outputs(source_root=root)

    def test_condition_clock_and_rate_changes_rejected(self):
        a = exporter.ABNORMAL
        self.mutate_source(a + 'PEAbnormalTrigger.cs', b'm_ElapseTime >= interval', b'm_ElapseTime > interval')
        self.mutate_source(a + 'PEAbnormalTrigger.cs', b'm_ElapseTime += Time.deltaTime;', b'm_ElapseTime += Time.fixedDeltaTime;')
        self.mutate_source(a + 'PEAbnormalCondition.cs', b'if (val < minThreshold || val > maxThreshold)', b'if (val <= minThreshold || val > maxThreshold)')
        self.mutate_source(a + 'PEAbnormalCondition.cs', b'UnityEngine.Random.value < hitRate)\n\t\t{', b'UnityEngine.Random.value <= hitRate)\n\t\t{')
        self.mutate_source('Assets/Scripts/SkillSystem/SkAttribsModifier.cs', b'var = var*_mul + _add;', b'var = var + _add;')

    def test_check_does_not_rewrite_stale_header(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            for name, data in exporter.build_outputs().items():
                (output / name).write_bytes(data)
            self.assertEqual(exporter.main(['--output', directory, '--check']), 0)
            header = output / 'pe_hunger_source.h'; header.write_bytes(b'stale')
            with redirect_stderr(io.StringIO()):
                self.assertEqual(exporter.main(['--output', directory, '--check']), 1)
            self.assertEqual(header.read_bytes(), b'stale')


if __name__ == '__main__':
    unittest.main()
