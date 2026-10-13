#!/usr/bin/env python3
"""Offline checks of the FullBodyBipedIK observation probe and its analyzer."""
import json
from pathlib import Path
import random
import re
import shutil
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_reference_fbbik as a
import test_fbbik_export as t

RIG=json.loads(a.RIG.read_text())['rig']

def capture(rig,cases,shuffle):
    """A capture whose post pose is the double-precision replay, in another bone order."""
    count=len(rig['bones']);order=list(range(count));rest=order[1:];shuffle.shuffle(rest)
    # Parents must still precede children, as the probe sorts by depth.
    depth=lambda b:0 if rig['bones'][b]['parent']<0 else 1+depth(rig['bones'][b]['parent'])
    order=[0]+sorted(rest,key=depth);place={r:c for c,r in enumerate(order)}
    bones=[dict(name='FemaleModel(Clone)' if r==0 else rig['bones'][r]['name'],parent=-1 if r==0 else place[rig['bones'][r]['parent']],lossyScale=[1,1,1]) for r in order]
    init=[(tuple(b['local_position']),tuple(b['local_rotation'])) for b in rig['bones']];start=[]
    for bone,(p,q) in zip(rig['bones'],init):start.append(t.Transform(None if bone['parent']<0 else start[bone['parent']],p,q))
    chains=[]
    for c,(ids,settings) in enumerate(zip(rig['chain_bones'],rig['chains'])):
        item=dict(settings,nodes=[place[b] for b in ids],bendInitiated=c>0,bendWeight=0.,bendGoal=False,bendDirection=[1,0,0],bendDefaultLocal=[0,0,0],bendDefaultChild=[0,0,0],constraints=[])
        if c:
            x,y,z=(start[i] for i in ids);upper=t.sub(y.position,x.position);limb=t.sub(z.position,x.position);direction=t.ortho(upper,t.ortho(limb,upper)[1])[1]
            item.update(bendDirection=list(direction),bendDefaultLocal=list(t.rot(t.qinv(x.rotation),direction)),bendDefaultChild=list(t.rot(t.qinv(z.rotation),t.cross(t.unit(limb),direction))))
        else:item['constraints']=[dict(bone1=place[rig['chain_bones'][k['chain1']][0]],bone2=place[rig['chain_bones'][k['chain2']][0]],pushElasticity=.5 if i<2 else k['push_elasticity'],pullElasticity=k['pull_elasticity']) for i,k in enumerate(rig['constraints'])]
        chains.append(item)
    record=dict(type='rig',iterations=rig['iterations'],spineStiffness=rig['spine_stiffness'],pullBodyVertical=rig['pull_body_vertical'],pullBodyHorizontal=rig['pull_body_horizontal'],
        root=place[rig['root']],rootNode=place[rig['chain_bones'][0][0]],bones=bones,chains=chains,
        effectorBones=[place[rig['chain_bones'][n//3][n%3]] for n in rig['effector_nodes']],spine=[place[b] for b in rig['spine']],
        spineIterations=rig['spine_iterations'],spineTwistWeight=rig['spine_twist_weight'],
        limbs=[dict(parent=-1 if p==0xff else place[p],maintainRotationWeight=m,weight=w) for p,m,w in zip(rig['limb_parents'],rig['limb_maintain_rotation'],rig['limb_weight'])],
        head=place[rig['head']],headMaintainRotationWeight=rig['head_maintain_rotation'],preUpdateSubscribers=['IKOffsetModifier::ModifyOffset'],modelBehaviours=[])
    lines=[dict(type='header',schema=1,kind='observed-original-fbbik'),record]
    for sample,case in enumerate(cases):
        post=t.replay(rig,init,case['pose'],case['weight'],case['effectors'],[0.]*4)
        pack=lambda local:[[list(local[r][0]),list(local[r][1])] for r in order]
        lines.append(dict(type='frame',sample=sample,weight=case['weight'],bendWeights=[0.]*4,pre=pack(case['pose']),post=pack(post),nodes=[[0,0,0]]*13,
            effectors=[dict(positionOffset=list(e['position_offset']),position=list(e['position']),rotation=list(e['rotation']),positionWeight=e['position_weight'],
                            rotationWeight=e['rotation_weight'],maintain=e['maintain'],target=False) for e in case['effectors']]))
    lines.append(dict(type='end',reason='stop-file'))
    return lines

@unittest.skipUnless(shutil.which('cc') and a.LIBRARY.is_file(),'C compiler or built core library unavailable')
class Analyzer(unittest.TestCase):
    def setUp(self):
        rng=random.Random(7);self.cases=[c for c in (t.random_case(RIG,rng,i*6) for i in range(12)) if c['iterations']==RIG['iterations']]
        for case in self.cases:case['bend']=[0.]*4
        self.lines=capture(RIG,self.cases,random.Random(3))
    def run_lines(self,lines):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'capture.jsonl';path.write_text(''.join(json.dumps(x)+'\n' for x in lines));return a.analyze(path)
    def test_reordered_capture_matches_the_solver(self):
        report=self.run_lines(self.lines)
        self.assertEqual(report['setting_differences'],[]);self.assertEqual(report['frames'],len(self.cases))
        for mode in ('observed_bend_defaults','prefab_rest_initiation'):
            self.assertEqual(report[mode]['failed_frames'],0)
            self.assertLess(report[mode]['bone_position_error_m']['max'],2e-4);self.assertLess(report[mode]['bone_rotation_error_degrees']['max'],.2)
            self.assertGreater(report[mode]['original_ik_displacement_m']['max'],.01)
    def test_wrong_bend_defaults_or_pose_are_visible(self):
        lines=json.loads(json.dumps(self.lines));lines[5]['post'][3][0][1]+=.05
        self.assertGreater(self.run_lines(lines)['observed_bend_defaults']['bone_position_error_m']['max'],.04)
    def test_changed_topology_and_settings_are_reported(self):
        lines=json.loads(json.dumps(self.lines));lines[1]['chains'][1]['nodes'].reverse()
        with self.assertRaises(ValueError):self.run_lines(lines)
        lines=json.loads(json.dumps(self.lines));lines[1]['iterations']=3;lines[1]['chains'][2]['bendGoal']=True;lines[4]['effectors'][5]['target']=True
        found=self.run_lines(lines)['setting_differences']
        self.assertTrue(any('iterations' in x for x in found) and any('bend goal' in x for x in found) and any('target' in x for x in found))
        lines=json.loads(json.dumps(self.lines));lines[1]['bones'][4]['lossyScale']=[1,1.1,1]
        with self.assertRaises(ValueError):self.run_lines(lines)

class Probe(unittest.TestCase):
    def test_probe_only_subscribes_and_reads(self):
        source=(a.ROOT/'rebuild/tools/ReferenceFbbikProbe.cs').read_text()
        writes=re.findall(r'\b(?:solver|ik|found|s|e|chain|bend|limb|t|bones\[i\])\.[A-Za-z_.\[\]0-9]+\s*=(?!=)',source)
        self.assertEqual(sorted(set(w.split('=')[0].strip() for w in writes)),['solver.OnPostUpdate','solver.OnPreRead'])
        self.assertEqual(source.count('Delegate.Combine'),2);self.assertEqual(source.count('Delegate.Remove'),2)
        for forbidden in ('SendMessage','Instantiate','AddForce','SetFloat','SetBool','SetTrigger','.Play(','Input.'):self.assertNotIn(forbidden,source)

if __name__=='__main__':unittest.main()
