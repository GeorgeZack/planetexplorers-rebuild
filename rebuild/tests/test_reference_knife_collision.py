#!/usr/bin/env python3
"""Knife source geometry and capture contract tests; no graphics or game input."""
import copy
import json
import math
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_reference_knife_collision as tool
from test_reference_pose_analysis import fixture,transform,matrix,write

CAPSULE=dict(radius=.3,heigh=1.5,offset=dict(x=0.,y=0.,z=-.1),axis=2)
IDENTITY=[matrix()[i:i+4] for i in range(0,16,4)]


def weapon_capture():
    records,rig=fixture()
    for row in records[1:-1]:
        bone=row['transforms'][1];base=bone['world'];path=bone['path']
        for identity,parent,name in [(4,3,'mountMain'),(5,4,'knife'),(6,5,'SingleHandSword')]:
            path+='/'+name;row['transforms'].append(transform(identity,parent,path,matrix(),base))
    return records


class KnifeGeometryTests(unittest.TestCase):
    def test_source_endpoint_and_local_x_radius_with_nonuniform_weapon(self):
        weapon=copy.deepcopy(IDENTITY);weapon[0][0]=2;weapon[1][1]=3;weapon[2][2]=4
        result=tool.posed_capsule(CAPSULE,IDENTITY,weapon)
        self.assertAlmostEqual(result['model_a'][2],.8);self.assertAlmostEqual(result['model_b'][2],4.4)
        self.assertAlmostEqual(result['world_radius'],.6)
        root=copy.deepcopy(IDENTITY)
        for k in range(3):root[k][k]=2
        root[0][3]=10
        weapon=tool.multiply(root,weapon);result=tool.posed_capsule(CAPSULE,root,weapon)
        self.assertAlmostEqual(result['model_radius'],.6);self.assertAlmostEqual(result['world_radius'],1.2)
        self.assertAlmostEqual(result['model_a'][0],0)

    def test_shear_nonuniform_root_nonfinite_and_singular_rejected(self):
        for mode in ('shear','nonuniform','zero','nan'):
            root=copy.deepcopy(IDENTITY)
            if mode=='shear':root[0][1]=.5
            if mode=='nonuniform':root[1][1]=2
            if mode=='zero':root[0][0]=0
            if mode=='nan':root[0][0]=float('nan')
            with self.assertRaises(ValueError):tool.posed_capsule(CAPSULE,root,IDENTITY)

    def test_capture_geometry_does_not_infer_attack_activation(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'capture.jsonl';write(path,weapon_capture());result=tool.capture_samples(path,CAPSULE)
            self.assertEqual(result['attachment_counts'],{'mountMain':2,'mountWaist':0})
            self.assertFalse(result['collision_authorized']);self.assertFalse(result['attack_activation_observed'])
            self.assertEqual(result['frames'][0]['model_a'][:2],[0.,2.])
            self.assertEqual(result['frames'][1]['model_a'][:2],[1.,2.])
            self.assertIsNone(result['frames'][1]['attack_active'])
            self.assertEqual(result['frames'][1]['knife_model_matrix'][0][3],1.)

    def test_missing_duplicate_weapon_incomplete_capture_and_clock_rejected(self):
        variants=[]
        rows=weapon_capture();rows[1]['transforms'].pop();variants.append(rows)
        rows=weapon_capture();rows[1]['transforms'].append(copy.deepcopy(rows[1]['transforms'][-1]));variants.append(rows)
        rows=weapon_capture();variants.append(rows[:-1])
        rows=weapon_capture();rows[2]['realtime']=rows[1]['realtime'];variants.append(rows)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'capture.jsonl'
            for rows in variants:
                write(path,rows)
                with self.assertRaises(ValueError):tool.capture_samples(path,CAPSULE)

    def test_obj_indices_material_separation_and_rigid_peac_clock(self):
        obj='v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\nusemtl blade\nf 1/1/1 2/2/1 3/3/1\nusemtl handle\nf 1/1/1 3/3/1 2/2/1\n'
        parts=tool.obj_parts(obj);self.assertEqual(set(parts),{'blade','handle'})
        rows,indices=parts['blade'];end=copy.deepcopy(IDENTITY);end[0][3]=2
        data=tool.rigid_peac(rows,indices,[IDENTITY,end],1,1)
        self.assertEqual(struct.unpack_from('<4s5I',data),(b'PEAC',2,3,3,1,1))
        clip=24+3*56+3*4+48
        name,fps,count,duration,loop=struct.unpack_from('<32sfIfI',data,clip)
        self.assertEqual((name.rstrip(b'\0'),fps,count,duration,loop),(b'captured',1,2,1,0))
        self.assertEqual(struct.unpack_from('<12f',data,clip+48+48)[3],2)
        for invalid in [obj.replace('3/3/1','9/3/1',1),obj.replace('f 1/1/1','f -1/1/1',1),obj.replace('v 0 0 0','v nan 0 0',1),obj+'unknown 2\n']:
            with self.assertRaises(ValueError):tool.obj_parts(invalid)

    def test_hitbox_binds_source_shape_to_exact_rigid_geometry(self):
        rows=[([0.,0.,0.],[0.,1.,0.],[0.,0.])]*3
        binary=tool.rigid_peac(rows,[0,1,2],[IDENTITY,IDENTITY],1,1)
        blade=copy.deepcopy(IDENTITY);blade[0][3]=.2
        selected=[dict(knife_model_matrix=IDENTITY,weapon_model_matrix=blade)]*2
        data,report=tool.knife_hitbox(binary,CAPSULE,selected)
        self.assertEqual(tool.HEADER.unpack_from(data)[:4],(b'PEHB',1,1,1))
        fields=tool.RECORD.unpack_from(data,32)
        self.assertEqual(fields[1:6],(1,1,0,0,0))
        self.assertAlmostEqual(fields[7],.2);self.assertAlmostEqual(fields[9],.2)
        self.assertFalse(report['activation_observed'])
        changed=tool.rigid_peac(rows,[0,2,1],[IDENTITY,IDENTITY],1,1)
        self.assertNotEqual(tool.knife_hitbox(changed,CAPSULE,selected)[0][:32],data[:32])
        moved=copy.deepcopy(selected);moved[1]=copy.deepcopy(selected[1]);moved[1]['weapon_model_matrix'][0][3]+=.1
        with self.assertRaises(ValueError):tool.knife_hitbox(binary,CAPSULE,moved)

    def test_combination_preserves_body_snapshot_and_rejects_clock_or_path(self):
        binary=tool.rigid_peac([([0.,0.,0.],[0.,1.,0.],[0.,0.])]*3,[0,1,2],[IDENTITY,IDENTITY],1,1)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);body=root/'body';body.mkdir();out=root/'out';out.mkdir()
            actor=body/'body.peac';actor.write_bytes(binary)
            (body/'body.png').write_bytes(b'\x89PNG\r\n\x1a\nfixture')
            scene=body/'body.peas';scene.write_text('PEAS1\nbody.peac body.png\n')
            report=dict(parts=[dict(file='knife.peac',texture='knife.png')])
            tool.combine_body(out,scene,tool.clip_bytes(binary),report)
            self.assertEqual((out/'body.peac').read_bytes(),binary)
            self.assertEqual(report['combined_scene']['parts'],2)
            self.assertEqual(actor.read_bytes(),binary)
            other=root/'other';other.mkdir()
            changed=tool.rigid_peac([([0.,0.,0.],[0.,1.,0.],[0.,0.])]*3,[0,1,2],[IDENTITY,IDENTITY],2,.5)
            with self.assertRaises(ValueError):tool.combine_body(other,scene,tool.clip_bytes(changed),report)
            self.assertEqual(list(other.iterdir()),[])
            legacy=bytearray(binary);struct.pack_into('<I',legacy,4,1)
            with self.assertRaises(ValueError):tool.combine_body(other,scene,tool.clip_bytes(legacy),report)
            scene.write_text('PEAS1\n../body.peac body.png\n')
            with self.assertRaises(ValueError):tool.combine_body(other,scene,tool.clip_bytes(binary),report)

if __name__=='__main__':unittest.main()
