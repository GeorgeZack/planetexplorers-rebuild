#!/usr/bin/env python3
"""Experimental isolated RunSwordAttack408 sequential root-motion sampler.

--inventory reads installed assets through optional UnityPy. Default compiles
only. --launch explicitly starts the isolated original; no desktop inputs are
issued. Source player is read, only new Transform/Animator clones are advanced.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import launch_reference
import reference_pose

TOOLS = Path(__file__).resolve().parent
BUILD = TOOLS.parent / 'build/root-motion-bake'
SOURCE = TOOLS / 'ReferenceRootMotionBake.cs'
LENGTH = 1.8333396911621094


def digest(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def inventory(asset):
    import UnityPy
    serialized = next(iter(UnityPy.load(str(asset)).files.values()))
    controller = serialized.objects[779].read_typetree()
    clip = serialized.objects[408].read_typetree()
    if controller['m_Name'] != 'FemaleAnimator2' or clip['m_Name'] != 'RunSwordAttack':
        raise ValueError('Installed controller/clip identity differs')
    names = dict(controller['m_TOS']); states = []
    for machine in controller['m_Controller']['m_StateMachineArray']:
        for entry in machine['data']['m_StateConstantArray']:
            s = entry['data']
            if names.get(s['m_NameID']) != 'RunSwordAttack':
                continue
            trees = s['m_BlendTreeConstantArray']
            if len(trees) != 1 or len(trees[0]['data']['m_NodeArray']) != 1:
                raise ValueError('Not a single clip state')
            leaf = trees[0]['data']['m_NodeArray'][0]['data']
            if leaf['m_ChildIndices'] or controller['m_AnimationClips'][leaf['m_ClipID']] != {'m_FileID': 0, 'm_PathID': 408}:
                raise ValueError('Direct leaf changed')
            states.append({'path': names[s['m_FullPathID']], 'speed': s['m_Speed'],
                           'leaf_duration': leaf['m_Duration'], 'transitions': s['m_TransitionConstantArray']})
    muscle = clip['m_MuscleClip']
    bindings = sum(b['classID'] == 95 and b['customType'] == 8 for b in clip['m_ClipBindingConstant']['genericBindings'])
    if len(states) != 1 or states[0]['speed'] != 1 or states[0]['leaf_duration'] != 1 or not bindings:
        raise ValueError('State speed/identity/humanoid bindings differ')
    return {'schema': 'pe-root-motion-plan-1', 'asset': str(asset.resolve()), 'asset_sha256': digest(asset),
            'unity': serialized.unity_version, 'avatar': 'FemaleAvatar', 'controller': 'FemaleAnimator2',
            'controller_path_id': 779, 'skill_id': 20110014, 'source_state': states[0],
            'clip': {'name': clip['m_Name'], 'path_id': 408, 'length': muscle['m_StopTime']-muscle['m_StartTime'],
                     'frame_rate': clip['m_SampleRate'], 'loop': muscle['m_LoopTime'], 'humanoid_bindings': bindings,
                     'events': [{'time': e['time'], 'function': e['functionName'], 'data': e['data']} for e in clip['m_Events']]}}


def unique_pairs(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError('Duplicate JSON key')
        result[key] = value
    return result


def read_plan(path, game):
    if path.stat().st_size > 65536:
        raise ValueError('Plan bound')
    p = json.loads(path.read_text(), object_pairs_hook=unique_pairs)
    asset = game / 'PE_Client_Data/sharedassets1.assets'
    if not isinstance(p, dict) or p.get('schema') != 'pe-root-motion-plan-1':
        raise ValueError('Plan schema differs')
    if Path(p['asset']).resolve() != asset.resolve() or p['asset_sha256'] != digest(asset):
        raise ValueError('Installed asset changed; regenerate plan')
    if (p['avatar'], p['controller'], p['controller_path_id'], p['skill_id']) != ('FemaleAvatar', 'FemaleAnimator2', 779, 20110014):
        raise ValueError('Only female running Knife source supported')
    c = p['clip']
    if c['name'] != 'RunSwordAttack' or type(c['path_id']) is not int or c['path_id'] != 408 or c['loop'] is not False:
        raise ValueError('Wrong clip identity')
    if type(c['length']) not in (float, int) or not math.isfinite(c['length']) or abs(c['length']-LENGTH) > 1e-7 or type(c['frame_rate']) not in (float, int) or c['frame_rate'] != 30:
        raise ValueError('Wrong source timing')
    if type(c['humanoid_bindings']) is not int or not 1 <= c['humanoid_bindings'] <= 512:
        raise ValueError('Humanoid binding bound')
    if p['source_state']['speed'] != 1 or p['source_state']['leaf_duration'] != 1:
        raise ValueError('State speed differs')
    return p


def build(game, directory=BUILD):
    temporary = TOOLS.parent / 'build/compiler-tmp'; temporary.mkdir(parents=True, exist_ok=True)
    os.environ['TMPDIR'] = str(temporary.resolve())
    mono, shim, _ = reference_pose.build(game, directory)
    managed = game / 'PE_Client_Data/Managed'; dll = (directory / 'ReferenceRootMotionBake.dll').resolve()
    subprocess.run([str(directory/'compiler'), str(mono), str(managed), '-noconfig', '-nostdlib', '-target:library',
                    f'-r:{managed/"mscorlib.dll"}', f'-r:{managed/"System.Core.dll"}', f'-r:{managed/"UnityEngine.dll"}',
                    f'-out:{dll}', str(SOURCE)], check=True)
    return mono, shim, dll


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    ap.add_argument('--user-data', type=Path, default=Path.home()/'.local/share/planet-explorers-bake-reference')
    ap.add_argument('--plan', type=Path, default=BUILD/'running-knife-plan.json')
    ap.add_argument('--inventory', action='store_true', help='write source plan offline, needs UnityPy')
    ap.add_argument('--launch', action='store_true')
    sampling = ap.add_mutually_exclusive_group()
    sampling.add_argument('--dense', action='store_true', help='clone speeds0.1/0.05, max60 real seconds per trial; no global clock change')
    sampling.add_argument('--denser', action='store_true', help='clone speeds0.05/0.025, max120 real seconds per trial; no global clock change')
    sampling.add_argument('--ultra-dense', action='store_true', help='clone speeds0.025/0.0125, max180 real seconds per trial; same3600 samples/4MiB bounds, no global clock change')
    ap.add_argument('--single-threaded-rendering', action='store_true')
    ap.add_argument('--wait-seconds', type=int, choices=range(1,1801), default=900, metavar='1..1800')
    ap.add_argument('--output', type=Path)
    a = ap.parse_args(argv)
    try:
        game = a.game_dir.expanduser().resolve(); profile = a.user_data.expanduser().resolve(); plan_path = a.plan.expanduser().resolve()
        if a.inventory:
            if a.launch or game == plan_path or game in plan_path.parents:
                raise ValueError('Inventory is offline and output must be outside installed game')
            plan = inventory(game/'PE_Client_Data/sharedassets1.assets')
            plan_path.parent.mkdir(parents=True, exist_ok=True)
            # Fresh output prevents overwriting provenance for a retained capture.
            with plan_path.open('x') as f:
                json.dump(plan, f, indent=2); f.write('\n')
            print(plan_path); return 0
        plan = read_plan(plan_path, game)
        output, arm, stop, manifest = reference_pose.capture_paths(profile, a.output.expanduser() if a.output else None)
        exit_path = output.with_suffix('.exit.json')
        if exit_path.exists() or exit_path.is_symlink():
            raise ValueError('Exit sidecar already exists')
        command, env = launch_reference.launch_command(game, profile, a.single_threaded_rendering)
        if a.launch and env.get('LD_PRELOAD'):
            raise ValueError('Existing preload refused')
        mono, shim, dll = build(game)
        if not a.launch:
            print(f'Offline sampler built: {dll}\nNo game launched; two fresh frame-driven trials at clone speeds{"0.025/0.0125" if a.ultra_dense else "0.05/0.025" if a.denser else "0.1/0.05" if a.dense else "1/0.5"}; max3,600 samples.'); return 0
        if any(c.isspace() or c == ':' for c in str(shim)):
            raise ValueError('Invalid shim path')
        env.update(LD_PRELOAD=str(shim), PE_POSE_EXECUTABLE=str(Path(command[0]).resolve()), PE_POSE_MONO=str(mono),
                   PE_POSE_ASSEMBLY=str(dll), PE_POSE_OUTPUT=str(output), PE_POSE_ARM=str(arm), PE_POSE_STOP=str(stop),
                   PE_POSE_WAIT=str(a.wait_seconds), PE_ROOT_DENSE='3' if a.ultra_dense else '2' if a.denser else '1' if a.dense else '0', PE_ROOT_LENGTH=format(plan['clip']['length'], '.9g'), PE_ROOT_RATE='30')
        launch_reference.prepare_settings(profile); launch_reference.prepare_unity_preferences(profile/'engine-config')
        output.parent.mkdir(parents=True, exist_ok=True)
        with manifest.open('x') as f:
            json.dump({'schema': 'pe-root-motion-launch-1', 'command': command, 'profile': str(profile), 'capture': str(output),
                       'arm': str(arm), 'stop': str(stop), 'plan': plan, 'sampling_profile': 'ultra-dense' if a.ultra_dense else 'denser' if a.denser else 'dense' if a.dense else 'standard', 'plan_sha256': digest(plan_path),
                       'assembly_sha256': digest(dll), 'probe_source_sha256': digest(SOURCE),
                       'unity_engine_sha256': digest(game/'PE_Client_Data/Managed/UnityEngine.dll')}, f, indent=2)
        print(f'Load female tutorial player; arm: {arm}\nStop: {stop}\nOutput: {output}', flush=True)
        code = subprocess.run(command, cwd=game, env=env).returncode
        with exit_path.open('x') as f:
            json.dump({'schema': 'pe-root-motion-exit-1', 'returncode': code, 'signal': -code if code < 0 else None,
                       'observed_utc': datetime.now(timezone.utc).isoformat()}, f, indent=2)
        return code if code >= 0 else 128-code
    except (OSError, ValueError, KeyError, TypeError, ImportError, subprocess.CalledProcessError) as e:
        print(f'Root-motion bake: {e}', file=sys.stderr); return 1

if __name__ == '__main__':
    sys.exit(main())
