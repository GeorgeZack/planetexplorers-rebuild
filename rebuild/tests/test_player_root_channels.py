"""Offline decoder/comparison contracts; synthetic samples are not Unity evidence."""
import copy
import math
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import verify_player_root_channels as tool


def fixture():
    bindings=[dict(classID=95,customType=8,isPPtrCurve=False,attribute=i,path=0) for i in range(7,134)]
    samples=[]
    for f in range(31):
        row=[0.0]*127
        row[0]=10+f/30;row[1]=1+f/10;row[2]=-20+f/15
        # Body orientation changes wildly; output must not rotate the root again.
        row[3]=math.sin(f);row[6]=math.cos(f)
        samples.extend(row)
    clip=dict(m_Name='Attack6',m_Legacy=False,m_Compressed=False,m_PositionCurves=[],
              m_RotationCurves=[],m_CompressedRotationCurves=[],m_ScaleCurves=[],m_PPtrCurves=[],
              m_ClipBindingConstant=dict(genericBindings=bindings),
              m_MuscleClip=dict(**tool.FLAGS,m_StartTime=0,m_StopTime=1,m_CycleOffset=0,
                               m_OrientationOffsetY=0,m_Level=0,m_Clip=dict(data=dict(
                m_StreamedClip=dict(curveCount=0),m_DenseClip=dict(m_FrameCount=31,m_CurveCount=127,
                m_SampleRate=30,m_BeginTime=0,m_SampleArray=samples),m_ConstantClip=dict(data=[])))))
    avatar=dict(m_TOS=[],m_Avatar=dict(m_Human=dict(data=dict(m_Scale=.5))))
    return clip,avatar


class RootTests(unittest.TestCase):
    def test_source_origin_scale_projection_and_changing_body_quaternion(self):
        c=tool.decode(*fixture())
        self.assertEqual(tool.candidate(c,0),[0,0,0])
        self.assertEqual(tool.candidate(c,1),[.5,0,1])
        for t in [.001,.017,.5,.731]:
            self.assertAlmostEqual(tool.candidate(c,t)[0],t*.5)
            self.assertAlmostEqual(tool.candidate(c,t)[2],t)
            self.assertEqual(tool.candidate(c,t)[1],0)
    def test_all_unqualified_flag_and_offset_branches_rejected(self):
        for name in tool.FLAGS:
            c,a=fixture();c['m_MuscleClip'][name]=not tool.FLAGS[name]
            with self.subTest(name=name),self.assertRaises(ValueError):tool.decode(c,a)
        for name in ['m_StartTime','m_CycleOffset','m_OrientationOffsetY','m_Level']:
            c,a=fixture();c['m_MuscleClip'][name]=.1
            with self.assertRaises(ValueError):tool.decode(c,a)
    def test_motion_binding_duplicate_missing_and_nonfinite_rejected(self):
        for attribute in [0,8,134]:
            c,a=fixture();c['m_ClipBindingConstant']['genericBindings'][0]['attribute']=attribute
            with self.assertRaises(ValueError):tool.decode(c,a)
        c,a=fixture();c['m_MuscleClip']['m_Clip']['data']['m_DenseClip']['m_SampleArray'][0]=float('nan')
        with self.assertRaises(ValueError):tool.decode(c,a)
    def test_auxiliary_direct_transform_is_not_a_root_binding(self):
        c,a=fixture()
        c['m_ClipBindingConstant']['genericBindings'].append(
            dict(classID=4,customType=0,isPPtrCurve=False,attribute=1,path=123))
        c['m_MuscleClip']['m_Clip']['data']['m_ConstantClip']['data']=[3,4,5]
        self.assertEqual(tool.candidate(tool.decode(c,a),1),[.5,0,1])
        c['m_ClipBindingConstant']['genericBindings'][-1]['path']=0
        with self.assertRaises(ValueError):tool.decode(c,a)
    def test_bad_scale_and_clock_rejected(self):
        for scale in [0,-1,float('nan'),float('inf'),11]:
            c,a=fixture();a['m_Avatar']['m_Human']['data']['m_Scale']=scale
            with self.assertRaises(ValueError):tool.decode(c,a)
        for key,val in [('m_FrameCount',1),('m_SampleRate',0),('m_BeginTime',.1)]:
            c,a=fixture();c['m_MuscleClip']['m_Clip']['data']['m_DenseClip'][key]=val
            with self.assertRaises(ValueError):tool.decode(c,a)
    def test_time_bounds(self):
        c=tool.decode(*fixture())
        for t in [-1,1.0001,float('nan'),float('inf'),True]:
            with self.assertRaises(ValueError):tool.candidate(c,t)
    def test_comparison_reports_errors_and_requires_origin_terminal_order(self):
        c=tool.decode(*fixture());samples=[dict(playableTime=t,position=[t*.5,0,t]) for t in [0,.4,.8,1.1]]
        report=tool.compare(c,samples)
        self.assertEqual(report['compared_samples'],3);self.assertAlmostEqual(report['maximum_error_m'],0)
        self.assertAlmostEqual(report['endpoint_interpolation_error_m'],0)
        self.assertEqual(report['endpoint_bracket_seconds'],[.8,1.1])
        changed=copy.deepcopy(samples);changed[1]['position'][0]+=.2
        self.assertAlmostEqual(tool.compare(c,changed)['maximum_error_m'],.2)
        for bad in [samples[1:],samples[:-1],[samples[0],samples[0],samples[-1]]]:
            with self.assertRaises(ValueError):tool.compare(c,bad)


if __name__=='__main__':unittest.main()
