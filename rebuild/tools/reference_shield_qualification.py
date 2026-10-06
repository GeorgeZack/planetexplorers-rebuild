#!/usr/bin/env python3
"""Offline-first repeatability preparation for exact source masked composition.

Repeatability uses two fresh pure clones and eight reordered/repeated samples.
A grid plan is available offline, but grid launch requires independently
reanalyzed, exact repeatability evidence. No automatic game or desktop actions.
"""
import argparse
import copy
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys
import launch_reference
import reference_pose
from analyze_reference_pose import decode
from analyze_shield_composition import analyze
from export_shield_controller import check, controller_blob, serialized_asset, CONTROLLER_ID
from inventory_shield_layers import sha

TOOLS=Path(__file__).resolve().parent


def verify_graph_pair(source,noik):
    if len(source)!=len(noik):raise ValueError('Paired graph sizes differ')
    offsets=[i for i,(a,b) in enumerate(zip(source,noik)) if a!=b]
    if offsets!=[40,48232] or any(source[i]!=1 or noik[i]!=0 for i in offsets):
        raise ValueError('Pair must differ only by the exact two source IK flags')
    return offsets


def controller_pair(noik_dir,sourceik_dir):
    import UnityPy
    folders=(noik_dir,sourceik_dir);manifests=[];files=[]
    for folder in folders:
        m=decode((folder/'manifest.json').read_bytes());game=Path(m['source_asset']).parents[1]
        m=check(game,folder);manifests.append(m)
        bundle=next(iter(UnityPy.load(str(folder/m['bundle'])).files.values()));files.append(serialized_asset(bundle))
    noik,sourceik=manifests
    if not noik.get('ik_disabled') or sourceik.get('ik_disabled'):
        raise ValueError('Pair must be ordered noIK/sourceIK')
    if noik['source_asset_sha256']!=sourceik['source_asset_sha256']:
        raise ValueError('Pair source identities differ')
    no_raw=files[0].objects[CONTROLLER_ID].get_raw_data();source_raw=files[1].objects[CONTROLLER_ID].get_raw_data()
    no_offset,no_graph=controller_blob(no_raw);source_offset,source_graph=controller_blob(source_raw)
    offsets=verify_graph_pair(source_graph,no_graph)
    if (source_offset!=no_offset or source_raw[:source_offset]!=no_raw[:no_offset] or
        source_raw[source_offset+len(source_graph):]!=no_raw[no_offset+len(no_graph):]):
        raise ValueError('Pair has controller differences outside the two flags')
    if set(files[0].objects)!=set(files[1].objects):raise ValueError('Pair closures differ')
    for identity in set(files[0].objects)-{1,CONTROLLER_ID}:
        if files[0].objects[identity].get_raw_data()!=files[1].objects[identity].get_raw_data():
            raise ValueError('Pair clip bytes differ')
    return {'noik_dir':str(noik_dir),'sourceik_dir':str(sourceik_dir),'source_asset_sha256':noik['source_asset_sha256'],
            'noik_bundle_sha256':noik['bundle_sha256'],'sourceik_bundle_sha256':sourceik['bundle_sha256'],
            'noik_manifest_sha256':sha(noik_dir/'manifest.json'),'sourceik_manifest_sha256':sha(sourceik_dir/'manifest.json'),
            'graph_ik_offsets':offsets,'identical_clip_bytes':True,'identical_masks_and_remaining_graph':True}


def repeat_sequence(frames):
    if len(frames)!=3:raise ValueError('Three source configurations required')
    result=[]
    for clone,order in enumerate(((0,1,2,0),(2,0,1,2))):
        seen={}
        for config in order:
            e=frames[config]['evidence'];repetition=seen.get(config,0);seen[config]=repetition+1
            result.append({'clone':clone,'config':config,'repetition':repetition,
                           'base_normalized_time':e['base_normalized_time'],'block_normalized_time':e['block_normalized_time']})
    return result


