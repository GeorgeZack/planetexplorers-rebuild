#!/usr/bin/env python3
"""Replay admission tests: retain actual mixed leaves and independent phases."""
import copy
from pathlib import Path
import sys
import unittest
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import reference_shield_composition as tool
import analyze_shield_composition as analyze
from test_shield_pose_analysis import shield_fixture


def example():
    rows,_=shield_fixture();r=rows[1];a=r['animators'][0]
    a['parameters'].update(FirstPerson=0.,attack=None,Owner=1)
    a['layers'][0]['current']['fullPathHash']=2858398145
    a['layers'][0]['current']['normalizedTime']=17.25
    a['layers'][3]['current']['normalizedTime']=2.75
    a['layers'][0]['currentClips'][0]['weight']=.99
    a['layers'][0]['currentClips'].append({'name':'NormalIdle','id':4,'weight':.01,'length':2.})
    types={'FirstPerson':1,'SheildBlock':4,'MoveStyle':1,'attack':9,'Owner':3}
    return r,types


class ReplayTests(unittest.TestCase):
    def test_mixed_evidence_and_independent_clocks_preserved(self):
        row,types=example();out=tool.frame_plan(row,types)
        self.assertIsNone(out['evidence']['steady_composition'])
        self.assertEqual(out['evidence']['base_normalized_time'],17.25)
        self.assertEqual(out['evidence']['block_normalized_time'],2.75)
        self.assertEqual(len(out['evidence']['base_clips']),2)
        self.assertEqual(out['evidence']['base_clips'][1]['weight'],.01)
        self.assertEqual(len(out['parameters']),4)
    def test_transition_or_other_override_rejected(self):
        for which in ('base','block','override'):
            row,types=example();a=row['animators'][0]
            if which=='base':a['layers'][0]['inTransition']=True
            elif which=='block':a['layers'][3]['inTransition']=True
            else:a['layers'][1]['currentClips']=copy.deepcopy(a['layers'][0]['currentClips'])
            with self.subTest(which=which),self.assertRaises(ValueError):tool.frame_plan(row,types)
    def test_first_person_rejected(self):
        row,types=example();row['animators'][0]['parameters']['FirstPerson']=1
        with self.assertRaisesRegex(ValueError,'Third-person'):tool.frame_plan(row,types)
    def test_parameter_missing_type_bad_bool_trigger_and_nonfinite_rejected(self):
        for name,value in [('extra',0),('SheildBlock',1),('attack',True),('Owner',1.5),('MoveStyle',float('nan'))]:
            row,types=example();row['animators'][0]['parameters'][name]=value
            with self.subTest(name=name),self.assertRaises(ValueError):tool.frame_plan(row,types)


class ClipAllowlistTests(unittest.TestCase):
    def source(self):
        class Clip:
            type=SimpleNamespace(name='AnimationClip')
            def __init__(self,i):
                self.data={'m_Name':'DuplicateName' if i in (1,2) else 'Clip'+str(i),
                           'm_Legacy':False,'m_SampleRate':30.,
                           'm_MuscleClip':{'m_StartTime':0.,'m_StopTime':1. if i!=2 else 2.}}
            def read_typetree(self):return self.data
            def get_raw_data(self):return repr(self.data).encode()
        objects={i:Clip(i) for i in range(1,453)}
        controller={'m_AnimationClips':[{'m_PathID':i,'m_FileID':0} for i in objects]}
        return controller,objects
    def test_distinct_identity_duplicate_names_and_raw_hashes_preserved(self):
        c,o=self.source();values=tool.clip_allowlist(c,o)
        self.assertEqual(len(values),452);self.assertEqual(values[0]['name'],values[1]['name'])
        self.assertNotEqual(values[0]['source_id'],values[1]['source_id'])
        self.assertNotEqual(values[0]['raw_sha256'],values[1]['raw_sha256'])
    def test_missing_identity_legacy_and_invalid_timing_fail(self):
        for mutation in ('missing','legacy','time','rate'):
            c,o=self.source()
            if mutation=='missing':c['m_AnimationClips'].pop()
            elif mutation=='legacy':o[1].data['m_Legacy']=True
            elif mutation=='time':o[1].data['m_MuscleClip']['m_StopTime']=float('nan')
            else:o[1].data['m_SampleRate']=60.
            with self.subTest(mutation=mutation),self.assertRaises(ValueError):tool.clip_allowlist(c,o)


