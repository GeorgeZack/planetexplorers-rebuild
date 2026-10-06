#!/usr/bin/env python3
"""Prepare three guarded clone replays; offline by default, never attach a live controller.

The source capture must be closed and pass full pose validation. --launch starts
an isolated original only; its operator enters a female tutorial and arms it.
"""
import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import launch_reference
import reference_pose
from analyze_shield_pose import report, classify
from analyze_reference_pose import decode, MAX_LINE
from export_shield_controller import check, CONTROLLER_ID
from inventory_shield_layers import sha

TOOLS=Path(__file__).resolve().parent
DEFAULT_SAMPLES=(1611,1686,2157)


def frame_plan(row,types):
    evidence=classify(row)
    if (not evidence['steady_sword_block'] or evidence['base_transition'] or evidence['other_active_layers']
            or evidence['base_state']!=2858398145):
        raise ValueError('Replay needs observed steady block and Base Move, with no active other layer')
    a=next(a for a in row['animators'] if a['id']==evidence['animator_id'])
    if a['parameters']['FirstPerson']!=0:raise ValueError('Third-person evidence required')
    if set(a['parameters'])!=set(types):raise ValueError('Observed/controller parameters differ')
    parameters=[]
    for name,kind in types.items():
        value=a['parameters'][name]
        if kind==9:
            if value is not None:raise ValueError('Trigger value must be unobserved/null')
            continue
        if kind==4:
            if type(value) is not bool:raise ValueError('Invalid boolean')
        elif kind in (1,3):
            if type(value) not in (int,float) or not math.isfinite(value):raise ValueError('Invalid number')
            if kind==3 and (int(value)!=value or not -2147483648<=value<2147483648):raise ValueError('Invalid integer')
        else:raise ValueError('Unknown controller parameter type')
        parameters.append({'name':name,'type':kind,'value':value})
    prefix=a['path'];bones=[]
    for t in row['transforms']:
        if t['path']==prefix or t['path'].startswith(prefix+'/'):
            bones.append({'path':t['path'][len(prefix):].lstrip('/'), 'local':t['local'],
                          'world':t['world']})
    if not 2<=len(bones)<=512:raise ValueError('Hierarchy bound')
    return {'sample':row['sample'],'source_frame':row['frame'],'evidence':evidence,
            'parameters':parameters,'layers':a['layers'],'root_inverse_world':a['rootInverseWorld'],
            'bones':bones,'human_paths':{name:next(t['path'][len(prefix):].lstrip('/') for t in row['transforms'] if t['id']==identity)
                                        for name,identity in a.get('humanBones',{}).items() if identity!=0},
            'live_ik_included':True}


def clip_allowlist(controller,objects):
    ids=sorted({p['m_PathID'] for p in controller['m_AnimationClips']})
    if len(ids)!=452:raise ValueError('Source clip identity count differs')
    result=[]
    for identity in ids:
        obj=objects[identity];clip=obj.read_typetree();muscle=clip['m_MuscleClip']
        length=muscle['m_StopTime']-muscle['m_StartTime'];rate=clip['m_SampleRate']
        if (obj.type.name!='AnimationClip' or clip['m_Legacy'] or not clip['m_Name'] or
                not math.isfinite(length) or not 0<length<=120 or rate!=30.):
            raise ValueError('Unexpected source clip metadata')
        result.append({'source_id':identity,'name':clip['m_Name'],'length':length,'rate':rate,
                       'raw_sha256':__import__('hashlib').sha256(obj.get_raw_data()).hexdigest()})
    return result


