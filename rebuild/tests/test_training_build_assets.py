#!/usr/bin/env python3
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from export_training_build_assets import beam_query, main
from export_reference_assets import IDENTITY


class BuildingAssets(unittest.TestCase):
    def test_ray_origin_is_fifteen_metres_behind_and_normalized(self):
        matrix = [row[:] for row in IDENTITY]
        matrix[0][0] = 0.; matrix[2][0] = -1.000004
        matrix[0][3],matrix[1][3],matrix[2][3] = 11.5,3.5,13.5
        query = beam_query(matrix)
        self.assertEqual(query['origin'],[11.5,3.5,28.5])
        self.assertEqual(query['direction'],[0.,0.,-1.])
        self.assertEqual((query['distance'],query['layer'],query['mask']),(20.,12,4096))
        matrix[2][0] = 0.
        with self.assertRaises(ValueError): beam_query(matrix)
        matrix[2][0] = float('nan')
        with self.assertRaises(ValueError): beam_query(matrix)

    def test_atomic_fresh_output_and_original_guard(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); output = root/'out'; game = root/'game'
            def fail(game,staging):
                (staging/'partial.obj').write_text('partial')
                raise ValueError('bad source')
            args = ['export','--output',str(output),'--game-dir',str(game)]
            with patch('sys.argv',args),patch('export_training_build_assets.recover',side_effect=fail):
                with self.assertRaises(ValueError): main()
            self.assertFalse(output.exists());self.assertEqual(list(root.iterdir()),[])
            for blocked in [game,game/'new']:
                with patch('sys.argv',['export','--output',str(blocked),'--game-dir',str(game)]),patch('export_training_build_assets.recover') as recover:
                    with self.assertRaises(ValueError): main()
                    recover.assert_not_called()
            output.mkdir();(output/'keep').write_text('original')
            with patch('sys.argv',args),patch('export_training_build_assets.recover') as recover:
                with self.assertRaises(ValueError): main()
                recover.assert_not_called()
            self.assertEqual((output/'keep').read_text(),'original')

    def test_optional_real_assets(self):
        directory = Path(__file__).resolve().parents[1]/'build/reference-assets/building'
        if not (directory/'manifest.json').exists(): self.skipTest('Optional original assets not exported')
        report = json.loads((directory/'manifest.json').read_text())
        self.assertTrue(report['source_installed_transforms_match'])
        self.assertEqual(len(report['props']),4);self.assertEqual(len(report['beams']),50)
        self.assertEqual({e['name'] for e in report['effects']},{'effect_true','effect_false'})
        queries = [b['query'] for b in report['beams']]
        self.assertEqual(sum(q['direction'][2]<-.99 for q in queries),25)
        self.assertEqual(sum(q['direction'][1]<-.99 for q in queries),25)
        self.assertEqual(len({tuple(round(v,4) for v in q['position']) for q in queries}),50)
        for query in queries:
            self.assertAlmostEqual(math.dist(query['position'],query['origin']),15.)
            self.assertEqual(query['mask'],4096)
        for prop in report['props']:
            lines = (directory/prop['obj']).read_text().splitlines()
            self.assertEqual(lines[0],'# Baked Unity XYZ meters; original vertex/normal coordinate convention')
            mtl = directory/(Path(prop['obj']).stem+'.mtl')
            self.assertEqual(mtl.read_text().splitlines()[0],'# Source material slots; diffuse-only preview conversion')
            self.assertEqual(sum(s.startswith('v ') for s in lines),prop['vertices'])
            self.assertEqual(sum(s.startswith('f ') for s in lines),prop['triangles'])
            if prop['name'].startswith('receiver-'): self.assertEqual(prop['triangles'],12)
            lo,hi=prop['world_bounds'];self.assertTrue(all(a<=b for a,b in zip(lo,hi)));self.assertTrue(any(a<b for a,b in zip(lo,hi)))
        for collider in report['receiver_colliders']:
            self.assertEqual(collider['layer'],12)
            self.assertTrue(collider['fields']['m_Enabled']);self.assertFalse(collider['fields']['m_IsTrigger'])
        for material in report['materials']:
            if 'diffuse_file' in material:
                self.assertTrue((directory/material['diffuse_file']).read_bytes().startswith(b'\x89PNG\r\n\x1a\n'))

if __name__=='__main__':unittest.main()
