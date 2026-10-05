#!/usr/bin/env python3
"""Math/format tests are stdlib; local asset integration requires optional UnityPy."""
import copy
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'rebuild/tools'))
import export_reference_player as exporter
GAME=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'
HAS_UNITY=importlib.util.find_spec('UnityPy') is not None


class PlayerFormatTests(unittest.TestCase):
    def test_default_selection_uses_source_indices_and_database_string_types(self):
        database=ROOT/'Assets/Resources/DataBase/localData.bytes'
        male,choice=exporter.default_parts(database,'male')
        self.assertEqual((choice['head_row_id'],choice['hair_row_id']),(15,48))
        self.assertEqual(male[4],'Model/PlayerModel/m_head_02-m_head_02')
        self.assertEqual(len(set(male)),8)
        female,choice=exporter.default_parts(database,'female')
        self.assertEqual((choice['head_row_id'],choice['hair_row_id']),(1,28))
        self.assertEqual(female[-1],'Model/PlayerModel/w_hair_00-hair_0C')

    def test_starter_clothing_replaces_only_source_equipment_slots(self):
        database=ROOT/'Assets/Resources/DataBase/localData.bytes'
        for sex,ids in [('female',[95,131,192]),('male',[113,149,210])]:
            original,_=exporter.default_parts(database,sex)
            clothed,selection=exporter.starter_parts(database,sex)
            self.assertEqual([x['item_id'] for x in selection['starter_clothing']],ids)
            self.assertEqual([x['replace_mask'] for x in selection['starter_clothing']],[2,64,128])
            self.assertEqual([i for i,(a,b) in enumerate(zip(original,clothed)) if a!=b],[0,1,3])
            self.assertEqual(clothed[0],f'Model/PlayerModel/{sex}01-torso')

    def test_bad_weight_sum_and_joint_rejected(self):
        row={**{f'weight[{i}]':1. if i==0 else 0. for i in range(4)},
             **{f'boneIndex[{i}]':0 for i in range(4)}}
        exporter.validate_skin([row],1)
        for key,value in [('weight[0]',float('nan')),('weight[0]',.5),('boneIndex[0]',1)]:
            bad=dict(row);bad[key]=value
            with self.assertRaises(ValueError): exporter.validate_skin([bad],1)

    def test_packed_channels_partition_dense_and_constant_without_retargeting(self):
        binding=lambda c,a,t:dict(classID=c,attribute=a,customType=t,isPPtrCurve=False,path=0)
        clip={'m_Name':'fixture','m_MuscleClip':{'m_StopTime':1.,'m_Clip':{'data':{
            'm_StreamedClip':{'curveCount':0},'m_DenseClip':{'m_FrameCount':2,'m_CurveCount':4,
            'm_SampleRate':1.,'m_BeginTime':0.,'m_SampleArray':[1,2,3,4,5,6,7,8]},
            'm_ConstantClip':{'data':[.5]}}}},
            'm_ClipBindingConstant':{'genericBindings':[binding(4,1,0),binding(95,7,8),binding(95,8,8)]}}
        result=exporter.packed_channels(clip,{'m_TOS':[(0,'')]})
        self.assertEqual(result['bindings'][0]['samples'],[[1,2,3],[5,6,7]])
        self.assertEqual(result['bindings'][1]['samples'],[[4],[8]])
        self.assertEqual(result['bindings'][2]['samples'],[[.5]])
        self.assertFalse(result['bone_poses_evaluated'])
        bad=copy.deepcopy(clip);bad['m_ClipBindingConstant']['genericBindings'][0]['attribute']=2
        with self.assertRaises(ValueError):exporter.packed_channels(bad,{'m_TOS':[]})

    def test_missing_input_preserves_existing_export(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/'out';output.mkdir();existing=output/'player-manifest.json'
            existing.write_text('preserve')
            result=subprocess.run([sys.executable,str(ROOT/'rebuild/tools/export_reference_player.py'),
                                   '--game-dir',str(Path(directory)/'missing'),'--output',str(output)],capture_output=True)
            self.assertNotEqual(result.returncode,0)
            self.assertEqual(existing.read_text(),'preserve')


@unittest.skipUnless(HAS_UNITY and (GAME/'PE_Client_Data/resources.assets').is_file(),'requires local original assets and optional UnityPy')
class InstalledPlayerTests(unittest.TestCase):
    def test_starter_female_mesh_and_material_closure(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)
            manifest=exporter.recover(GAME,output,['female'],'starter')
            player=manifest['players'][0]
            self.assertEqual((player['vertices'],player['triangles']),(4001,5560))
            rig=json.loads((output/player['rig']).read_text())
            self.assertEqual([rig['parts'][i]['mesh_id'] for i in (0,1,3)],[7134,6078,7273])
            self.assertEqual(rig['appearance'],'starter')
            self.assertEqual(sum(len(p['triangles']) for p in rig['parts']),9)
            for material in rig['materials']:
                self.assertTrue((output/material['diffuse_file']).is_file())
            self.assertEqual(sum(len(rig['parts'][i]['bone_names'])*len(rig['parts'][i]['triangles']) for i in (5,6,7,4,0,2,1,3)),67)

    def test_real_default_meshes_skin_channels_and_repeatability(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)
            first=exporter.recover(GAME,output,['male','female'])
            hashes={p.name:exporter.digest(p) for p in output.iterdir()}
            self.assertEqual([(p['vertices'],p['triangles']) for p in first['players']],[(3480,5210),(3707,5280)])
            for player in first['players']:
                rig=json.loads((output/player['rig']).read_text())
                self.assertFalse(rig['animated']);self.assertEqual(len(rig['parts']),8)
                self.assertLess(abs(player['bounds'][0][1]),.002)
                self.assertTrue(1.6<player['bounds'][1][1]<1.9)
                for part in rig['parts']:
                    exporter.validate_skin(part['skin'],len(part['bone_names']))
                    for normal in part['default_normals']:
                        self.assertAlmostEqual(sum(v*v for v in normal),1.,places=6)
                channel=json.loads((output/f"player-{player['sex']}-default-attack6-channels.json").read_text())
                self.assertEqual(sum(b['kind']=='humanoid_scalar_unretargeted' for b in channel['bindings']),127)
                self.assertFalse(channel['bone_poses_evaluated'])
                # Main hand is humanoid; the direct forearm helper is not the hand.
                self.assertFalse(any(b['kind']=='direct_transform' and (b['path'] or '').endswith('/Bip01 R Hand') for b in channel['bindings']))
            self.assertEqual(first,exporter.recover(GAME,output,['male','female']))
            self.assertEqual(hashes,{p.name:exporter.digest(p) for p in output.iterdir()})

if __name__=='__main__':unittest.main()
