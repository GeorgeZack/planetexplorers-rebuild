#!/usr/bin/env python3
"""Guard the exact source playback response; optional offline UnityPy prefab checks."""
from contextlib import redirect_stderr
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_attack_playback as e
HAVE_UNITY=importlib.util.find_spec('UnityPy') is not None


class SourceGuards(unittest.TestCase):
    def mutate(self,path,old,new):
        with tempfile.TemporaryDirectory(prefix='playback-source-') as directory:
            root=Path(directory)
            for p in e.SOURCE_FILES:
                target=root/p;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes((e.ROOT/p).read_bytes())
            target=root/path;text=target.read_text(encoding='utf-8-sig',errors='replace');self.assertIn(old,text)
            target.write_text(text.replace(old,new,1))
            with self.assertRaises(ValueError):e.validate_sources(root)

    def test_valid_source_chain(self):self.assertEqual(len(e.validate_sources(e.ROOT)),len(e.SOURCE_FILES))
    def test_curve_window_strict_boundary(self):self.mutate(e.ACTION,'dtTime < animScaleTime','dtTime <= animScaleTime')
    def test_stamina_threshold_is_inclusive(self):self.mutate(e.ACTION,'stamina <= sword.m_AnimDownThreshold[i]','stamina < sword.m_AnimDownThreshold[i]')
    def test_no_final_speed_clamp(self):self.mutate(e.ACTION,'animScaleCurve.Evaluate(dtTime / animScaleTime) * animSpeed','Mathf.Clamp(animScaleCurve.Evaluate(dtTime / animScaleTime) * animSpeed, 0.5f, 1.5f)')
    def test_contact_is_not_damage_callback(self):self.mutate(e.ACTION,'animScaleStartTime = Time.time;','animScaleStartTime = Time.time + 1;')
    def test_adding_action_reset_rejected(self):self.mutate(e.ACTION,'UpdateAnimSpeed();','animScaleStartTime = Time.time; UpdateAnimSpeed();')
    def test_registered_player_action_required(self):self.mutate(e.EQUIP,'m_MotionMgr.AddAction(m_SwordAttack);','m_MotionMgr.AddAction(m_TwoHandWeaponAttack);')
    def test_actual_player_resource_required(self):self.mutate(e.CREATOR,'PlayerPrefabPath = "EntityPlayer"','PlayerPrefabPath = "EntityNpc"')
    def test_default_input_direction_required(self):self.mutate(e.SETTINGS,'AttackWhithMouseDir = false','AttackWhithMouseDir = true')
    def test_movement_direction_required(self):self.mutate(e.PLAYER,'mEquip.SwordAttack (m_MoveDir.normalized)','mEquip.SwordAttack (GetMouseClickDir ())')
    def test_quaternion_lerp_required(self):self.mutate(e.ACTION,'Quaternion.Lerp(trans.rotation, Quaternion.LookRotation(m_AttackDir), m_RotateAcc * Time.deltaTime)','Quaternion.Slerp(trans.rotation, Quaternion.LookRotation(m_AttackDir), m_RotateAcc * Time.deltaTime)')
    def test_fallback_body_forward_required(self):self.mutate(e.ACTION,'m_AttackDir = trans.forward;','m_AttackDir = Vector3.forward;')


