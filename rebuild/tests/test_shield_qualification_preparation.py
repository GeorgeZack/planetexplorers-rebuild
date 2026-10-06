#!/usr/bin/env python3
"""Offline plan/IK-pair/grid admission checks; no original is launched."""
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import reference_shield_qualification as tool


class PreparationTests(unittest.TestCase):
    def test_two_fresh_clones_reorder_and_repeat_same_independent_phases(self):
        frames=[{'evidence':{'base_normalized_time':10+i+.25,'block_normalized_time':2+i*.1}} for i in range(3)]
        rows=tool.repeat_sequence(frames)
        self.assertEqual([r['config'] for r in rows[:4]],[0,1,2,0])
        self.assertEqual([r['config'] for r in rows[4:]],[2,0,1,2])
        self.assertEqual([r['clone'] for r in rows],[0]*4+[1]*4)
        self.assertEqual(rows[0]['base_normalized_time'],rows[3]['base_normalized_time'])
        self.assertEqual(rows[0]['block_normalized_time'],rows[3]['block_normalized_time'])
        self.assertEqual([r['repetition'] for r in rows],[0,0,0,1,0,0,0,1])
    def test_grid_cross_product_seams_and_withheld_points_are_separate(self):
        rows=tool.grid_sequence();self.assertEqual(len(rows),87)
        for config in range(3):
            group=[r for r in rows if r['config']==config]
            regular=[r for r in group if r['kind']=='grid']
            self.assertEqual(len(regular),16)
            self.assertEqual({(r['base_normalized_time'],r['block_normalized_time']) for r in regular},
                             {(b,k) for b in (0,.25,.5,.75) for k in (0,.25,.5,.75)})
            self.assertEqual(sum(r['kind']=='withheld' for r in group),4)
            self.assertEqual(sum('seam' in r['kind'] for r in group),9)
        self.assertEqual({r['clone'] for r in rows},{0})
    def test_ik_pair_has_only_two_exact_directional_bit_changes(self):
        source=bytearray(50000);source[40]=source[48232]=1;noik=bytearray(source);noik[40]=noik[48232]=0
        self.assertEqual(tool.verify_graph_pair(source,noik),[40,48232])
        for mutation in ('extra','wrong-value','reverse','size'):
            a,b=bytearray(source),bytearray(noik)
            if mutation=='extra':b[123]=1
            elif mutation=='wrong-value':a[40]=2
            elif mutation=='reverse':a,b=b,a
            else:b.append(0)
            with self.subTest(mutation=mutation),self.assertRaises(ValueError):tool.verify_graph_pair(a,b)
    def test_missing_repeatability_never_opens_grid_gate(self):
        with self.assertRaisesRegex(ValueError,'completed repeatability'):tool.repeatability_gate(None,None,'noik','hash')
    def test_grid_gate_reanalyzes_raw_evidence_and_binds_variant(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);capture=root/'closed.jsonl';capture.write_text('source evidence');plan=root/'plan.json'
            plan.write_text(json.dumps({'phase':'repeatability','variant':'noik','bundle_sha256':'expected'}))
            fake=types.ModuleType('analyze_shield_qualification');calls=[]
            def analyze(c,p):calls.append((c,p));return {'repeatability_qualified':True,'grid_preparation_allowed':True}
            fake.analyze=analyze
            with patch.dict(sys.modules,{'analyze_shield_qualification':fake}):
                gate=tool.repeatability_gate(capture,plan,'noik','expected')
                self.assertEqual(calls,[(capture,plan)]);self.assertEqual(gate['capture_sha256'],tool.sha(capture))
                with self.assertRaisesRegex(ValueError,'variant/bundle'):tool.repeatability_gate(capture,plan,'sourceik','expected')
                fake.analyze=lambda c,p:{'repeatability_qualified':False,'grid_preparation_allowed':True}
                with self.assertRaisesRegex(ValueError,'gate failed'):tool.repeatability_gate(capture,plan,'noik','expected')
    def test_pre_evaluation_hook_precedes_animator_creation(self):
        source=(tool.TOOLS/'ShieldQualificationClone.cs').read_text()
        self.assertLess(source.index('beforeEvaluation(original,source,clone.paths,clone.bones)'),source.index('clone.animator=clone.root.AddComponent<Animator>()'))
        self.assertIn('MaxClones=2',source)
        probe=(tool.TOOLS/'ReferenceShieldQualificationProbe.cs').read_text()
        self.assertIn('clone.Release();clone=null;return;',probe)
        self.assertIn('Post-cleanup alive120frames',probe)


if __name__=='__main__':unittest.main()
