#!/usr/bin/env python3
"""Guard the passive recovery source chain; mutate disposable copies only."""
from contextlib import closing, redirect_stderr
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import export_passive_recovery as exporter


class PassiveRecoveryExportTests(unittest.TestCase):
    def test_regeneration_and_reference_inputs_unchanged(self):
        paths = [exporter.DEFAULT_DATABASE, *(exporter.ROOT / p for p in exporter.SOURCE_FILES)]
        before = [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
        first = exporter.build_outputs()
        self.assertEqual(first, exporter.build_outputs())
        for name, data in first.items():
            self.assertEqual((exporter.ROOT / 'rebuild/generated' / name).read_bytes(), data)
        self.assertEqual(before, [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths])

    def test_exact_rule_and_scheduler_provenance(self):
        j = json.loads(exporter.build_outputs()['pe_passive_recovery_source.json'])
        self.assertEqual(j['rule'], dict(player_init_id=1, buff_id=30200046, interval_seconds=1,
            cooldown_seconds=4, heal_fraction=.01, threshold_fraction=.33, lifetime_seconds=-1,
            initialization_check_delay_seconds=.5, attribute_frame_interval=8))
        self.assertEqual(j['initial_player']['InitBuffList'], exporter.INITIAL_BUFFS)
        self.assertEqual(j['source_buff']['_mods'], exporter.EXPRESSION)
        self.assertEqual(j['scheduler']['source_attribute_frame_interval'], 8)
        self.assertEqual(j['scheduler']['max_overdue_ticks_per_evaluation'], 1)
        self.assertTrue(j['scheduler']['initial_evaluation'])
        self.assertTrue(j['scheduler']['tick_before_expiration'])
        self.assertEqual(len(j['source_files']), len(exporter.SOURCE_FILES))
        for source in [j['source'], *j['source_files']]:
            self.assertEqual(len(source['sha256']), 64)

    def mutate_database(self, sql, args=()):
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / 'changed.db'
            with closing(exporter.connect_readonly(exporter.DEFAULT_DATABASE)) as source, closing(sqlite3.connect(database)) as dest:
                source.backup(dest)
                dest.execute(sql, args)
                dest.commit()
            with self.assertRaises((ValueError, KeyError)):
                exporter.build_outputs(database)

    def test_buff_formula_and_clock_changes_rejected(self):
        for field, value in [('_mods', exporter.EXPRESSION.replace('<0.33f', '<=0.33f')),
                             ('_mods', exporter.EXPRESSION.replace('(4)', '(3)')),
                             ('_mods', exporter.EXPRESSION.replace('*0.01f', '*0.02f')),
                             ('_timeInterval', '0.5'), ('_timeActive', '10')]:
            with self.subTest(field=field, value=value):
                self.mutate_database(f'UPDATE skBuff SET "{field}"=? WHERE _id=\'30200046\'', (value,))

    def test_buff_stack_priority_and_effect_changes_rejected(self):
        for field, value in [('_stackLimit', '2'), ('_priority', '1'), ('_type', '65509'), ('_eff', '57')]:
            with self.subTest(field=field):
                self.mutate_database(f'UPDATE skBuff SET "{field}"=? WHERE _id=\'30200046\'', (value,))

    def test_active_player_membership_and_row_binding_rejected(self):
        self.mutate_database("UPDATE initProp SET InitBuffList=replace(InitBuffList, '30200046,', '') WHERE id='1'")
        self.mutate_database("UPDATE initProp SET id='9' WHERE id='1'")
        self.mutate_database("UPDATE initProp SET HpMax='1000' WHERE id='1'")

    def test_duplicate_or_missing_passive_buff_rejected(self):
        self.mutate_database("INSERT INTO skBuff SELECT * FROM skBuff WHERE _id='30200046'")
        self.mutate_database("DELETE FROM skBuff WHERE _id='30200046'")

    def mutate_source(self, index, old, new):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in exporter.SOURCE_FILES:
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(exporter.ROOT / relative, path)
            path = root / exporter.SOURCE_FILES[index]
            data = path.read_bytes()
            self.assertIn(old, data)
            path.write_bytes(data.replace(old, new))
            with self.assertRaises(ValueError):
                exporter.build_outputs(source_root=root)

    def test_scheduler_strictness_backlog_and_initial_evaluation_rejected(self):
        for old, new in [(b'elapse > _n*_buff._interval', b'elapse >= _n*_buff._interval'),
                         (b'if(_n == 0)', b'if(_n == 1)'),
                         (b'if(_buff._interval >', b'while(_buff._interval >'),
                         (b'inst._startTime = Time.time;', b'inst._startTime = 0;')]:
            with self.subTest(old=old):
                self.mutate_source(0, old, new)

    def test_cooldown_comparison_and_damage_timestamp_rejected(self):
        self.mutate_source(1, b'Time.time >= SkRuntimeInfo.Current.Target._lastestTimeOfGettingHurt',
                           b'Time.time > SkRuntimeInfo.Current.Target._lastestTimeOfGettingHurt')
        self.mutate_source(1, b'target._lastestTimeOfGettingHurt = curTime;',
                           b'target._lastestTimeOfGettingHurt = curTime + 1;')

    def test_death_alive_and_attribute_mapping_rejected(self):
        self.mutate_source(2, b'SetAutoBuffActive(false);', b'SetAutoBuffActive(true);')
        self.mutate_source(2, b'if(GetAttribute(AttribType.Hp) <= 0)', b'if(GetAttribute(AttribType.Hp) < 0)')
        self.mutate_source(2, b'HpMax = 0,', b'HpMax = 1,')

    def test_source_clock_binding_and_loaded_player_rejected(self):
        self.mutate_source(3, b'_lastestTimeOfGettingHurt = 0;', b'_lastestTimeOfGettingHurt = -1;')
        self.mutate_source(4, b'UpdateFrameInterval = 8;', b'UpdateFrameInterval = 1;')
        self.mutate_source(5, b'items.Count >0 ? items[0] : null', b'items.Count >0 ? items[1] : null')
        self.mutate_source(6, b'm_InitBuffList = playerPDB.initBuff;', b'm_InitBuffList = null;')

    def test_deferred_initialization_and_remount_phase_rejected(self):
        self.mutate_source(6, b'Invoke("CheckInitAttr", 0.5f);', b'Invoke("CheckInitAttr", 1f);')
        self.mutate_source(0, b'Unmount(buffPak, stackBuffs[0]);', b'return stackBuffs[0];')

    def test_check_is_readonly_and_rejects_stale_output(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            for name, data in exporter.build_outputs().items():
                (output / name).write_bytes(data)
            self.assertEqual(exporter.main(['--output', directory, '--check']), 0)
            header = output / 'pe_passive_recovery_source.h'
            header.write_bytes(b'stale')
            with redirect_stderr(io.StringIO()):
                self.assertEqual(exporter.main(['--output', directory, '--check']), 1)
            self.assertEqual(header.read_bytes(), b'stale')

    def test_header_compiles_and_preserves_source_float_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            header = root / 'pe_passive_recovery_source.h'
            header.write_bytes(exporter.build_outputs()[header.name])
            source = root / 'check.c'
            source.write_text('''#include "pe_passive_recovery_source.h"
_Static_assert(_Generic(pe_passive_recovery_source.heal_fraction, float: 1, default: 0), "float healing");
_Static_assert(_Generic(pe_passive_recovery_source.threshold_fraction, float: 1, default: 0), "float threshold");
int main(void) {
    const PePassiveRecoverySource *r=&pe_passive_recovery_source;
    if(r->player_init_id!=1 || r->buff_id!=30200046 || r->interval_seconds!=1 ||
       r->cooldown_seconds!=4 || r->lifetime_seconds!=-1 || r->initialization_check_delay_seconds!=.5 ||
       r->attribute_frame_interval!=8) return 1;
    float hp=164.0f, max_hp=500.0f;
    if(!(hp/max_hp < r->threshold_fraction)) return 2;
    hp+=max_hp*r->heal_fraction;
    if(hp!=169.0f || hp/max_hp < r->threshold_fraction) return 3;
    return 165.0f/max_hp < r->threshold_fraction;
}
''')
            binary = root / 'check'
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                            str(source), '-o', str(binary)], check=True, capture_output=True)
            subprocess.run([str(binary)], check=True, capture_output=True)


if __name__ == '__main__':
    unittest.main()
