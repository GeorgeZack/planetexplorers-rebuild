#!/usr/bin/env python3
"""Offline checks of the GrounderFBBIK observation probe and its analyzer."""
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
import analyze_reference_grounder as a
import test_grounder_export as t

SETTINGS=json.loads(a.SETTINGS.read_text())['settings']

def rig():
    s=SETTINGS;solver=dict(s['solver'],rotateSolver=bool(s['solver']['rotateSolver']))
    spine=[dict(effector=e,horizontal=h,vertical=v) for e,(h,v) in enumerate(zip(s['spine_horizontal'],s['spine_vertical']))]
    names=['']*32
    for bit,name in a.SOURCE_LAYERS.items():names[bit]=name
    return dict(solver,type='rig',enabled=True,weight=s['weight'],spineBend=s['spine_bend'],spineSpeed=s['spine_speed'],spine=spine,layers=s['layers'],layerNames=names,
        root=s['root'],pelvis=s['pelvis'],feet=s['feet'],legTransforms=s['feet'],footEffectors=s['feet'],rootLossyScale=[1,1,1],
        preGrounderSubscribers=[],postGrounderSubscribers=[],preUpdateSubscribers=['IKAnimEffectCtrl::ModifyOffset'])

def capture(seed,flat=False):
    """A capture written from the double-precision second transcription over rolling ground."""
    rng=random.Random(seed);p,frames=t.scenario(rng,0)
    if flat:p[1]=p[4]=0.
    step=SETTINGS['solver']['maxStep'];rays=[]
    inner=t.caster(p)
    def cast(origin,limit):
        hit=inner(origin,limit);rays.append(dict(origin=list(origin),distance=limit,hit=hit is not None))
        if hit:rays[-1].update(point=list(hit.point),normal=list(hit.normal),hitDistance=hit.distance,layer=12,collider='MeshCollider:chunk',trigger=False)
        return hit
    root=t.Root();feet=[t.Foot(),t.Foot()];grounder=None;lines=[dict(type='header',schema=1,kind='observed-original-grounder'),rig()];turn=(0.,.6,0.,.8)
    for k,f in enumerate(frames):
        t.Time.time=f['time'];t.Time.delta=f['delta'];root.position=f['root'];root.forward=f['forward'];root.right=f['right']
        if flat:
            root.position=(f['root'][0],p[0],f['root'][2]);f['feet']=[(x,p[0]+.1,z) for x,_,z in f['feet']]
        for foot,position in zip(feet,f['feet']):foot.position=position
        if not k:grounder=t.Grounder(SETTINGS,root,feet,cast);continue
        g=grounder.solver;weight=min(1.,max(0.,f['weight']))
        if not f['enabled'] or weight<=0:continue   # OnPreGrounder is never raised
        offsets=[[rng.uniform(-.2,.2) for _ in range(3)] for _ in range(9)];bone=[root.position[0],root.position[1]+.9,root.position[2]]
        pre=dict(enabled=True,weight=weight,root=[list(root.position),list(root.forward),list(root.right),list(root.up)],pelvisBone=bone,spineOffset=list(grounder.spine_offset),offsets=offsets,
            pelvis=dict(lastTime=g.pelvis.last_time,heightOffset=g.pelvis.height,damperF=g.pelvis.damper,lastRootPosition=list(g.pelvis.last_root)),
            legs=[dict(position=list(leg.transform.position),rotation=list(turn),lastTime=leg.last_time,lastPosition=list(leg.last_position),ikOffset=leg.ik_offset,r=list(leg.r),rotationOffset=list(leg.rotation_offset)) for leg in g.legs])
        del rays[:];pelvis,foot_offsets,rotations,spine=grounder.on_solver_update(True,weight);cast_rays=list(rays);hits=iter(cast_rays[1:]);heels=[]
        for leg in g.legs:
            if leg.delta==0:heels.append([0.,0.,0.]);continue
            heel=next(hits);next(hits);next(hits);heels.append(heel['point'] if heel['hit'] else [heel['origin'][0],heel['origin'][1]-3*step,heel['origin'][2]])
        first=cast_rays[0];added=[list(x) for x in spine]
        for i,e in enumerate(a.FEET):added[e]=[x+y for x,y in zip(added[e],foot_offsets[i])]
        post=dict(pelvisBone=[x+y for x,y in zip(bone,pelvis)],spineOffset=list(grounder.spine_offset),offsets=[[x+y for x,y in zip(o,d)] for o,d in zip(offsets,added)],
            grounded=g.grounded,rootGrounded=g.root_grounded,
            rootHit=dict(point=first['point'],distance=first['hitDistance']) if first['hit'] else dict(point=[0,0,0],distance=0),
            pelvis=dict(ikOffset=list(pelvis),heightOffset=g.pelvis.height,damperF=g.pelvis.damper),
            legs=[dict(position=[x+y for x,y in zip(leg.transform.position,pelvis)],rotation=list(a.qmul(rotations[i],turn)),ikOffset=leg.ik_offset,ikPosition=list(leg.ik_position),
                       rotationOffset=list(leg.rotation_offset),r=list(leg.r),heightFromGround=leg.height,grounded=leg.grounded,velocity=[0,0,0],heelPoint=heels[i],heelNormal=[0,1,0]) for i,leg in enumerate(g.legs)])
        lines.append(dict(pre,type='frame',sample=len(lines)-2,frame=k,time=f['time'],dt=f['delta'],rays=cast_rays,post=post))
    lines.append(dict(type='end',reason='stop-file'));return lines

