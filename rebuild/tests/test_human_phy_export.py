#!/usr/bin/env python3
"""Static CIL/prefab qualification of HumanPhyCtrl; no game or GPU launch."""
from contextlib import redirect_stderr
import copy
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import cil_disasm
import export_human_phy as e
HAVE_UNITY=importlib.util.find_spec('UnityPy') is not None
INSTALLED=all((e.MANAGED/name).is_file() for name in e.ASSEMBLIES)


def mnemonics(method):
    return [m for _,m,_ in cil_disasm.decode(bytes.fromhex(method['raw_il_hex']))]


class Decoder(unittest.TestCase):
    def test_operands_and_branch_targets(self):
        code=bytes.fromhex('02'+'22'+struct.pack('<f',10.0).hex()+'7d'+'c2890004'+'2b02'+'0000'+'2a')
        self.assertEqual(cil_disasm.decode(code),[(0,'ldarg.0',None),(1,'ldc.r4',10.0),(6,'stfld',0x040089c2),
                                                  (11,'br.s',15),(13,'nop',None),(14,'nop',None),(15,'ret',None)])
        self.assertEqual(cil_disasm.listing(code,{0x040089c2:'T::f'})[2],'IL_0006: stfld T::f')
        self.assertEqual(cil_disasm.decode(bytes.fromhex('fe01'+'fe0c0500'+'4502000000'+'01000000'+'02000000')),
                         [(0,'ceq',None),(2,'ldloc',5),(6,'switch',(20,21))])

    def test_malformed_code_rejected(self):
        for code in ('22000000','24','fe','feff','4505000000','7d0000'):
            with self.subTest(code=code),self.assertRaises(ValueError): cil_disasm.decode(bytes.fromhex(code))

    def test_header_mismatch_rejected(self):
        raw=bytes([0x0a,0x00,0x2a]); body=dict(file_offset=0,header_format='tiny',il_size=2)
        self.assertEqual(cil_disasm.body_bytes(raw,body),b'\x00\x2a')
        with self.assertRaises(ValueError): cil_disasm.body_bytes(raw,dict(body,il_size=1))
        with self.assertRaises(ValueError): cil_disasm.body_bytes(raw,dict(body,header_format='fat'))


