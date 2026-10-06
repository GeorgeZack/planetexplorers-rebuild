#!/usr/bin/env python3
"""Fail closed if any exact capsule method or qualified Unity assembly changes."""
from contextlib import redirect_stderr
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import export_capsule_history as e


class CapsuleHistoryExportTests(unittest.TestCase):
    @unittest.skipUnless(e.DEFAULT_UNITY.is_file(), "optional installed UnityEngine assembly unavailable")
    def test_deterministic_checked_outputs_and_unchanged_inputs(self):
        paths = [e.DEFAULT_UNITY, *(e.ROOT / p for p in e.SOURCE_FILES)]
        before = [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
        first = e.build_outputs()
        self.assertEqual(first, e.build_outputs())
        for name, data in first.items():
            self.assertEqual((e.ROOT / 'rebuild/generated' / name).read_bytes(), data)
        self.assertEqual(before, [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths])

    @unittest.skipUnless(e.DEFAULT_UNITY.is_file(), "optional installed UnityEngine assembly unavailable")
    def test_exact_sampling_and_threshold_contract(self):
        j = json.loads(e.build_outputs()['pe_capsule_history_source.json'])
        self.assertEqual(j['rule']['sampling_distance_factor'], .5)
        self.assertEqual(j['rule']['contact_distance'], 0)
        self.assertTrue(j['rule']['strict_overlap'])
        self.assertTrue(j['rule']['count_one_uses_previous'])
        self.assertTrue(j['rule']['target_uses_current'])
        self.assertEqual(j['native_limits']['max_samples'], 4096)
        self.assertAlmostEqual(j['rule']['vector_equality_squared'], 1e-10, delta=1e-17)
        self.assertAlmostEqual(j['rule']['normalize_magnitude'], 1e-5, delta=1e-12)
        self.assertEqual(len(j['method_guards']), 5)
        self.assertEqual(j['unity_engine']['sha256'], e.UNITY_SHA256)

    def mutate_source(self, index, old, new):
        with tempfile.TemporaryDirectory(prefix='capsule-guard-') as directory:
            root = Path(directory)
            for path in e.SOURCE_FILES:
                dst = root / path
                dst.parent.mkdir(parents=True, exist_ok=True)
                dst.write_bytes((e.ROOT / path).read_bytes())
            path = root / e.SOURCE_FILES[index]
            text = path.read_text(encoding='utf-8-sig')
            self.assertIn(old, text)
            path.write_text(text.replace(old, new, 1))
            with self.assertRaises(ValueError):
                e.validate_sources(root)

    def test_sampling_spacing_mutation(self):
        self.mutate_source(0, '0.5f * moveDir1', '0.6f * moveDir1')

    def test_count_one_mutation(self):
        self.mutate_source(0, 'count > 1 ? count - 1 : count', 'count > 1 ? count - 1 : 0')

    def test_overlap_boundary_mutation(self):
        self.mutate_source(0, 'dir.sqrMagnitude < radiusSum', 'dir.sqrMagnitude <= radiusSum')

    def test_entity_center_mutation(self):
        self.mutate_source(0, 'm_ParentCenterPos = centerPos;', 'm_ParentCenterPos = Vector3.zero;')

    def test_segment_cutoff_mutation(self):
        self.mutate_source(1, 't < 0.000001f', 't < 0.00001f')

    def test_parallel_fallback_mutation(self):
        self.mutate_source(1, 'float.IsNaN(a) || float.IsNaN(b)', 'float.IsNaN(a) && float.IsNaN(b)')

    def test_unqualified_unity_fails(self):
        with tempfile.TemporaryDirectory(prefix='capsule-engine-') as directory:
            path = Path(directory) / 'UnityEngine.dll'
            path.write_bytes(b'unqualified')
            with self.assertRaisesRegex(ValueError, 'Unqualified'):
                e.build_outputs(unity_engine=path)

    @unittest.skipUnless(e.DEFAULT_UNITY.is_file(), "optional installed UnityEngine assembly unavailable")
    def test_missing_and_stale_check_fail(self):
        with tempfile.TemporaryDirectory(prefix='capsule-output-') as directory:
            args = ['--output', directory]
            with redirect_stderr(io.StringIO()):
                self.assertEqual(e.main(args + ['--check']), 1)
            self.assertEqual(e.main(args), 0)
            self.assertEqual(e.main(args + ['--check']), 0)
            (Path(directory) / 'pe_capsule_history_source.h').write_text('stale')
            with redirect_stderr(io.StringIO()):
                self.assertEqual(e.main(args + ['--check']), 1)


if __name__ == '__main__':
    unittest.main()
