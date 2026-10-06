#!/usr/bin/env python3
"""Synthetic simultaneous-layer qualification; no original runtime is launched."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import analyze_shield_pose as tool
from test_reference_pose_analysis import fixture, write


def shield_fixture():
    rows,rig=fixture();rig['sex']='female';rig['avatar']['m_Name']='FemaleAvatar'
    frames=[]
    for index,gait in enumerate(tool.GAITS):
        frame=copy.deepcopy(rows[1]);frame.update(sample=index,frame=100+index,time=3.+index,realtime=5.+index,fixedTime=3.+index,gender='Female')
        a=frame['animators'][0];a.update(avatar='FemaleAvatar',controller='FemaleAnimator2',parameters={'SheildBlock':True,'MoveStyle':1.})
        base=a['layers'][0];base['name']='Base Layer';base['currentClips'][0]['name']=gait
        block=copy.deepcopy(base);block['name']='LArmOnly';block['current']['fullPathHash']=tool.BLOCK_STATE
        block['currentClips'][0]['name']='SwordSheildBlock'
        full=copy.deepcopy(base);full.update(name='FullBody',currentClips=[])
        upper=copy.deepcopy(full);upper['name']='UpbodyOnly'
        a['layers']=[base,full,upper,block];frames.append(frame)
    return [rows[0],*frames,{'type':'end','frames':3,'reason':'capture-limit'}],rig


class ShieldTests(unittest.TestCase):
    def test_same_frame_three_gaits(self):
        rows,_=shield_fixture()
        self.assertEqual([tool.classify(f)['steady_composition'] for f in rows[1:-1]],list(tool.GAITS))
    def test_missing_base_not_substituted(self):
        rows,_=shield_fixture();rows[1]['animators'][0]['layers'][0]['currentClips']=[]
        self.assertIsNone(tool.classify(rows[1])['steady_composition'])
    def test_normal_leaf_false_bool_or_transition_not_steady(self):
        for mutation in ('normal','false','transition','weight','style'):
            rows,_=shield_fixture();a=rows[1]['animators'][0];b=a['layers'][3]
            if mutation=='normal':b['currentClips'][0]['name']='NormalSheildBlock'
            elif mutation=='false':a['parameters']['SheildBlock']=False
            elif mutation=='transition':b['inTransition']=True
            elif mutation=='weight':b['weight']=0
            else:a['parameters']['MoveStyle']=0
            with self.subTest(mutation=mutation):self.assertFalse(tool.classify(rows[1])['steady_sword_block'])
    def test_split_animators_cannot_combine(self):
        rows,_=shield_fixture();a=rows[1]['animators'][0];other=copy.deepcopy(a);other['id']=101
        rows[1]['animators'].append(other)
        with self.assertRaisesRegex(ValueError,'one active'):tool.classify(rows[1])
    def test_mixed_gait_stays_mixed(self):
        rows,_=shield_fixture();clips=rows[1]['animators'][0]['layers'][0]['currentClips'];clips[0]['weight']=.5
        clips.append(dict(name='SwordWF',id=44,weight=.5,length=1.1))
        self.assertIsNone(tool.classify(rows[1])['steady_composition'])
    def test_other_active_override_excluded_from_steady_pair(self):
        rows,_=shield_fixture();a=rows[1]['animators'][0]
        a['layers'][1]['currentClips']=[dict(name='Attack6',id=22,weight=1.,length=1.)]
        evidence=tool.classify(rows[1])
        self.assertTrue(evidence['steady_sword_block'])
        self.assertIsNone(evidence['steady_composition'])
        self.assertEqual(evidence['other_active_layers'][0]['name'],'FullBody')
    def test_report_closure_and_provenance(self):
        rows,rig=shield_fixture()
        with tempfile.TemporaryDirectory() as directory:
            d=Path(directory);asset=d/'source';asset.write_bytes(b'synthetic source only')
            plan=d/'plan.json';plan.write_text(json.dumps({'schema':'pe-shield-layer-plan-1','layer_index':3,
                'layer':{'m_BodyMask':tool.MASK},'asset':str(asset),'asset_sha256':tool.sha(asset)}))
            rig_path=d/'rig.json';rig_path.write_text(json.dumps(rig));capture=d/'capture.jsonl'
            capture.with_suffix('.launch.json').write_text(json.dumps({'capture':str(capture)}));write(capture,rows)
            result=tool.report(capture,rig_path,plan)
            self.assertTrue(result['all_three_steady_compositions_observed'])
            self.assertFalse(result['runtime_pose_export_ready'])
            self.assertEqual(result['steady_composition_frames'],dict.fromkeys(tool.GAITS,1))
            write(capture,rows[:-1])
            with self.assertRaises(ValueError):tool.report(capture,rig_path,plan)
            rows[-1]['reason']='error';write(capture,rows)
            with self.assertRaisesRegex(ValueError,'error'):tool.report(capture,rig_path,plan)
            rows[-1]['reason']='capture-limit';write(capture,rows);asset.write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError,'Source asset changed'):tool.report(capture,rig_path,plan)


if __name__=='__main__':unittest.main()
