#!/usr/bin/env python3
"""Export strictly qualified female408 clone motion; never install or enable attacks."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import analyze_reference_root_motion as analyzer
import reference_root_motion_bake as reference

ASSET_SHA256 = 'd9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67'
MAX_GAP = 1 / 60
PROFILES = {'standard': [1, .5], 'dense': [.1, .05], 'denser': [.05, .025], 'ultra-dense': [.025, .0125]}


def read_json(path, limit=65536):
    if path.stat().st_size > limit:
        raise ValueError('Provenance size bound')
    data = json.loads(path.read_text(), object_pairs_hook=reference.unique_pairs)
    if not isinstance(data, dict):
        raise ValueError('Provenance object required')
    return data


def provenance(capture, launch_path, plan_path, game, probe_source, probe_assembly):
    plan = reference.read_plan(plan_path, game)
    if (plan.get('asset_sha256') != ASSET_SHA256 or plan.get('unity') != '5.2.4f1'
            or plan['source_state'].get('path') != 'FullBody.Attack.RunSwordAttack'):
        raise ValueError('Exact installed source identity differs')
    launch = read_json(launch_path)
    if (launch.get('schema') != 'pe-root-motion-launch-1' or launch.get('plan') != plan
            or launch.get('plan_sha256') != reference.digest(plan_path)
            or Path(launch.get('capture', '')).resolve() != capture.resolve()):
        raise ValueError('Launch capture/plan provenance differs')
    profile = Path(launch.get('profile', '')).resolve()
    if capture.resolve().parent != profile / 'pose-captures':
        raise ValueError('Capture must belong to declared isolated profile')
    command = launch.get('command')
    if (not isinstance(command, list) or not command or any(not isinstance(x, str) for x in command)
            or Path(command[0]).resolve() != (game / 'PE_Client.x86_64').resolve()):
        raise ValueError('Launch executable provenance differs')
    if command.count('-userpath') != 1 or command.index('-userpath') + 1 >= len(command) or Path(command[command.index('-userpath') + 1]).resolve() != profile:
        raise ValueError('Launch profile command differs')
    hashes = {'probe_source_sha256': reference.digest(probe_source),
              'assembly_sha256': reference.digest(probe_assembly),
              'unity_engine_sha256': reference.digest(game / 'PE_Client_Data/Managed/UnityEngine.dll')}
    if any(launch.get(key) != value for key, value in hashes.items()):
        raise ValueError('Launch source/assembly/engine hashes differ; supply retained exact inputs')
    if launch.get('sampling_profile') not in PROFILES:
        raise ValueError('Unsupported sampling profile')
    return plan, launch


def outputs(capture, launch_path, plan_path, game, probe_source, probe_assembly):
    plan, launch = provenance(capture, launch_path, plan_path, game, probe_source, probe_assembly)
    report = analyzer.analyze(capture, require_motion=True)
    if (report.get('schema') != 2 or not report.get('clone_motion_qualified')
            or not report.get('all_authored_gaps_within_60hz')
            or any(t['maximum_authored_sample_gap'] > MAX_GAP for t in report['trials'])):
        raise ValueError('Requires qualified frame-driven motion with every authored gap <=1/60s')
    raw = capture.read_bytes()
    if hashlib.sha256(raw).hexdigest() != report['capture_sha256']:
        raise ValueError('Capture changed during validation')
    rows = [json.loads(line, object_pairs_hook=reference.unique_pairs) for line in raw.splitlines()]
    speeds = PROFILES[launch['sampling_profile']]
    if rows[0]['playbackSpeeds'] != speeds:
        raise ValueError('Capture speed profile differs from launch')
    # Select the declared slower trial, never combine or resample observations.
    selected = [r for r in rows if r.get('type') == 'sample' and r['trial'] == 1]
    if not 2 <= len(selected) <= 3600 or selected[0]['playableTime'] != 0:
        raise ValueError('Selected trial count/origin differs')
    origin = selected[0]['position']
    samples = []
    binary = bytearray(struct.pack('<4s5Id', b'PERM', 1, 408, 20110014, len(selected), 0, reference.LENGTH))
    for row in selected:
        position = [x - base for x, base in zip(row['position'], origin)]
        if any(abs(x) > 10000 for x in position):
            raise ValueError('Displacement bound')
        binary.extend(struct.pack('<4d', row['playableTime'], *position))
        samples.append({'authored_seconds': row['playableTime'], 'displacement_xyz': position,
                        'raw_sample': row})
    manifest = {'schema': 'pe-running-knife-motion-export-1', 'source_clip': 408,
                'source_skill': 20110014, 'source_controller': 779,
                'source_plan': plan, 'launch': launch, 'qualification': report,
                'input_sha256': {'capture': report['capture_sha256'], 'launch': reference.digest(launch_path),
                                 'plan': reference.digest(plan_path)},
                'selection': {'trial': 1, 'playback_speed': speeds[1], 'samples': len(selected),
                              'authored_duration': reference.LENGTH, 'observed_origin_xyz': origin,
                              'maximum_allowed_authored_gap': MAX_GAP},
                'coordinates': 'Original Unity XYZ, identity clone axes, +Z forward; translation relative to measured time0 root only.',
                'policy': 'Keep raw knots including first end overshoot; sample only [0,source duration], linear interpolation inside observed brackets. No rescaling, endpoint replacement, yaw transform or motor integration.',
                'binary': {'file': 'running-knife.perm', 'sha256': hashlib.sha256(binary).hexdigest(),
                           'bytes': len(binary), 'format': 'PERM1 little endian: magic,version,clip,skill,count,reserved(u32),duration(f64); count*(time,x,y,z)(f64)'},
                'gameplay_motion_validated': False, 'process_lifetime_from_trace_validated': False,
                'samples': samples}
    return {'running-knife.perm': bytes(binary),
            'provenance.json': (json.dumps(manifest, indent=2, allow_nan=False) + '\n').encode()}


def export(capture, launch_path, plan_path, game, probe_source, probe_assembly, output, check=False):
    expected = outputs(capture, launch_path, plan_path, game, probe_source, probe_assembly)
    if check:
        if not output.is_dir() or {p.name for p in output.iterdir()} != set(expected):
            raise ValueError('Export file set differs')
        for name, data in expected.items():
            if (output / name).read_bytes() != data:
                raise ValueError('Export differs: ' + name)
    else:
        output.mkdir(parents=True, exist_ok=False)
        try:
            for name, data in expected.items():
                with (output / name).open('xb') as f:
                    f.write(data)
        except OSError:
            for name in expected:
                path = output / name
                if path.is_file():
                    path.unlink()
            output.rmdir()
            raise
    return expected


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture', type=Path)
    p.add_argument('--launch', type=Path)
    p.add_argument('--plan', type=Path, default=reference.BUILD / 'running-knife-plan.json')
    p.add_argument('--game-dir', type=Path, default=Path.home() / '.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--probe-source', type=Path, default=reference.SOURCE)
    p.add_argument('--probe-assembly', type=Path, default=reference.BUILD / 'ReferenceRootMotionBake.dll')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--check', action='store_true')
    args = p.parse_args()
    try:
        result = export(args.capture, args.launch or args.capture.with_suffix('.launch.json'), args.plan,
                        args.game_dir, args.probe_source, args.probe_assembly, args.output, args.check)
        print(('Checked' if args.check else 'Exported') + f' qualified motion: {len(result["running-knife.perm"])} bytes; no gameplay installation')
    except (OSError, ValueError, KeyError, TypeError, struct.error) as e:
        print('Running Knife motion: ' + str(e), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
