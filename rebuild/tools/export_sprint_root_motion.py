#!/usr/bin/env python3
"""Stage clip401 source root knots using the existing bounded native proof.

No Unity launch or observed sprint playback. This one-record PESR1 is a separate
staging group; the ordinary four-record runtime loader deliberately rejects it.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import verify_player_root_channels as evidence

IDENT = 401
NAME = 'SprintSwordAttack'
COUNT = 56
DURATION = 1.8000068664550781
SCALE = .9932681322097778
END_ACTION = 1.1924036741256714


def encode(curve):
    expected = dict(name=NAME, count=COUNT, duration=DURATION, begin=0., rate=30., scale=SCALE)
    if any(type(curve.get(k)) is bool or curve.get(k) != v for k, v in expected.items()):
        raise ValueError('Unsupported sprint source profile')
    x, z = curve['roots'][7], curve['roots'][9]
    if len(x) != COUNT or len(z) != COUNT:
        raise ValueError('Require complete dense XZ knots')
    out = bytearray(struct.pack('<4sIIIdIIddd', b'PESR', 1, 777, 1, SCALE,
                                IDENT, COUNT, DURATION, 0., 30.))
    for a, b in zip(x, z):
        if any(not evidence.finite(v) or abs(v) > 10000 for v in (a, b)):
            raise ValueError('Invalid source translation')
        out += struct.pack('<dd', a, b)
    return bytes(out)


def qualify(clip, avatar, binary):
    if clip['m_Name'] != NAME or avatar['m_Name'] != 'FemaleAvatar':
        raise ValueError('Sprint clip/avatar identity changed')
    curve = evidence.decode(clip, avatar)
    ends = [e['time'] for e in clip['m_Events']
            if e['functionName'] == 'AnimEvent' and e['data'] == 'EndAction']
    if ends != [END_ACTION]:
        raise ValueError('Sprint EndAction identity changed')
    data = encode(curve)
    q = [curve['start_transform']['q'][k] for k in 'xyzw']
    arithmetic = evidence.replay_arithmetic(binary, [q])
    if len(arithmetic['cases']) != 1 or arithmetic['cases'][0]['start_q'] != q:
        raise ValueError('Missing sprint starting-basis replay')
    return data, dict(id=IDENT, name=NAME, duration=DURATION, dense_frames=COUNT,
        rate=30., begin=0., flags=evidence.FLAGS,
        start_transform=curve['start_transform'],
        root_scalar_indices=curve['root_scalar_indices'], end_action=END_ACTION,
        candidate_at_action_end=evidence.candidate(curve, END_ACTION),
        candidate_at_clip_end=evidence.candidate(curve, DURATION),
        isolated_arithmetic=arithmetic)


def build_outputs(game, capture):
    base = evidence.verify(game, capture)
    if (base.get('ordinary_source_extraction_qualified') is not True or
            base.get('running_oracle_qualified') is not True):
        raise ValueError('Existing root proof is incomplete')
    import UnityPy
    asset = game/'PE_Client_Data/sharedassets1.assets'
    if evidence.digest(asset) != evidence.ASSET_SHA:
        raise ValueError('Source asset changed during export')
    file = next(iter(UnityPy.load(str(asset)).files.values()))
    data, clip = qualify(file.objects[IDENT].read_typetree(),
                         file.objects[777].read_typetree(), game/'PE_Client.x86_64')
    report = dict(schema='pe-sprint-root-source-1', format='PESR1',
        sprint_source_extraction_qualified=True, sprint_observed_sequentially=False,
        sprint_runtime_motion_qualified=False, base_source_extraction=base,
        exporter_sha256=evidence.digest(__file__), clip=clip,
        formula=base['formula'],
        output=dict(file='sprint-root.pesr', bytes=len(data),
                    sha256=hashlib.sha256(data).hexdigest()),
        scope='Single unblended female humanoid clip401, same exact bounded importer/channel predicate as the running408 oracle; its starting quaternion is independently replayed through the pinned native arithmetic.',
        limitations=['No observed sprint root playback, transition/layer mixing or motor/collision qualification.',
                     'Dense double interpolation and native fixed-step integration remain adapters.',
                     'Separate one-record staging group, rejected by the four-record ordinary runtime loader; no host admission or installation.'])
    return {'sprint-root.pesr': data,
            'sprint-root.json': (json.dumps(report, indent=2, allow_nan=False)+'\n').encode()}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    parser.add_argument('--capture', type=Path, default=Path.home()/'.local/share/planet-explorers-strafe-reference/pose-captures/female-running-root-06.jsonl')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        if not output.is_relative_to(evidence.ROOT/'rebuild/build'):
            raise ValueError('Recovered motion outputs belong under rebuild/build')
        values = build_outputs(args.game_dir.resolve(), args.capture.resolve())
        if args.check:
            for name, data in values.items():
                if not (output/name).is_file() or (output/name).read_bytes() != data:
                    raise ValueError('Stale or missing '+name)
        else:
            if output.exists():
                raise ValueError('Use a fresh output directory or --check')
            output.mkdir(parents=True)
            for name, data in values.items():
                with (output/name).open('xb') as stream:
                    stream.write(data)
    except (OSError, ValueError, KeyError, ImportError) as error:
        print(error, file=sys.stderr)
        return 1
    print('Sprint source motion '+('checked' if args.check else 'staged'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
