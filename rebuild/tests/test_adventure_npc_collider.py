#!/usr/bin/env python3
"""Source-to-native supplier collider checks; requires UnityPy, no installed game."""
import json
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'rebuild/tools'))
from export_adventure_npc_collider import generate, OUTPUT
import UnityPy


class SupplierColliderTest(unittest.TestCase):
    def test_generated_source_and_exported_binary(self):
        text, binary, source = generate()
        self.assertEqual(OUTPUT.read_text(), text)
        self.assertEqual(len(binary), 36)
        fields = struct.unpack('<4sI5fII', binary)
        self.assertEqual(fields, (b'PESC', 1, 0., .875, 0., .3700000047683716, 1.75, 1, 1))
        self.assertEqual(source['physical']['component'], 13600000)
        self.assertEqual(source['defence']['component'], 13690738)
        self.assertEqual(source['defence']['center'], [0., 1., 0.])
        self.assertEqual((source['defence']['radius'], source['defence']['height']), (.5, 2.))
        folder = ROOT/'rebuild/build/reference-assets/adventure-npc-collision'
        if folder.exists():
            self.assertEqual((folder/'supplier-collider.pesc').read_bytes(), binary)
            self.assertEqual(json.loads((folder/'supplier-collider.json').read_text()), source)

    def test_physical_player_npc_collision_policy(self):
        tags = next(iter(UnityPy.load(str(ROOT/'ProjectSettings/TagManager.asset')).objects)).read_typetree()['layers']
        matrix = next(iter(UnityPy.load(str(ROOT/'ProjectSettings/DynamicsManager.asset')).objects)).read_typetree()['m_LayerCollisionMatrix']
        self.assertEqual((tags[8], tags[9], tags[10]), ('AIPlayer', 'Show Model', 'Player'))
        self.assertEqual(matrix[10] & (1 << 10), 0)
        self.assertEqual(matrix[9] & (1 << 9), 0)
        self.assertNotEqual(matrix[8] & (1 << 10), 0)
        self.assertNotEqual(matrix[10] & (1 << 8), 0)


if __name__ == '__main__':
    unittest.main()
