#!/usr/bin/env python3
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_animfollow as e
class Evidence(unittest.TestCase):
    def test_pinned_evidence(self):
        d=e.load_evidence();self.assertEqual(d['authority'],'shipped-artifact');self.assertIn('Base Layer.Ragdoll.GetupFront',e.hash_names(d))
    def test_evidence_mutations(self):
        original=json.loads((e.ROOT/e.EVIDENCE).read_text())
        for field in ('il','fields'):
            with self.subTest(field=field),tempfile.TemporaryDirectory() as d:
                root=Path(d);p=root/e.EVIDENCE;p.parent.mkdir(parents=True);data=copy.deepcopy(original);part=data['assemblies']['Assembly-CSharp.dll']
                if field=='il':part['methods'][0]['raw_il_hex']='00'+part['methods'][0]['raw_il_hex'][2:]
                else:part['fields'][0]['serialize_field']=not part['fields'][0]['serialize_field']
                p.write_text(json.dumps(data))
                with self.assertRaises(ValueError):e.load_evidence(root)
    def test_decoder_bounds(self):
        fields=e.load_evidence()['assemblies']['Assembly-CSharp.dll']['fields']
        with self.assertRaises(ValueError):e.decode_component(bytes(32),fields,('AnimFollow.RagdollControl_AF',))
@unittest.skipUnless(importlib.util.find_spec('UnityPy'),'optional UnityPy unavailable')
class Installed(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.evidence=e.load_evidence();cls.components=e.recover_components(cls.evidence)
    def test_serialized_settings(self):
        c=self.components['PERagdollController'];v=c['values'];self.assertEqual(c['layout'][-1]['end'],564)
        self.assertEqual(v['getupDrag'],17);self.assertEqual(v['wakeUpStrength'],.5);self.assertAlmostEqual(v['maxErrorWhenMatching'],.3)
        f=self.components['AnimFollow_AF'];self.assertEqual(f['layout'][-1]['end'],2496);self.assertEqual(f['values']['secondaryUpdate'],2)
    def test_complete_payload_mutation(self):
        # Same-size mutations remain parseable but must fail the installed hash
        # gate; partial/trailing payloads fail the decoder itself.
        assets=e.Assets(e.DATA);objects=assets.file('resources.assets').objects
        for cls,(identity,_) in e.OBJECTS.items():
            raw=objects[identity].get_raw_data();fields=self.evidence['assemblies']['Assembly-CSharp.dll']['fields']
            classes=('AnimFollow.RagdollControl_AF','PERagdollController') if cls=='PERagdollController' else ('AnimFollow.AnimFollow_AF',)
            changed=bytearray(raw);changed[-1]^=1
            with self.assertRaises(ValueError):e.qualify_component(changed,cls,self.evidence)
            for changed in (raw[:-1],raw+b'\0'):
                with self.assertRaises(ValueError):e.decode_component(changed,fields,classes)
    def test_regeneration(self):
        outputs=e.build_outputs()
        for name,data in outputs.items():self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
        data=json.loads(outputs['pe_animfollow_source.json']);self.assertTrue(all(not Path(s['path']).is_absolute() for s in data['installed_sources']))
if __name__=='__main__':unittest.main()
