#!/usr/bin/env python3
import copy
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from export_sandbox_world_assets import HEADER, export, local_mesh, prepare, source_capsule
from export_reference_assets import IDENTITY, transform_point
from export_tree_collision import OUTPUT, generate


class SandboxWorldAssets(unittest.TestCase):
    def fixture(self, matrix=IDENTITY):
        vertices=[[-1,0,0],[1,0,0],[0,4,0]]
        obj=HEADER+'mtllib tree.mtl\n'+''.join('v '+' '.join(map(str,transform_point(matrix,p)))+'\n' for p in vertices)
        obj+='vt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\nvn 0 0 1\nvn 0 0 1\nusemtl mAF\nf 1/1/1 2/2/2 3/3/3\n'
        return obj

    def test_inverse_authored_translation_and_mirror(self):
        matrix=copy.deepcopy(IDENTITY);matrix[0][3]=10;matrix[1][3]=2;matrix[2][3]=-8
        text,bounds=local_mesh(self.fixture(matrix),matrix,3,1)
        self.assertEqual(bounds,[[-1.,0.,0.],[1.,4.,0.]])
        self.assertIn('v -1 0 0\n',text)
        matrix[0][0]=-1
        text,bounds=local_mesh(self.fixture(matrix),matrix,3,1)
        self.assertIn('f 3/3/3 2/2/2 1/1/1\n',text)

    def test_reject_malformed_geometry_before_publication(self):
        for text in [self.fixture().replace('f 1/','f 4/'),self.fixture().replace('v -1','v nan'),
                     self.fixture().replace('tree.mtl','../tree.mtl'),self.fixture()+'garbage\n']:
            with self.assertRaises(ValueError):local_mesh(text,IDENTITY,3,1)
        matrix=copy.deepcopy(IDENTITY);matrix[1][1]=0
        with self.assertRaises(ValueError):local_mesh(self.fixture(),matrix,3,1)
        with self.assertRaises(ValueError):local_mesh(self.fixture(),IDENTITY,4,1)

    def source(self):
        try:import UnityPy
        except ImportError:self.skipTest('Optional UnityPy source-prefab decoder unavailable')
        source=Path(__file__).resolve().parents[1]/'build/reference-assets/gathering'
        if not (source/'manifest.json').is_file():self.skipTest('Recovered gathering fixtures unavailable')
        return source

    def test_source_capsule_mesh_identity_and_binary_layout(self):
        self.source()
        import struct
        data,metadata=source_capsule()
        self.assertEqual(len(data),36)
        self.assertEqual(data[:4],b'PESC')
        fields=struct.unpack('<I5fII',data[4:])
        self.assertEqual(fields[0],1)
        self.assertEqual(fields[2],3)
        self.assertEqual(fields[4:],(1,7,1,1))
        self.assertTrue(metadata['source_mesh_matches_tutorial'])
        self.assertEqual(metadata['mesh_guid'],'71fbbe8530d15a4f8408390cd27df9d4')

    def test_generated_headless_collider_matches_source_and_visual_sidecar(self):
        self.source()
        import re
        import struct
        text,binary=generate()
        self.assertEqual(OUTPUT.read_text(),text)
        values=[float.fromhex(v) for v in re.findall(r'(-?0x[0-9a-f]+\.[0-9a-f]+p[+-][0-9]+)f',text)]
        self.assertEqual(len(values),5)
        self.assertEqual(b'PESC'+struct.pack('<I5fII',1,*values,1,1),binary)
        sidecar=OUTPUT.parents[1]/'build/reference-assets/sandbox-world/tree-collider.pesc'
        if sidecar.is_file():self.assertEqual(sidecar.read_bytes(),binary)

    def test_actual_geometry_roundtrip_texture_identity_and_regeneration(self):
        source=self.source();payload=prepare(source);report=json.loads(payload['manifest.json'])
        before=[list(map(float,line.split()[1:])) for line in (source/'tree.obj').read_text().splitlines() if line.startswith('v ')]
        after=[list(map(float,line.split()[1:])) for line in payload['tree.obj'].decode().splitlines() if line.startswith('v ')]
        self.assertEqual(len(after),3150)
        error=max(abs(a-b) for p,q in zip(after,before) for a,b in zip(transform_point(report['removed_authored_matrix'],p),q))
        self.assertLess(error,1e-7)
        self.assertEqual(payload['material-mAF.png'],(source/'material-mAF.png').read_bytes())
        self.assertTrue(-.247<report['local_bounds'][0][1]<-.246)
        self.assertTrue(7.786<report['local_bounds'][1][1]<7.787)
        with tempfile.TemporaryDirectory(dir=source.parent) as temp:
            output=Path(temp)/'shared';export(source,output);export(source,output,check=True)
            originals={p.name:p.read_bytes() for p in output.iterdir()}
            with self.assertRaises(ValueError):export(source,output)
            self.assertEqual(originals,{p.name:p.read_bytes() for p in output.iterdir()})
            (output/'tree.obj').write_text('broken')
            with self.assertRaises(ValueError):export(source,output,check=True)

    def test_changed_source_material_fails_without_publishing(self):
        source=self.source()
        with tempfile.TemporaryDirectory(dir=source.parent) as temp:
            root=Path(temp);inputs=root/'inputs';inputs.mkdir()
            for name in ('manifest.json','tree.obj','tree.mtl','material-mAF.png'):
                (inputs/name).write_bytes((source/name).read_bytes())
            (inputs/'tree.mtl').write_text((inputs/'tree.mtl').read_text().replace('map_Kd material-mAF.png','map_Kd ../outside.png'))
            with self.assertRaises(ValueError):export(inputs,root/'result')
            self.assertFalse((root/'result').exists())


if __name__=='__main__':unittest.main()