def grid_sequence():
    result=[]
    for config in range(3):
        def add(base,block,kind):result.append({'clone':0,'config':config,'repetition':0,
                                  'base_normalized_time':base,'block_normalized_time':block,'kind':kind})
        phases=(0.,.25,.5,.75)
        for base in phases:
            for block in phases:add(base,block,'grid')
        for block in phases:add(1.,block,'base-seam')
        for base in phases:add(base,1.,'block-seam')
        add(1.,1.,'corner-seam')
        for base,block in ((.125,.125),(.375,.625),(.625,.375),(.875,.875)):add(base,block,'withheld')
    if len(result)!=87:raise ValueError('Grid bound differs')
    return result


def repeatability_gate(capture,plan,variant,bundle_sha):
    if capture is None or plan is None:raise ValueError('Grid launch requires completed repeatability capture and plan')
    from analyze_shield_qualification import analyze as analyze_repeatability
    report=analyze_repeatability(capture,plan)
    if not report.get('repeatability_qualified') or not report.get('grid_preparation_allowed'):
        raise ValueError('Exact repeatability gate failed; grid launch prohibited')
    evidence=decode(plan.read_bytes())
    if evidence.get('phase')!='repeatability' or evidence.get('variant')!=variant or evidence.get('bundle_sha256')!=bundle_sha:
        raise ValueError('Repeatability variant/bundle differs')
    return {'capture':str(capture),'capture_sha256':sha(capture),'plan':str(plan),'plan_sha256':sha(plan),
            'qualification':'reanalyzed-exact-repeatability'}


def prepared_files(folder):
    m=decode((folder/'prepared-build.json').read_bytes())
    for path,digest in m['files'].items():
        if sha(Path(path))!=digest:raise ValueError('Prepared file changed: '+path)
    return m


def emit_plan(folder,plan):
    path=folder/'qualification-plan.json';path.write_text(json.dumps(plan,indent=2,allow_nan=False)+'\n')
    sequence=plan['sequence'];q=json.dumps
    out=['using UnityEngine; namespace PeReference { internal static class ShieldQualificationPlan {',
         'internal const string BundleHash='+q(plan['bundle_sha256'])+';', 'internal const string PlanHash='+q(sha(path))+';',
         'internal const string Phase='+q(plan['phase'])+';', 'internal const string Variant='+q(plan['variant'])+';',
         'internal static readonly int[] Samples={'+','.join(str(f['sample']) for f in plan['frames'])+'};']
    for name,key,kind in [('CloneIndices','clone','int'),('ConfigIndices','config','int'),('Repetitions','repetition','int'),
                          ('BaseTimes','base_normalized_time','float'),('BlockTimes','block_normalized_time','float')]:
        values=','.join(str(r[key]) if kind=='int' else format(r[key],'.9g')+'f' for r in sequence)
        out.append('internal static readonly '+kind+'[] '+name+'={'+values+'};')
    out.append('internal static readonly ShieldQualificationClone.Parameter[][] Parameters={')
    kinds={1:'Float',3:'Int',4:'Bool'}
    for frame in plan['frames']:
        out.append('new ShieldQualificationClone.Parameter[]{')
        for p in frame['parameters']:
            field='boolean='+str(p['value']).lower() if p['type']==4 else 'number='+format(p['value'],'.9g')+'f'
            out.append('new ShieldQualificationClone.Parameter{name='+q(p['name'])+',type=AnimatorControllerParameterType.'+kinds[p['type']]+','+field+'},')
        out.append('},')
    out.append('}; internal static readonly ShieldQualificationClone.ClipIdentity[] Clips={')
    for c in plan['clips']:
        out.append('new ShieldQualificationClone.ClipIdentity{sourceId='+str(c['source_id'])+',name='+q(c['name'])+',length='+format(c['length'],'.9g')+'f,rate='+format(c['rate'],'.9g')+'f},')
    out.append('}; }}');(folder/'ShieldQualificationPlan.cs').write_text('\n'.join(out)+'\n')


