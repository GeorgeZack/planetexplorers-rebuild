#!/usr/bin/env python3
"""Bounded source-mound interpretation fixtures; no Unity installation needed."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_training_gathering as export

class TerrainSourceTests(unittest.TestCase):
    def test_exact_source_samples_and_absolute_coordinates(self):
        samples=export.terrain_samples(export.TERRAIN.read_text(encoding='utf-8-sig'))
        data={tuple(row[:3]):tuple(row[3:]) for row in samples}
        self.assertEqual(len(data),726)
        self.assertEqual(data[(-2,0,2)],(255,1));self.assertEqual(data[(15,0,21)],(255,1))
        self.assertEqual(data[(14,1,20)],(210,1));self.assertEqual(data[(6,3,11)],(190,1))
        self.assertEqual(data[(3,2,9)],(130,1));self.assertEqual(data[(7,3,14)],(110,1))
        self.assertNotIn((0,2,0),data)
        self.assertEqual([sum(p[1]==y for p in data) for y in range(4)],[360,288,59,19])
    def test_unknown_statement_duplicate_and_unbounded_loop_rejected(self):
        original=export.TERRAIN.read_text(encoding='utf-8-sig')
        for modified in [original.replace('i <= 15','i <= 1000',1),
                         original.replace('VFVoxel vol1','Unknown();\nVFVoxel vol1',1),
                         original.replace('VFVoxelTerrain.self.Voxels.SafeWrite(6, 3, 9, vol4);','VFVoxelTerrain.self.Voxels.SafeWrite(6, 3, 11, vol4);',1)]:
            with self.assertRaises(ValueError):export.terrain_samples(modified)
if __name__=='__main__':unittest.main()
