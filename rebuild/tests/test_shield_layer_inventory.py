#!/usr/bin/env python3
"""Read-only source guard tests; optional UnityPy/source fixture is explicit."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import inventory_shield_layers as tool


def fixture():
    names = {1:'LArmOnly', 2:'LArmOnly.SheildBlock', 3:'LArmOnly.DoNothing',
             4:'SheildBlock', 5:'MoveStyle', 6:'UpbodyOnly.SwordPutOn',
             7:'UpbodyOnly.SwordPutOff', 8:'InWater', 9:'MoveStyleH', 10:'MoveStyleV'}
    def transition(mode, dest, duration):
        return {'data': {'m_ConditionConstantArray':[{'data':{'m_EventID':4,'m_ConditionMode':mode}}],
                'm_DestinationState':dest,'m_HasFixedDuration':False,'m_HasExitTime':False,
                'm_TransitionDuration':duration}}
    def blend(param, clips):
        return [{'data': {'m_NodeArray': [{'data': {'m_BlendEventID':param,'m_BlendType':0,
                    'm_ChildIndices':[1,2], 'm_Blend1dData':{'data':{'m_ChildThresholdArray':[0.,1.]}}}},
                    *[{'data':{'m_ClipID':i, 'm_ChildIndices':[]}} for i in clips]]}}]
    block={'m_FullPathID':2,'m_Speed':1.,'m_IKOnFeet':False,
           'm_BlendTreeConstantArray':blend(5,(0,1)),
           'm_TransitionConstantArray':[transition(2,0,.10209385305643082)]}
    idle={'m_FullPathID':3,'m_TransitionConstantArray':[transition(1,1,.09999996423721313)]}
    draws=[{'m_FullPathID':i,'m_Speed':1.2000000476837158,
            'm_BlendTreeConstantArray':blend(8,(j,j))} for i,j in ((6,2),(7,3))]
    layer={'m_Binding':1,'m_StateMachineIndex':3,'m_BodyMask':dict(tool.MASK),
           'm_SkeletonMask':{'data':{'m_Data':[]}},'(int&)m_LayerBlendingMode':0,
           'm_DefaultWeight':1.,'m_IKPass':False,'m_SyncedLayerAffectsTiming':False}
    return {'m_Name':'FemaleAnimator2','m_TOS':list(names.items()),
            'm_Controller':{'m_LayerArray':[{}, {}, {}, {'data':layer}],
              'm_StateMachineArray':[{'data':{'m_StateConstantArray':[{'data':s} for s in [idle,block,*draws]]}}],
              'm_Values':{'data':{'m_ValueArray':[{'m_ID':i,'m_Type':4 if i==4 else 1} for i in (4,5,8,9,10)]}}},
            'm_AnimationClips':[{'m_FileID':0,'m_PathID':i} for i in (420,413,270,275)],
            'm_StateMachineBehaviours':[{'m_FileID':0,'m_PathID':111}],
            'm_StateMachineBehaviourVectorDescription':{'m_StateMachineBehaviourRanges':[
                [{'m_StateID':6,'m_LayerIndex':2},{'m_StartIndex':0,'m_Count':1}]],
                'm_StateMachineBehaviourIndices':[0]}}


class ContractTests(unittest.TestCase):
    def setUp(self): self.c=fixture()
    def test_exact_mask_and_callback_retained(self):
        plan=tool.contract(self.c)
        self.assertEqual(plan['layer']['m_BodyMask'],tool.MASK)
        self.assertEqual(plan['states']['UpbodyOnly.SwordPutOn']['behaviour_references'],[{'m_FileID':0,'m_PathID':111}])
        self.assertEqual(plan['states']['LArmOnly.SheildBlock']['behaviour_references'],[])
    def test_fullbody_substitute_rejected(self):
        self.c['m_Controller']['m_LayerArray'][3]['data']['m_BodyMask']['word0']=0xffffffff
        with self.assertRaisesRegex(ValueError,'mask'):tool.contract(self.c)
    def test_wrong_layer_weight_rejected(self):
        self.c['m_Controller']['m_LayerArray'][3]['data']['m_DefaultWeight']=0
        with self.assertRaisesRegex(ValueError,'weight'):tool.contract(self.c)
    def test_normal_block_substitute_rejected(self):
        self.c['m_AnimationClips'][1]['m_PathID']=420
        with self.assertRaisesRegex(ValueError,'identity'):tool.contract(self.c)
    def test_transition_seconds_substitute_rejected(self):
        self.c['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'][0]['data']['m_TransitionConstantArray'][0]['data']['m_HasFixedDuration']=True
        with self.assertRaisesRegex(ValueError,'Normalized'):tool.contract(self.c)
    def test_wrong_style_threshold_rejected(self):
        self.c['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'][1]['data']['m_BlendTreeConstantArray'][0]['data']['m_NodeArray'][0]['data']['m_Blend1dData']['data']['m_ChildThresholdArray']=[0,2]
        with self.assertRaisesRegex(ValueError,'thresholds'):tool.contract(self.c)
    def test_wrong_draw_clip_rejected(self):
        self.c['m_AnimationClips'][2]['m_PathID']=413
        with self.assertRaisesRegex(ValueError,'draw/stow identity'):tool.contract(self.c)


class InstalledTests(unittest.TestCase):
    def test_installed_plan_preserves_unqualified_gate(self):
        asset=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers/PE_Client_Data/sharedassets1.assets'
        if not importlib.util.find_spec('UnityPy') or not asset.is_file():
            self.skipTest('optional UnityPy and installed asset required')
        plan=tool.inventory(asset)
        self.assertFalse(plan['pure_clone_capture_ready'])
        self.assertFalse(plan['runtime_pose_export_ready'])
        self.assertEqual(plan['controller_behaviour_count'],140)
        clips={c['path_id']:c for c in plan['clips']}
        self.assertEqual(clips[413]['events'],[])
        self.assertEqual(clips[270]['events'][0]['data'],'PutOnEquipment')
        self.assertAlmostEqual(clips[270]['events'][0]['time'],.16318383812904358)
        self.assertEqual(clips[275]['events'][0]['data'],'PutOffEquipment')
        self.assertEqual(len(plan['asset_sha256']),64)
        json.dumps(plan,allow_nan=False)


if __name__=='__main__':unittest.main()
