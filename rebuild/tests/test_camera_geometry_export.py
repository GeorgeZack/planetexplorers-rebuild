#!/usr/bin/env python3
"""Standard-library camera graph qualification, drift and regeneration checks."""
import copy
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_camera_geometry as ex

class CameraGeometryExport(unittest.TestCase):
    def test_exact_regeneration(self):
        for name,data in ex.build_outputs().items():self.assertEqual((ex.ROOT/'rebuild/generated'/name).read_bytes(),data)

    def test_active_graph_and_contract(self):
        m=json.loads(ex.build_outputs()['pe_camera_geometry_source.json'])
        self.assertAlmostEqual(m['graph']['incoming_near'],.8,places=6)
        self.assertEqual(m['graph']['fov_input'],'Original Fov')
        self.assertEqual(m['graph']['normal_path'],[0,25,2,1,24,3,4,5,23,27,9,17,18,13,26,29])
        self.assertEqual(m['contract']['mask'],8468481)
        self.assertTrue(m['contract']['ignore_triggers'])
        self.assertEqual(m['contract']['rings'],[dict(radius=0,iterations=4),dict(radius=1.5,iterations=4),dict(radius=3,iterations=8)])

    def test_active_graph_mutations_rejected(self):
        source=ex.graph(ex.ROOT/ex.UPDATE)
        for what in ('near','fov','follow','geometry','blend','final'):
            value=copy.deepcopy(source);p=value['poses']
            if what=='near':p[2]['modifier']['output']['slots']['Nearclip']['value']['number']=.3
            elif what=='fov':p[2]['modifier']['nodes'][0]['slots']['Var']['value']['text']='Fov'
            elif what=='follow':p[1]['poses']['Prev']=0
            elif what=='geometry':p[3]['kind']='CameraForge.WaterClamp'
            elif what=='blend':p[23]['slots']['Index']['value']['number']=1
            else:value['final']['poses']['Final']=26
            with self.subTest(mutation=what),self.assertRaises(ValueError):ex.qualify_graph(value)

    def test_source_mutation_rejected(self):
        with tempfile.TemporaryDirectory(dir=ex.ROOT/'rebuild/build') as d:
            root=Path(d)
            for name in ex.GUARDS:
                target=root/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ex.ROOT/name,target)
            ex.validate_sources(root)
            mutations=[('Assets/PeCamera/Scripts/ScriptModifier/GeometryClamp.cs',b'else if (r < 1f)',b'else if (r < 3f)'),
                ('Assets/Camera Forge/Scripts/Utilities/Utils.cs',b'max - min > 0.005f',b'max - min > 0.01f'),
                ('Assets/Scripts/Lilj/SystemSettingData.cs',b'CameraFov',b'CameraFovChanged')]
            for name,old,new in mutations:
                target=root/name;before=target.read_bytes();self.assertIn(old,before);target.write_bytes(before.replace(old,new,1))
                with self.subTest(source=name),self.assertRaises(ValueError):ex.build_outputs(root)
                target.write_bytes(before)

    def test_cli_check_rejects_stale_output(self):
        with tempfile.TemporaryDirectory(dir=ex.ROOT/'rebuild/build') as d:
            directory=Path(d)
            self.assertEqual(ex.main(['--output',str(directory)]),0)
            self.assertEqual(ex.main(['--output',str(directory),'--check']),0)
            (directory/'pe_camera_geometry_source.h').write_text('stale')
            self.assertEqual(ex.main(['--output',str(directory),'--check']),1)

if __name__=='__main__':unittest.main()
