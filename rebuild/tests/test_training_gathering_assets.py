#!/usr/bin/env python3
"""Bounded geometry checks; optional real exported source fixtures."""
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from export_training_gathering_assets import checked_mesh, produced_matrix, check_same_transform, main
from export_reference_assets import transform_point


def transform(position=(0,0,0), scale=(1,1,1)):
    return dict(m_LocalPosition=dict(zip('xyz',position)),m_LocalRotation=dict(x=0.,y=0.,z=0.,w=1.),m_LocalScale=dict(zip('xyz',scale)))


class GatherAssets(unittest.TestCase):
    def test_produced_leaf_preserves_hierarchy(self):
        root=transform((12,1.5,12));root['m_LocalRotation']=dict(x=0.,y=-2**-.5,z=0.,w=2**-.5)
        leaf=transform((-6.31975365,0,-12.35621929),(0,0,0))
        matrix=produced_matrix([root,transform((-.020661,.082331,1.079169)),leaf])
        for actual,want in zip(transform_point(matrix,[0,0,0]),(23.27705029,1.582331,5.65958535)):
            self.assertAlmostEqual(actual,want,places=5)
        self.assertEqual(leaf['m_LocalScale'],dict(x=0,y=0,z=0))
        with self.assertRaises(ValueError):produced_matrix([transform()])
        with self.assertRaises(ValueError):produced_matrix([transform(scale=(0,0,0)),leaf])

    def test_reject_changed_installed_placement(self):
        source=transform();actual=copy.deepcopy(source);check_same_transform(source,actual)
        actual['m_LocalPosition']['x']=.01
        with self.assertRaises(ValueError):check_same_transform(source,actual)
        actual['m_LocalPosition']['x']=float('nan')
        with self.assertRaises(ValueError):check_same_transform(source,actual)

    def test_mesh_validation(self):
        vertices=[[0.,0.,0.],[1.,0.,0.],[0.,1.,0.]];normals=[[0.,0.,1.]]*3;uvs=[[0.,0.]]*3
        checked_mesh(vertices,normals,uvs,[[(0,1,2)]])
        for faces in [[[(0,1,3)]],[[(0,-1,2)]],[[(0,1)]]]:
            with self.assertRaises(ValueError):checked_mesh(vertices,normals,uvs,faces)
        with self.assertRaises(ValueError):checked_mesh(vertices,normals[:1],uvs,[[(0,1,2)]])
        vertices[0][0]=float('inf')
        with self.assertRaises(ValueError):checked_mesh(vertices,normals,uvs,[[(0,1,2)]])

    def test_failed_export_does_not_publish_or_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            output=Path(temporary)/'output';args=['export','--output',str(output),'--game-dir',str(Path(temporary)/'original')]
            def failure(game,staging):
                (staging/'partial.obj').write_text('partial')
                raise ValueError('invalid source')
            with patch('sys.argv',args),patch('export_training_gathering_assets.recover',side_effect=failure):
                with self.assertRaises(ValueError):main()
            self.assertFalse(output.exists());self.assertEqual(list(Path(temporary).iterdir()),[])
            output.mkdir();(output/'sentinel').write_text('keep')
            with patch('sys.argv',args),patch('export_training_gathering_assets.recover') as recover:
                with self.assertRaises(ValueError):main()
                recover.assert_not_called()
            self.assertEqual((output/'sentinel').read_text(),'keep')

    def test_optional_actual_export(self):
        directory=Path(__file__).resolve().parents[1]/'build/reference-assets/gathering'
        if not (directory/'manifest.json').exists():self.skipTest('Optional original assets not exported')
        report=json.loads((directory/'manifest.json').read_text())
        self.assertTrue(report['source_installed_transforms_match']);self.assertEqual(len(report['props']),6)
        counts=[(1285,1373),(1136,1275),(628,592),(1581,2215),(493,510),(3150,4216)]
        for index,(prop,(vertices,triangles)) in enumerate(zip(report['props'],counts)):
            lines=(directory/prop['obj']).read_text().splitlines()
            self.assertEqual(lines[0],'# Baked Unity XYZ meters; original vertex/normal coordinate convention')
            material_file=next(line.split()[1] for line in lines if line.startswith('mtllib '))
            mtl=(directory/material_file).read_text().splitlines()
            self.assertEqual(mtl[0],'# Source material slots; diffuse-only preview conversion')
            self.assertEqual({line.split()[1] for line in lines if line.startswith('usemtl ')},
                             {line.split()[1] for line in mtl if line.startswith('newmtl ')})
            self.assertEqual(sum(s.startswith('v ') for s in lines),vertices)
            self.assertEqual(sum(s.startswith('f ') for s in lines),triangles)
            self.assertEqual(prop['vertices'],vertices);self.assertEqual(prop['triangles'],triangles)
            lo,hi=prop['world_bounds'];self.assertTrue(all(a<b for a,b in zip(lo,hi)))
            if index<5:
                self.assertEqual(prop['gather_index'],index)
                self.assertAlmostEqual(prop['gather_world_position'][1],1.882331,places=5)
        self.assertEqual(len(report['materials']),4)
        for material in report['materials']:
            self.assertTrue((directory/material['diffuse_file']).read_bytes().startswith(b'\x89PNG\r\n\x1a\n'))

if __name__=='__main__':unittest.main()
