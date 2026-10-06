#!/usr/bin/env python3
import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'rebuild/tools'))
import export_hare_history_center as export

class Generated(unittest.TestCase):
    def test_header_tracks_qualified_report(self):
        report=json.loads((export.OUTPUT/'pe_hare_history_center_source.json').read_text())
        self.assertEqual(export.render(report),(export.OUTPUT/'pe_hare_history_center_source.h').read_text())
        self.assertEqual(report['local_center'],[0,.75,0])
        self.assertEqual(report['solid_colliders'],[export.CAPSULE])
        self.assertEqual(len(report['excluded_triggers']),3)
        self.assertEqual(len(report['excluded_other_subtree_colliders']),18)

@unittest.skipUnless(importlib.util.find_spec('UnityPy'),'optional UnityPy unavailable')
class Original(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import UnityPy
        cls.bundle=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'/export.BUNDLE
        if not cls.bundle.is_file():raise unittest.SkipTest('original hare bundle unavailable')
        cls.objects={o.path_id:(o.type.name,o.read_typetree()) for o in UnityPy.load(str(cls.bundle)).objects if o.type.name in ('Transform','GameObject','MonoBehaviour','MonoScript','CapsuleCollider','SphereCollider','BoxCollider','MeshCollider','TerrainCollider','CharacterController')}
    def test_regeneration(self):
        self.assertEqual(export.recover(self.bundle),json.loads((export.OUTPUT/'pe_hare_history_center_source.json').read_text()))
    def test_identity_scale_and_active_guards(self):
        for tid in (export.MODEL,export.MODEL_ROOT_ID,export.COLLIDER_TRANSFORM):
            objects=copy.deepcopy(self.objects);objects[tid][1]['m_LocalScale']['x']=2
            with self.assertRaisesRegex(ValueError,'Nonidentity'):export.qualify(objects)
            objects=copy.deepcopy(self.objects);go=export.pointer(objects[tid][1]['m_GameObject']);objects[go][1]['m_IsActive']=False
            with self.assertRaisesRegex(ValueError,'Inactive'):export.qualify(objects)
    def test_capsule_changes_fail_closed(self):
        for key,value in [('m_Enabled',False),('m_IsTrigger',True),('m_Height',2),('m_Radius',.7),('m_Direction',2)]:
            objects=copy.deepcopy(self.objects);objects[export.CAPSULE][1][key]=value
            with self.assertRaises(ValueError):export.qualify(objects)
    def test_ragdoll_is_not_silently_included(self):
        objects=copy.deepcopy(self.objects)
        # Reparent an excluded ragdoll collider into model; exporter must refuse
        # to pretend sole capsule center still describes the source Bounds union.
        ragdoll=objects[6686478514934414328][1];go=export.pointer(ragdoll['m_GameObject'])
        tid=next(i for i,(k,t) in objects.items() if k=='Transform' and export.pointer(t['m_GameObject'])==go)
        objects[tid][1]['m_Father']['m_PathID']=export.MODEL
        with self.assertRaisesRegex(ValueError,'exactly'):export.qualify(objects)
    def test_trigger_becoming_solid_requires_new_derivation(self):
        objects=copy.deepcopy(self.objects);objects[4751048594153301391][1]['m_IsTrigger']=False
        with self.assertRaisesRegex(ValueError,'exactly'):export.qualify(objects)
    def test_controller_and_model_selection_guarded(self):
        objects=copy.deepcopy(self.objects);objects[export.VIEW_ROOT][1]['modelController']['m_PathID']=8300455160741710545
        with self.assertRaisesRegex(ValueError,'binding'):export.qualify(objects)
        objects=copy.deepcopy(self.objects);script=export.pointer(objects[export.CONTROLLER][1]['m_Script'])
        objects[script][1]['m_ClassName']='PERagdollController'
        with self.assertRaisesRegex(ValueError,'script'):export.qualify(objects)

if __name__=='__main__':unittest.main()