class ComparisonTests(unittest.TestCase):
    def fixture(self):
        row,types=example();expected=tool.frame_plan(row,types)
        identity=[1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.]
        expected['human_paths']={'Hips':'hips'};expected['root_inverse_world']=identity
        expected['bones']=[{'path':'hips','world':identity}]
        actual={'sourceSample':expected['sample'],'animatorSpeed':0.,
                'parameters':{p['name']:p['value'] for p in expected['parameters']},
                'bones':[{'path':'','model':identity},{'path':'hips','model':identity}],
                'layers':[{'name':l['name'],'state':l['current']['fullPathHash'],
                           'normalizedTime':l['current']['normalizedTime'],'weight':l['weight'],
                           'inTransition':l['inTransition'],'clips':l['currentClips'],
                           'length':None,'lengthAbsentReason':'frozen-effective-duration','stateSpeed':1.,'stateSpeedMultiplier':1.}
                          for l in expected['layers']]}
        return actual,expected
    def test_matching_readback_not_production_qualification(self):
        a,e=self.fixture();r=analyze.compare_frame(a,e)
        self.assertTrue(r['state_clock_weight_readback_matches'])
        self.assertFalse(r['pose_parity_qualified']);self.assertEqual(r['max_human_position_error_metres'],0.)
    def test_requested_clock_does_not_replace_actual_clock(self):
        a,e=self.fixture();a['layers'][3]['normalizedTime']+=.05
        self.assertFalse(analyze.compare_frame(a,e)['state_clock_weight_readback_matches'])
    def test_real_residual_weights_required(self):
        a,e=self.fixture();a['layers'][0]['clips']=copy.deepcopy(a['layers'][0]['clips'])
        a['layers'][0]['clips'][1]['weight']=0.
        self.assertFalse(analyze.compare_frame(a,e)['state_clock_weight_readback_matches'])
    def test_zero_block_layer_and_unexpected_override_rejected(self):
        a,e=self.fixture();a['layers'][3]['weight']=0.
        self.assertFalse(analyze.compare_frame(a,e)['state_clock_weight_readback_matches'])
        a,e=self.fixture();a['layers'][1]['clips']=[{'name':'Attack6','weight':1.,'length':1.}];a['layers'][1]['weight']=1.
        r=analyze.compare_frame(a,e);self.assertFalse(r['state_clock_weight_readback_matches'])
        self.assertEqual(r['unexpected_active_overrides'][0]['name'],'FullBody')
    def test_frozen_effective_length_cannot_relax_other_finite_fields(self):
        for mutation in ('speed','reason','state-speed','clip-length'):
            a,e=self.fixture()
            if mutation=='speed':a['animatorSpeed']=1.
            elif mutation=='reason':a['layers'][0]['lengthAbsentReason']=None
            elif mutation=='state-speed':a['layers'][0]['stateSpeed']=float('nan')
            else:
                a['layers'][0]['clips']=copy.deepcopy(a['layers'][0]['clips'])
                a['layers'][0]['clips'][0]['length']=float('inf')
            with self.subTest(mutation=mutation),self.assertRaises(ValueError):analyze.compare_frame(a,e)
    def test_bone_position_error_reported_and_missing_rejected(self):
        a,e=self.fixture();a['bones'][1]['model']=list(a['bones'][1]['model']);a['bones'][1]['model'][3]=.02
        self.assertAlmostEqual(analyze.compare_frame(a,e)['max_human_position_error_metres'],.02)
        a['bones'][1]['path']='wrong'
        with self.assertRaisesRegex(ValueError,'Missing human'):analyze.compare_frame(a,e)


if __name__=='__main__':unittest.main()
