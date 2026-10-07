#!/usr/bin/env python3
"""Offline machine/source/archive qualification; requires isolated UnityPy."""
import copy
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_camp_repair as ex


class CampRepairExport(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Absence is an explicit failure here, not a silently passing skip.
        import UnityPy  # noqa: F401
        cls.outputs=ex.build_outputs()
        cls.manifest=json.loads(cls.outputs['pe_camp_repair_source.json'])
        cls.archive=ex.yird_records(ex.DEFAULT_SAVE/'MainAdventure')[0]['ArchiveKeyCSDataMgr']

    def test_exact_regeneration(self):
        for name,data in self.outputs.items():
            self.assertEqual((ex.ROOT/'rebuild/generated'/name).read_bytes(),data)

    def test_machine_binding_and_empty_saved_owner(self):
        m=self.manifest
        self.assertEqual(m['source_prefab']['object_type'],5)
        self.assertEqual(m['source_prefab']['ordinary_repair_prototype'],1130)
        self.assertEqual(m['source_prefab']['item_id_field'],0)
        self.assertEqual(m['installed_prefab']['work_transforms'],0)
        self.assertEqual(m['catalog']['colony_info']['_property'],'60')
        for profile in m['archives']:
            self.assertEqual(profile['entity_id'],20006)
            self.assertEqual(profile['doodad_prototype'],145)
            self.assertEqual(profile['pose']['position'],[-613.5,29.5,404.])
            self.assertEqual(profile['repair']['job'],dict(item_instance_id=-1,current_seconds=0,final_seconds=-1))
            self.assertEqual(next(c for c in profile['creators'] if c['id']==10000)['type'],1)
        self.assertFalse(m['archives'][0]['repair']['alive'])
        self.assertTrue(m['archives'][1]['repair']['alive'])

    def test_source_interaction_box_and_real_range(self):
        m=self.manifest
        self.assertEqual(m['source_prefab']['collider']['center'],[0,.5,0])
        self.assertEqual(m['source_prefab']['collider']['size'],[1,1,1])
        self.assertEqual(m['installed_prefab']['collider'],dict(center=[0,.5,0],size=[1,1,1]))
        self.assertEqual(m['source_prefab']['root_transform'],dict(position=[0,0,0],rotation=[0,0,0,1],scale=[1,1,1]))
        self.assertEqual(m['interaction']['operate_distance'],19)
        self.assertEqual(m['interaction']['window_close_distance'],8)
        self.assertEqual(m['interaction']['ray_distance'],100)
        self.assertEqual(m['interaction']['source_obstacle_allowance'],10)
        for a in m['archives']:self.assertEqual(a['pose']['rotation'],[0,1,0,-4.371138828673793e-08])

    def test_catalog_closure_and_explicit_exclusions(self):
        c=self.manifest['catalog'];q={r['item_id']:r for r in c['qualified']};excluded={r['item_id']:r for r in c['excluded']}
        self.assertEqual((c['catalog_item_count'],c['repair_component_count'],len(q)),(341,71,63))
        self.assertEqual(set(excluded),{33,67,365,370,1473,1474,1475,1527})
        for identity in (1,12,19,26,34):self.assertEqual(q[identity]['materials'],[dict(item_id=279,count=4)])
        for identity in (95,131):self.assertEqual(q[identity]['materials'],[dict(item_id=1006,count=2)])
        self.assertEqual(q[192]['materials'],[dict(item_id=1006,count=1)])
        for identity in (33,67,1527):
            self.assertFalse(excluded[identity]['materials']);self.assertIn('nonempty',excluded[identity]['exclusion'])
        self.assertEqual(excluded[365]['missing_materials'],[1111])
        self.assertTrue(all(not r['missing_materials'] for r in q.values()))
        self.assertEqual(self.manifest['semantics']['ticks_per_second'],100000)
        self.assertEqual(self.manifest['semantics']['base_seconds'],60)

    def test_archive_decoder_rejects_every_truncation_and_trailing_data(self):
        for n in range(len(self.archive)):
            with self.subTest(length=n),self.assertRaises(ValueError):ex.read_colony(self.archive[:n])
        with self.assertRaises(ValueError):ex.read_colony(self.archive+b'\0')

    def test_archive_rejects_wrong_creator_type_and_unobserved_job(self):
        # Offsets refer to the explicitly pinned326-byte snapshot, not a general archive layout.
        for offset,fmt,value in [(0,'i',16102101),(4,'i',5000),(12,'i',0),(28,'i',1),
                                 (56,'i',9999),(96,'B',2),(178,'B',2),(258,'i',42),
                                 (262,'f',float('nan')),(266,'f',60)]:
            data=bytearray(self.archive);struct.pack_into('<'+fmt,data,offset,value)
            with self.subTest(offset=offset),self.assertRaises(ValueError):ex.read_colony(bytes(data))

    def test_material_parser_is_bounded_and_preserves_order(self):
        self.assertEqual(ex.materials('0'),[])
        self.assertEqual(ex.materials('279,4;1006,2'),[dict(item_id=279,count=4),dict(item_id=1006,count=2)])
        for value in ('279','279,0','279,-1','0,4','279,4;279,2','279,4,5',';',';'.join(f'{i},1' for i in range(1,10))):
            with self.subTest(value=value),self.assertRaises(ValueError):ex.materials(value)

    def test_source_drift_is_not_accepted(self):
        with tempfile.TemporaryDirectory(dir=ex.ROOT/'rebuild/build') as tmp:
            root=Path(tmp)
            for path in ex.SOURCE_PINS:
                target=root/path;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ex.ROOT/path,target)
            ex.validate_sources(root)
            mutations=[('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRepairWnd.cs',b'Mathf.CeilToInt',b'Mathf.FloorToInt'),
                ('Assets/Scripts/WuYiqiu/Colony System/Scripts/Entities/CSRepair.cs',b'CounterToRunning();',b'StopCounter();'),
                ('Assets/Scripts/WuYiqiu/Colony/CounterScript.cs',b'm_Timer.ElapseSpeed = -1;',b'm_Timer.ElapseSpeed = -2;'),
                ('Assets/Scripts/Items/ItemManager.cs',b'IsCurrentMax()',b'ChangedCurrentMax()'),
                ('Assets/Scripts/WuYiqiu/Colony System/Scripts/CSDataInst.cs',b'w.Write(csrd.m_ObjID);',b'w.Write(-1);')]
            for path,before,after in mutations:
                target=root/path;original=target.read_bytes();self.assertIn(before,original);target.write_bytes(original.replace(before,after,1))
                with self.subTest(path=path),self.assertRaises(ValueError):ex.validate_sources(root)
                target.write_bytes(original)

    def test_source_prefab_requires_real_components_and_material_binding(self):
        static=ex.read_prefab(ex.ROOT/ex.STATIC_PREFAB);furniture=ex.read_prefab(ex.ROOT/ex.FURNITURE_PREFAB)
        for what in ('disabled','type','workers','ordinary_id','box_size','root_offset','extra_collider','range_override'):
            a,b=copy.deepcopy(static),copy.deepcopy(furniture)
            repair=next(v['data'] for v in a.values() if v.get('script_guid')==ex.REPAIR_GUID)
            ordinary=next(v['data'] for v in b.values() if v.get('script_guid')==ex.REPAIR_GUID)
            if what=='disabled':repair['m_Enabled']=0
            elif what=='type':repair['m_Type']=6
            elif what=='workers':repair['m_WorkTrans']=[{'m_FileID':0,'m_PathID':400000}]
            elif what=='ordinary_id':ordinary['m_ItemID']=1132
            elif what=='box_size':next(v['data'] for v in a.values() if v['kind']=='BoxCollider')['m_Size']['x']=2
            elif what=='root_offset':next(v['data'] for v in a.values() if v['kind']=='Transform' and v['data']['m_GameObject']['m_PathID']==100048)['m_LocalPosition']['x']=1
            elif what=='extra_collider':a[-999]=copy.deepcopy(next(v for v in a.values() if v['kind']=='BoxCollider'))
            else:next(v['data'] for v in a.values() if v.get('script_guid')==ex.OPERATE_GUID)['operateDistance']=6
            with self.subTest(what=what),patch.object(ex,'read_prefab',side_effect=[a,b]),self.assertRaises(ValueError):ex.source_prefabs()

    def test_missing_catalog_material_fails_profile_closure(self):
        catalog=json.loads((ex.ROOT/'rebuild/generated/content_manifest.json').read_text())
        catalog['selected_item_ids'].remove(279);catalog['items']=[r for r in catalog['items'] if r['id']!=279]
        with tempfile.TemporaryDirectory(dir=ex.ROOT/'rebuild/build') as tmp:
            p=Path(tmp)/'catalog.json';p.write_text(json.dumps(catalog))
            result=ex.catalog_profiles(catalog_path=p)
            self.assertNotIn(1,{r['item_id'] for r in result['qualified']})
            row=next(r for r in result['excluded'] if r['item_id']==1)
            self.assertEqual(row['exclusion'],'missing runtime material')
            self.assertEqual(row['missing_materials'],[279])
            catalog['items'][0]['durability_max']+=1;p.write_text(json.dumps(catalog))
            with self.assertRaises(ValueError):ex.catalog_profiles(catalog_path=p)

    def test_changed_database_or_bundle_rejected(self):
        with tempfile.TemporaryDirectory(dir=ex.ROOT/'rebuild/build') as tmp:
            p=Path(tmp)/'changed';p.write_bytes(b'changed')
            with self.assertRaises(ValueError):ex.catalog_profiles(database=p)
            with self.assertRaises(ValueError):ex.installed_prefab(bundle=p)

if __name__=='__main__':unittest.main()
