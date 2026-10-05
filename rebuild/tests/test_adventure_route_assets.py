"""Bounded source-town building manifests; no game or graphics launch."""
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_adventure_route_assets as tool
from read_reference_adventure import DEFAULT_SAVE


def encoded(towns):
    def integer(n):return struct.pack('<i',n)
    return integer(20160902)+integer(0)+integer(len(towns))+b''.join(integer(t)+integer(len(ids))+b''.join(integer(i) for i in ids) for t,ids in towns)


class RouteAssetsTests(unittest.TestCase):
    def test_alive_records_keep_town_identity(self):
        self.assertEqual(tool.town_buildings(encoded([(0,[20001,20003]),(4,[21000])])),{0:[20001,20003],4:[21000]})
        for towns in ([(0,[20001,20001])],[(0,[20001]),(4,[20001])],[(0,[]),(0,[])],[(-1,[1])],[(4,[0])]):
            with self.assertRaises(ValueError):tool.town_buildings(encoded(towns))
        with self.assertRaises(ValueError):tool.town_buildings(encoded([(0,[20001])])[:-1])
        with self.assertRaises(ValueError):tool.town_buildings(encoded([])+b'\0')

    def test_compact_visual_keeps_exact_triangle_attributes_and_winding(self):
        import tempfile
        from export_adventure_settlement_assets import write_obj
        from export_reference_assets import material_symbols
        key=(20021,1);part=dict(entity_id=20021,component_id=1,collision=False,
            vertices=[[99,99,99],[1,2,3],[4,5,6],[7,8,9]],normals=[[0,1,0],[1,0,0],[0,0,1],[0,-1,0]],
            uvs=[[99,99],[.1,.2],[.3,.4],[.5,.6]],submeshes=[[(3,1,2)]],materials=[key])
        materials={key:dict(uv_scale=[2,3],uv_offset=[.1,.2])}
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'visual.obj'
            stats=write_obj(path,[part],material_symbols([key]),materials,False,compact=True)
            self.assertEqual(stats['vertices'],3);self.assertEqual(stats['triangles'],1)
            text=path.read_text();self.assertNotIn('99',text)
            self.assertIn('v 1 2 3\n',text);self.assertIn('vn 1 0 0\n',text)
            self.assertIn('vt 0.3 0.8\n',text);self.assertIn('f 3/3/3 1/1/1 2/2/2\n',text)

    @unittest.skipUnless(DEFAULT_SAVE.is_dir(),'preserved source save0 required; skip is not archive validation')
    def test_original_starting_archive_cannot_invent_annot_buildings(self):
        with self.assertRaisesRegex(ValueError,'loaded living'):tool.build_manifest(DEFAULT_SAVE)
        m=tool.build_manifest(DEFAULT_SAVE,require_annot=False)
        self.assertEqual(len(m['buildings']),6)
        self.assertEqual(m['crop_origin'],[-640.,0.,64.])
        self.assertEqual({b['town_id'] for b in m['buildings']},{0})
        self.assertEqual(m['buildings'][0]['position'],[-583.0,27.5,356.0])

if __name__=='__main__':unittest.main()