def prepare(baseline,baseline_capture,noik,sourceik,folder,variant,phase):
    if folder.exists():raise ValueError('Fresh build directory required')
    prepared_files(baseline)
    baseline_plan=baseline/'replay-plan.json';original=decode(baseline_plan.read_bytes())
    if not analyze(baseline_capture,baseline_plan)['readback_matches']:raise ValueError('Successful baseline replay required')
    pair=controller_pair(noik,sourceik)
    if pair['source_asset_sha256']!=original['source_asset_sha256']:raise ValueError('Baseline source differs')
    selected=noik if variant=='noik' else sourceik
    plan=copy.deepcopy(original);plan.update(schema='pe-shield-qualification-plan-1',phase=phase,variant=variant,
          bundle_dir=str(selected),bundle_sha256=pair[variant+'_bundle_sha256'],controller_manifest_sha256=sha(selected/'manifest.json'),
          ik_disabled=variant=='noik',pair=pair,source_avatar='FemaleAvatar',source_controller='FemaleAnimator2',controller_source_id=779,
          baseline_plan=str(baseline_plan),baseline_plan_sha256=sha(baseline_plan),baseline_capture=str(baseline_capture),
          baseline_capture_sha256=sha(baseline_capture),expected_clones=2 if phase=='repeatability' else 1,
          sequence=repeat_sequence(original['frames']) if phase=='repeatability' else grid_sequence(),
          runtime_qualified=False,grid_execution_requires_exact_repeatability=True)
    plan['max_frames']=len(plan['sequence']);plan['limits']=[
        'Two pure clones for repeatability; no live Animator writes or gameplay components.',
        'Source-IK means only the two original built-in flags; external gameplay IK remains absent.',
        'Grid planning is offline; actual launch requires completed exact repeatability for the same variant/bundle.',
        'Neither repeated pose equality nor a grid capture alone qualifies interpolation or production assets.']
    folder.mkdir(parents=True);emit_plan(folder,plan)
    game=Path(decode((selected/'manifest.json').read_bytes())['source_asset']).parents[1]
    return plan,game


def build(game,folder):
    mono,shim,_=reference_pose.build(game,folder)
    managed=game/'PE_Client_Data/Managed';assembly=folder/'ReferenceShieldQualificationProbe.dll'
    sources=[TOOLS/'ShieldQualificationClone.cs',TOOLS/'ReferenceShieldQualificationProbe.cs',TOOLS/'ShieldCaptureNumbers.cs',folder/'ShieldQualificationPlan.cs']
    subprocess.run([str(folder/'compiler'),str(mono),str(managed),'-noconfig','-nostdlib','-target:library',
                   *[f'-r:{managed/n}' for n in ('mscorlib.dll','System.Core.dll','UnityEngine.dll')],f'-out:{assembly}',*[str(p) for p in sources]],check=True)
    record={'schema':'pe-shield-qualification-build-1','game':str(game),'files':{str(p):sha(p) for p in
            [*sources,assembly,shim,mono,managed/'UnityEngine.dll',folder/'qualification-plan.json',TOOLS/'reference_shield_qualification.py',TOOLS/'reference_pose_shim.c']}}
    (folder/'prepared-build.json').write_text(json.dumps(record,indent=2)+'\n');return mono,shim,assembly


