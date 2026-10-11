#!/usr/bin/env python3
"""Coordinate tests run with stdlib; installed bundle tests need optional UnityPy."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/export_reference_assets.py'
sys.path.insert(0, str(SCRIPT.parent))
spec = importlib.util.spec_from_file_location('export_reference_assets', SCRIPT)
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)
GAME = Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'
HAS_UNITY = importlib.util.find_spec('UnityPy') is not None


class CoordinateTests(unittest.TestCase):
    def assertVector(self, actual, expected):
        for a, b in zip(actual, expected):
            self.assertAlmostEqual(a, b, places=6)

    def test_source_room_rotation_and_translation_keep_unity_coordinates(self):
        matrix = exporter.trs(dict(x=12, y=1.5, z=12),
                              dict(x=0, y=-math.sqrt(.5), z=0, w=math.sqrt(.5)),
                              dict(x=1, y=1, z=1))
        self.assertVector(exporter.transform_point(matrix, [0, 0, 0]), [12, 1.5, 12])
        self.assertVector(exporter.transform_point(matrix, [1, 0, 0]), [12, 1.5, 13])
        self.assertVector(exporter.transform_point(matrix, [0, 0, 1]), [11, 1.5, 12])
        self.assertVector(exporter.transform_normal(matrix, [0, 0, 1]), [-1, 0, 0])
        self.assertAlmostEqual(exporter.determinant(matrix), 1)

    def test_parent_child_order_and_inverse_transpose_normal(self):
        identity_rotation = dict(x=0, y=0, z=0, w=1)
        parent = exporter.trs(dict(x=10, y=20, z=30), identity_rotation, dict(x=2, y=1, z=3))
        child = exporter.trs(dict(x=1, y=2, z=3), identity_rotation, dict(x=1, y=1, z=1))
        world = exporter.multiply(parent, child)
        self.assertVector(exporter.transform_point(world, [0, 0, 0]), [12, 22, 39])
        normal = exporter.transform_normal(world, [1, 1, 0])
        self.assertVector(normal, [1/math.sqrt(5), 2/math.sqrt(5), 0])
        self.assertAlmostEqual(sum(normal[i]*v for i, v in enumerate([2, -1, 0])), 0)

    def test_material_symbols_avoid_tinyobj_probe_deadlock(self):
        # The actual room has 22 materials; tinyobj grows 10 -> 20 -> 40.
        ids = list(range(-11, 11))
        symbols = exporter.material_symbols(ids)
        capacity, occupied = 10, set()
        previous = []
        for index, pid in enumerate(ids):
            if index == capacity:
                capacity *= 2
                occupied = {value % capacity for value in previous}
                self.assertEqual(len(occupied), index)
            value = 5381
            for character in symbols[pid]:
                value = value * 33 + ord(character)
            self.assertLess(value, 2**32)  # Same buckets on 32/64-bit builds.
            self.assertNotIn(value % capacity, occupied)
            occupied.add(value % capacity)
            previous.append(value)
        self.assertEqual(symbols, exporter.material_symbols(ids))


    def test_singular_transform_and_external_reference_rejected(self):
        with self.assertRaises(ValueError):
            exporter.transform_normal([[0]*4 for _ in range(4)], [0, 1, 0])
        with self.assertRaises(ValueError):
            exporter.pointer({'m_FileID': 1, 'm_PathID': 4})
        with self.assertRaises(ValueError):
            exporter.floats([float('nan')])


@unittest.skipUnless(HAS_UNITY and (GAME/'AssetBundles/Item/scene_TrainingRoom.unity3d').is_file(),
                     'optional UnityPy and installed original are required')
class InstalledBundleTests(unittest.TestCase):
    def test_training_knife_keeps_local_pose_and_source_attachment_provenance(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            protected = {'manifest.json': b'room metadata', 'room.obj': b'room mesh',
                         'training-kit-manifest.json': b'kit metadata'}
            for name, content in protected.items():
                (output/name).write_bytes(content)
            command = [sys.executable, str(SCRIPT), '--game-dir', str(GAME), '--output',
                       str(output), '--asset', 'training-knife', '--no-textures']
            first = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            contents = {p.name: p.read_bytes() for p in output.iterdir()}
            second = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertEqual(contents, {p.name: p.read_bytes() for p in output.iterdir()})
            for name, content in protected.items():
                self.assertEqual(contents[name], content)
            manifest = json.loads(contents['training-knife-manifest.json'])
            knife = manifest['assets']['training-knife']
            self.assertEqual(len(knife['parts']), 1)
            part = knife['parts'][0]
            self.assertEqual(part['matrix'], exporter.IDENTITY)
            self.assertEqual((part['vertices'], part['triangles'], part['submeshes']), (151, 153, 2))
            self.assertEqual(part['game_object_path_id'], 9)
            provenance = knife['placement_provenance']
            self.assertEqual(provenance['item_id'], 33)
            self.assertEqual(provenance['model_path'], 'Prefab/Item/Equip/Weapon/Sword/knife')
            self.assertEqual(provenance['attachment']['m_PutOnBone'], 'mountMain')
            self.assertEqual(provenance['attachment']['m_PutOffBone'], 'mountWaist')
            self.assertEqual(knife['unity_versions'], ['4.3.0f4'])
            self.assertIn('swing animation are not exported', provenance['limitations'])
            for name, info in manifest['files'].items():
                self.assertEqual(info['sha256'], hashlib.sha256(contents[name]).hexdigest())

    def test_training_kit_exact_shared_visual_placement_and_noninterference(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            protected = {'room.obj': b'existing room geometry',
                         'manifest.json': b'existing room provenance'}
            for name, data in protected.items():
                (output/name).write_bytes(data)
            command = [sys.executable, str(SCRIPT), '--game-dir', str(GAME),
                       '--output', str(output), '--asset', 'training-kit', '--no-textures']
            first = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            contents = {p.name: p.read_bytes() for p in output.iterdir()}
            for name, data in protected.items():
                self.assertEqual(contents[name], data)
            second = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertEqual(contents, {p.name: p.read_bytes() for p in output.iterdir()})
            manifest = json.loads(contents['training-kit-manifest.json'])
            kit = manifest['assets']['training-kit']
            provenance = kit['placement_provenance']
            self.assertEqual(provenance['warehouse_id'], 3)
            self.assertEqual(provenance['prefab_guid'], 'a34b46db1f3f9514bb8df822943d958f')
            sources = provenance['sources']
            self.assertEqual(sources['emergencyKit2']['references'],
                             sources['emergencyKit3']['references'])
            self.assertEqual(sources['emergencyKit2']['references'], [
                [['e34ea7833f1d2146b84e98f030a6bf24', 4300004]],
                [['68f6847d3b647e438de10e0b82ba597e', 2100000]]])
            self.assertEqual(len(kit['parts']), 1)
            part = kit['parts'][0]
            self.assertEqual(part['triangles'], 1572)
            self.assertEqual(part['vertices'], 1436)
            self.assertEqual([part['matrix'][i][3] for i in range(3)], [7, 1.5, 12])
            self.assertAlmostEqual(part['matrix'][0][0], .5, places=6)
            self.assertAlmostEqual(part['matrix'][0][2], math.sqrt(3)/2, places=6)
            self.assertAlmostEqual(part['matrix'][2][0], -math.sqrt(3)/2, places=6)
            self.assertAlmostEqual(exporter.determinant(part['matrix']), 1, places=6)
            self.assertFalse(provenance['box_collider']['m_IsTrigger'])
            self.assertEqual(sum(line.startswith(b'f ') for line in contents['training-kit.obj'].splitlines()), 1572)
            self.assertEqual(len(kit['materials']), 1)
            self.assertNotIn('room.obj', manifest['files'])
            for name, info in manifest['files'].items():
                self.assertEqual(info['sha256'], hashlib.sha256(contents[name]).hexdigest())

    def test_repeatable_room_material_collision_and_geometry_export(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)/'recovered'
            command = [sys.executable, str(SCRIPT), '--game-dir', str(GAME),
                       '--output', str(output), '--no-textures']
            first = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            contents = {p.name: p.read_bytes() for p in output.iterdir()}
            second = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertEqual(contents, {p.name: p.read_bytes() for p in output.iterdir()})
            manifest = json.loads(contents['manifest.json'])
            room = manifest['assets']['room']
            visual = [p for p in room['parts'] if not p['collision']]
            collision = [p for p in room['parts'] if p['collision']]
            self.assertEqual(len(visual), 2)
            self.assertEqual(len(collision), 3)
            self.assertEqual(sum(p['triangles'] for p in visual), 55618)
            self.assertEqual(sum(p['triangles'] for p in collision), 56446)
            self.assertEqual(len(room['materials']), 22)
            for p in visual:
                self.assertEqual(len(p['materials']), p['submeshes'])
                self.assertEqual([p['matrix'][i][3] for i in range(3)], [12, 1.5, 12])
            self.assertEqual(sum(line.startswith(b'f ') for line in contents['room.obj'].splitlines()), 55618)
            self.assertEqual(sum(line.startswith(b'f ') for line in contents['room-collision.obj'].splitlines()), 56446)
            axe = manifest['assets']['wood-axe']['parts']
            self.assertEqual(len(axe), 1)  # Do not instantiate the duplicate imported root.
            self.assertEqual(axe[0]['game_object_path_id'], 6)
            self.assertEqual(axe[0]['triangles'], 228)
            for name, info in manifest['files'].items():
                self.assertEqual(info['sha256'], hashlib.sha256(contents[name]).hexdigest())


if __name__ == '__main__':
    unittest.main()
