"""Exact static-model binding and shared source material qualification."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'rebuild/tools'))
import export_reference_materials as m


class ReferenceMaterials(unittest.TestCase):
    def test_exact_order_texture_and_input_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);obj=root/'model.obj';mtl=root/'settlement.mtl';png=root/'a.png'
            obj.write_bytes(b'exact mesh');mtl.write_text('newmtl second\nmap_Kd a.png\nnewmtl first\n');png.write_bytes(b'exact PNG')
            materials=[dict(symbol='first',texture_file=None),dict(symbol='second',texture_file='a.png')]
            rules={'first':dict(mode='opaque',cutoff=0,alpha=1,two_sided=False),'second':dict(mode='cutout',cutoff=.7,alpha=1,two_sided=True)}
            text=m.sidecar(obj,mtl,materials,rules)
            self.assertIn('second a.png cutout 0.7 1 1',text.splitlines()[1]);self.assertIn('first - opaque 0 1 0 0000000000000000',text.splitlines()[2])
            obj.write_bytes(b'changed mesh');self.assertNotEqual(text.splitlines()[0],m.sidecar(obj,mtl,materials,rules).splitlines()[0])
            materials[1]['texture_file']='wrong.png'
            with self.assertRaisesRegex(ValueError,'texture differs'):m.sidecar(obj,mtl,materials,rules)
    def test_duplicate_and_unknown_materials_reject(self):
        with tempfile.TemporaryDirectory() as directory:
            mtl=Path(directory)/'settlement.mtl';mtl.write_text('newmtl duplicate\nnewmtl duplicate\n')
            with self.assertRaisesRegex(ValueError,'duplicate'):m.material_order(mtl)
    def test_installed_source_rules_and_hashes(self):
        try:import UnityPy
        except ImportError:self.skipTest('UnityPy unavailable')
        root=ROOT/'rebuild/build/reference-assets'
        if not (root/'adventure-route/route-manifest.json').is_file():self.skipTest('Recovered settlement unavailable')
        resolver=m.Resolver(Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers/PE_Client_Data')
        for group,obj_name,manifest in [('adventure-settlement','settlement.obj','manifest.json'),('adventure-route','route.obj','route-manifest.json')]:
            path=root/group/obj_name;text,report=m.recover(path,path.parent/manifest,resolver)
            self.assertEqual(Path(str(path)+'.materials').read_text(),text)
            self.assertEqual(m.digest(path),report['obj_sha256'])
            rules={r['name']:r for r in report['materials']}
            for name in ('Town2_store01','town2_dwelling01B'):
                rule=rules[name];self.assertEqual(rule['mode'],'cutout');self.assertAlmostEqual(rule['cutoff'],.7)
                self.assertTrue(rule['two_sided']);self.assertTrue(rule['renderers'][0]['replacement']['transform_chain'])


if __name__=='__main__':unittest.main()
