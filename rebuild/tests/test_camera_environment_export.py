import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_camera_environment as tool

class CameraEnvironmentSource(unittest.TestCase):
    def test_reproduction(self):
        for name,data in tool.build_outputs().items():self.assertEqual(data,(tool.ROOT/'rebuild/generated'/name).read_bytes())
    def test_active_graph_not_constructor(self):
        c=json.loads(tool.build_outputs()['pe_camera_environment_source.json'])['contract']
        self.assertEqual(c['graph']['normal_follow']['slots']['Damp Rate']['value']['number'],0.4000000059604645)
        self.assertEqual(c['graph']['build_mode_input']['slots']['Var']['value']['text'],'Build Mode')
        self.assertEqual(c['distance']['maximum_build'],20)
        self.assertEqual(c['recovery']['seconds'],2)
        self.assertEqual(c['activity']['obstacle_mask'],8468481)
        self.assertEqual(c['graph']['initial_distance_nodes'][0]['slots']['Value']['value']['number'],6)
    def test_changed_source_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);name=next(iter(tool.GUARDS));path=root/name;path.parent.mkdir(parents=True)
            path.write_bytes((tool.ROOT/name).read_bytes()+b'x')
            with self.assertRaisesRegex(ValueError,'Camera source changed'):tool.build_outputs(root)
    def test_truncated_or_ambiguous_graph_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'camera.asset';path.write_bytes(b'\x0fControllerAsset')
            with self.assertRaises(ValueError):tool.graph(path)
            path.write_bytes(b'\x0fControllerAsset'*2)
            with self.assertRaises(ValueError):tool.graph(path)

if __name__=='__main__':unittest.main()
