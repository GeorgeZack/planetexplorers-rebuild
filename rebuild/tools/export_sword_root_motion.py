#!/usr/bin/env python3
"""Export the bounded female ordinary sword root profile without starting Unity.

PESR1 contains source dense knots, not captured or resampled engine positions.
All four curves form one immutable group; PERM1 running motion is unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import verify_player_root_channels as evidence

IDS = (231, 240, 247, 254)
COUNTS = (34, 35, 40, 31)
DURATIONS = (1.0666667222976685, 1.100000262260437, 1.2666668891906738, .9666671752929688)
BEGINS = (-1.4901161193847656e-8, 0., 0., 0.)
SCALE = .9932681322097778


def encode(curves):
    if tuple(curves) != IDS:
        raise ValueError('Require complete source-ordered ordinary group')
    out = bytearray(struct.pack('<4sIIId', b'PESR', 1, 777, 4, SCALE))
    for index, ident in enumerate(IDS):
        c = curves[ident]
        if (c['count'] != COUNTS[index] or c['duration'] != DURATIONS[index] or
                c['begin'] != BEGINS[index] or c['rate'] != 30 or c['scale'] != SCALE or
                c['name'] != evidence.CLIPS[ident]):
            raise ValueError('Unsupported ordinary source profile')
        x, z = c['roots'][7], c['roots'][9]
        if len(x) != c['count'] or len(z) != c['count']:
            raise ValueError('Require dense XZ knots, no inferred samples')
        out += struct.pack('<IIddd', ident, c['count'], c['duration'], c['begin'], c['rate'])
        for a, b in zip(x, z):
            if not evidence.finite(a) or not evidence.finite(b) or abs(a) > 10000 or abs(b) > 10000:
                raise ValueError('Invalid source translation')
            out += struct.pack('<dd', a, b)
    return bytes(out)


def build_outputs(game, capture):
    proof = evidence.verify(game, capture)
    if proof.get('ordinary_source_extraction_qualified') is not True:
        raise ValueError('Ordinary source extraction proof is incomplete')
    import UnityPy
    asset = game/'PE_Client_Data/sharedassets1.assets'
    # The source pin is checked again before serializing the selected records.
    if evidence.digest(asset) != evidence.ASSET_SHA:
        raise ValueError('Source asset changed during export')
    file = next(iter(UnityPy.load(str(asset)).files.values()))
    avatar = file.objects[777].read_typetree()
    curves = {ident: evidence.decode(file.objects[ident].read_typetree(), avatar) for ident in IDS}
    data = encode(curves)
    report = dict(schema=1, format='PESR1', derivation='source dense RootT XZ knots; per-clip origin subtraction and Avatar human scale at query time',
                  source_extraction=proof, exporter_sha256=evidence.digest(__file__),
                  output=dict(file='sword-root.pesr', bytes=len(data), sha256=hashlib.sha256(data).hexdigest()),
                  runtime=dict(avatar=777, gender='female', view='third-person', source_clips=list(IDS), actor_scale=1,
                               no_root_y=True, no_root_rotation=True, no_layer_transition_blend=True),
                  limitations='Source-derived unblended curves, not observed ordinary gameplay; double interpolation and native fixed-step collision remain adapters. No captured PERM1 data is replaced.')
    return {'sword-root.pesr': data, 'sword-root.json': (json.dumps(report, indent=2, allow_nan=False)+'\n').encode()}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--capture', type=Path, default=Path.home()/'.local/share/planet-explorers-strafe-reference/pose-captures/female-running-root-06.jsonl')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--check', action='store_true')
    a = p.parse_args(argv)
    try:
        output = a.output.resolve()
        if not output.is_relative_to(evidence.ROOT/'rebuild/build'):
            raise ValueError('Recovered motion outputs belong under rebuild/build')
        values = build_outputs(a.game_dir.resolve(), a.capture.resolve())
        if a.check:
            for name, data in values.items():
                if not (output/name).is_file() or (output/name).read_bytes() != data:
                    raise ValueError('Stale or missing '+name)
        else:
            if output.exists():
                raise ValueError('Use a fresh output directory or --check')
            output.mkdir(parents=True)
            for name, data in values.items():
                with (output/name).open('xb') as f:
                    f.write(data)
    except (OSError, ValueError, KeyError, ImportError) as error:
        print(error, file=sys.stderr)
        return 1
    print('Ordinary source motion '+('checked' if a.check else 'exported'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