def reuse(folder):
    record=prepared_files(folder)
    if record.get('schema')!='pe-shield-qualification-build-1':raise ValueError('Wrong prepared build')
    plan=decode((folder/'qualification-plan.json').read_bytes());pair=plan['pair']
    if controller_pair(Path(pair['noik_dir']),Path(pair['sourceik_dir']))!=pair:raise ValueError('Controller pair changed')
    for name in ('capture','source_plan','rig','baseline_plan','baseline_capture'):
        if sha(Path(plan[name]))!=plan[name+'_sha256']:raise ValueError('Source provenance changed: '+name)
    game=Path(record['game'])
    return plan,game,game/'PE_Client_Data/Mono/x86_64/libmono.so',folder/'pose-shim.so',folder/'ReferenceShieldQualificationProbe.dll'


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline-build',type=Path,default=TOOLS.parent/'build/shield-composition/smoke-06')
    p.add_argument('--baseline-capture',type=Path,default=Path.home()/'.local/share/planet-explorers-shield-composition-reference/pose-captures/shield-composition-04.jsonl')
    p.add_argument('--noik-bundle',type=Path,default=TOOLS.parent/'build/shield-composition/controller-noik-01')
    p.add_argument('--sourceik-bundle',type=Path,default=TOOLS.parent/'build/shield-composition/controller-sourceik-01')
    p.add_argument('--build-dir',required=True,type=Path)
    p.add_argument('--variant',choices=('noik','sourceik'),default='noik')
    p.add_argument('--phase',choices=('repeatability','grid'),default='repeatability')
    p.add_argument('--reuse-prepared',action='store_true')
    p.add_argument('--repeatability-capture',type=Path);p.add_argument('--repeatability-plan',type=Path)
    p.add_argument('--user-data',type=Path,default=Path.home()/'.local/share/planet-explorers-shield-qualification-reference')
    p.add_argument('--output',type=Path);p.add_argument('--single-threaded-rendering',action='store_true');p.add_argument('--launch',action='store_true')
    a=p.parse_args(argv)
    try:
        folder=a.build_dir.resolve()
        if a.reuse_prepared:plan,game,mono,shim,assembly=reuse(folder)
        else:
            plan,game=prepare(a.baseline_build.resolve(),a.baseline_capture.resolve(),a.noik_bundle.resolve(),a.sourceik_bundle.resolve(),folder,a.variant,a.phase)
            mono,shim,assembly=build(game,folder)
        if plan['phase']!=a.phase or plan['variant']!=a.variant:raise ValueError('Requested stage/variant differs from prepared plan')
        if not a.launch:
            print(json.dumps({'prepared':str(folder),'phase':plan['phase'],'frames':plan['max_frames'],'launched':False}));return 0
        gate=None
        if plan['phase']=='grid':gate=repeatability_gate(a.repeatability_capture.resolve() if a.repeatability_capture else None,
                                                       a.repeatability_plan.resolve() if a.repeatability_plan else None,plan['variant'],plan['bundle_sha256'])
        profile=a.user_data.resolve();output,arm,stop,manifest=reference_pose.capture_paths(profile,a.output)
        command,env=launch_reference.launch_command(game,profile,a.single_threaded_rendering)
        if env.get('LD_PRELOAD'):raise ValueError('Use clean LD_PRELOAD environment')
        if any(c.isspace() or c==':' for c in str(shim)):raise ValueError('Invalid shim path')
        env.update(LD_PRELOAD=str(shim),PE_POSE_EXECUTABLE=str(Path(command[0]).resolve()),PE_POSE_MONO=str(mono),PE_POSE_ASSEMBLY=str(assembly),
                   PE_POSE_OUTPUT=str(output),PE_POSE_ARM=str(arm),PE_POSE_STOP=str(stop),PE_SHIELD_BUNDLE=str(Path(plan['bundle_dir'])/'reference-shield-controller.unity3d'))
        launch_reference.prepare_settings(profile);launch_reference.prepare_unity_preferences(profile/'engine-config');output.parent.mkdir(parents=True,exist_ok=True)
        with manifest.open('x') as f:json.dump({'schema':'pe-shield-qualification-launch-1','capture':str(output),'command':command,'profile':str(profile),
            'plan':str(folder/'qualification-plan.json'),'plan_sha256':sha(folder/'qualification-plan.json'),'assembly':str(assembly),'assembly_sha256':sha(assembly),
            'shim_sha256':sha(shim),'bundle_sha256':plan['bundle_sha256'],'repeatability_gate':gate},f,indent=2)
        print('Load female tutorial, arm: '+str(arm)+'\nOutput: '+str(output)+'\nStop: '+str(stop),flush=True)
        code=subprocess.run(command,cwd=game,env=env).returncode
        with output.with_suffix('.exit.json').open('x') as f:json.dump({'returncode':code,'signal':-code if code<0 else None,'observed_utc':datetime.now(timezone.utc).isoformat()},f,indent=2)
        return code if code>=0 else 128-code
    except (OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError) as e:p.error(str(e))


if __name__=='__main__':sys.exit(main())
