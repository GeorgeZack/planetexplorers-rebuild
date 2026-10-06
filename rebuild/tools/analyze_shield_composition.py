#!/usr/bin/env python3
"""Read actual clone state/weight/matrix differences without claiming pose parity."""
import argparse
import json
import math
from pathlib import Path
from analyze_reference_pose import decode
from inventory_shield_layers import sha


def finite(x):
    if type(x) not in (int,float) or not math.isfinite(x):raise ValueError('Nonfinite/nonnumeric replay output')
    return x


def matrix(values):
    if not isinstance(values,list) or len(values)!=16:raise ValueError('Invalid matrix')
    return [finite(x) for x in values]


def multiply(a,b):
    a,b=matrix(a),matrix(b)
    return [sum(a[r*4+k]*b[k*4+c] for k in range(4)) for r in range(4) for c in range(4)]


def weights(clips):
    result={}
    for clip in clips:
        weight=finite(clip['weight'])
        if finite(clip['length'])<=0:raise ValueError('Invalid authored clip length')
        if weight<0 or weight>1.00001:raise ValueError('Invalid clip weight')
        if weight>0:result[clip['name']]=result.get(clip['name'],0.)+weight
    return result


def compare_frame(actual,expected):
    if actual['sourceSample']!=expected['sample']:raise ValueError('Source sample differs')
    if finite(actual['animatorSpeed'])!=0:raise ValueError('Clone Animator must remain frozen')
    layers=actual['layers'];source=expected['layers']
    for layer in layers:
        finite(layer['stateSpeed']);finite(layer['stateSpeedMultiplier'])
        if layer['length'] is None:
            if layer.get('lengthAbsentReason')!='frozen-effective-duration':raise ValueError('Unexplained absent state length')
        else:
            finite(layer['length'])
            if layer.get('lengthAbsentReason') is not None:raise ValueError('Finite length has absent reason')
    if len(layers)!=len(source):raise ValueError('Layer count differs')
    checks=[]
    for i in (0,3):
        a,s=layers[i],source[i]
        if a['name']!=s['name']:raise ValueError('Layer identity differs')
        aw,sw=weights(a['clips']),weights(s['currentClips'])
        error=max((abs(aw.get(n,0)-sw.get(n,0)) for n in set(aw)|set(sw)),default=0.)
        time_error=abs(finite(a['normalizedTime'])-finite(s['current']['normalizedTime']))
        checks.append({'index':i,'state_matches':a['state']&0xffffffff==s['current']['fullPathHash']&0xffffffff,
                       'transition':a['inTransition'],'layer_weight_error':abs(finite(a['weight'])-finite(s['weight'])),'normalized_time_error':time_error,'max_clip_weight_error':error,
                       'source_weights':sw,'clone_weights':aw})
    overrides=[{'index':i,'name':a['name'],'weight':finite(a['weight'])} for i,a in enumerate(layers)
               if i not in (0,3) and finite(a['weight'])>1e-4 and
               (weights(a['clips']) or (a['inTransition'] and weights(a.get('nextClips',[]))))]
    params={p['name']:p['value'] for p in expected['parameters']}
    if set(actual['parameters'])!=set(params):raise ValueError('Readback parameter set differs')
    param_error=max((abs(finite(actual['parameters'][n])-v) for n,v in params.items() if type(v) is not bool),default=0.)
    bool_match=all(type(actual['parameters'][n]) is bool and actual['parameters'][n]==v for n,v in params.items() if type(v) is bool)
    bones={b['path']:b for b in actual['bones']}
    if len(bones)!=len(actual['bones']) or not 2<=len(bones)<=512:raise ValueError('Duplicate/excess bones')
    source_bones={b['path']:b for b in expected['bones']}
    human=expected.get('human_paths',{})
    if not human:raise ValueError('Source human-bone mapping required')
    differences=[]
    for name,path in human.items():
        if path not in bones or path not in source_bones:raise ValueError('Missing human bone: '+name)
        model=multiply(expected['root_inverse_world'],source_bones[path]['world']);other=matrix(bones[path]['model'])
        differences.append({'human_bone':name,'path':path,'max_matrix_error':max(abs(a-b) for a,b in zip(model,other)),
                            'position_error_metres':math.sqrt(sum((model[i]-other[i])**2 for i in (3,7,11)))})
    return {'source_sample':expected['sample'],'layers':checks,'parameter_max_error':param_error,'bool_parameters_match':bool_match,
            'unexpected_active_overrides':overrides,
            'state_clock_weight_readback_matches':not overrides and bool_match and param_error<=1e-6 and all(c['state_matches'] and not c['transition'] and
                                      c['normalized_time_error']<=1e-4 and c['max_clip_weight_error']<=1e-5 and c['layer_weight_error']<=1e-5 for c in checks),
            'human_bones':differences,'max_human_position_error_metres':max(d['position_error_metres'] for d in differences),
            'pose_parity_qualified':False}


