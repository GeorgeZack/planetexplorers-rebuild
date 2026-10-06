"""Offline hierarchy/binding guards and optional real installed-reference validation."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from inventory_creature_sampler import DESCRIPTORS,avatar_mapping,clip_inventory,controller_leaves,hierarchy,inventory,pointer,compact_report


def ref(i):return dict(m_FileID=0,m_PathID=i)


def fixture():
    transforms={};objects={}
    for i,name,parent,children in [(1,'Wrapper',0,[2]),(2,'Model',1,[3]),(3,'Bone',2,[4]),(4,'Tip',3,[])]:
        transforms[i]=dict(m_GameObject=ref(10+i),m_Father=ref(parent),m_Children=[ref(c) for c in children],
                           m_LocalPosition=dict(x=0,y=0,z=0),m_LocalRotation=dict(x=0,y=0,z=0,w=1),m_LocalScale=dict(x=1,y=1,z=1))
        objects[10+i]=dict(m_Name=name,m_IsActive=1)
    paths=['','Bone','Bone/Tip'];ids=[zlib.crc32(p.encode()) for p in paths]
    avatar=dict(m_TOS=list(zip(ids,paths)),m_Avatar=dict(m_AvatarSkeleton=dict(data=dict(m_ID=ids,m_Node=[dict(m_ParentId=p) for p in [-1,0,1]])),
                                                      m_RootMotionBoneIndex=1,m_HumanSkeletonIndexArray=[]))
    return transforms,objects,avatar


class InventoryTests(unittest.TestCase):
    def test_exact_child_binding_root_not_silent_prefix_rewrite(self):
        ts,gs,a=fixture();r=avatar_mapping(a,ts,gs,1)
        self.assertEqual(r['binding_root'],2);self.assertEqual(r['animator_root'],1)
        self.assertEqual(len(r['direct_animator_missing_paths']),2)
        self.assertEqual(r['root_motion_path'],'Bone');self.assertEqual(r['nodes'][2]['path'],'Bone/Tip')

    def test_cycle_duplicate_name_and_parent_conflict_rejected(self):
        for kind in ('cycle','name','parent'):
            ts,gs,_=fixture()
            if kind=='cycle':ts[4]['m_Children']=[ref(2)];ts[2]['m_Father']=ref(4)
            if kind=='name':ts[2]['m_Children'].append(ref(4));ts[4]['m_Father']=ref(2);gs[14]['m_Name']='Bone';ts[3]['m_Children']=[]
            if kind=='parent':ts[3]['m_Father']=ref(1)
            with self.assertRaises(ValueError):hierarchy(ts,gs,1)

    def test_nonfinite_and_degenerate_trs_fail(self):
        for part,key,value in [('m_LocalScale','x',0),('m_LocalRotation','w',2),('m_LocalPosition','z',float('nan'))]:
            ts,gs,_=fixture();ts[3][part][key]=value
            with self.assertRaises(ValueError):hierarchy(ts,gs,1)

    def test_bad_avatar_hash_parent_and_motion_index_fail(self):
        for kind in ('hash','parent','root'):
            ts,gs,a=fixture()
            if kind=='hash':a['m_TOS'][1]=(42,'Bone')
            if kind=='parent':a['m_Avatar']['m_AvatarSkeleton']['data']['m_Node'][2]['m_ParentId']=0
            if kind=='root':a['m_Avatar']['m_RootMotionBoneIndex']=100
            with self.assertRaises(ValueError):avatar_mapping(a,ts,gs,1)

    def test_missing_binding_root_fails(self):
        ts,gs,a=fixture();gs[13]['m_Name']='Renamed'
        with self.assertRaises(ValueError):avatar_mapping(a,ts,gs,1)

    def test_external_pointer_rejected(self):
        with self.assertRaises(ValueError):pointer(dict(m_FileID=1,m_PathID=3))

    def test_controller_resolves_exact_clip_id_not_name_guess(self):
        state=dict(m_FullPathID=1,m_Speed=1,m_Loop=False,m_BlendTreeConstantArray=[dict(data=dict(m_NodeArray=[dict(data=dict(m_ChildIndices=[],m_ClipID=0))]))])
        c=dict(m_TOS=[(1,'Base Layer.Attack')],m_AnimationClips=[ref(123)],m_Controller=dict(m_StateMachineArray=[dict(data=dict(m_StateConstantArray=[dict(data=state)]))]))
        self.assertEqual(controller_leaves(c,{123:dict(m_Name='Exact')})[0]['leaves'][0]['clip_id'],123)
        with self.assertRaises(ValueError):controller_leaves(c,{124:dict(m_Name='Exact')})

    def test_unresolved_transform_and_nontrs_are_reported_not_dropped(self):
        t=dict(m_Name='attack',m_SampleRate=30,m_Legacy=False,m_MuscleClip=dict(m_StartTime=0,m_StopTime=1,m_Clip=dict(data=dict(m_StreamedClip=dict(curveCount=1),m_DenseClip=dict(m_CurveCount=0)))),
               m_ClipBindingConstant=dict(genericBindings=[dict(classID=4,customType=0,isPPtrCurve=0,script=ref(0),attribute=1,path=12),
                                                          dict(classID=95,customType=8,isPPtrCurve=0,script=ref(0),attribute=7,path=0)]))
        r=clip_inventory(5,t,{0});self.assertEqual(r['unresolved_transform_path_hashes'],[12]);self.assertEqual(r['non_trs_bindings'][0]['class_id'],95)
        self.assertEqual(r['streamed_curve_count'],1)

    def test_changed_bundle_hash_fails_before_unity_decode(self):
        if importlib.util.find_spec('UnityPy') is None:self.skipTest('Optional UnityPy unavailable')
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'bundle';p.write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError,'hash changed'):inventory(p,DESCRIPTORS['wolf'])

    def test_installed_wolf_and_rhino_repeatable(self):
        if importlib.util.find_spec('UnityPy') is None:self.skipTest('Optional UnityPy unavailable')
        base=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers/AssetBundles/Monster'
        if not all((base/d['bundle']).is_file() for d in DESCRIPTORS.values()):self.skipTest('Installed optional references unavailable')
        for name,descriptor in DESCRIPTORS.items():
            result=inventory(base/descriptor['bundle'],descriptor)
            self.assertEqual(result,inventory(base/descriptor['bundle'],descriptor))
            compact=compact_report(dict(schema=1,evidence='test',tool_sha256='a'*64,species=[result]))['species'][0]
            self.assertNotIn('nodes',compact['avatar_mapping']);self.assertEqual(compact['hierarchy_node_count'],len(result['avatar_mapping']['nodes']))
            self.assertFalse(result['runtime_sampler_qualified']);self.assertEqual(result['avatar_mapping']['human_bone_count'],0)
            self.assertEqual(len(result['clips']),23 if name=='wolf' else 37)
            self.assertEqual(len(result['unresolved_clip_path_hashes']),4 if name=='wolf' else 8)
            self.assertEqual(len(result['live_renderer']['submeshes']),1 if name=='wolf' else 4)
            if name=='wolf':self.assertEqual(result['auxiliary_pounce']['path'],'Bone037/Bip01')


if __name__=='__main__':unittest.main()
