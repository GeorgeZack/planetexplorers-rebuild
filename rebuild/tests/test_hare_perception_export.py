#!/usr/bin/env python3
import importlib.util
import json
import math
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'rebuild/tools'))
import export_hare_perception as export
from export_reference_assets import transform_point
from export_reference_hitboxes import skeleton_data

class Pure(unittest.TestCase):
    def test_direction_rejects_nonconformal_samples(self):
        self.assertEqual(export.conformal_error([[2,0,0,7],[0,2,0,8],[0,0,2,9],[0,0,0,1]]),0)
        self.assertGreater(export.conformal_error([[2,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]]),.4)
        with self.assertRaises(ValueError):export.conformal_error([[0,0,0,0]]*4)
        with self.assertRaises(ValueError):export.conformal_error([[float('nan'),0,0,0]]*4)
    def test_generated_contract(self):
        report=json.loads((export.OUTPUT/'pe_hare_perception_source.json').read_text())
        self.assertEqual(export.render_header(report),(export.OUTPUT/'pe_hare_perception_source.h').read_text())
        self.assertEqual(report['block_layers'],[11,12,13,14,16,21])
        self.assertIn('TransformDirection',report['limitations'][1])

@unittest.skipUnless(importlib.util.find_spec('UnityPy'),'optional UnityPy unavailable')
class Recovered(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.assets=ROOT/'rebuild/build/reference-assets'
        cls.bundle=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'/export.actor.BUNDLE
        if not cls.bundle.is_file() or not (cls.assets/'hare.peac').is_file():raise unittest.SkipTest('recovered hare bundle/actor unavailable')
        cls.report=export.recover(cls.bundle,cls.assets)
    def test_exact_mount_and_settings(self):
        report=self.report;v=report['sensors']['PEVision'];h=report['sensors']['PEHearing']
        self.assertEqual((v['joint'],v['radius'],v['angle'],v['axis']),(2,20,60,[-.5,1,0]))
        self.assertEqual((h['joint'],h['radius'],h['ignore_block']),(2,15,False))
        self.assertEqual(v['transform'],h['transform']);self.assertTrue(v['enabled'] and h['enabled'] and v['active'] and h['active'])
        self.assertTrue(v['path'].endswith('/Bip01 Neck/Bip01 Head'))
        _,fingerprint,binds=skeleton_data((self.assets/'hare.peac').read_bytes())
        self.assertEqual(f'{fingerprint:016x}',report['skeleton_hash'])
        for point,want in [(v['bind_origin'],[0,0,0]),(v['bind_axis_point'],v['axis'])]:
            self.assertLess(math.dist(transform_point(binds[v['joint']],point),want),1e-12)
        self.assertLess(v['sampled_conformal_error'],1e-4)
    def test_regeneration(self):
        self.assertEqual(self.report,json.loads((export.OUTPUT/'pe_hare_perception_source.json').read_text()))
        self.assertEqual(export.render_header(self.report),(export.OUTPUT/'pe_hare_perception_source.h').read_text())
    def test_wrong_joint_and_path_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp=Path(tmp);shutil.copyfile(self.assets/'hare.peac',tmp/'hare.peac')
            manifest=json.loads((self.assets/'hare-manifest.json').read_text())
            original=list(manifest['joint_transform_path_ids']);manifest['joint_transform_path_ids'][0],manifest['joint_transform_path_ids'][2]=original[2],original[0]
            (tmp/'hare-manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError,'Joint order'):export.recover(self.bundle,tmp)
            manifest['joint_transform_path_ids']=original
            node=next(n for n in manifest['hierarchy'] if n['path_id']==original[2]);node['path']='wrong/head'
            (tmp/'hare-manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError,'hierarchy path'):export.recover(self.bundle,tmp)
    def test_actor_hash_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp=Path(tmp);shutil.copyfile(self.assets/'hare-manifest.json',tmp/'hare-manifest.json')
            data=bytearray((self.assets/'hare.peac').read_bytes());data[-1]^=1;(tmp/'hare.peac').write_bytes(data)
            with self.assertRaisesRegex(ValueError,'hash mismatch'):export.recover(self.bundle,tmp)

if __name__=='__main__':unittest.main()
