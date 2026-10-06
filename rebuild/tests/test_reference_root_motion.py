"""Offline schema/lifecycle/plan tests. Synthetic curves are not source evidence."""
import copy
import hashlib
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import reference_root_motion_bake as tool
import analyze_reference_root_motion as analyzer


def fixture():
    rows=[dict(type='header',schema='pe-root-motion-bake-1',rates=[30,60],clip='RunSwordAttack',sourcePathId=408,
               skillId=20110014,applyRootMotion=True,fireEvents=False,controllerAttached=False),
          dict(type='source',avatar='FemaleAvatar',controller='FemaleAnimator2',length=tool.LENGTH,nodes=42,animatorId=70)]
    count=0
    for trial,fps in enumerate((30,60)):
        n=math.ceil(tool.LENGTH*fps)+1;previous=0
        for i in range(n):
            t=min(i/fps,tool.LENGTH);dt=t-previous;previous=t
            q=[0,math.sin(t/2),0,math.cos(t/2)];dq=[0,math.sin(dt/2),0,math.cos(dt/2)]
            rows.append(dict(type='sample',trial=trial,fps=fps,index=i,cloneId=80+trial,frame=100,time=t,playableTime=t,dt=dt,
                             position=[0,0,t],rotation=q,deltaPosition=[0,0,dt],deltaRotation=dq,
                             cumulativeDeltaPosition=[0,0,t],cumulativeDeltaRotation=q,animatorRootPosition=[0,0,t],animatorRootRotation=q))
        rows.append(dict(type='trialEnd',trial=trial,samples=n));count+=n
    rows.append(dict(type='end',reason='complete',samples=count));return rows


def frame_fixture(speeds=(1,.5)):
    rows=fixture()[:2];rows[0].pop('rates');rows[0].update(schema='pe-root-motion-bake-2',playbackSpeeds=list(speeds))
    if speeds==(.025,.0125):rows[0].update(samplingProfile='ultra-dense',trialWallLimitSeconds=180)
    total=0
    for trial,speed in enumerate(speeds):
        t=0;i=0
        while True:
            dt=0 if i==0 else (.071+(i%3)*.01 if speeds==(.025,.0125) else .031+(i%3)*.005)
            delta=dt*speed;t+=delta;q=[0,math.sin((.4+t)/2),0,math.cos((.4+t)/2)]
            rows.append(dict(type='sample',trial=trial,playbackSpeed=speed,index=i,cloneId=80+trial,frame=100+i,
                time=t,playableTime=t,dt=dt,directorTime=t/speed,directorMode='GameTime',position=[3,2,1+t],rotation=q,
                deltaPosition=[0,0,delta],deltaRotation=[0,math.sin(delta/2),0,math.cos(delta/2)],
                cumulativeDeltaPosition=[0,0,t],cumulativeDeltaRotation=[0,math.sin(t/2),0,math.cos(t/2)],animatorRootPosition=[3,2,1+t],animatorRootRotation=q))
            i+=1
            if t>=tool.LENGTH:break
        rows.append(dict(type='trialEnd',trial=trial,samples=i,terminalTime=t));total+=i
    rows.append(dict(type='end',reason='complete',samples=total));return rows


class RootMotionTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.capture=self.root/'capture.jsonl'
        self.game=self.root/'game';self.asset=self.game/'PE_Client_Data/sharedassets1.assets';self.asset.parent.mkdir(parents=True);self.asset.write_bytes(b'synthetic asset')
        self.plan=self.root/'plan.json'
        self.data=dict(schema='pe-root-motion-plan-1',asset=str(self.asset),asset_sha256=tool.digest(self.asset),avatar='FemaleAvatar',controller='FemaleAnimator2',controller_path_id=779,skill_id=20110014,source_state=dict(speed=1,leaf_duration=1),clip=dict(name='RunSwordAttack',path_id=408,length=tool.LENGTH,frame_rate=30,loop=False,humanoid_bindings=127))
        self.plan.write_text(json.dumps(self.data))
    def tearDown(self):self.tmp.cleanup()
    def analyze(self,rows,required=False):
        self.capture.write_text(''.join(json.dumps(r)+'\n' for r in rows));return analyzer.analyze(self.capture,required)
    def test_comparison_includes_off_grid_measured_peak(self):
        rows=frame_fixture();previous=0
        for r in rows:
            if r['type']=='sample' and r['trial']==1:
                offset=.01 if r['index']==1 else 0
                r['position'][0]+=offset;r['cumulativeDeltaPosition'][0]=offset
                r['deltaPosition'][0]=offset-previous;previous=offset
        result=self.analyze(rows)
        self.assertTrue(result['comparison_includes_all_measured_knots'])
        self.assertAlmostEqual(result['maximum_cross_speed_position_error'],.01,places=10)
        self.assertFalse(result['clone_motion_qualified'])

    def test_dense_frame_profile_preserves_clock_and_reports_sampling_gap(self):
        for speeds in ((.1,.05),(.05,.025),(.025,.0125)):
            with self.subTest(speeds=speeds):
                result=self.analyze(frame_fixture(speeds),True)
                self.assertTrue(result['all_authored_gaps_within_60hz'])
                self.assertEqual([t['playback_speed'] for t in result['trials']],list(speeds))
                self.assertGreater(result['samples'],1000)
                self.assertLessEqual(result['samples'],3600)
                if speeds==(.025,.0125):
                    self.assertEqual(result['sampling_profile'],'ultra-dense')
                    self.assertEqual(result['trial_wall_limit_seconds'],180)

    def test_ultra_dense_requires_matching_explicit_profile_and_bounds(self):
        for key,value in [('samplingProfile','denser'),('samplingProfile',None),('trialWallLimitSeconds',120),('trialWallLimitSeconds',True),('playbackSpeeds',[.025,.00625])]:
            rows=frame_fixture((.025,.0125));rows[0][key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):self.analyze(rows)

    def test_frame_driven_variable_intervals_baseline_and_explicit_overshoot(self):
        result=self.analyze(frame_fixture(),True)
        self.assertEqual(result['schema'],2);self.assertTrue(result['clone_motion_qualified'])
        self.assertTrue(all(t['terminal_overshoot']>0 for t in result['trials']))
        self.assertLess(result['maximum_cross_speed_position_error'],1e-12)
        self.assertFalse(result['gameplay_motion_validated'])
        for endpoint in result['interpolated_source_end_displacements']:
            self.assertAlmostEqual(endpoint[2],tool.LENGTH)
    def test_frame_driven_clock_stall_frame_repeat_and_manual_mode_rejected(self):
        for key,value in [('playableTime',0),('frame',100),('dt',0),('playbackSpeed',.5),('directorMode','Manual')]:
            rows=frame_fixture();rows[3][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.analyze(rows)
    def test_frame_driven_requires_measured_terminal_crossing(self):
        rows=frame_fixture();first_end=next(r for r in rows if r['type']=='trialEnd');first_end['terminalTime']=tool.LENGTH
        with self.assertRaises(ValueError):self.analyze(rows)
    def test_frame_driven_cannot_promote_unapplied_root_deltas(self):
        rows=frame_fixture()
        for r in rows:
            if r['type']=='sample':r['position']=[3,2,1]
        self.assertFalse(self.analyze(rows)['clone_motion_qualified'])
        with self.assertRaises(ValueError):self.analyze(rows,True)

    def test_two_sequential_translating_rotating_trials(self):
        result=self.analyze(fixture(),True)
        self.assertEqual(result['samples'],169);self.assertTrue(result['clone_motion_qualified'])
        self.assertFalse(result['gameplay_motion_validated']);self.assertFalse(result['process_teardown_validated'])
        self.assertLess(result['cross_rate_endpoint_position_error'],1e-10)
    def test_partial_closed_failure_and_extra_records_rejected(self):
        for rows in (fixture()[:-1],fixture()+[dict(type='end',reason='complete',samples=169)],fixture()[:20]+[dict(type='end',reason='stop-file',samples=18)]):
            with self.subTest(count=len(rows)),self.assertRaises(ValueError):self.analyze(rows)
    def test_clock_drift_seeks_zero_dt_and_automatic_frame_advance_rejected(self):
        for key,value in [('dt',0),('time',.6),('playableTime',.6),('frame',101),('cloneId',999),('index',10)]:
            rows=fixture();rows[4][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.analyze(rows)
    def test_wrong_isolation_source_and_shared_clones_rejected(self):
        for key,value in [('applyRootMotion',False),('controllerAttached',True),('fireEvents',True),('sourcePathId',315)]:
            rows=fixture();rows[0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.analyze(rows)
        rows=fixture()
        for row in rows:
            if row['type']=='sample':row['cloneId']=80
        with self.assertRaises(ValueError):self.analyze(rows)
    def test_nonfinite_malformed_cumulative_and_rotations_rejected(self):
        for key,value in [('deltaPosition',[float('nan'),0,0]),('rotation',[0,0,0,0]),('position',[0,0]),('cumulativeDeltaPosition',[9,0,0])]:
            rows=fixture();rows[4][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.analyze(rows)
    def test_zero_or_unapplied_motion_not_qualified(self):
        rows=fixture()
        for r in rows:
            if r['type']=='sample':r['position']=[0,0,0]
        self.assertFalse(self.analyze(rows)['clone_motion_qualified'])
        with self.assertRaises(ValueError):self.analyze(rows,True)
    def test_endpoint_rate_disagreement_not_qualified(self):
        rows=fixture()
        for r in rows:
            if r['type']=='sample' and r['trial']==1:
                for key in ('position','deltaPosition','cumulativeDeltaPosition'):r[key]=[2*x for x in r[key]]
        self.assertFalse(self.analyze(rows)['clone_motion_qualified'])
        with self.assertRaises(ValueError):self.analyze(rows,True)
    def test_plan_source_hash_exact_id_timing_and_bindings(self):
        self.assertEqual(tool.read_plan(self.plan,self.game)['clip']['path_id'],408)
        for key,value in [('path_id',315),('length',float('nan')),('frame_rate',True),('humanoid_bindings',0),('loop',True)]:
            data=copy.deepcopy(self.data);data['clip'][key]=value;self.plan.write_text(json.dumps(data))
            with self.subTest(key=key),self.assertRaises(ValueError):tool.read_plan(self.plan,self.game)
        self.plan.write_text(json.dumps(self.data));self.asset.write_bytes(b'changed')
        with self.assertRaises(ValueError):tool.read_plan(self.plan,self.game)
    def test_duplicate_keys_rejected(self):
        self.plan.write_text('{"schema":1,"schema":2}')
        with self.assertRaises(ValueError):tool.read_plan(self.plan,self.game)
        self.capture.write_text('{"type":"header","type":"end"}\n')
        with self.assertRaises(ValueError):analyzer.analyze(self.capture)
    def test_default_build_only_has_no_launch_or_settings_side_effect(self):
        with mock.patch.object(tool,'build',return_value=('mono',self.root/'shim',self.root/'probe.dll')) as build, \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['fake-client'],{})), \
             mock.patch.object(tool.launch_reference,'prepare_settings') as settings, \
             mock.patch.object(tool.launch_reference,'prepare_unity_preferences') as prefs, \
             mock.patch.object(tool.subprocess,'run') as run:
            code=tool.main(['--game-dir',str(self.game),'--plan',str(self.plan),'--user-data',str(self.root/'profile')])
            self.assertEqual(code,0);build.assert_called_once();settings.assert_not_called();prefs.assert_not_called();run.assert_not_called()
    def test_preload_conflict_rejected_before_build_or_launch(self):
        with mock.patch.object(tool,'build') as build,mock.patch.object(tool.launch_reference,'launch_command',return_value=(['fake-client'],{'LD_PRELOAD':'other'})):
            self.assertEqual(tool.main(['--launch','--game-dir',str(self.game),'--plan',str(self.plan),'--user-data',str(self.root/'profile')]),1);build.assert_not_called()
    def test_launch_preserves_negative_exit_and_uses_direct_render_flag(self):
        managed=self.game/'PE_Client_Data/Managed';managed.mkdir();(managed/'UnityEngine.dll').write_bytes(b'fake')
        dll=self.root/'probe.dll';dll.write_bytes(b'fake');profile=self.root/'profile'
        with mock.patch.object(tool,'build',return_value=('mono',self.root/'shim',dll)), \
             mock.patch.object(tool.launch_reference,'launch_command',return_value=(['fake-client'],{})) as command, \
             mock.patch.object(tool.launch_reference,'prepare_settings'),mock.patch.object(tool.launch_reference,'prepare_unity_preferences'), \
             mock.patch.object(tool.subprocess,'run',return_value=mock.Mock(returncode=-9)) as run:
            output=profile/'pose-captures/fresh.jsonl'
            code=tool.main(['--launch','--ultra-dense','--single-threaded-rendering','--game-dir',str(self.game),'--plan',str(self.plan),'--user-data',str(profile),'--output',str(output)])
            self.assertEqual(code,137);self.assertEqual(command.call_args.args[2],True)
            self.assertEqual(json.loads(output.with_suffix('.exit.json').read_text())['signal'],9)
            env=run.call_args.kwargs['env'];self.assertEqual(env['PE_ROOT_RATE'],'30');self.assertIn('PE_ROOT_LENGTH',env);self.assertEqual(env['PE_ROOT_DENSE'],'3')
            self.assertEqual(json.loads(output.with_suffix('.launch.json').read_text())['sampling_profile'],'ultra-dense')
            self.assertFalse(output.exists())

if __name__=='__main__':unittest.main()
