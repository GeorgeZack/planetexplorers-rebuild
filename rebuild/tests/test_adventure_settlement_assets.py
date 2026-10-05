"""Pure geometry contracts for settlement recovery; installed assets checked separately."""
import math
from pathlib import Path
import struct
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0,str(TOOLS))
from export_adventure_settlement_assets import (matrix,box_mesh,validate_convex,
    alive_building_ids,write_obj,CROP_ORIGIN)
from export_reference_assets import multiply,transform_point,transform_normal,material_symbols
from export_training_gathering_assets import checked_mesh
from export_training_npcs import skin_rest


class SettlementAssetsTest(unittest.TestCase):
    def test_crop_and_instance_child_transform(self):
        world_position = [-583,27.5,356]
        m = matrix([a-b for a,b in zip(world_position,CROP_ORIGIN)],[0,math.sqrt(.5),0,-math.sqrt(.5)])
        child = matrix([1,2,3],[0,0,0,1],[2,1,.5])
        got = transform_point(multiply(m,child),[1,0,0])
        for value,expected in zip(got,[41,13.5,36]): self.assertAlmostEqual(value,expected)
        n = transform_normal(multiply(m,child),[1,0,0])
        for value,expected in zip(n,[0,0,1]): self.assertAlmostEqual(value,expected)

    def test_box_exact_extents_closed_and_convex(self):
        vertices,faces = box_mesh([0,.5,0],[1,1,1])
        self.assertEqual(len(vertices),8)
        self.assertEqual(len(faces[0]),12)
        self.assertEqual([min(v[a] for v in vertices) for a in range(3)],[-.5,0,-.5])
        self.assertEqual([max(v[a] for v in vertices) for a in range(3)],[.5,1,.5])
        validate_convex(vertices,faces)
        # Each face winds outward, preserving two-sided/nonpenetrating mesh queries.
        for face in faces[0]:
            a,b,c = [vertices[i] for i in face]
            u,v = [b[i]-a[i] for i in range(3)],[c[i]-a[i] for i in range(3)]
            n = [u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
            self.assertGreater(sum(n[i]*(a[i]-[0,.5,0][i]) for i in range(3)),0)

    def test_invalid_geometry_and_convex_cook_inputs_rejected(self):
        for center,size in [([0,math.nan,0],[1,1,1]),([0,0,0],[1,0,1])]:
            with self.assertRaises(ValueError): box_mesh(center,size)
        vertices,faces = box_mesh([0,0,0],[1,1,1])
        with self.assertRaisesRegex(ValueError,'Open'): validate_convex(vertices,[faces[0][1:]])
        vertices[7] = [0,0,0]
        with self.assertRaises(ValueError): validate_convex(vertices,faces)
        with self.assertRaisesRegex(ValueError,'triangle index'):
            checked_mesh([[0,0,0]]*3,[[0,1,0]]*3,[[0,0]]*3,[[(0,1,3)]])
        for scale in ([0,1,1],[math.nan,1,1]):
            with self.assertRaises(ValueError): matrix([0,0,0],[0,0,0,1],scale)

    def test_static_skin_rest_honors_bone_bind_and_instance(self):
        bone = matrix([10,3,5],[0,0,0,1])
        bind = matrix([-1,0,0],[0,0,0,1])
        skin = [{**{f'weight[{i}]':float(i==0) for i in range(4)},
                 **{f'boneIndex[{i}]':0 for i in range(4)}}]
        positions,normals = skin_rest([[2,0,0]],[[0,1,0]],skin,[multiply(bone,bind)])
        self.assertEqual(positions,[[11,3,5]])
        self.assertEqual(normals,[[0,1,0]])

    def test_obj_material_uv_and_collision_indices(self):
        vertices,faces = box_mesh([0,.5,0],[1,1,1])
        key = (20006,7)
        part = dict(entity_id=20006,component_id=1,collision=False,vertices=vertices,
            normals=[[0,1,0]]*8,uvs=[[.25,.5]]*8,submeshes=faces,materials=[key])
        materials = {key:dict(uv_scale=[2,3],uv_offset=[.1,.2])}
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            stats = write_obj(root/'visual.obj',[part],material_symbols([key]),materials,False)
            text = (root/'visual.obj').read_text()
            self.assertIn('vt 0.6 1.7\n',text)
            self.assertIn('f 1/1/1 5/5/5 7/7/7\n',text)
            self.assertEqual(stats['triangles'],12)
            part['collision'] = True
            stats = write_obj(root/'collision.obj',[part],{},materials,True)
            text = (root/'collision.obj').read_text()
            self.assertIn('f 1 5 7\n',text)
            self.assertNotIn('usemtl',text)
            self.assertNotIn('vn ',text)
            self.assertEqual(stats['bounds'],[[-.5,0,-.5],[.5,1,.5]])

    def test_living_building_archive_bounds(self):
        data = struct.pack('<6i',20160902,0,1,0,1,20001)
        self.assertEqual(alive_building_ids(data),{20001})
        with self.assertRaises(ValueError): alive_building_ids(data[:-1])
        with self.assertRaisesRegex(ValueError,'Trailing'): alive_building_ids(data+b'\0')
        repeated = struct.pack('<7i',20160902,0,1,0,2,20001,20001)
        with self.assertRaisesRegex(ValueError,'Duplicate'): alive_building_ids(repeated)


if __name__=='__main__': unittest.main()
