#!/usr/bin/env python3
"""Measure isolated clip poses against stable observed original-player frames.

No evaluation or fabricated reference. Differences include procedural IK,
controller layers, retarget initialization and sampled interpolation.
"""
import argparse
import json
import math
from pathlib import Path
import tempfile

import analyze_reference_bake as bake
import analyze_reference_pose as pose


def compare(bake_path, capture_path, rig, clip, first=0, last=None, layer='FullBody', first_person=0):
    baked,baked_samples=bake.analyze(bake_path,rig,True)
    if clip not in baked_samples:pose.fail('Requested clip absent from bake')
    definition=next(c for c in baked['clips'] if c['name']==clip)
    observed,observed_samples=pose.analyze(capture_path,rig,first_sample=first,last_sample=last,retain=True)
    observed_by_sample={s['sample']:s for s in observed_samples}
    selected=[]
    with capture_path.open('rb') as stream:
        for line in stream:
            row=pose.decode(line)
            if row.get('type')!='frame' or row['sample'] not in observed_by_sample:continue
            animator=next(a for a in row['animators'] if a['id']==observed['rig_mapping']['animator_id'])
            if abs(animator['parameters'].get('FirstPerson',0)-first_person)>1e-6:continue
            matches=[l for l in animator['layers'] if l['name']==layer]
            if len(matches)!=1:continue
            current=matches[0];clips=current['currentClips']
            if current['inTransition'] or current['weight']<.99 or len(clips)!=1 or clips[0]['name']!=clip or clips[0]['weight']<.99:continue
            if abs(clips[0]['length']-definition['length'])>.0001:pose.fail('Observed runtime clip duration differs from baked source')
            normalized=current['current']['normalizedTime']
            if not 0<=normalized<=1:continue
            selected.append((row['sample'],normalized*clips[0]['length']))
    if not selected:pose.fail('No stable matching observed frames')
    source=baked_samples[clip];times=[s['realtime'] for s in source]
    names=sorted(set(baked['rig_mapping']['bones'])&set(observed['rig_mapping']['bones']))
    if not names:pose.fail('No common mapped bones')
    totals={n:{'sum_squared_m':0.,'max_position_error_m':0.,'max_matrix_element_error':0.} for n in names}
    comparisons=[]
    for sample,at in selected:
        if not 0<=at<=times[-1]+.0001:pose.fail('Observed clip time exceeds bake')
        matrices=pose.sample_matrices(source,times,min(at,times[-1]),names)
        maximum=0.
        for name,expected in zip(names,matrices):
            actual=observed_by_sample[sample]['bones'][name]
            squared=sum((actual[i][3]-expected[i][3])**2 for i in range(3));distance=math.sqrt(squared)
            total=totals[name];total['sum_squared_m']+=squared;total['max_position_error_m']=max(total['max_position_error_m'],distance)
            total['max_matrix_element_error']=max(total['max_matrix_element_error'],pose.matrix_error(actual,expected));maximum=max(maximum,distance)
        comparisons.append({'capture_sample':sample,'authored_seconds':at,'maximum_position_error_m':maximum})
    for total in totals.values():total['rms_position_error_m']=math.sqrt(total.pop('sum_squared_m')/len(selected))
    return {'schema':1,'clip':clip,'layer':layer,'FirstPerson':first_person,'compared_frames':len(selected),'compared_bones':len(names),
            'coordinate_space':'each Animator/model root removed independently',
            'clock':'observed current-state normalizedTime * runtime clip.length',
            'frames':comparisons,'bones':totals,
            'maximum_position_error_m':max(t['max_position_error_m'] for t in totals.values()),
            'general_retargeting_validated':False,
            'limitations':['Only selected stable current-state frames; no transition validation.',
                           'Observed poses include controller layers/procedural IK; baked poses contain only source clip.',
                           'Affine interpolation between baked samples introduces sampling error.',
                           'Root trajectories removed; no accepted contact, damage or movement equivalence claimed.']}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('bake',type=Path);p.add_argument('capture',type=Path)
    p.add_argument('--rig',type=Path,required=True);p.add_argument('--clip',default='Attack6');p.add_argument('--first-sample',type=int,default=0)
    p.add_argument('--last-sample',type=int);p.add_argument('--layer',default='FullBody');p.add_argument('--first-person',type=int,choices=(0,1),default=0)
    p.add_argument('--report',type=Path,required=True);a=p.parse_args()
    try:
        inputs=[p.expanduser().resolve() for p in (a.bake,a.capture,a.rig)];report_path=a.report.expanduser().resolve()
        if report_path in inputs:pose.fail('Report cannot replace inputs')
        if inputs[2].stat().st_size>64*1024*1024:pose.fail('Rig too large')
        result=compare(inputs[0],inputs[1],pose.decode(inputs[2].read_text()),a.clip,a.first_sample,a.last_sample,a.layer,a.first_person)
        result['sources']=[{'path':str(f),'sha256':pose.sha256(f)} for f in inputs+[Path(__file__).resolve()]]
        report_path.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.NamedTemporaryFile('w',dir=report_path.parent,delete=False) as stream:
            temp=Path(stream.name);json.dump(result,stream,indent=2);stream.write('\n')
        temp.replace(report_path)
        print(f'Compared {result["compared_frames"]} frames / {result["compared_bones"]} bones; maximum translation error {result["maximum_position_error_m"]:.6g} m')
    except (OSError,ValueError,KeyError,TypeError,IndexError) as e:p.exit(1,f'Bake comparison: {e}\n')


if __name__=='__main__':main()
