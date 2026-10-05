#!/usr/bin/env python3
"""Headless math/schema tests; optional original bundle recovery needs UnityPy."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0, str(TOOLS))
import export_reference_actor as actor

GAME = Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'
HAS_SOURCE = importlib.util.find_spec('UnityPy') is not None and (GAME/actor.BUNDLE).is_file()


def binding(path, attribute):
    return dict(path=path, attribute=attribute, classID=4, customType=0,
                isPPtrCurve=0, script={'m_FileID': 0, 'm_PathID': 0})


class ActorMathTests(unittest.TestCase):
    def test_root_placement_cancels_and_inverse_bind_is_applied_once(self):
        root = actor.local_matrix([10, 20, 30], [0, math.sqrt(.5), 0, math.sqrt(.5)], [2, 2, 2])
        joint = actor.local_matrix([0, 3, 0], [0, 0, 0, 1], [1, 1, 1])
        bind = actor.local_matrix([0, -2, 0], [0, 0, 0, 1], [1, 1, 1])
        nodes = [{'path_id': 1, 'parent': 0}, {'path_id': 2, 'parent': 1}]
        palette = actor.palette_for(nodes, {1: root, 2: joint}, [2], [bind], 1)
        result = actor.skin_position([4, 5, 6], [1, 0, 0, 0], [0, 0, 0, 0], palette)
        for actual, expected in zip(result, [4, 6, 6]):
            self.assertAlmostEqual(actual, expected)
        inverse = actor.inverse_affine(root)
        product = actor.multiply(inverse, root)
        for row in range(4):
            for column in range(4):
                self.assertAlmostEqual(product[row][column], actor.IDENTITY[row][column])

    def test_dense_then_constant_binding_partition_and_unsupported_rejection(self):
        bindings = [binding(17, 1), binding(17, 2), binding(17, 3)]
        self.assertEqual(actor.binding_channels(bindings, {17: 90}, 3, 7),
                         [(90, 1, 0, 3), (90, 2, 3, 4), (90, 3, 7, 3)])
        for changed, paths, dense, constant in [
                (bindings, {}, 3, 7), (bindings, {17: 90}, 4, 6),
                (bindings, {17: 90}, 3, 6), ([binding(17, 4)], {17: 90}, 3, 0),
                ([binding(17, 1), binding(17, 1)], {17: 90}, 6, 0)]:
            with self.assertRaises(ValueError):
                actor.binding_channels(changed, paths, dense, constant)

    def test_invalid_transform_and_non_topological_hierarchy_rejected(self):
        with self.assertRaises(ValueError):
            actor.inverse_affine([[0.]*4 for _ in range(4)])
        for rotation in ([0, 0, 0, 0], [0, 0, 0, float('nan')]):
            with self.assertRaises(ValueError):
                actor.local_matrix([0, 0, 0], rotation, [1, 1, 1])
        with self.assertRaises(ValueError):
            actor.hierarchy_world([{'path_id': 2, 'parent': 1}], {2: actor.IDENTITY})


@unittest.skipUnless(HAS_SOURCE, 'optional UnityPy and original hare bundle required')
class InstalledActorTests(unittest.TestCase):
    def test_original_dense_samples_binary_schema_motion_loop_and_repeatability(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)/'export'
            output.mkdir()
            (output/'room.obj').write_bytes(b'previous room')
            command = [sys.executable, str(TOOLS/'export_reference_actor.py'), '--game-dir', str(GAME),
                       '--output', str(output)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            files = {p.name: p.read_bytes() for p in output.iterdir()}
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(files, {p.name: p.read_bytes() for p in output.iterdir()})
            self.assertEqual(files['room.obj'], b'previous room')
            manifest = json.loads(files['hare-manifest.json'])
            self.assertEqual(manifest['renderer_path_id'], 6629980832213904343)
            self.assertNotIn('room.obj', manifest['files'])
            self.assertLess(manifest['inverse_bind_reconstruction_max_error'], 1e-12)
            raw = files['hare.peac']
            magic, version, vertices, indices, joints, clips = struct.unpack_from('<4s5I', raw)
            self.assertEqual((magic, version, vertices, indices, joints, clips), (b'PEAC', 1, 1766, 6540, 48, 8))
            offset = 24
            parsed_vertices = []
            for _ in range(vertices):
                values = struct.unpack_from('<12f4H', raw, offset); offset += 56
                self.assertAlmostEqual(sum(values[8:12]), 1, places=6)
                self.assertTrue(all(0 <= i < joints for i in values[12:]))
                parsed_vertices.append(values)
            faces = struct.unpack_from(f'<{indices}I', raw, offset); offset += 4*indices
            self.assertTrue(all(0 <= index < vertices for index in faces))
            offset += joints*48
            expected = [('idle', 72, 2.3333334922790527, 312, 318),
                        ('run', 20, .600000262260437, 310, 310),
                        ('attack0', 32, 1.0, 302, 318),
                        ('attack1', 32, 1.0000009536743164, 302, 318),
                        ('attack2', 47, 1.5000004768371582, 302, 318),
                        ('attack3', 32, 1.0, 302, 318),
                        ('attack4', 34, 1.066667079925537, 298, 322),
                        ('walk', 38, 1.2000000476837158, 309, 311)]
            for report, (name, count, duration, dense, constant) in zip(manifest['clips'], expected):
                name_bytes, fps, frame_count, length, loop = struct.unpack_from('<32sfIfI', raw, offset); offset += 48
                self.assertEqual(name_bytes.split(b'\0')[0].decode(), name)
                self.assertEqual((fps, frame_count, loop), (30., count, int(name in ('idle', 'run', 'walk'))))
                self.assertAlmostEqual(length, duration, places=7)
                self.assertEqual((report['dense_channels'], report['constant_channels']), (dense, constant))
                if loop:
                    self.assertLess(report['loop_endpoint_max_vertex_gap'], .0003 if name == 'walk' else .00005)
                    self.assertGreater(report['max_vertex_displacement_from_frame0'], {'idle': .08, 'run': 1.3, 'walk': .1}[name])
                else:
                    self.assertEqual(report['animation_events'], [])
                    combat = report['combat']
                    self.assertEqual(combat['state'], 'Base Layer.Attack.'+name)
                    self.assertEqual(combat['prepare_effect']['_anms'], name)
                    self.assertEqual(combat['state_speed'], 1)
                    self.assertEqual(combat['clip_animation_events'], [])
                    self.assertGreater(report['max_vertex_displacement_from_frame0'], .1)
                    windows = combat['attack_windows']
                    expected_windows = {
                        'attack0': [(.372, .642, ['Bip01 R Forearm'])],
                        'attack1': [(.372, .642, ['Bip01 L Forearm'])],
                        'attack2': [(.24, .34, ['Bip01 R Forearm']), (.5, .66, ['Bip01 L Forearm'])],
                        'attack3': [(.241, .631, ['Bip01 R Forearm'])],
                        'attack4': [(.419, .7, ['Bip01 L Forearm', 'Bip01 R Forearm'])],
                    }[name]
                    self.assertEqual(len(windows), len(expected_windows))
                    for window, (start, end, bones) in zip(windows, expected_windows):
                        self.assertAlmostEqual(window['start_normalized'], start, places=6)
                        self.assertAlmostEqual(window['end_normalized'], end, places=6)
                        self.assertEqual(window['bones'], bones)
                        self.assertAlmostEqual(window['end_seconds_at_state_speed'], end*duration, places=6)
                        self.assertEqual(window['end_inclusive'], name != 'attack2')
                        self.assertEqual(window['end_event'], 'MonsterEndAttack' if name == 'attack2' else None)
                # Verify serialized samples themselves deform the mesh rather than
                # trusting summary metadata or copying a static palette each frame.
                sample_bounds = None
                first_positions = None
                max_distance = 0
                for frame in range(frame_count):
                    palette = []
                    for _ in range(joints):
                        values = struct.unpack_from('<12f', raw, offset); offset += 48
                        self.assertTrue(all(math.isfinite(v) for v in values))
                        palette.append([list(values[i:i+4]) for i in (0, 4, 8)]+[[0, 0, 0, 1]])
                    posed = [actor.skin_position(v[:3], v[8:12], v[12:], palette) for v in parsed_vertices]
                    sample_bounds = actor.union_bounds(sample_bounds, actor.bounds(posed))
                    if frame == 0:
                        first_positions = posed
                    else:
                        max_distance = max(max_distance, max(math.dist(a, b) for a, b in zip(first_positions, posed)))
                self.assertAlmostEqual(max_distance, report['max_vertex_displacement_from_frame0'], places=5)
                for actual, expected_bound in zip(sum(sample_bounds, []), sum(report['bounds'], [])):
                    self.assertAlmostEqual(actual, expected_bound, places=5)
            self.assertEqual(offset, len(raw))
            self.assertTrue(manifest['combat_provenance']['missing_hurt_death_clips'])
            self.assertFalse(any('death' in name.lower() or 'hurt' in name.lower() for name in manifest['combat_provenance']['controller_state_names']))
            for name, details in manifest['files'].items():
                self.assertEqual(details['sha256'], hashlib.sha256(files[name]).hexdigest())

    def test_missing_or_corrupt_bundle_preserves_existing_outputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); game = root/'game'; output = root/'export'; output.mkdir()
            previous = {'hare.peac': b'previous actor', 'hare-manifest.json': b'previous provenance'}
            for name, value in previous.items():
                (output/name).write_bytes(value)
            command = [sys.executable, str(TOOLS/'export_reference_actor.py'), '--game-dir', str(game), '--output', str(output)]
            for corrupt in (False, True):
                if corrupt:
                    bundle = game/actor.BUNDLE; bundle.parent.mkdir(parents=True)
                    bundle.write_bytes(b'not a Unity bundle')
                result = subprocess.run(command, capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('Actor export failed', result.stderr)
                self.assertEqual(previous, {p.name: p.read_bytes() for p in output.iterdir()})


if __name__ == '__main__':
    unittest.main()
