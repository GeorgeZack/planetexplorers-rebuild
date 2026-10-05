"""Source-atlas boundary/orientation checks; UnityPy assets checked separately."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('terrain_export', ROOT/'rebuild/tools/export_terrain_assets.py')
terrain = importlib.util.module_from_spec(spec)
spec.loader.exec_module(terrain)


class TerrainAssetsTests(unittest.TestCase):
    def test_all_tiles_are_unique_and_bounded(self):
        slots = [terrain.material_slot(i) for i in range(1,85)]
        self.assertEqual(len({(s['atlas'],s['column'],s['row_from_bottom']) for s in slots}),84)
        self.assertEqual([sum(s['atlas']==a for s in slots) for a in range(6)], [16,16,16,16,4,16])
        for s in slots:
            self.assertLess(s['column'],s['grid'])
            self.assertLess(s['row_from_bottom'],s['grid'])

    def test_source_shader_boundaries(self):
        expected = {1:(0,0,0),16:(0,3,3),17:(1,0,0),64:(3,3,3),
                    65:(4,0,0),68:(4,1,1),69:(5,0,0),84:(5,3,3)}
        for material, value in expected.items():
            s = terrain.material_slot(material)
            self.assertEqual((s['atlas'],s['column'],s['row_from_bottom']),value)
            large = 65 <= material <= 68
            self.assertEqual(s['repetition'],10. if large else 3.)
            self.assertEqual(s['inset_fraction'],.015625 if large else .03125)

    def test_no_unknown_id_aliasing(self):
        for material in [-1,0,85,255,None,True,1.0]:
            with self.assertRaises(ValueError):
                terrain.material_slot(material)

    def test_procedural_palette_distinct_from_original_ids(self):
        self.assertEqual(terrain.ALIASES, {'dirt':1,'stone':17,'grass':3})
        self.assertNotEqual(terrain.ALIASES['stone'],2)

    def test_existing_export_orientation_and_provenance(self):
        try:
            from PIL import Image
        except ImportError:
            self.skipTest('Pillow unavailable; run with asset-tools Python')
        import json
        output=terrain.DEFAULT_OUTPUT
        if not (output/'manifest.json').is_file():
            self.skipTest('Recovered terrain assets unavailable')
        report=json.loads((output/'manifest.json').read_text())
        self.assertEqual(report['format'],'PE-terrain-assets-1')
        self.assertEqual(len(report['textures']),13)
        self.assertEqual(report['shader'],'VoxelMultiMaterial_Def')
        for record in report['textures']:
            path=output/record['file']
            self.assertEqual(terrain.sha256(path),record['sha256'])
            with Image.open(path) as image:
                self.assertEqual(image.size,(record['width'],record['height']))
        # Native PNG row0 is the original atlas bottom, where grass material3
        # lives. Comparing coarse means tolerates installed DXT compression.
        with Image.open(output/'diffuse0.png') as installed, Image.open(ROOT/'Assets/Terrain/textures/terrainset1_d.jpg') as source:
            source=source.transpose(Image.Transpose.FLIP_TOP_BOTTOM).convert('RGB').resize((8,8))
            installed=installed.convert('RGB').resize((8,8))
            error=sum(abs(a-b) for a,b in zip(source.tobytes(),installed.tobytes()))/(8*8*3)
            self.assertLess(error,4.0)


if __name__=='__main__':
    unittest.main()