def prepare(capture,rig,source_plan,bundle_dir,build_dir):
    import UnityPy
    if build_dir.exists():raise ValueError('Use a fresh build directory; never rebuild an in-process DLL')
    manifest=json.loads((bundle_dir/'manifest.json').read_text());game=Path(manifest['source_asset']).parents[1]
    bundle=check(game,bundle_dir)
    if not bundle.get('ik_disabled'):raise ValueError('Smoke requires explicit two-flag IK-disabled clone bundle')
    evidence=report(capture,rig,source_plan)
    if evidence['source_asset_sha256']!=bundle['source_asset_sha256']:raise ValueError('Source identities differ')
    objects=UnityPy.load(str(game/'PE_Client_Data/sharedassets1.assets')).file.objects
    c=objects[CONTROLLER_ID].read_typetree();clips=clip_allowlist(c,objects)
    names=dict(c['m_TOS']);types={names[v['m_ID']]:v['m_Type'] for v in c['m_Controller']['m_Values']['data']['m_ValueArray']}
    selected={}
    with capture.open('rb') as stream:
        while line:=stream.readline(MAX_LINE+1):
            row=decode(line)
            if row['type']=='frame' and row['sample'] in DEFAULT_SAMPLES:
                selected[row['sample']]=frame_plan(row,types)
    if set(selected)!=set(DEFAULT_SAMPLES) or sha(capture)!=evidence['capture_sha256']:
        raise ValueError('Required closed-capture examples absent or capture changed')
    plan={'schema':'pe-shield-composition-replay-1','capture':str(capture),'capture_sha256':sha(capture),
          'source_plan':str(source_plan),'rig':str(rig),
          'source_plan_sha256':sha(source_plan),'rig_sha256':sha(rig),'bundle_dir':str(bundle_dir),
          'bundle_sha256':bundle['bundle_sha256'],'source_asset_sha256':bundle['source_asset_sha256'],
          'controller_manifest_sha256':sha(bundle_dir/'manifest.json'),
          'frames':[selected[n] for n in DEFAULT_SAMPLES],'max_frames':3,'clips':clips,
          'ik_disabled':True,'runtime_qualified':False,
          'limits':['Source matrices include live IK; clone disables external IK and Base Move foot IK.',
                    'Independent source Base Move/LArmOnly clocks and all nontrigger parameters are replayed.',
                    'Actual states, clocks and contributions must be read back; requested time is not proof.',
                    'Three points do not qualify a production phase grid or transitions.']}
    build_dir.mkdir(parents=True);path=build_dir/'replay-plan.json';path.write_text(json.dumps(plan,indent=2,allow_nan=False)+'\n')
    source=['using UnityEngine; namespace PeReference { internal static class ShieldReplayPlan {',
            'internal const string BundleHash='+json.dumps(bundle['bundle_sha256'])+';',
            'internal const string PlanHash='+json.dumps(sha(path))+';',
            'internal static readonly int[] Samples={'+','.join(str(n) for n in DEFAULT_SAMPLES)+'};']
    for field,key in [('BaseTimes','base_normalized_time'),('BlockTimes','block_normalized_time')]:
        source.append('internal static readonly float[] '+field+'={'+','.join(format(f['evidence'][key],'.9g')+'f' for f in plan['frames'])+'};')
    source.append('internal static readonly ShieldCompositionClone.Parameter[][] Parameters={')
    kind_names={1:'Float',3:'Int',4:'Bool'}
    for frame in plan['frames']:
        source.append('new ShieldCompositionClone.Parameter[]{')
        for p in frame['parameters']:
            field='boolean='+str(p['value']).lower() if p['type']==4 else 'number='+format(p['value'],'.9g')+'f'
            source.append('new ShieldCompositionClone.Parameter{name='+json.dumps(p['name'])+',type=AnimatorControllerParameterType.'+kind_names[p['type']]+','+field+'},')
        source.append('},')
    source.append('};')
    source.append('internal static readonly ShieldCompositionClone.ClipIdentity[] Clips={')
    for clip in clips:
        source.append('new ShieldCompositionClone.ClipIdentity{sourceId='+str(clip['source_id'])+',name='+json.dumps(clip['name'])+',length='+format(clip['length'],'.9g')+'f,rate='+format(clip['rate'],'.9g')+'f},')
    source.append('}; }}')
    (build_dir/'ShieldReplayPlan.cs').write_text('\n'.join(source)+'\n')
    return plan,game


def build(game,build_dir):
    mono,shim,_=reference_pose.build(game,build_dir)
    managed=game/'PE_Client_Data/Managed';assembly=build_dir/'ReferenceShieldCompositionProbe.dll'
    subprocess.run([str(build_dir/'compiler'),str(mono),str(managed),'-noconfig','-nostdlib','-target:library',
                    *[f'-r:{managed/n}' for n in ('mscorlib.dll','System.Core.dll','UnityEngine.dll')],
                    f'-out:{assembly}',str(TOOLS/'ReferenceShieldComposition.cs'),str(TOOLS/'ReferenceShieldCompositionProbe.cs'),str(TOOLS/'ShieldCaptureNumbers.cs'),
                    str(build_dir/'ShieldReplayPlan.cs')],check=True)
    record={'schema':'pe-shield-composition-build-1','game':str(game),
            'files':{str(path.resolve()):sha(path) for path in
                     (assembly,shim,mono,managed/'UnityEngine.dll',build_dir/'replay-plan.json',build_dir/'ShieldReplayPlan.cs',
                      TOOLS/'ReferenceShieldComposition.cs',TOOLS/'ReferenceShieldCompositionProbe.cs',TOOLS/'ShieldCaptureNumbers.cs',
                      TOOLS/'reference_shield_composition.py',TOOLS/'reference_pose_shim.c')}}
    (build_dir/'prepared-build.json').write_text(json.dumps(record,indent=2)+'\n')
    return mono,shim,assembly


