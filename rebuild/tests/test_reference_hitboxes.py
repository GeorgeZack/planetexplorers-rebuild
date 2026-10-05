#!/usr/bin/env python3
"""Source capsule geometry/schema tests, with optional installed-source recovery."""
import importlib.util
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

TOOLS=Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0,str(TOOLS))
import export_reference_hitboxes as hitboxes
import export_reference_actor as actor

GAME=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'
ACTOR=TOOLS.parent/'build/reference-assets'
HAS_SOURCE=importlib.util.find_spec('UnityPy') is not None and (GAME/actor.BUNDLE).is_file() and (ACTOR/'hare.peac').is_file()


def capsule(axis=1,radius=.2,height=1):
    return dict(radius=radius,heigh=height,axis=axis,offset=dict(x=1.,y=2.,z=3.))


class CapsuleMathTests(unittest.TestCase):
    def test_endpoint_rule_all_six_axes_and_radius_always_local_x(self):
        for axis in range(6):
            points=hitboxes.local_points(capsule(axis))
            direction=[0.,0.,0.];direction[axis%3]=1 if axis<3 else -1
            self.assertEqual(points[2:],[ [0.,0.,0.],[.2,0.,0.] ])
            for i,base in enumerate([1,2,3]):
                self.assertAlmostEqual(points[0][i],base+.2*direction[i])
                self.assertAlmostEqual(points[1][i],base+.8*direction[i])

    def test_short_capsule_preserves_source_resetinfo_without_editor_clamp(self):
        points=hitboxes.local_points(capsule(1,.6,1))
        self.assertAlmostEqual(points[0][1],2.6)
        self.assertAlmostEqual(points[1][1],2.4)

    def test_bind_mapping_agrees_with_animated_bone_hierarchy_and_x_scale(self):
        # Nonuniform scale means radius must follow X, even for a Y capsule.
        bind=actor.local_matrix([2,-3,1],[0,0,0,1],[.5,.25,2])
        bone=actor.local_matrix([1,4,2],[0,math.sqrt(.5),0,math.sqrt(.5)],[3,5,7])
        points=hitboxes.bind_points(capsule(),bind)
        palette=actor.multiply(bone,bind)
        posed=[actor.transform_point(palette,p) for p in points]
        expected=[actor.transform_point(bone,p) for p in hitboxes.local_points(capsule())]
        for p,q in zip(posed,expected):
            for a,b in zip(p,q):self.assertAlmostEqual(a,b)
        self.assertAlmostEqual(math.dist(posed[2],posed[3]),.2*3)

    def test_invalid_source_geometry_rejected(self):
        for c in [capsule(6),capsule(-1),capsule(0,0),capsule(0,float('nan')),capsule(0,.2,-1)]:
            with self.assertRaises(ValueError):hitboxes.local_points(c)

    def test_skeleton_fingerprint_binds_joint_order_and_inverse_bind(self):
        matrix=struct.pack('<12f',*actor.matrix_values(actor.IDENTITY))
        data=struct.pack('<4s5I',b'PEAC',1,3,3,2,1)+bytes(3*56+3*4)+matrix*2
        count,fingerprint,binds=hitboxes.skeleton_data(data)
        self.assertEqual(count,2);self.assertEqual(binds,[actor.IDENTITY,actor.IDENTITY])
        changed=bytearray(data);struct.pack_into('<f',changed,len(data)-48+12,1)
        self.assertNotEqual(hitboxes.skeleton_data(changed)[1],fingerprint)
        changed=bytearray(data);struct.pack_into('<f',changed,24,1)
        self.assertNotEqual(hitboxes.skeleton_data(changed)[1],fingerprint)
        changed=bytearray(data);struct.pack_into('<I',changed,20,2)
        self.assertEqual(hitboxes.skeleton_data(changed)[1],fingerprint)
        for invalid in [b'',data[:23],data[:-1],b'NOPE'+data[4:]]:
            with self.assertRaises(ValueError):hitboxes.skeleton_data(invalid)


@unittest.skipUnless(HAS_SOURCE,'optional installed hare, actor export and UnityPy required')
class OriginalRecoveryTests(unittest.TestCase):
    def test_exact_counts_disabled_parts_joint_contract_and_repeatable_export(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory);(output/'keep.obj').write_text('previous asset')
            command=[sys.executable,str(TOOLS/'export_reference_hitboxes.py'),'--game-dir',str(GAME),'--actor-dir',str(ACTOR),'--output',str(output)]
            run=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
            binary=(output/'hare.pehb').read_bytes();manifest=(output/'hare-hitboxes.json').read_bytes();report=json.loads(manifest)
            self.assertEqual(len(binary),32+19*104)
            self.assertEqual(report['enabled_defence'],12);self.assertEqual(report['attack_records'],4)
            self.assertEqual({e['name'] for e in report['entries'] if not e['enabled']},{'LForearm_M','RForearm_M','Tail3'})
            self.assertEqual([e['damage_scale'] for e in report['entries'] if e['name']=='Head1'],[2])
            self.assertEqual(len(report['unexported_triggers']),1)
            for entry in report['entries']:
                if entry['kind']==1:self.assertEqual(entry['group'],1 if 'R Forearm' in entry['source_owner'] else 2)
            self.assertLess(report['hierarchy_reconstruction_max_error'],1e-5)
            nj,fingerprint,_=hitboxes.skeleton_data((ACTOR/'hare.peac').read_bytes())
            header=hitboxes.HEADER.unpack_from(binary)
            self.assertEqual(header,(b'PEHB',1,19,nj,fingerprint,0,0))
            for i,entry in enumerate(report['entries']):
                record=hitboxes.RECORD.unpack_from(binary,32+i*104)
                self.assertEqual(record[0].rstrip(b'\0').decode(),entry['name'])
                self.assertEqual(record[3],entry['joint'])
                self.assertTrue(all(math.isfinite(v) for v in record[6:]))
            again=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(again.returncode,0,again.stderr)
            self.assertEqual((output/'hare.pehb').read_bytes(),binary)
            self.assertEqual((output/'hare-hitboxes.json').read_bytes(),manifest)
            self.assertEqual((output/'keep.obj').read_text(),'previous asset')
            bad=command.copy();bad[bad.index('--game-dir')+1]=str(output/'missing')
            failed=subprocess.run(bad,capture_output=True,text=True)
            self.assertNotEqual(failed.returncode,0)
            self.assertEqual((output/'hare.pehb').read_bytes(),binary)
            self.assertEqual((output/'hare-hitboxes.json').read_bytes(),manifest)

if __name__=='__main__':unittest.main()