@unittest.skipUnless(shutil.which('cc') and a.LIBRARY.is_file(),'C compiler or built core library unavailable')
class Analyzer(unittest.TestCase):
    def run_lines(self,lines):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'capture.jsonl';path.write_text(''.join(json.dumps(x)+'\n' for x in lines));return a.analyze(path)
    def close(self,part,position=2e-5,angle=.01):
        self.assertEqual((part['failed_frames'],part['frames_with_more_rays_than_recorded'],part['frames_with_a_different_ray_count'],part['grounded_flag_differences']),(0,0,0,0))
        for key in ('leg_ik_offset_m','leg_ik_position_m','leg_height_from_ground_m','pelvis_height_m','pelvis_bone_move_m','foot_effector_offset_m','other_effector_offset_m','spine_offset_m','ray_origin_m'):
            self.assertLess(part[key]['max'],position,key)
        self.assertLess(part['leg_rotation_degrees']['max'],angle);self.assertLess(part['foot_rotation_degrees']['max'],angle)
    def test_rolling_ground_matches_in_both_replays(self):
        for seed in (3,11,29):
            lines=capture(seed);report=self.run_lines(lines)
            self.assertEqual(report['setting_differences'],[]);self.assertEqual(report['frames'],len(lines)-3);self.assertGreater(report['frames'],50)
            self.close(report['single_update']);self.close(report['free_running'],2e-4,.05)
            self.assertTrue(report['coverage']['uneven_ground_observed']);self.assertGreater(report['coverage']['foot_offset_m']['max'],.05)
            self.assertLess(report['probe_rays_against_original']['heel_hit_m']['max'],1e-9);self.assertLess(report['probe_rays_against_original']['root_hit_m']['max'],1e-9)
            self.assertEqual(report['layers']['12'],'VFVoxelTerrain');self.assertEqual(sorted(report['layers']),sorted(str(b) for b in (0,8,11,12,13,14,16,19)))
            self.assertEqual(report['probe_rays_against_original']['hits_by_collider_type'].keys(),{'MeshCollider'})
    def test_flat_ground_is_reported_as_not_qualifying(self):
        coverage=self.run_lines(capture(5,flat=True))['coverage']
        self.assertFalse(coverage['uneven_ground_observed']);self.assertIn('cannot qualify',coverage['verdict'])
    def test_wrong_state_requests_and_rays_are_visible(self):
        base=capture(3)
        lines=json.loads(json.dumps(base));lines[20]['post']['legs'][1]['ikOffset']+=.03
        self.assertGreater(self.run_lines(lines)['single_update']['leg_ik_offset_m']['max'],.029)
        lines=json.loads(json.dumps(base));lines[20]['post']['pelvisBone'][1]+=.04
        self.assertGreater(self.run_lines(lines)['single_update']['pelvis_bone_move_m']['max'],.039)
        lines=json.loads(json.dumps(base));lines[20]['post']['offsets'][2][0]+=.05
        self.assertGreater(self.run_lines(lines)['single_update']['other_effector_offset_m']['max'],.049)
        lines=json.loads(json.dumps(base));lines[20]['rays'][2]['origin'][0]+=.07
        self.assertGreater(self.run_lines(lines)['single_update']['ray_origin_m']['max'],.069)
        lines=json.loads(json.dumps(base));lines[20]['post']['legs'][0]['heelPoint'][1]-=.02
        self.assertGreater(self.run_lines(lines)['probe_rays_against_original']['heel_hit_m']['max'],.019)
        # A wrong recorded starting state misleads the update restarted from it; the free run never reads it.
        lines=json.loads(json.dumps(base));lines[20]['legs'][0]['ikOffset']+=.5;report=self.run_lines(lines)
        self.assertGreater(report['single_update']['leg_ik_offset_m']['max'],.01);self.assertLess(report['free_running']['leg_ik_offset_m']['max'],2e-4)
    def test_changed_settings_and_modes_are_reported(self):
        lines=capture(3);lines[1].update(maxStep=.5,layers=1,spineBend=3.0);lines[1]['spine'][1]['horizontal']=.5;lines[1]['legTransforms']=['Bip01 L Toe0','Bip01 R Foot']
        found=self.run_lines(lines)['setting_differences']
        lines[1]['layerNames'][13]='Other'
        found=self.run_lines(lines)['setting_differences']
        for word in ('maxStep','layers','spineBend','spine 1 horizontal','grounding legs','layer 13'):self.assertTrue(any(word in x for x in found),word)
        for key,value in (('rotateSolver',True),('quality',2)):
            lines=capture(3);lines[1][key]=value
            with self.assertRaises(ValueError):self.run_lines(lines)
    def test_incomplete_or_nonfinite_captures_are_rejected(self):
        lines=capture(3)
        for bad in (lines[:2],[lines[0]]+lines[2:],[dict(lines[0],kind='observed-original-fbbik')]+lines[1:],lines[:2]+[lines[1]]+lines[2:]):
            with self.assertRaises(ValueError):self.run_lines(bad)
        lines=json.loads(json.dumps(lines));lines[9]['post']['legs'][0]['ikOffset']=None
        with self.assertRaises(ValueError):self.run_lines(lines)

