#!/usr/bin/env python3
"""Structural rejection tests; optional real Unity prefab qualification."""
from pathlib import Path
import copy
import importlib.util
import json
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'rebuild/tools'))
import export_player_collider_center as center


def ptr(i):return {'m_FileID':0,'m_PathID':i}
def obj(kind,data,guid=None):return {'kind':kind,'data':data,'script_guid':guid}
def transform(go,parent):return obj('Transform',{'m_GameObject':ptr(go),'m_Father':ptr(parent),
    'm_LocalPosition':dict(x=0,y=0,z=0),'m_LocalScale':dict(x=1,y=1,z=1),
    'm_LocalRotation':dict(x=0,y=0,z=0,w=1)})
def fixture():
    return {1:obj('GameObject',{'m_Name':'FemaleModel','m_IsActive':True}),
        2:obj('GameObject',{'m_Name':'Model','m_IsActive':True}),
        3:obj('GameObject',{'m_Name':'Ragdoll','m_IsActive':True}),
        11:transform(1,0),12:transform(2,11),13:transform(3,11),
        20:obj('MonoBehaviour',{'m_GameObject':ptr(1),'m_Enabled':True,'modelController':ptr(21)},'view'),
        21:obj('MonoBehaviour',{'m_GameObject':ptr(2),'m_Enabled':True,'m_Colliders':[ptr(30)]},'model'),
        30:obj('CapsuleCollider',{'m_GameObject':ptr(2),'m_IsTrigger':False,'m_Enabled':True,
            'm_Direction':1,'m_Radius':.3700000047683716,'m_Height':1.75,'m_Center':dict(x=0,y=.875,z=0)}),
        31:obj('CapsuleCollider',{'m_GameObject':ptr(3),'m_IsTrigger':False,'m_Enabled':True})}


class CenterTests(unittest.TestCase):
    def qualify(self,data):return center.qualify(data,'Female',dict(model='model',view='view'))
    def reject(self,change):
        data=fixture();change(data)
        with self.assertRaises(ValueError):self.qualify(data)
    def test_model_subtree_excludes_sibling_ragdoll(self):
        result=self.qualify(fixture())
        self.assertEqual(result['local_center'],[0,.875,0])
        self.assertEqual(result['nontrigger_colliders_outside_model'],[31])
        self.assertEqual(result['collider'],30)
    def test_extra_model_collider_rejected(self):
        self.reject(lambda d:d[31]['data'].update(m_GameObject=ptr(2)))
    def test_disabled_or_trigger_root_rejected(self):
        for field in ['m_Enabled','m_IsTrigger']:
            self.reject(lambda d:d[30]['data'].update({field:field=='m_IsTrigger'}))
    def test_nonidentity_model_and_view_rejected(self):
        for tid in [11,12]:
            self.reject(lambda d:d[tid]['data']['m_LocalPosition'].update(y=.1))
            self.reject(lambda d:d[tid]['data']['m_LocalScale'].update(y=2))
    def test_binding_and_ancestry_rejected(self):
        self.reject(lambda d:d[20]['data'].update(modelController=ptr(999)))
        self.reject(lambda d:d[12]['data'].update(m_Father=ptr(13)))
        self.reject(lambda d:d[21]['data'].update(m_Colliders=[]))
    def test_changed_and_nonfinite_shape_rejected(self):
        for value in [.9,float('nan'),float('inf')]:
            self.reject(lambda d:d[30]['data']['m_Center'].update(y=value))
        self.reject(lambda d:d[30]['data'].update(m_Height=.1))
        self.reject(lambda d:d[30]['data'].update(m_Direction=0))
    def test_duplicate_controller_rejected(self):
        self.reject(lambda d:d.update({22:copy.deepcopy(d[21])}))
    def test_inactive_model_rejected(self):
        self.reject(lambda d:d[2]['data'].update(m_IsActive=False))
    def test_trigger_child_does_not_expand_bounds(self):
        data=fixture();data[31]['data'].update(m_GameObject=ptr(2),m_IsTrigger=True)
        result=self.qualify(data)
        self.assertEqual(result['ignored_trigger_colliders'],[31])
        self.assertEqual(result['local_center'],[0,.875,0])
    def test_manifest_and_header_are_finite(self):
        report={'actors':[self.qualify(fixture()),self.qualify(fixture())]}
        self.assertEqual(center.header(report).count('{{0.0f, 0.875f, 0.0f}}'),2)

    @unittest.skipUnless(importlib.util.find_spec('UnityPy'),'UnityPy unavailable: actual prefab qualification not exercised')
    def test_actual_prefabs_match_generated(self):
        actual=center.build(ROOT)
        expected=json.loads((ROOT/'rebuild/generated/pe_player_collider_center_source.json').read_text())
        self.assertEqual(actual,expected)
        self.assertEqual([a['collider'] for a in actual['actors']],[13600000,13654112])
        self.assertTrue(all(len(a['nontrigger_colliders_outside_model'])==11 for a in actual['actors']))
        self.assertEqual(center.header(actual),(ROOT/'rebuild/generated/pe_player_collider_center_source.h').read_text())

if __name__=='__main__':unittest.main()
