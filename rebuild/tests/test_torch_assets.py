#!/usr/bin/env python3
"""Check source Torch metadata and the installed asset manifest independently."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'rebuild/tools'))
import export_torch_assets as exporter

class TorchAssets(unittest.TestCase):
    def setUp(self):
        self.data=json.loads((ROOT/'rebuild/generated/pe_torch_source.json').read_text())

    def test_generated_header_matches_metadata(self):
        for name,value in exporter.generated_outputs(self.data).items():
            self.assertEqual((ROOT/'rebuild/generated'/name).read_bytes(),value)

    def test_source_hashes_are_current(self):
        for entry in [self.data['prefab'],self.data['database'],self.data['physics'],*self.data['scripts'].values()]:
            self.assertEqual(hashlib.sha256((ROOT/entry['path']).read_bytes()).hexdigest(),entry['sha256'])

    def test_bounded_source_contract(self):
        self.assertEqual(self.data['item_id'],10)
        self.assertEqual(self.data['placement']['max_distance'],10)
        self.assertFalse(self.data['placement']['item_bounds'])
        self.assertFalse(self.data['placement']['physical_blocker'])
        self.assertEqual(self.data['pickup']['reward_item'],1001)
        self.assertEqual(self.data['pickup']['reward_count'],1)
        self.assertTrue(self.data['pickup']['destroys_original_instance'])
        self.assertFalse(self.data['burn']['fuel_timer'])
        self.assertAlmostEqual(self.data['light']['position'][2],.45,places=6)
        self.assertAlmostEqual(self.data['flame_position'][2],.41,places=6)
        self.assertEqual(self.data['light']['range'],20)
        self.assertAlmostEqual(self.data['light']['intensity'],1.2,places=6)
        for capsule in self.data['query_capsules']:
            self.assertTrue(capsule['trigger'])
            self.assertAlmostEqual(capsule['radius'],.08,places=6)
            self.assertAlmostEqual(capsule['height'],.7,places=6)

    def test_changed_size_curve_fails_closed(self):
        changed=copy.deepcopy(self.data)
        changed['particle_systems']['19800004']['SizeModule']['curve']['maxCurve']['m_Curve'][0]['outSlope']=1
        with self.assertRaisesRegex(ValueError,'Unsupported source flame size curve'):
            exporter.generated_outputs(changed)

    def test_installed_asset_hashes(self):
        directory=ROOT/'rebuild/build/reference-assets/torch'
        if not (directory/'torch.json').exists():
            self.skipTest('Recovered Torch assets not installed')
        manifest=json.loads((directory/'torch.json').read_text())
        self.assertEqual(manifest['source'],self.data)
        self.assertEqual(len(manifest['visual']['parts']),1)
        self.assertEqual(manifest['visual']['parts'][0]['matrix'],[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]])
        self.assertEqual(set(manifest['particle_materials']),{'fx_flame01','fx_smoke02','fx_rubble02'})
        for name,digest in manifest['files'].items():
            self.assertEqual(hashlib.sha256((directory/name).read_bytes()).hexdigest(),digest,name)

if __name__=='__main__':
    unittest.main()
