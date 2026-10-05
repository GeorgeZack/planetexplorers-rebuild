#!/usr/bin/env python3
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from export_training_npcs import roster,skin_rest,main
from export_reference_assets import IDENTITY

class TrainingNpcs(unittest.TestCase):
    def test_actual_roster_includes_other_two_training_npcs(self):
        rows=roster();self.assertEqual([r['entity_id'] for r in rows],[9004,9005,9006,9009,9020,9031,9041,9047,9049])
        by_id={r['entity_id']:r for r in rows}
        self.assertEqual(by_id[9041]['name'],'Andy');self.assertEqual(by_id[9041]['position'],[32.,1.8,12.])
        self.assertEqual(by_id[9004]['appearance_resource'],'Model/Npc/Adisa_Mostafa:Adisa-Mostafa_1')
        self.assertEqual(len({r['bundle'] for r in rows}),9)
        self.assertTrue(all('Prefab/Human/' in r['source_prefab'] for r in rows))

    def test_actual_skinning_preserves_weighted_bone_translation(self):
        skin={**{f'weight[{i}]':(.25 if i==0 else .75 if i==1 else 0.) for i in range(4)},**{f'boneIndex[{i}]':min(i,1) for i in range(4)}}
        second=copy.deepcopy(IDENTITY);second[0][3]=2.
        positions,normals=skin_rest([[1.,0.,0.]],[[0.,1.,0.]],[skin],[IDENTITY,second])
        self.assertEqual(positions,[[2.5,0.,0.]]);self.assertEqual(normals,[[0.,1.,0.]])
        bad=dict(skin);bad['weight[0]']=.5
        with self.assertRaises(ValueError):skin_rest([[1,0,0]],[[0,1,0]],[bad],[IDENTITY,second])

    def test_failed_export_keeps_destination_unpublished(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);out=root/'out';game=root/'game'
            def fail(game,staging):
                (staging/'partial.obj').write_text('partial');raise ValueError('missing asset')
            args=['export','--output',str(out),'--game-dir',str(game)]
            with patch('sys.argv',args),patch('export_training_npcs.recover',side_effect=fail):
                with self.assertRaises(ValueError):main()
            self.assertFalse(out.exists());self.assertEqual(list(root.iterdir()),[])
            out.mkdir();(out/'keep').write_text('keep')
            with patch('sys.argv',args),patch('export_training_npcs.recover') as recover:
                with self.assertRaises(ValueError):main()
                recover.assert_not_called()
            self.assertEqual((out/'keep').read_text(),'keep')

    def test_optional_real_npc_assets(self):
        directory=Path(__file__).resolve().parents[1]/'build/reference-assets/npcs'
        if not (directory/'manifest.json').exists():self.skipTest('Optional original NPC assets not exported')
        report=json.loads((directory/'manifest.json').read_text());self.assertEqual(len(report['npcs']),9)
        plan=json.loads((directory/'npc-plan.json').read_text());self.assertEqual(len(plan['npcs']),9)
        meshes=set();avatars=set()
        for npc in report['npcs']:
            folder=directory/str(npc['entity_id']);rig=json.loads((folder/'rig.json').read_text());self.assertFalse(rig['animated'])
            from analyze_reference_bake import mapping_for
            skeleton=dict(gender=rig['sex'].capitalize(),avatar=rig['avatar']['m_Name'],nodes=[dict(path=n['relative_path']) for n in rig['skeleton']])
            self.assertFalse(mapping_for(rig,skeleton)['missing_bones'])
            self.assertEqual(len({n['relative_path'] for n in rig['skeleton']}),len(rig['skeleton']))
            avatars.add(npc['avatar']['sha256']);self.assertEqual(npc['effective_idle']['humanoid_bindings'],127)
            self.assertEqual((folder/'model.obj').read_text().splitlines()[0],'# Baked Unity XYZ meters; original vertex/normal coordinate convention')
            self.assertEqual((folder/'model.mtl').read_text().splitlines()[0],'# Source material slots; diffuse-only preview conversion')
            for part in rig['parts']:
                meshes.add(part['mesh']['sha256']);self.assertEqual(len(part['bone_names']),len(part['inverse_binds']))
                self.assertEqual(len(part['bone_paths']),len(part['bone_names']))
                self.assertTrue(all(path.startswith('Bip01') for path in part['bone_paths']))
            for material in rig['materials']:
                if 'diffuse_file' in material:self.assertTrue((folder/material['diffuse_file']).read_bytes().startswith(b'\x89PNG\r\n\x1a\n'))
        self.assertEqual(len(meshes),9);self.assertEqual(len(avatars),9)

if __name__=='__main__':unittest.main()
