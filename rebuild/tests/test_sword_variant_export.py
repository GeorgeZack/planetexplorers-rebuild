#!/usr/bin/env python3
from contextlib import redirect_stderr
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_sword_variant as e
import qualify_sword_variant as oracle
from types import SimpleNamespace
HAVE_UNITY=importlib.util.find_spec('UnityPy') is not None

class SourceTests(unittest.TestCase):
    def test_source_guards(self):self.assertEqual(len(e.validate_sources()),len(e.SOURCE_FILES))
    def mutate(self,path,old,new):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for p in e.SOURCE_FILES:
                target=root/p;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes((e.ROOT/p).read_bytes())
            target=root/path;text=target.read_text(encoding='utf-8-sig');self.assertIn(old,text);target.write_text(text.replace(old,new))
            with self.assertRaises(ValueError):e.validate_sources(root)
    def test_move_strict(self):self.mutate(e.ACTION,'move.velocity.magnitude > m_MoveAttackSpeed','move.velocity.magnitude >= m_MoveAttackSpeed')
    def test_sprint_strict(self):self.mutate(e.ACTION,'move.velocity.magnitude > m_SprintAttackSpeed','move.velocity.magnitude >= m_SprintAttackSpeed')
    def test_air_precedence(self):self.mutate(e.ACTION,'if(m_AttackInAir)','if(m_AttackInWater)')
    def test_missing_motor_water(self):self.mutate(e.ACTION,'null != phyMotor && phyMotor.spineInWater','phyMotor.spineInWater')
    def test_full_velocity(self):self.mutate(e.ACTION,'move.velocity.magnitude','Vector3.ProjectOnPlane(move.velocity, Vector3.up).magnitude')
    def test_cap_is_not_acceleration(self):self.mutate(e.ACTION,'dVelocity.normalized * 12f','dVelocity.normalized * (12f * Time.deltaTime)')
    def test_cap_equality(self):self.mutate(e.ACTION,'dVelocity.magnitude >= 12f','dVelocity.magnitude > 12f')

@unittest.skipUnless(HAVE_UNITY,'optional UnityPy unavailable')
class PrefabTests(unittest.TestCase):
    def test_exact_regeneration_preserves_inputs(self):
        hashes=[hashlib.sha256((e.ROOT/p).read_bytes()).hexdigest() for p in e.SOURCE_FILES]
        outputs=e.build_outputs();self.assertEqual(outputs,e.build_outputs())
        for name,data in outputs.items():self.assertEqual(data,(e.ROOT/'rebuild/generated'/name).read_bytes())
        self.assertEqual(hashes,[hashlib.sha256((e.ROOT/p).read_bytes()).hexdigest() for p in e.SOURCE_FILES])
    def test_instantiated_thresholds_not_class_defaults(self):
        j=json.loads(e.build_outputs()['pe_sword_variant_source.json']);self.assertEqual(j['rule'],dict(move_speed=4.5,sprint_speed=7,velocity_change_limit=12));self.assertEqual(j['binding']['component_path_id'],11400022)
    def test_prefab_threshold_drift(self):
        d,_=e.player.source_definition();objects=e.player.read_prefab(e.ROOT/e.player.PREFAB)
        for field,value in [('m_MoveAttackSpeed',4),('m_SprintAttackSpeed',6)]:
            with self.subTest(field=field):
                changed=copy.deepcopy(objects);changed[11400022]['data']['m_SwordAttack'][field]=value
                with self.assertRaises(ValueError):e.player.qualify(changed,d['script_guid'])
    def test_stale_missing_and_fresh_report_contract(self):
        with tempfile.TemporaryDirectory() as tmp:
            with redirect_stderr(io.StringIO()):self.assertEqual(e.main(['--output',tmp,'--check']),1)
            self.assertEqual(e.main(['--output',tmp]),0);self.assertEqual(e.main(['--output',tmp,'--check']),0)
            (Path(tmp)/'pe_sword_variant_source.h').write_bytes(b'stale')
            with redirect_stderr(io.StringIO()):self.assertEqual(e.main(['--output',tmp,'--check']),1)
            with redirect_stderr(io.StringIO()):self.assertEqual(e.main(['--output',tmp,'--game',tmp]),1)


class OracleContractTests(unittest.TestCase):
    def test_bounded_corpus_is_repeatable(self):
        rows=oracle.cases();self.assertEqual(rows,oracle.cases());self.assertEqual(len(rows),4101)
        self.assertEqual(rows[4],(-2.2190206050872803,-2.0396177768707275,3.341542959213257))
    def test_nonbuild_output_rejected_before_runtime_load(self):
        with self.assertRaises(ValueError):oracle.run(SimpleNamespace(build=e.ROOT/'Assets/not-an-output',report=e.ROOT/'docs/reference/not-an-output.json',game=Path('/nonexistent')))
    def test_compact_evidence_and_hashes(self):
        j=json.loads((e.ROOT/'docs/reference/sword-variant-numerics.json').read_text())
        self.assertEqual((j['cases'],j['exact_matches'],j['mismatch_count']),(4101,4101,0))
        for source in j['sources']:self.assertEqual(hashlib.sha256((e.ROOT/source['file']).read_bytes()).hexdigest(),source['sha256'])
        self.assertEqual(j['golden'][4]['branch'],0);self.assertEqual(j['golden'][4]['speed_bits'],'40900000')

if __name__=='__main__':unittest.main()
