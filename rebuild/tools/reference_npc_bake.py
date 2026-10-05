#!/usr/bin/env python3
"""Build a separate bounded nine-NPC idle sampler; launch only with --launch."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import launch_reference
import reference_pose

TOOLS=Path(__file__).resolve().parent
BUILD=TOOLS.parent/'build/npc-bake'
ROSTER={9004:'Adisa_Mostafa',9005:'Moric_Gahn',9006:'Vera_Novikova',9009:'Allen_Carter',9020:'Anjum_Nahas',9031:'Viola_Strand',9041:'Andy',9047:'Percy_Malik',9049:'Malus_Tepes'}

def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def read_plan(path,game,rate):
    if path.stat().st_size>1024*1024:raise ValueError('NPC plan too large')
    def pairs(items):
        result={}
        for k,v in items:
            if k in result:raise ValueError('Duplicate NPC plan key')
            result[k]=v
        return result
    data=json.loads(path.read_text(),object_pairs_hook=pairs)
    if not isinstance(data,dict) or data.get('schema')!=1 or not isinstance(data.get('npcs'),list) or len(data['npcs'])!=9:raise ValueError('Expected exact nine-NPC schema1 plan')
    if type(rate)is not int or not 1<=rate<=60:raise ValueError('Invalid sample rate')
    seen=set();rows=[];spec=[]
    for row in data['npcs']:
        npc=row['npc_id']
        if type(npc)is not int or npc not in ROSTER or npc in seen or row['model']!=ROSTER[npc]:raise ValueError('Unknown/duplicate NPC identity')
        seen.add(npc)
        if row['avatar']!=row['model']+'Avatar' or row['gender'] not in ('Female','Male'):raise ValueError('Invalid NPC Avatar/gender')
        expected_gender='Female' if npc in (9004,9006,9031) else 'Male'
        expected_controller='FemaleAnimator2' if expected_gender=='Female' else 'AllenCarterAnimator' if npc==9009 else 'MaleAnimator'
        if row['gender']!=expected_gender or row['controller']!=expected_controller:raise ValueError('Unexpected NPC controller/gender')
        for key in ('asset','clip_asset'):
            file=Path(row.get(key,row['asset'])).resolve()
            if not file.is_relative_to(game) or not file.is_file():raise ValueError('NPC asset outside requested installation')
            if digest(file)!=row.get(key+'_sha256',row['asset_sha256']):raise ValueError('NPC source asset changed; regenerate plan')
        clip=row['clip']
        if clip['name']!='NormalIdle' or type(clip['path_id'])is not int or not -(1<<63)<=clip['path_id']<(1<<63) or not clip['path_id']:raise ValueError('Invalid signed source clip identity')
        if type(clip['loop'])is not bool or type(clip['humanoid_bindings'])is not int or not 1<=clip['humanoid_bindings']<=512:raise ValueError('Missing authored clip metadata')
        for key,maximum in [('length',10),('frame_rate',120)]:
            if type(clip[key])not in (int,float) or not math.isfinite(clip[key]) or not 0<clip[key]<=maximum:raise ValueError('Invalid clip timing')
        count=math.ceil(clip['length']*rate)+1
        normalized=dict(row,sample_rate=rate,planned_samples=count,clips=[clip])
        rows.append(normalized)
        spec.append('|'.join(map(str,[npc,row['model'],row['avatar'],row['controller'],row['gender'],format(clip['length'],'.9g'),format(clip['frame_rate'],'.9g'),clip['path_id'],int(clip['loop'])])))
    if sum(row['planned_samples'] for row in rows)>3600:raise ValueError('Aggregate sample limit exceeded')
    return rows,';'.join(spec)

def build(game,build_dir=BUILD):
    mono,shim,_=reference_pose.build(game,build_dir)
    managed=game/'PE_Client_Data/Managed';assembly=build_dir/'ReferenceNpcBake.dll'
    subprocess.run([str(build_dir/'compiler'),str(mono),str(managed),'-noconfig','-nostdlib','-target:library',
                    '-r:'+str(managed/'mscorlib.dll'),'-r:'+str(managed/'System.Core.dll'),'-r:'+str(managed/'UnityEngine.dll'),
                    '-out:'+str(assembly),str(TOOLS/'ReferenceNpcBake.cs')],check=True)
    return mono,shim,assembly.resolve()

def outputs_for(base,rows):
    return [base.with_name(base.stem+'.npc-'+str(r['npc_id'])+'.jsonl') for r in rows]

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--user-data',type=Path,default=Path.home()/'.local/share/planet-explorers-npc-bake-reference')
    p.add_argument('--plan',type=Path,default=TOOLS.parent/'build/reference-assets/npcs/npc-plan.json')
    p.add_argument('--output',type=Path,help='Fresh session basename under profile; each NPC gets .npc-ID.jsonl')
    p.add_argument('--fps',type=int,choices=range(1,61),default=30)
    p.add_argument('--seconds',type=int,choices=range(1,601),default=180)
    p.add_argument('--wait-seconds',type=int,choices=range(1,1801),default=900)
    p.add_argument('--launch',action='store_true')
    a=p.parse_args(argv)
    try:
        game=a.game_dir.expanduser().resolve();profile=a.user_data.expanduser().resolve();plan_path=a.plan.expanduser().resolve()
        rows,spec=read_plan(plan_path,game,a.fps)
        base,arm,stop,manifest=reference_pose.capture_paths(profile,a.output.expanduser() if a.output else None)
        outputs=outputs_for(base,rows);exit_record=base.with_suffix('.exit.json')
        for path in [exit_record]+outputs+[o.with_suffix('.launch.json')for o in outputs]:
            if path.exists() or path.is_symlink():raise ValueError('NPC capture/provenance already exists; choose fresh basename')
        command,env=launch_reference.launch_command(game,profile)
        mono,shim,dll=build(game)
        if not a.launch:
            print(f'NPC sampler built: {dll}; nine clips, {sum(r["planned_samples"] for r in rows)} samples. No game launched or profile modified.')
            return 0
        if env.get('LD_PRELOAD'):raise ValueError('Existing LD_PRELOAD requires clean launch environment')
        if any(c.isspace()or c==':'for c in str(shim)):raise ValueError('Invalid shim path')
        env.update(LD_PRELOAD=str(shim),PE_POSE_EXECUTABLE=str(Path(command[0]).resolve()),PE_POSE_MONO=str(mono),PE_POSE_ASSEMBLY=str(dll),
                   PE_POSE_OUTPUT=str(base),PE_POSE_ARM=str(arm),PE_POSE_STOP=str(stop),PE_POSE_WAIT=str(a.wait_seconds),
                   PE_BAKE_SECONDS=str(a.seconds),PE_BAKE_RATE=str(a.fps),PE_NPC_PLAN=spec)
        launch_reference.prepare_settings(profile);launch_reference.prepare_unity_preferences(profile/'engine-config');base.parent.mkdir(parents=True,exist_ok=True)
        common={'command':command,'profile':str(profile),'arm':str(arm),'stop':str(stop),'plan_sha256':digest(plan_path),
                'assembly':str(dll),'assembly_sha256':digest(dll),'shim':str(shim),'probe_source_sha256':digest(TOOLS/'ReferenceNpcBake.cs'),
                'unity_engine_sha256':digest(game/'PE_Client_Data/Managed/UnityEngine.dll'),'seconds':a.seconds,'wait_seconds':a.wait_seconds}
        with manifest.open('x')as stream:json.dump(dict(common,schema='pe-npc-bake-session-1',captures=list(map(str,outputs))),stream,indent=2)
        for row,output in zip(rows,outputs):
            with output.with_suffix('.launch.json').open('x')as stream:json.dump(dict(common,schema='pe-animator-bake-launch-1',capture=str(output),plan=row),stream,indent=2)
        print('NPC outputs:\n'+'\n'.join(map(str,outputs))+f'\nEnter tutorial; create arm file: {arm}\nStop file: {stop}',flush=True)
        code=subprocess.run(command,cwd=game,env=env).returncode
        with exit_record.open('x')as stream:json.dump({'schema':'pe-npc-bake-exit-1','captures':list(map(str,outputs)),'returncode':code,'signal':-code if code<0 else None,'observed_utc':datetime.now(timezone.utc).isoformat()},stream,indent=2)
        return code if code>=0 else 128-code
    except(OSError,ValueError,TypeError,KeyError,subprocess.CalledProcessError)as e:
        print('NPC bake: '+str(e),file=sys.stderr);return 1
    except KeyboardInterrupt:return 130
if __name__=='__main__':sys.exit(main())