def analyze(capture,plan_path):
    if capture.stat().st_size>4*1024*1024 or plan_path.stat().st_size>4*1024*1024:raise ValueError('Byte bound')
    before=sha(capture);plan=decode(plan_path.read_bytes());rows=[]
    with capture.open('rb') as f:
        for line in f:
            if len(line)>1024*1024:raise ValueError('Line bound')
            rows.append(decode(line))
    diagnostics=[r for r in rows if r['type']=='diagnostic']
    if len(diagnostics)>20 or any(type(r.get('message')) is not str or len(r['message'])>4096 for r in diagnostics):raise ValueError('Diagnostic bound')
    rows=[r for r in rows if r['type']!='diagnostic']
    if len(rows)!=6 or [r['type'] for r in rows]!=['header','cloneReady','frame','frame','frame','end']:
        raise ValueError('Require exact closed three-frame smoke')
    if rows[-1]['reason']!='complete' or rows[-1]['frames']!=3:raise ValueError('Capture failed/incomplete')
    if plan.get('schema')!='pe-shield-composition-replay-1' or rows[0].get('schema')!='pe-shield-composition-1':raise ValueError('Wrong schema')
    if rows[0]['planSha256']!=sha(plan_path) or rows[0]['bundleSha256']!=plan['bundle_sha256']:raise ValueError('Provenance mismatch')
    if [r['sample'] for r in rows[2:5]]!=[0,1,2]:raise ValueError('Sample order differs')
    result=[compare_frame(a,s) for a,s in zip(rows[2:5],plan['frames'])]
    if sha(capture)!=before:raise ValueError('Capture changed')
    return {'schema':'pe-shield-composition-comparison-1','capture_sha256':before,'plan_sha256':sha(plan_path),
            'diagnostics':diagnostics,'frames':result,'readback_matches':all(r['state_clock_weight_readback_matches'] for r in result),
            'runtime_pose_export_ready':False,
            'limits':['Source includes live IK; clone disables external and Base Move foot IK.',
                      'Readback agreement is not matrix parity; residual human-bone differences are reported without tolerance relaxation.',
                      'Three replay points cannot qualify independent phase coverage or moving transitions.',
                      'Post-cleanup survival/ordinary responsiveness and quit require separate operator evidence.']}


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path);p.add_argument('--plan',type=Path,required=True);p.add_argument('--report',type=Path,required=True)
    a=p.parse_args(argv)
    try:
        if a.report.exists() or a.report.resolve() in (a.capture.resolve(),a.plan.resolve()):raise ValueError('Fresh report path required')
        value=analyze(a.capture,a.plan);a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')
    except (OSError,ValueError,KeyError,TypeError) as e:p.error(str(e))
    print(json.dumps({'readback_matches':value['readback_matches'],'runtime_pose_export_ready':False}))


if __name__=='__main__':main()