class Probe(unittest.TestCase):
    def test_probe_only_subscribes_reads_and_queries(self):
        source=(a.ROOT/'rebuild/tools/ReferenceGrounderProbe.cs').read_text()
        writes=re.findall(r'\b(?:grounder|grounding|ik|body|found|g|leg|pelvis|root|feet\[i\])\.[A-Za-z_.\[\]0-9]+\s*=(?!=)',source)
        self.assertEqual(sorted(set(w.split('=')[0].strip() for w in writes)),['grounder.OnPostGrounder','grounder.OnPreGrounder'])
        self.assertEqual(source.count('Delegate.Combine'),2);self.assertEqual(source.count('Delegate.Remove'),2)
        self.assertEqual(len(re.findall(r'Physics\.\w+\(',source)),1);self.assertIn('Physics.Raycast(origin, Vector3.down, out hit, distance, grounding.layers)',source)
        for forbidden in ('SendMessage','Instantiate','AddForce','SetFloat','SetBool','SetTrigger','.Play(','Input.','SetValue','.position =','.rotation =','Invoke('):self.assertNotIn(forbidden,source)
    def test_mask_layers_are_the_source_names(self):
        self.assertEqual(sum(1<<bit for bit in a.SOURCE_LAYERS),SETTINGS['layers'])
        path=a.ROOT/'Assets/Scripts/ZhouXun/Voxel Creation/Scripts/WhiteCat/PE-VC/Layer.cs'
        if not path.is_file():self.skipTest('Reference source unavailable')
        constants={}
        for name,value in re.findall(r'public const int (\w+) = (\d+);',path.read_text(errors='replace')):constants.setdefault(name,int(value))   # the class precedes its Mask twin
        for bit,name in a.SOURCE_LAYERS.items():self.assertEqual(constants[name],bit,name)
        self.assertEqual((constants['Player'],constants['Ragdoll']),(10,15))   # neither is in the mask
    def test_launcher_defaults_to_an_offline_build(self):
        source=(a.ROOT/'rebuild/tools/reference_grounder_probe.py').read_text()
        self.assertIn('reference_fbbik_probe.main(argv, source=SOURCE',source);self.assertNotIn('subprocess',source)

if __name__=='__main__':unittest.main()