def reuse(build_dir,bundle_dir,capture):
    record=decode((build_dir/'prepared-build.json').read_bytes())
    if record.get('schema')!='pe-shield-composition-build-1':raise ValueError('Wrong prepared-build schema')
    for path,digest in record['files'].items():
        if sha(Path(path))!=digest:raise ValueError('Prepared input changed: '+path)
    plan=decode((build_dir/'replay-plan.json').read_bytes());game=Path(record['game'])
    if plan['capture']!=str(capture) or plan['bundle_dir']!=str(bundle_dir):raise ValueError('Prepared paths differ')
    if sha(capture)!=plan['capture_sha256'] or sha(bundle_dir/'manifest.json')!=plan['controller_manifest_sha256']:
        raise ValueError('Prepared provenance changed')
    for key in ('source_plan','rig'):
        if sha(Path(plan[key]))!=plan[key+'_sha256']:raise ValueError('Prepared '+key+' changed')
    manifest=check(game,bundle_dir)
    if not manifest.get('ik_disabled') or manifest['bundle_sha256']!=plan['bundle_sha256']:raise ValueError('Prepared bundle differs')
    return plan,game,game/'PE_Client_Data/Mono/x86_64/libmono.so',build_dir/'pose-shim.so',build_dir/'ReferenceShieldCompositionProbe.dll'


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture',type=Path,required=True)
    p.add_argument('--rig',type=Path,default=TOOLS.parent/'build/reference-player-starter/player-female-starter-rig.json')
    p.add_argument('--source-plan',type=Path,default=TOOLS.parent/'build/shield-layer/plan.json')
    p.add_argument('--bundle-dir',type=Path,required=True)
    p.add_argument('--build-dir',type=Path,required=True)
    p.add_argument('--user-data',type=Path,default=Path.home()/'.local/share/planet-explorers-shield-composition-reference')
    p.add_argument('--output',type=Path)
    p.add_argument('--single-threaded-rendering',action='store_true')
    p.add_argument('--launch',action='store_true')
    p.add_argument('--reuse-prepared',action='store_true',help='read-only verify prepared hashes and reuse DLL, never compile')
    a=p.parse_args(argv)
    try:
        capture=a.capture.resolve();folder=a.build_dir.resolve();bundle_dir=a.bundle_dir.resolve()
        if a.reuse_prepared:
            plan,game,mono,shim,assembly=reuse(folder,bundle_dir,capture)
        else:
            plan,game=prepare(capture,a.rig.resolve(),a.source_plan.resolve(),bundle_dir,folder)
            mono,shim,assembly=build(game,folder)
        if not a.launch:
            print('Offline clone replay prepared: '+str(folder)+'; no game launched.');return 0
        profile=a.user_data.resolve();output,arm,stop,manifest=reference_pose.capture_paths(profile,a.output)
        command,env=launch_reference.launch_command(game,profile,a.single_threaded_rendering)
        if env.get('LD_PRELOAD'):raise ValueError('Use clean LD_PRELOAD environment')
        if any(c.isspace() or c==':' for c in str(shim)):raise ValueError('Invalid shim path')
        env.update(LD_PRELOAD=str(shim),PE_POSE_EXECUTABLE=str(Path(command[0]).resolve()),PE_POSE_MONO=str(mono),
                   PE_POSE_ASSEMBLY=str(assembly),PE_POSE_OUTPUT=str(output),PE_POSE_ARM=str(arm),PE_POSE_STOP=str(stop),
                   PE_SHIELD_BUNDLE=str(bundle_dir/'reference-shield-controller.unity3d'))
        launch_reference.prepare_settings(profile);launch_reference.prepare_unity_preferences(profile/'engine-config')
        output.parent.mkdir(parents=True,exist_ok=True)
        provenance={'schema':'pe-shield-composition-launch-1','capture':str(output),'command':command,'profile':str(profile),
                    'assembly':str(assembly),'assembly_sha256':sha(assembly),'plan':str(folder/'replay-plan.json'),
                    'plan_sha256':sha(folder/'replay-plan.json'),'bundle_sha256':plan['bundle_sha256'],
                    'probe_sha256':sha(TOOLS/'ReferenceShieldCompositionProbe.cs'),
                    'clone_sha256':sha(TOOLS/'ReferenceShieldComposition.cs'),'shim_sha256':sha(shim)}
        with manifest.open('x') as f:json.dump(provenance,f,indent=2)
        print('Load female tutorial, then arm: '+str(arm)+'\nOutput: '+str(output)+'\nStop: '+str(stop),flush=True)
        code=subprocess.run(command,cwd=game,env=env).returncode
        with output.with_suffix('.exit.json').open('x') as f:json.dump({'returncode':code,'signal':-code if code<0 else None,
                     'observed_utc':datetime.now(timezone.utc).isoformat()},f,indent=2)
        return code if code>=0 else 128-code
    except (OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError) as e:p.error(str(e))


if __name__=='__main__':sys.exit(main())
