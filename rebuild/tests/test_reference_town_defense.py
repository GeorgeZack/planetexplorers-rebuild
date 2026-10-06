"""Offline observer contract and malformed-capture checks; never launches graphics."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import reference_town_defense as launcher
from analyze_reference_town_defense import analyze, main


def entity(identity,prototype=13,kind=3):
    return dict(id=identity,prototype=prototype,prototypeType=kind,position=[-300,27,140],rotation=[0,0,0,1],scale=[1,1,1],
                hp=212,maxHp=212,attack=64,defense=29,campId=26,damageId=45,defaultPlayerId=8)


def agent(index,prototype):
    return dict(sourceOrder=index,agentId=index+10,prototype=500000+index,step='Created',canRide=False,
                agentPosition=[-310+index,28,145],agentScale=[1,1,1],agentRotation=[0,0,0,1],entity=entity(100+index,prototype))


def capture():
    header=dict(type='header',schema=1,kind='observed-original-town-defense',mission=9077,target=7507,
                phase='WaitForEndOfFrame',probeSourceSha256='a'*64)
    first=[agent(i,13 if i<8 else 4) for i in range(10)]
    second=first+[agent(i,7 if i<18 else 4) for i in range(10,20)]
    frames=[]
    for i,agents in enumerate([[],first,second]):
        frames.append(dict(type='frame',sample=i,frame=100+i,time=30*i,realtime=30*i,
                           missionActive=True,missionPointPresent=True,counter='0_0_0_0_0',player=entity(20000,0,0),
                           beacon=dict(id=500,waveIndex=i,prepTime=30 if i==0 else 0,position=[-284,27,134],forward=[0,0,1],
                                       ui=dict(mission=9077,count=0,goal=15,remainingWaves=2-i,totalWaves=2)),
                           protectedNpcs=[dict(id=20018,entity=entity(20018,3,1))],agents=copy.deepcopy(agents)))
    return [header,*frames,dict(type='end',reason='stop-file',frames=3)]


class DefenseProbeTests(unittest.TestCase):
    def read(self,rows):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'capture.jsonl';p.write_text(''.join(json.dumps(r)+'\n' for r in rows));return analyze(p)

    def test_both_waves_preserve_source_anchors_and_resolved_species(self):
        rows=capture();rows[2]['agents'][0]['entity']['position'][0]=-333
        r=self.read(rows);self.assertEqual(r['complete_two_wave_epochs'],[0]);e=r['beacon_epochs'][0]
        self.assertEqual(e['resolved_species_by_wave'],[{'13':8,'4':2},{'7':8,'4':2}])
        a=e['members'][0]['agent'];self.assertEqual(a['agentPosition'][0],-310);self.assertEqual(a['entity']['position'][0],-333)
        self.assertNotEqual(a['prototype'],a['entity']['prototype'])
        self.assertEqual(e['members'][10]['source_wave'],1)

    def test_late_capture_does_not_infer_wave_boundary(self):
        rows=capture();rows[1:3]=[];rows[1].update(sample=0);rows[-1]['frames']=1
        r=self.read(rows);self.assertEqual(r['complete_two_wave_epochs'],[])
        self.assertTrue(all(m['source_wave'] is None for m in r['beacon_epochs'][0]['members']))

    def test_unresolved_then_resolved_retains_first_actual_entity(self):
        rows=capture();rows[2]['agents'][0]['entity']=None;rows[2]['agents'][0]['step']='InCreating'
        r=self.read(rows);self.assertEqual(r['beacon_epochs'][0]['members'][0]['sample'],2)
        self.assertEqual(r['complete_two_wave_epochs'],[0])

    def test_missing_species_never_substituted(self):
        rows=capture();rows[3]['agents'][10]['entity']['prototype']=13
        self.assertFalse(self.read(rows)['complete_two_wave_epochs'])

    def test_dead_first_pose_is_recorded_without_full_health_claim(self):
        rows=capture();rows[2]['agents'][0]['entity']['hp']=0
        r=self.read(rows);self.assertEqual(r['beacon_epochs'][0]['members'][0]['agent']['entity']['hp'],0)

    def test_reload_does_not_merge_beacons(self):
        rows=capture();rows[3]['beacon']['id']=501
        r=self.read(rows);self.assertEqual(len(r['beacon_epochs']),2);self.assertFalse(r['complete_two_wave_epochs'])

    def test_absent_beacon_and_missing_npc_are_valid_negative_evidence(self):
        rows=capture()
        for f in rows[1:-1]:f.update(beacon=None,agents=[],protectedNpcs=[dict(id=20018,entity=None)])
        self.assertEqual(self.read(rows)['beacon_epochs'],[])

    def test_duplicate_id_or_changed_membership_rejected(self):
        for kind in ('agent','entity','changed'):
            rows=capture()
            if kind=='agent':rows[2]['agents'][1]['agentId']=rows[2]['agents'][0]['agentId']
            if kind=='entity':rows[2]['agents'][1]['entity']['id']=rows[2]['agents'][0]['entity']['id']
            if kind=='changed':rows[3]['agents'][0]['agentId']=999
            with self.assertRaises(ValueError):self.read(rows)

    def test_malformed_stats_clocks_and_protected_identity_rejected(self):
        for kind in ('nan','time','npc','phase'):
            rows=capture()
            if kind=='nan':rows[2]['agents'][0]['entity']['hp']=float('nan')
            if kind=='time':rows[3]['realtime']=-1
            if kind=='npc':rows[2]['protectedNpcs'][0]['entity']['id']=999
            if kind=='phase':rows[2]['beacon']['waveIndex']=3
            with self.assertRaises(ValueError):self.read(rows)

    def test_incomplete_error_and_miscounted_captures_fail(self):
        for kind in ('open','error','count','schema'):
            rows=capture()
            if kind=='open':rows.pop()
            if kind=='error':rows[-1]['reason']='error'
            if kind=='count':rows[-1]['frames']=50
            if kind=='schema':rows[0]['mission']=9126
            with self.assertRaises(ValueError):self.read(rows)

    def test_line_size_bound(self):
        rows=capture();rows[1]['padding']='a'*65536
        with self.assertRaises(ValueError):self.read(rows)

    def test_strict_cli_rejects_negative_without_writing_report(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'capture.jsonl';out=Path(d)/'report.json';rows=capture();rows[1:3]=[];rows[1]['sample']=0;rows[-1]['frames']=1
            p.write_text(''.join(json.dumps(r)+'\n' for r in rows))
            with self.assertRaises(SystemExit):main([str(p),'--require-waves','--output',str(out)])
            self.assertFalse(out.exists())

    def test_fresh_sidecars_and_profile_containment(self):
        with tempfile.TemporaryDirectory() as d:
            profile=Path(d);out=profile/'defense.jsonl'
            paths=launcher.paths(profile,out);self.assertEqual(paths[1],profile/'defense.arm')
            paths[-1].write_text('{}')
            with self.assertRaises(ValueError):launcher.paths(profile,out)
            with self.assertRaises(ValueError):launcher.paths(profile,profile.parent/'outside.jsonl')

    def test_default_build_never_prepares_profile_or_launches_game(self):
        with tempfile.TemporaryDirectory() as d, patch.object(launcher,'build',return_value=(Path('/m'),Path('/s'),Path('/a'))), \
             patch.object(launcher.launch_reference,'launch_command',return_value=(['/original'],{})), \
             patch.object(launcher.launch_reference,'prepare_settings') as settings,patch.object(launcher.subprocess,'run') as run:
            self.assertEqual(launcher.main(['--user-data',d]),0);settings.assert_not_called();run.assert_not_called()

    def test_probe_has_no_gameplay_or_animator_mutation(self):
        s=launcher.SOURCE.read_text()
        for forbidden in ('SetAttribute(', 'ModifyQuestVariable(', 'StartSkill(', 'Random.', 'Animator', 'SetPosition(', 'AddSceneObj(', 'CreateAgent('):
            self.assertNotIn(forbidden,s)
        self.assertIn('entityManagerField.GetValue(null)',s)
        self.assertIn('FileMode.CreateNew',s)


if __name__=='__main__':unittest.main()
