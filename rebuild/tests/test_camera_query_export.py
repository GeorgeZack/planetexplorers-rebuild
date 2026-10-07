"""Offline source-layer and collider identity guard; optional UnityPy required."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('camera_export', ROOT / 'rebuild/tools/export_camera_query.py')
EXPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORT)
AVAILABLE = importlib.util.find_spec('UnityPy') is not None and EXPORT.MANIFEST.exists()

@unittest.skipUnless(AVAILABLE, 'requires installed route manifest and UnityPy')
class CameraQueryExport(unittest.TestCase):
    def changed(self, mutate):
        data = json.loads(EXPORT.MANIFEST.read_text())
        mutate(data)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'manifest.json'
            path.write_text(json.dumps(data))
            with self.assertRaises((ValueError, KeyError)):
                EXPORT.recover(path)

    def test_exact_regeneration_and_source_layers(self):
        data = EXPORT.recover()
        self.assertEqual(len(data['groups']), 16)
        self.assertEqual({g['layer'] for g in data['groups']}, {13, 16})
        self.assertTrue(all((8468481 & (1 << g['layer'])) != 0 for g in data['groups']))
        self.assertEqual(data, json.loads((ROOT / 'rebuild/generated/pe_camera_query_source.json').read_text()))
        self.assertEqual(EXPORT.header(data), (ROOT / 'rebuild/generated/pe_camera_query_source.h').read_text())
        self.assertTrue(any(g['convex_solid'] for g in data['groups']))
        self.assertTrue(any(not g['convex_solid'] for g in data['groups']))

    def test_bundle_change_rejected(self):
        self.changed(lambda data: data['assets'][0].update(sha256='0' * 64))

    def test_owner_change_rejected(self):
        def change(data):
            next(p for p in data['parts'] if p['collision'])['game_object_id'] = 0
        self.changed(change)

    def test_shape_change_rejected(self):
        def change(data):
            p = next(p for p in data['parts'] if p['collision'])
            p['kind'] = 'SphereCollider'
        self.changed(change)

    def test_missing_collider_rejected(self):
        def change(data):
            next(p for p in data['parts'] if p['collision'])['collision'] = False
        self.changed(change)

if __name__ == '__main__':
    unittest.main()
