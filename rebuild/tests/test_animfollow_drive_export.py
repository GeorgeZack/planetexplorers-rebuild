#!/usr/bin/env python3
"""Installed rig qualification and bounded PD CIL arithmetic comparison; no Unity execution."""
import copy
import importlib.util
import json
from pathlib import Path
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_animfollow_drive as e
import cil_disasm

def f32(value):return struct.unpack('<f',struct.pack('<f',value))[0]

def pd_cil(method,p,d,error,last,reciprocal):
    """Only this pinned method, with independently modelled float32 Vector3 ops.
    Does not interpret a general assembly or run engine operators/JIT code.
    """
    args=[p,d,'signal',error,'last',reciprocal];refs={'last':last};stack=[]
    for _,op,value in cil_disasm.decode(bytes.fromhex(method['raw_il_hex'])):
        if op.startswith('ldarg.'):
            index=value if op=='ldarg.s' else int(op.rsplit('.',1)[1]);stack.append(args[index])
        elif op=='ldobj':stack.append(refs[stack.pop()])
        elif op=='stobj':
            result=stack.pop();refs[stack.pop()]=result
        elif op=='call':
            b,a=stack.pop(),stack.pop()
            if value in (0x0a000012,0x0a00005f):
                if not isinstance(a,tuple):a,b=b,a
                result=tuple(f32(v*b) for v in a)
            elif value==0x0a00000c:result=tuple(f32(x-y) for x,y in zip(a,b))
            elif value==0x0a00000d:result=tuple(f32(x+y) for x,y in zip(a,b))
            else:raise ValueError('Unqualified PD operator token')
            stack.append(result)
        elif op=='ret':break
        else:raise ValueError('Unqualified PD instruction')
    if stack or refs['last']!=error:raise ValueError('Unexpected PD stack/ref contract')
    return refs['signal'],refs['last']

class Portable(unittest.TestCase):
    def test_bound_pd_il_and_control_modes(self):
        evidence=e.base.load_evidence();methods=evidence['assemblies']['Assembly-CSharp.dll']['methods']
        method=next(m for m in methods if m['name']=='PDControl')
        result,last=pd_cil(method,2.,.5,(2.,4.,-2.),(1.,2.,-1.),10.)
        self.assertEqual(result,(14.,28.,-14.));self.assertEqual(last,(2.,4.,-2.))
        drive=next(m for m in methods if m['name']=='DoAnimFollow')
        ops=cil_disasm.decode(bytes.fromhex(drive['raw_il_hex']))
        self.assertIn((0x294,'ldc.i4.2',None),ops);self.assertIn((0x361,'ldc.i4.2',None),ops)

    @unittest.skipUnless(shutil.which('cc'),'C compiler unavailable')
    def test_c_pd_requests_against_bounded_cil_replay(self):
        library=e.ROOT/'rebuild/build/obj/core/libpe_core.a'
        if not library.is_file():self.skipTest('Built core library unavailable')
        data=json.loads((e.ROOT/'rebuild/generated/pe_animfollow_drive_source.json').read_text())
        method=next(m for m in e.base.load_evidence()['assemblies']['Assembly-CSharp.dll']['methods'] if m['name']=='PDControl')
        randomizer=random.Random(777);samples=[(0.,0.,0.),(1.,-1.,.25),(1.,-1.,.25)]
        samples += [tuple(f32(randomizer.uniform(-2,2)) for _ in range(3)) for _ in range(256)]
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);code=root/'pd.c';binary=root/'pd'
            code.write_text('''#include "pe_animfollow_drive.h"
#include <stdio.h>
int main(void){PeAnimFollowDrive s;if(!pe_animfollow_drive_init(&s,.02f))return 1;s.torque_enabled=false;
PeAnimFollowDriveTick in={.active=true,.secondary_update=2,.force_cap=1000,.torque_cap=100,.joint_cap=80,.joint_damping=.5f};
while(scanf("%f %f %f",&in.bodies[0].master_position.x,&in.bodies[0].master_position.y,&in.bodies[0].master_position.z)==3){
PeAnimFollowDriveUpdate u;if(!pe_animfollow_drive_step(&s,in,&u))return 2;
printf("%.9g %.9g %.9g\\n",u.bodies[0].force.x,u.bodies[0].force.y,u.bodies[0].force.z);}return 0;}
''')
            subprocess.run(['cc','-std=c11','-I'+str(e.ROOT/'rebuild/src'),str(code),str(library),'-lm','-o',str(binary)],check=True)
            rows=subprocess.run([str(binary)],input=''.join(' '.join(format(v,'.9g') for v in row)+'\n' for row in samples),text=True,capture_output=True,check=True).stdout.splitlines()
        self.assertEqual(len(rows),len(samples));last=(0.,0.,0.)
        for index,(error,line) in enumerate(zip(samples,rows)):
            expected,last=pd_cil(method,data['scalars']['PForce'],data['scalars']['DForce'],error,last,50.)
            actual=tuple(float(v) for v in line.split())
            self.assertEqual([struct.pack('<f',v) for v in actual],[struct.pack('<f',v) for v in expected],index)

@unittest.skipUnless(importlib.util.find_spec('UnityPy'),'optional UnityPy unavailable')
class Installed(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        evidence=e.base.load_evidence();cls.components=e.base.recover_components(evidence);cls.assets=e.base.Assets(e.base.DATA)
        cls.values=cls.components['AnimFollow_AF']['values'];cls.rows,cls.nonrigids=e.recover_rig(cls.values,cls.assets)
    def test_ordered_rig_and_profiles(self):
        self.assertEqual(len(self.rows),11);self.assertEqual(len(self.nonrigids),66)
        self.assertEqual([r['slot'] for r in self.rows],[2,4,5,9,10,15,18,19,34,42,43])
        self.assertEqual([r['rigidbody'] for r in self.rows],[r['id'] for r in self.values['slaveRigidbodies']])
        self.assertEqual(self.rows[0]['body']['m_Mass'],40);self.assertIsNone(self.rows[0]['joint'])
        self.assertTrue(all(len(r['colliders'])==1 for r in self.rows))
        self.assertEqual(sum(r['joint'] is not None for r in self.rows),10)
    def test_reordered_body_and_malformed_profiles_fail(self):
        for kind in ('order','profile','references'):
            values=copy.deepcopy(self.values)
            if kind=='order':values['slaveRigidbodies'][1:3]=reversed(values['slaveRigidbodies'][1:3])
            if kind=='profile':values['maxForceProfile'][0]=float('nan')
            if kind=='references':values['masterTransforms'].pop()
            with self.subTest(kind=kind),self.assertRaises(ValueError):e.recover_rig(values,self.assets)
    def test_regeneration_and_finite_evidence(self):
        outputs=e.build_outputs()
        for name,data in outputs.items():self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
        metadata=json.loads(outputs['pe_animfollow_drive_source.json'])
        self.assertEqual(metadata['requests']['force_mode'],2)
        self.assertEqual(metadata['rigid_bodies'][1]['joint']['data']['m_BreakForce'],'+Infinity')
        self.assertTrue(all(not Path(s['path']).is_absolute() for s in metadata['installed_sources']))

if __name__=='__main__':unittest.main()
