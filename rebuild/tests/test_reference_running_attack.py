"""Synthetic observer contract tests, not evidence about source motion."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import reference_running_attack as launcher
import analyze_reference_running_attack as analyzer


def fixture():
    state=dict(fullPathHash=1,shortNameHash=2,normalizedTime=.2,length=1.833339691,speed=1,speedMultiplier=1,loop=False)
    layer=dict(name='FullBody',weight=1,transition=False,current=state,next=copy.deepcopy(state),
               clips=[dict(name='RunSwordAttack',id=4,weight=1,length=1.833339691)],nextClips=[])
    action=dict(moveThreshold=4.5,sprintThreshold=7,weaponPrototype=33,firstPerson=False,useStamina=True,
                m_AttackInAir=False,m_AttackInWater=False,m_AttackInMove=True,m_AttackInSprint=False,m_EndAction=False,
                combo=0,direction=[0,0,1],targetDirection=[0,0,1],
                skill=dict(instance=1,id=20110014,guide=1,active=True,execExtEnable=False,startTime=10,coolStat=3))
    a=dict(id=-50,controller='FemaleAnimator2',avatar='FemaleAvatar',speed=1,deltaPosition=[0,0,.1],deltaRotation=[0,0,0,1],
           rootPosition=[0,0,1],rootRotation=[0,0,0,1],applyRootMotion=True,layers=[layer])
    frames=[]
    for i in range(3):
        f=dict(type='frame',sample=i,frame=100+i,time=10+i*.1,realtime=20+i*.1,dt=.1,fixedDt=.02,
               playerId=20000,position=[0,0,i*.1],rotation=[0,0,0,1],modelPosition=[0,0,i*.1],modelRotation=[0,0,0,1],
               velocity=[0,0,5],grounded=True,motorVelocity=[0,0,5],lastMove=[0,0,.1],lastRotation=[0,0,0,1],stamina=100,
               action=copy.deepcopy(action),animator=copy.deepcopy(a))
        f['animator']['layers'][0]['current']['normalizedTime']+=i*.1
        frames.append(f)
    return [dict(type='header',schema=1,kind='observed-original-running-attack',phase='WaitForEndOfFrame',unity='5.2.4f1',
                 probeSourceSha256='a'*64,maxFrames=7200,maxSeconds=120,maxBytes=analyzer.MAX_BYTES)]+frames+[dict(type='end',reason='stop-file',frames=3,observedLoops=5)]


class RunningObserverTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.path=self.root/'capture.jsonl'
    def tearDown(self):self.temp.cleanup()
    def write(self,rows):self.path.write_text(''.join(json.dumps(r)+'\n' for r in rows))
    def analyze(self,rows):self.write(rows);return analyzer.analyze(self.path)
    def manifest(self):
        p=dict(schema='pe-running-attack-launch-1',capture=str(self.path),probe_source_sha256='a'*64)
        for k in ('assembly_sha256','shim_sha256','engine_sha256','game_assembly_sha256','asset_sha256'):p[k]='b'*64
        self.path.with_suffix('.launch.json').write_text(json.dumps(p))
    def test_observed_branch_and_clocks_no_exact_motor_claim(self):
        r=self.analyze(fixture());self.assertEqual(r['running_knife_observed_samples'],3)
        self.assertEqual(r['thresholds'],[dict(move=4.5,sprint=7)])
        self.assertEqual(len(r['skills']),1);self.assertEqual(r['skills'][0]['samples'],3)
        self.assertEqual(r['skills'][0]['clip_clock_ranges']['FullBody/clips/RunSwordAttack'],[.2,.4])
        for key in ('exact_selection_velocity_observed','exact_motor_order_observed','root_curve_qualified'):self.assertFalse(r[key])
    def test_stationary_sprint_air_wrong_weapon_not_running(self):
        for key,value in [('weaponPrototype',10),('firstPerson',True),('m_AttackInAir',True),('m_AttackInWater',True),('m_AttackInSprint',True),('m_AttackInMove',False)]:
            rows=fixture()
            for f in rows[1:-1]:f['action'][key]=value
            with self.subTest(key=key):self.assertEqual(self.analyze(rows)['running_knife_observed_samples'],0)
        for id in (20110011,20110013):
            rows=fixture()
            for f in rows[1:-1]:f['action']['skill']['id']=id
            self.assertEqual(self.analyze(rows)['running_knife_observed_samples'],0)
    def test_idle_and_unobserved_next_clip_not_running(self):
        rows=fixture()
        for f in rows[1:-1]:f['action']['skill']=None
        self.assertEqual(self.analyze(rows)['skills'],[])
        rows=fixture()
        for f in rows[1:-1]:f['animator']['layers'][0]['clips'][0]['weight']=0
        self.assertEqual(self.analyze(rows)['running_knife_observed_samples'],0)
    def test_source_negative_ending_guides_supported(self):
        rows=fixture();rows[2]['action']['skill']['guide']=-1;rows[3]['action']['skill'].update(guide=-2,active=False)
        r=self.analyze(rows);self.assertEqual(r['skills'][0]['guides'],[1,-1,-2]);self.assertEqual(r['running_knife_observed_samples'],2)
    def test_missing_error_empty_closure_rejected(self):
        rows=fixture()
        for bad in (rows[:-1],rows[:1]+rows[-1:],rows+[rows[-1]]):
            with self.assertRaises(ValueError):self.analyze(bad)
        rows[-1]['reason']='error'
        with self.assertRaises(ValueError):self.analyze(rows)
    def test_bad_clock_identity_finite_and_bounds(self):
        for key,val in [('sample',0),('frame',100),('realtime',0),('playerId',20001),('dt',float('nan')),('velocity',[1,2]),('position',[1,None,3])]:
            rows=fixture();rows[2][key]=val
            with self.subTest(key=key),self.assertRaises(ValueError):self.analyze(rows)
        rows=fixture();rows[2]['action']['skill']['id']=1
        with self.assertRaises(ValueError):self.analyze(rows)
    def test_launch_provenance_required_for_claim_and_match(self):
        self.write(fixture());self.assertEqual(analyzer.main([str(self.path),'--require-running']),1)
        self.manifest();self.assertEqual(analyzer.main([str(self.path),'--require-running']),0)
        rows=fixture();rows[0]['probeSourceSha256']='c'*64
        with self.assertRaises(ValueError):self.analyze(rows)
    def test_fresh_outputs_report_and_symlink(self):
        self.write(fixture());out=self.root/'report.json'
        self.assertEqual(analyzer.main([str(self.path),'--report',str(out)]),0)
        self.assertEqual(analyzer.main([str(self.path),'--report',str(out)]),1)
        with self.assertRaises(ValueError):launcher.paths(self.root,self.path)
        target=self.root/'fresh.jsonl';target.with_suffix('.exit.json').symlink_to(self.root/'missing')
        with self.assertRaises(ValueError):launcher.paths(self.root,target)
    def test_duplicate_keys_rejected(self):
        self.write(fixture());self.path.write_text(self.path.read_text().replace('"sample": 0','"sample": 0, "sample": 0'))
        with self.assertRaises(ValueError):analyzer.analyze(self.path)
    def test_default_compile_does_not_launch_or_change_profile(self):
        with mock.patch.object(launcher,'build',return_value=(self.root/'mono',self.root/'shim',self.root/'dll')) as build, \
             mock.patch.object(launcher.launch_reference,'launch_command',return_value=(['game'],{})), \
             mock.patch.object(launcher.launch_reference,'prepare_settings') as settings, \
             mock.patch.object(launcher.launch_reference,'prepare_unity_preferences') as prefs, \
             mock.patch.object(launcher.subprocess,'run') as run:
            self.assertEqual(launcher.main(['--user-data',str(self.root),'--output',str(self.path)]),0)
            build.assert_called_once();settings.assert_not_called();prefs.assert_not_called();run.assert_not_called()
            self.assertFalse(self.path.exists())
    def test_preload_launch_refused_before_build(self):
        with mock.patch.object(launcher,'build') as build, mock.patch.object(launcher.launch_reference,'launch_command',return_value=(['game'],{'LD_PRELOAD':'existing'})):
            self.assertEqual(launcher.main(['--launch','--user-data',str(self.root)]),1);build.assert_not_called()
    def test_single_threaded_flag_and_negative_exit(self):
        for name in ('mono','shim','dll'):(self.root/name).write_bytes(b'fixture')
        with mock.patch.object(launcher,'build',return_value=(self.root/'mono',self.root/'shim',self.root/'dll')), \
             mock.patch.object(launcher,'digest',return_value='a'*64), \
             mock.patch.object(launcher.launch_reference,'launch_command',return_value=(['game'],{})) as command, \
             mock.patch.object(launcher.launch_reference,'prepare_settings'), \
             mock.patch.object(launcher.launch_reference,'prepare_unity_preferences'), \
             mock.patch.object(launcher.subprocess,'run',return_value=mock.Mock(returncode=-9)) as run:
            self.assertEqual(launcher.main(['--launch','--single-threaded-rendering','--user-data',str(self.root),'--output',str(self.path)]),137)
            self.assertTrue(command.call_args.args[2]);self.assertEqual(run.call_args.kwargs['env']['PE_POSE_SECONDS'],'120')
            self.assertEqual(json.loads(self.path.with_suffix('.exit.json').read_text())['returncode'],-9)


if __name__=='__main__':unittest.main()
