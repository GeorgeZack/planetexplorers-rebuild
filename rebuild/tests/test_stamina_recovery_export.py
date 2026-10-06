#!/usr/bin/env python3
"""Source/table drift tests for stamina and comfort; only disposable copies change."""
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
import export_stamina_recovery as exporter


class StaminaRecoveryExportTests(unittest.TestCase):
    def test_regeneration_is_deterministic_and_originals_unchanged(self):
        paths = [exporter.DEFAULT_DATABASE, *(exporter.ROOT / p for p in exporter.SOURCE_FILES)]
        before = [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
        first = exporter.build_outputs()
        self.assertEqual(first, exporter.build_outputs())
        for name, data in first.items():
            self.assertEqual((exporter.ROOT / 'rebuild/generated' / name).read_bytes(), data)
        self.assertEqual(before, [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths])

    def test_source_rule_and_order_are_preserved(self):
        j = json.loads(exporter.build_outputs()['pe_stamina_recovery_source.json'])
        r = j['rule']
        self.assertEqual((r['stamina_max'], r['initial_stamina'], r['recovery_speed'], r['reduction_percent']), (100, 100, 40, 1))
        self.assertEqual((r['comfort_max'], r['initial_comfort'], r['comfort_recovery_speed'], r['comfort_reduction_percent']), (500, 500, -.1388, 1))
        self.assertEqual((r['minimum_delay_seconds'], r['base_delay_seconds'], r['comfort_delay_scale']), (.1, 0, 2))
        self.assertEqual((r['stamina_interval_seconds'], r['comfort_interval_seconds'], r['delay_interval_seconds']), (1, 5, 1))
        self.assertEqual(j['semantics']['relevant_execution_order'], [30200175, 30200052, 30200049])
        self.assertEqual(j['initial_player']['InitBuffList'], exporter.shared.INITIAL_BUFFS)
        for row in j['source_buffs']:
            self.assertEqual(row['_mods'], exporter.BUFFS[int(row['_id'])][2])
        for source in [j['source'], *j['source_files']]:
            self.assertEqual(len(source['sha256']), 64)

    def mutate_database(self, sql, args=()):
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / 'changed.db'
            with closing(exporter.connect_readonly(exporter.DEFAULT_DATABASE)) as source, closing(sqlite3.connect(database)) as dest:
                source.backup(dest); dest.execute(sql, args); dest.commit()
            with self.assertRaises((ValueError, KeyError)):
                exporter.build_outputs(database)

    def test_player_stats_and_initializer_order_changes_rejected(self):
        for field, value in [('StaminaMax', '101'), ('Stamina', '99'), ('StaminaRecoverSpeed', '41'),
                             ('StaminaReducePercent', '.5'), ('StaminaRecoverDelay', '.1'), ('ComfortMax', '499'),
                             ('Comfort', '499'), ('ComfortRecoverSpeed', '-.1'), ('ComfortReducePercent', '.5'),
                             ('InitBuffList', exporter.shared.INITIAL_BUFFS.replace('30200049,30200050', '30200050,30200049'))]:
            with self.subTest(field=field):
                self.mutate_database(f'UPDATE initProp SET "{field}"=? WHERE id=\'1\'', (value,))

    def test_missing_and_duplicate_buffs_rejected(self):
        for identity in exporter.BUFFS:
            with self.subTest(identity=identity):
                self.mutate_database('DELETE FROM skBuff WHERE _id=?', (str(identity),))
                self.mutate_database('INSERT INTO skBuff SELECT * FROM skBuff WHERE _id=?', (str(identity),))

    def test_all_buff_expressions_and_clocks_rejected_on_drift(self):
        for identity, (_kind, _interval, expression) in exporter.BUFFS.items():
            for field, value in [('_mods', expression + ';target.raws[3]+=1;'), ('_timeInterval', '.25'),
                                 ('_timeActive', '60'), ('_stackLimit', '2'), ('_priority', '1'), ('_effBeg', '57')]:
                with self.subTest(identity=identity, field=field):
                    self.mutate_database(f'UPDATE skBuff SET "{field}"=? WHERE _id=?', (value, str(identity)))

    def mutate_source(self, relative, old, new):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in exporter.SOURCE_FILES:
                path = root / name; path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(exporter.ROOT / name, path)
            path = root / relative; data = path.read_bytes(); self.assertIn(old, data)
            path.write_bytes(data.replace(old, new))
            with self.assertRaises(ValueError):
                exporter.build_outputs(source_root=root)

    def test_cooldown_strictness_minimum_and_last_consumption_rejected(self):
        path = 'Assets/Scripts/SkillSystem/SkExpFuncSet.cs'
        self.mutate_source(path, b'Time.time > SkRuntimeInfo.Current.Target._lastestTimeOfConsumingStamina',
                           b'Time.time >= SkRuntimeInfo.Current.Target._lastestTimeOfConsumingStamina')
        self.mutate_source(path, b'Mathf.Max(cooltime, 0.1f)', b'Mathf.Max(cooltime, 0.2f)')
        self.mutate_source('Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs', b'if(oldVal > newVal)', b'if(oldVal >= newVal)')

    def test_reverse_buff_and_ascending_attribute_order_rejected(self):
        path = 'Assets/Scripts/SkillSystem/SkAttribs.cs'
        self.mutate_source(path, b'for(int i = n-1; i >= 0; i--)', b'for(int i = 0; i < n; i++)')
        self.mutate_source(path, b'for(int i = 0; i < nAttribs; i++)', b'for(int i = nAttribs-1; i >= 0; i--)')
        self.mutate_source(path, b'_dirties[idx] = true;', b'_dirties[idx] = false;')

    def test_temporary_delay_classification_and_dirtying_rejected(self):
        path = 'Assets/Scripts/SkillSystem/SkAttribsModifier.cs'
        self.mutate_source(path, b'args[1].Contains("buff")', b'false')
        self.mutate_source(path, b'dst.modflags[_tmpOpsIdxs[i]] = true;', b'dst.modflags[_tmpOpsIdxs[i]] = false;')
        self.mutate_source(path, b'for(int j = 0; j < times; j++)', b'for(int j = 0; j < 1; j++)')

    def test_clamping_and_loaded_sum_contract_rejected(self):
        self.mutate_source('Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs',
            b'Mathf.Clamp(value, 0, entity.GetAttribute(m_Max))', b'Mathf.Clamp(value, 1, entity.GetAttribute(m_Max))')
        self.mutate_source('Assets/Scripts/SkillSystem/SkAttribs.cs',
            b'_sums[i] = _raws[i] = r.ReadSingle();', b'_raws[i] = r.ReadSingle();')

    def test_source_active_action_locks_are_not_silently_removed(self):
        for path in exporter.ADDITIONAL_SOURCES[2:]:
            with self.subTest(path=path):
                self.mutate_source(path, b'skillCmpt._lastestTimeOfConsumingStamina = Time.time;',
                    b'skillCmpt._lastestTimeOfConsumingStamina = 0;')

    def test_check_does_not_rewrite_stale_header(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            for name, data in exporter.build_outputs().items():
                (output / name).write_bytes(data)
            self.assertEqual(exporter.main(['--output', directory, '--check']), 0)
            header = output / 'pe_stamina_recovery_source.h'; header.write_bytes(b'stale')
            with redirect_stderr(io.StringIO()):
                self.assertEqual(exporter.main(['--output', directory, '--check']), 1)
            self.assertEqual(header.read_bytes(), b'stale')

    def test_header_compiles_with_source_float_arithmetic(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); header = root / 'pe_stamina_recovery_source.h'
            header.write_bytes(exporter.build_outputs()[header.name])
            source = root / 'check.c'; binary = root / 'check'
            source.write_text('''#include "pe_stamina_recovery_source.h"
_Static_assert(_Generic(pe_stamina_recovery_source.minimum_delay_seconds, float: 1, default: 0), "float comparison");
int main(void) {
    const PeStaminaRecoverySource *s=&pe_stamina_recovery_source;
    if(s->stamina_buff_id!=30200052 || s->comfort_buff_id!=30200049 || s->delay_buff_id!=30200175) return 1;
    if(s->stamina_interval_seconds!=1 || s->comfort_interval_seconds!=5 || s->delay_interval_seconds!=1 || s->lifetime_seconds!=-1) return 2;
    float comfort=.1f+s->comfort_recovery_speed*s->comfort_reduction_percent*5.0f;
    float delay=(1.0f-comfort/s->comfort_max)*s->comfort_delay_scale;
    if(!(comfort<0 && delay>2.0f)) return 3;
    if(s->recovery_speed*s->reduction_percent!=40.0f || s->minimum_delay_seconds!=.1f) return 4;
    return 0;
}
''')
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                            str(source), '-o', str(binary)], check=True, capture_output=True)
            subprocess.run([str(binary)], check=True, capture_output=True)


if __name__ == '__main__':
    unittest.main()
