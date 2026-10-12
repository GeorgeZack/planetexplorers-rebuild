#!/usr/bin/env python3
"""Build an optional isolated original-Unity clip sampler; default never launches.

With --launch, use a fresh dedicated profile and launch-only probe shim. Enter
the tutorial (or load an archived scene with --source-entity), then create the
printed .arm file. The probe reads the selected live entity's Avatar/clip references but builds only its own Transform+Animator graph.
No original files, live Animator values, game save data or input are modified.
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
from inventory_reference_bake import CLIPS, SWORD_CLIPS, SWORD_BACKWARD_CLIPS, DIRECTIONAL_CLIPS, REACTION_CLIPS

TOOLS = Path(__file__).resolve().parent
BUILD = TOOLS.parent/'build/animator-bake'
MAX_CLIPS = 32
# NormalIdle first as the repeatability anchor shared with earlier bakes.
REACTION_SELECTION = ('NormalIdle',)+tuple(REACTION_CLIPS)
DEFAULT_CLIPS = ('NormalIdle', 'NormalWF', 'NormalRF', 'Jump', 'Fall', 'Land', 'Attack6', 'Attack7', 'Attack8', 'Attack9')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_plan(path, game_dir, sex, selected, rate):
    if path.stat().st_size > 1024*1024:
        raise ValueError('Clip plan too large')
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError('Duplicate clip-plan key')
            result[key] = value
        return result
    plan = json.loads(path.read_text(), object_pairs_hook=pairs)
    if not isinstance(plan, dict):
        raise ValueError('Clip plan must be an object')
    asset = game_dir/'PE_Client_Data/sharedassets1.assets'
    if plan.get('schema') != 1 or Path(plan['asset']).resolve() != asset.resolve() or plan['asset_sha256'] != digest(asset):
        raise ValueError('Clip plan does not match installed source asset; regenerate inventory')
    if sex not in ('female', 'male') or not 1 <= rate <= 60:
        raise ValueError('Unsupported gender or sampling rate')
    if not 1 <= len(selected) <= MAX_CLIPS or len(set(selected)) != len(selected) or any(n not in CLIPS for n in selected):
        raise ValueError('Clip selection must be a unique subset of the verified allowlist')
    gender = plan['genders'][sex]
    if gender['avatar'] != ('FemaleAvatar' if sex == 'female' else 'MaleAvatar') or gender['controller'] != ('FemaleAnimator2' if sex == 'female' else 'MaleAnimator'):
        raise ValueError('Unexpected Avatar/controller identity')
    clips = gender['clips']
    if not isinstance(clips, list) or any(not isinstance(c, dict) for c in clips):
        raise ValueError('Clip identities must be objects')
    if len(clips) > len(CLIPS) or len({c['name'] for c in clips}) != len(clips) or any(c['name'] not in CLIPS for c in clips):
        raise ValueError('Duplicate/excess clip identities')
    found = {c['name']: c for c in clips}
    result = []
    for name in selected:
        c = found[name]
        if type(c['path_id']) is not int or c['path_id'] <= 0:
            raise ValueError('Invalid source clip ID')
        if sex=='female' and name in SWORD_BACKWARD_CLIPS and c['path_id']!=SWORD_BACKWARD_CLIPS[name]:
            raise ValueError('Sword backward source identity differs')
        if type(c['loop']) is not bool or type(c['humanoid_bindings']) is not int or not 1 <= c['humanoid_bindings'] <= 512:
            raise ValueError('Missing verified source humanoid/loop metadata; regenerate inventory')
        if any(type(c[k]) not in (int, float) or not math.isfinite(c[k]) or not 0 < c[k] <= bound for k, bound in [('length', 10), ('frame_rate', 120)]):
            raise ValueError('Invalid source clip timing')
        result.append(c)
    count = sum(math.ceil(c['length']*rate)+1 for c in result)
    if count > 3600:
        raise ValueError('Clip sampling exceeds 3,600 samples')
    spec = ';'.join(f"{c['name']}|{c['length']:.9g}|{c['frame_rate']:.9g}|{c['path_id']}|{int(c['loop'])}" for c in result)
    return {'avatar': gender['avatar'], 'controller': gender['controller'], 'gender': sex.title(),
            'sample_rate': rate, 'planned_samples': count, 'clips': result, 'spec': spec,
            'asset': str(asset), 'asset_sha256': plan['asset_sha256']}


def build(game_dir, build_dir=BUILD):
    # Separate directory: never replaces the installed observer/probe output.
    mono, shim, unused_observer = reference_pose.build(game_dir, build_dir)
    assembly = build_dir/'ReferenceAnimatorBake.dll'
    managed = game_dir/'PE_Client_Data/Managed'
    subprocess.run([str(build_dir/'compiler'), str(mono), str(managed), '-noconfig', '-nostdlib',
                    '-target:library', f'-r:{managed/"mscorlib.dll"}', f'-r:{managed/"System.Core.dll"}', f'-r:{managed/"UnityEngine.dll"}',
                    f'-out:{assembly}', str(TOOLS/'ReferenceAnimatorBake.cs')], check=True)
    return mono, shim, assembly.resolve()


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--user-data', type=Path, default=Path.home()/'.local/share/planet-explorers-bake-reference')
    p.add_argument('--plan', type=Path, default=BUILD/'clip-plan.json')
    p.add_argument('--sex', choices=('female', 'male'), default='female')
    p.add_argument('--source-entity', type=int, help='read one loaded entity instead of the main player (1..1000000)')
    p.add_argument('--preset', choices=('default','torch-sword','sword-backward','normal-directional','hit-reaction'), default='default', help='bounded clip group; default remains the original ten Normal/attack clips')
    p.add_argument('--clip', choices=tuple(CLIPS), action='append', dest='clips')
    p.add_argument('--fps', type=int, choices=range(1,61), default=30, metavar='1..60')
    p.add_argument('--seconds', type=int, choices=range(1,601), default=180, metavar='1..600')
    p.add_argument('--wait-seconds', type=int, choices=range(1,1801), default=900, metavar='1..1800')
    p.add_argument('--output', type=Path, help='fresh JSONL path within user-data')
    p.add_argument('--single-threaded-rendering', action='store_true', help='opt-in original -force-gfx-direct launch workaround')
    p.add_argument('--launch', action='store_true', help='explicitly launch original; otherwise build only')
    a = p.parse_args(argv)
    try:
        if a.source_entity is not None and not 1 <= a.source_entity <= 1000000:
            raise ValueError('Source entity outside bound')
        game = a.game_dir.expanduser().resolve(); profile = a.user_data.expanduser().resolve()
        plan_path = a.plan.expanduser().resolve()
        if a.preset!='default' and a.clips:
            raise ValueError('Choose either a preset or explicit clips')
        selected=a.clips or (tuple(SWORD_CLIPS) if a.preset=='torch-sword' else tuple(SWORD_BACKWARD_CLIPS) if a.preset=='sword-backward' else DEFAULT_CLIPS+tuple(DIRECTIONAL_CLIPS) if a.preset=='normal-directional' else REACTION_SELECTION if a.preset=='hit-reaction' else DEFAULT_CLIPS)
        plan = read_plan(plan_path, game, a.sex, selected, a.fps)
        if a.source_entity is not None: plan['source_entity_id'] = a.source_entity
        output, arm, stop, manifest = reference_pose.capture_paths(profile, a.output.expanduser() if a.output else None)
        exit_record = output.with_suffix('.exit.json')
        if exit_record.exists() or exit_record.is_symlink():
            raise ValueError('Exit record already exists; choose a fresh capture path')
        command, env = launch_reference.launch_command(game, profile, a.single_threaded_rendering)
        mono, shim, assembly = build(game)
        if not a.launch:
            print(f'Offline bake probe built: {assembly}\nPlan: {len(plan["clips"])} clips, {plan["planned_samples"]} samples at {a.fps} Hz.\nNo game launched or profile settings changed.')
            return 0
        if env.get('LD_PRELOAD'):
            raise ValueError('Existing LD_PRELOAD is set; use a clean launch environment')
        if any(c.isspace() or c == ':' for c in str(shim)):
            raise ValueError('Shim path cannot contain whitespace or colon')
        env.update(LD_PRELOAD=str(shim), PE_POSE_EXECUTABLE=str(Path(command[0]).resolve()), PE_POSE_MONO=str(mono),
                   PE_POSE_ASSEMBLY=str(assembly), PE_POSE_OUTPUT=str(output), PE_POSE_ARM=str(arm), PE_POSE_STOP=str(stop),
                   PE_POSE_WAIT=str(a.wait_seconds), PE_BAKE_SECONDS=str(a.seconds), PE_BAKE_RATE=str(a.fps),
                   PE_BAKE_AVATAR=plan['avatar'], PE_BAKE_CONTROLLER=plan['controller'], PE_BAKE_GENDER=plan['gender'], PE_BAKE_CLIPS=plan['spec'])
        # Never inherit an unrelated source override into the default player path.
        env.pop('PE_BAKE_SOURCE_ENTITY',None)
        if a.source_entity is not None: env['PE_BAKE_SOURCE_ENTITY'] = str(a.source_entity)
        launch_reference.prepare_settings(profile)
        launch_reference.prepare_unity_preferences(profile/'engine-config')
        output.parent.mkdir(parents=True, exist_ok=True)
        with manifest.open('x') as stream:
            json.dump({'schema': 'pe-animator-bake-launch-1', 'command': command, 'profile': str(profile),
                       'capture': str(output), 'arm': str(arm), 'stop': str(stop), 'seconds': a.seconds,
                       'wait_seconds': a.wait_seconds, 'plan': plan, 'plan_sha256': digest(plan_path),
                       'assembly': str(assembly), 'assembly_sha256': digest(assembly), 'shim': str(shim),
                       'probe_source_sha256': digest(TOOLS/'ReferenceAnimatorBake.cs'),
                       'unity_engine_sha256': digest(game/'PE_Client_Data/Managed/UnityEngine.dll')}, stream, indent=2)
        print(f'Isolated profile: {profile}\nBake output: {output}\nLoad a scene with the selected {a.sex} source; create to arm: {arm}\nCreate to stop: {stop}', flush=True)
        code = subprocess.run(command, cwd=game, env=env).returncode
        # A distinct record preserves raw negative signal codes; never rewrite
        # source launch provenance or infer process success from a bake end.
        with exit_record.open('x') as stream:
            json.dump({'schema': 'pe-animator-bake-exit-1', 'capture': str(output),
                       'returncode': code, 'signal': -code if code < 0 else None,
                       'observed_utc': datetime.now(timezone.utc).isoformat()}, stream, indent=2)
            stream.write('\n')
        return code if code >= 0 else 128-code
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as e:
        print(f'Animator bake: {e}', file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == '__main__':
    sys.exit(main())