class Evidence(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.evidence,cls.found=e.load_evidence()

    def staged(self,change):
        with tempfile.TemporaryDirectory(prefix='human-phy-evidence-') as directory:
            root=Path(directory); target=root/e.EVIDENCE; target.parent.mkdir(parents=True)
            data=copy.deepcopy(self.evidence); change(data); target.write_text(json.dumps(data))
            with self.assertRaises(ValueError): e.load_evidence(root)

    def test_constructor_and_literals(self):
        defaults=e.constructor_defaults(self.evidence,self.found)
        self.assertEqual(defaults['MaxSubSpeed'],10.0)
        self.assertEqual(defaults['m_MoveSpeed'],5.0)
        # Serialized female values override these constructor defaults.
        for name in ('m_GroundDrag','m_StopSpeed','m_LeaveGroundTime','m_SpineHeight','m_CheckGroundRadius'):
            self.assertNotEqual(defaults[name],e.SERIALIZED[name])
        self.assertNotIn('MaxSubSpeed',e.SERIALIZED); self.assertNotIn('m_MoveSpeed',e.SERIALIZED)

    def test_readings_the_owner_depends_on(self):
        request=mnemonics(self.found['HumanPhyCtrl::ApplyMoveRequest'])
        self.assertEqual(request.count('ble.un'),1)      # strict magnitude > MaxSubSpeed
        self.assertEqual([m for m in request if m.startswith('b')],['ble.un'])
        late=cil_disasm.decode(bytes.fromhex(self.found['HumanPhyCtrl::LateUpdate']['raw_il_hex']))
        fields={int(x['token'],16):x['name'] for x in self.evidence['assemblies']['Assembly-CSharp.dll']['fields']}
        self.assertEqual([(m,fields[t]) for _,m,t in late if m in ('ldfld','stfld')],
                         [('ldfld','m_RequestSubVelocity'),('stfld','m_ApplySubVelocity'),('stfld','m_RequestSubVelocity')])
        cancel=cil_disasm.decode(bytes.fromhex(self.evidence_method('CancelMoveRequest')['raw_il_hex']))
        self.assertEqual([fields[t] for _,m,t in cancel if m=='stfld'],['m_RequestSubVelocity','m_ApplySubVelocity'])
        fixed=cil_disasm.decode(bytes.fromhex(self.found['HumanPhyCtrl::FixedUpdate']['raw_il_hex']))
        counter=[i for i,(_,m,t) in enumerate(fixed) if m=='stfld' and fields.get(t)=='_cntFixedUpdate']
        self.assertEqual(len(counter),1)
        self.assertEqual([m for _,m,_ in fixed[counter[0]+1:counter[0]+5]],['ldloc.s','ldc.i4.2','blt','ret'])
        # Four ForceMode operands precede AddForce: VelocityChange 2 or Acceleration 5.
        modes=[fixed[i-1][1] for i,(_,m,t) in enumerate(fixed) if m=='callvirt' and fixed[i-1][1].startswith('ldc.i4.')]
        self.assertEqual(modes,['ldc.i4.2','ldc.i4.5','ldc.i4.5','ldc.i4.5','ldc.i4.5','ldc.i4.2','ldc.i4.2'])
        update=cil_disasm.decode(bytes.fromhex(self.found['HumanPhyCtrl::Update']['raw_il_hex']))
        names={int(m['token'],16):m['name'] for m in self.evidence['assemblies']['Assembly-CSharp.dll']['methods']}
        self.assertEqual([names[t] for _,m,t in update if m=='call'],['UpdateGroundState','UpdateWaterState','UpdateDrag'])

    def evidence_method(self,name):
        match=[m for m in self.evidence['assemblies']['Assembly-CSharp.dll']['methods']
               if m['declaring_type']=='HumanPhyCtrl' and m['name']==name]
        self.assertEqual(len(match),1); return match[0]

    def test_shipped_repulsed_still_requests_through_human_phy(self):
        callers=self.evidence['assemblies']['Assembly-CSharp.dll']['external_callers']
        self.assertIn('Pathea.Action_Repulsed::Update',callers['ApplyMoveRequest'])
        self.assertIn('Pathea.Action_Repulsed::EndImmediately',callers['CancelMoveRequest'])
        # Another per-frame caller whose request sums with Repulsed before the clamp.
        self.assertIn('Pathea.MainPlayerCmpt::UpdateMoveState',callers['ApplyMoveRequest'])

    def test_changed_body_rejected(self):
        def clamp(data):
            method=next(m for m in data['assemblies']['Assembly-CSharp.dll']['methods'] if m['name']=='.ctor' and m['declaring_type']=='HumanPhyCtrl')
            ten=struct.pack('<f',10.0).hex(); self.assertIn(ten,method['raw_il_hex'])
            method['raw_il_hex']=method['raw_il_hex'].replace(ten,struct.pack('<f',16.0).hex(),1)
            return method
        self.staged(clamp)
        def rehashed(data):
            method=clamp(data)
            method['body']['il_sha256']=hashlib.sha256(bytes.fromhex(method['raw_il_hex'])).hexdigest()
        self.staged(rehashed)

    def test_changed_identity_or_coverage_rejected(self):
        self.staged(lambda d:d['assemblies']['Assembly-CSharp.dll'].__setitem__('mvid','0'*36))
        self.staged(lambda d:d['assemblies']['UnityEngine.dll'].__setitem__('sha256','0'*64))
        self.staged(lambda d:d['assemblies'].pop('Assembly-CSharp-firstpass.dll'))
        self.staged(lambda d:d['assemblies']['Assembly-CSharp.dll']['methods'].pop())

    def test_unqualified_installation_rejected(self):
        with tempfile.TemporaryDirectory(prefix='human-phy-managed-') as directory:
            for name in e.ASSEMBLIES: (Path(directory)/name).write_bytes(b'unsupported')
            with self.assertRaises(ValueError): e.qualify_installed(self.evidence,Path(directory))

    @unittest.skipUnless(INSTALLED,'installed managed assemblies unavailable')
    def test_installed_bytes(self):
        e.qualify_installed(self.evidence)
        with tempfile.TemporaryDirectory(prefix='human-phy-managed-') as directory:
            for name in e.ASSEMBLIES: shutil.copyfile(e.MANAGED/name,Path(directory)/name)
            method=self.found['HumanPhyCtrl::ApplyMoveRequest']; target=Path(directory)/'Assembly-CSharp.dll'
            raw=bytearray(target.read_bytes()); raw[method['body']['file_offset']+20]^=1; target.write_bytes(raw)
            with self.assertRaises(ValueError): e.qualify_installed(self.evidence,Path(directory))


@unittest.skipUnless(HAVE_UNITY,'optional UnityPy unavailable')
class PrefabGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model=e.read_objects(e.ROOT/e.MODEL)
        cls.time=e.read_objects(e.ROOT/e.TIME); cls.dynamics=e.read_objects(e.ROOT/e.DYNAMICS)

    def changed(self,change):
        model=copy.deepcopy(self.model); change(model)
        with self.assertRaises(ValueError): e.qualify_model(model)

    def test_active_binding(self):
        data=e.qualify_model(self.model)
        self.assertEqual(data['serialized']['m_GroundDrag'],1.5)
        self.assertEqual(data['rigidbody']['m_Mass'],200.0)
        self.assertFalse(data['rigidbody']['m_UseGravity'])
        self.assertEqual(data['capsule']['m_Radius'],0.3700000047683716)

    def test_serialized_mutations(self):
        for name,value in (('m_GroundDrag',.5),('m_LeaveGroundTime',.05),('m_IsContrler',0),('maxSpeed',40.0),
                           ('netMoveSpeedScale',.5),('MaxSubSpeed',16.0)):
            with self.subTest(field=name): self.changed(lambda m:m[e.HUMAN_PHY]['data'].__setitem__(name,value))
        self.changed(lambda m:m[e.HUMAN_PHY]['data']['m_GroundLayer'].__setitem__('m_Bits',1))
        self.changed(lambda m:m[e.HUMAN_PHY]['data'].__setitem__('m_Enabled',0))
        self.changed(lambda m:m[e.HUMAN_PHY].__setitem__('script_guid','0'*32))
        self.changed(lambda m:m.__setitem__(123456,copy.deepcopy(m[e.HUMAN_PHY])))

    def test_body_and_capsule_mutations(self):
        for name,value in (('m_IsKinematic',True),('m_UseGravity',True),('m_Drag',.1),('m_Constraints',0),('m_Mass',70.0)):
            with self.subTest(field=name): self.changed(lambda m:m[e.RIGIDBODY_ID]['data'].__setitem__(name,value))
        self.changed(lambda m:m[e.CAPSULE_ID]['data'].__setitem__('m_Radius',.5))
        self.changed(lambda m:m[e.CAPSULE_ID]['data'].__setitem__('m_IsTrigger',True))
        self.changed(lambda m:m[e.MODEL_OBJECT]['data'].__setitem__('m_Layer',0))
        self.changed(lambda m:m[e.RIGIDBODY_ID]['data']['m_GameObject'].__setitem__('m_PathID',1))
        self.changed(lambda m:m.__setitem__(123456,copy.deepcopy(m[e.RIGIDBODY_ID])))

    def test_project_settings(self):
        data=e.qualify_settings(self.time,self.dynamics)
        self.assertEqual(data['fixed_timestep'],0.019999999552965164)
        time=copy.deepcopy(self.time); next(iter(time.values()))['data']['Fixed Timestep']=1/60
        with self.assertRaises(ValueError): e.qualify_settings(time,self.dynamics)
        dynamics=copy.deepcopy(self.dynamics); next(iter(dynamics.values()))['data']['m_Gravity']['y']=-10.0
        with self.assertRaises(ValueError): e.qualify_settings(self.time,dynamics)

    @unittest.skipUnless(INSTALLED,'installed managed assemblies unavailable')
    def test_regeneration_and_portable_labels(self):
        outputs=e.build_outputs(); self.assertEqual(outputs,e.build_outputs())
        for name,data in outputs.items():
            self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
        data=json.loads(outputs['pe_human_phy_source.json'])
        self.assertIn('Disconnected',data['scope'])
        self.assertTrue(all(not Path(s['path']).is_absolute() for s in data['source_files']))
        self.assertNotIn(str(Path.home()),outputs['pe_human_phy_source.json'].decode())
        self.assertNotIn(str(Path.home()),(e.ROOT/e.EVIDENCE).read_text())

    @unittest.skipUnless(INSTALLED,'installed managed assemblies unavailable')
    def test_check_missing_and_stale(self):
        with tempfile.TemporaryDirectory(prefix='human-phy-output-') as directory:
            with redirect_stderr(io.StringIO()): self.assertEqual(e.main(['--output',directory,'--check']),1)
            self.assertEqual(e.main(['--output',directory]),0)
            self.assertEqual(e.main(['--output',directory,'--check']),0)
            (Path(directory)/'pe_human_phy_source.h').write_text('stale')
            with redirect_stderr(io.StringIO()): self.assertEqual(e.main(['--output',directory,'--check']),1)


if __name__=='__main__': unittest.main()