@unittest.skipUnless(HAVE_UNITY,'optional UnityPy unavailable')
class PrefabGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.objects=e.read_prefab(e.ROOT/e.PREFAB)
        cls.guid=re.search(r'^guid: ([a-f0-9]{32})$',(e.ROOT/(e.EQUIP+'.meta')).read_text(),re.M)[1]

    def test_regeneration_and_inputs_unchanged(self):
        before=[hashlib.sha256((e.ROOT/p).read_bytes()).hexdigest() for p in e.SOURCE_FILES]
        outputs=e.build_outputs();self.assertEqual(outputs,e.build_outputs())
        for name,data in outputs.items():self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
        self.assertEqual(before,[hashlib.sha256((e.ROOT/p).read_bytes()).hexdigest() for p in e.SOURCE_FILES])

    def test_serialized_duration_not_class_default(self):
        d=e.qualify(self.objects,self.guid)
        self.assertAlmostEqual(d['duration_seconds'],.65,delta=3e-8)
        self.assertNotEqual(d['duration_seconds'],.15)
        self.assertEqual([k['tangentMode'] for k in d['curve']['m_Curve']],[0,17,5,0,0])
        self.assertLess(d['curve']['m_Curve'][-1]['time'],1)

    def changed(self,edit):
        objects=copy.deepcopy(self.objects);edit(objects)
        with self.assertRaises(ValueError):e.qualify(objects,self.guid)

    def test_duration_change_rejected(self):self.changed(lambda x:x[11400022]['data']['m_SwordAttack'].__setitem__('animScaleTime',.15))
    def test_serialized_facing_acceleration_not_class_default(self):
        d=e.qualify(self.objects,self.guid)
        self.assertEqual(d['action']['m_RotateAcc'],60)
        j=json.loads(e.build_outputs()['pe_attack_playback_source.json'])
        self.assertEqual(j['facing']['rotate_acceleration'],60)
        self.assertFalse(j['facing']['mouse_direction_default'])
        self.assertIn(b'#define PE_SWORD_ROTATE_ACCELERATION 60.0f',e.build_outputs()['pe_attack_playback_source.h'])
    def test_rotate_acceleration_change_rejected(self):self.changed(lambda x:x[11400022]['data']['m_SwordAttack'].__setitem__('m_RotateAcc',3))
    def test_tangent_change_rejected(self):self.changed(lambda x:x[11400022]['data']['m_SwordAttack']['animScaleCurve']['m_Curve'][2].__setitem__('outSlope',0))
    def test_wrap_change_rejected(self):self.changed(lambda x:x[11400022]['data']['m_SwordAttack']['animScaleCurve'].__setitem__('m_PostInfinity',1))
    def test_disabled_component_rejected(self):self.changed(lambda x:x[11400022]['data'].__setitem__('m_Enabled',0))
    def test_wrong_script_rejected(self):
        with self.assertRaises(ValueError):e.qualify(self.objects,'0'*32)
    def test_reordered_keys_rejected(self):self.changed(lambda x:x[11400022]['data']['m_SwordAttack']['animScaleCurve']['m_Curve'].reverse())
    def test_engine_limitations_preserved(self):
        j=json.loads(e.build_outputs()['pe_attack_playback_source.json'])
        self.assertTrue(j['rule']['clamp_before_curve']);self.assertFalse(j['rule']['clamp_after_curve'])
        self.assertFalse(j['rule']['action_start_resets']);self.assertFalse(j['rule']['combo_resets'])
        self.assertIn('internal-call',j['qualification']['engine_evaluate'])
        self.assertIn('Separate pinned native-binary',j['qualification']['interpolation'])
        evidence=j['interpolation_evidence']
        self.assertEqual(evidence['compared_points'],4109)
        self.assertEqual(evidence['exact_float_matches'],4109)
        for key in ('audit','result','tool'):self.assertTrue((e.ROOT/evidence[key]).is_file())
        self.assertIn('statically qualified',j['qualification']['tail'])
    def test_stale_and_missing_check(self):
        with tempfile.TemporaryDirectory(prefix='playback-output-') as directory:
            args=['--output',directory]
            with redirect_stderr(io.StringIO()):self.assertEqual(e.main(args+['--check']),1)
            self.assertEqual(e.main(args),0);self.assertEqual(e.main(args+['--check']),0)
            (Path(directory)/'pe_attack_playback_source.h').write_text('stale')
            with redirect_stderr(io.StringIO()):self.assertEqual(e.main(args+['--check']),1)


if __name__=='__main__':unittest.main()
