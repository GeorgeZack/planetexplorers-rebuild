#!/usr/bin/env python3
"""Offline root-channel evidence against the preserved running408 engine oracle.

Does not start Unity, create a motion asset, or qualify ordinary clip playback.
Exact native branch evidence is hashed separately from numerical extrapolation.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

from export_reference_player import packed_channels
from analyze_reference_root_motion import analyze

ROOT = Path(__file__).resolve().parents[2]
ASSET_SHA = 'd9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67'
ELF_SHA = '1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6'
CAPTURE_SHA = 'b6ccd0199470a14b10182943d18db566409a03d27029a65bea657a06e63a43b4'
CLIPS = {231: 'Attack6', 240: 'Attack7', 247: 'Attack8', 254: 'Attack9', 408: 'RunSwordAttack'}
FLAGS = dict(m_LoopTime=False, m_LoopBlend=False, m_LoopBlendOrientation=True,
             m_LoopBlendPositionY=True, m_LoopBlendPositionXZ=False,
             m_KeepOriginalOrientation=True, m_KeepOriginalPositionY=True,
             m_KeepOriginalPositionXZ=False, m_HeightFromFeet=False, m_Mirror=False)
# Named serializer, humanoid root orientation/Y/XZ masks, humanScale getter,
# and the human-scale multiplication before final transform composition.
RANGES = [(0xb99b53, 0xb99be5), (0xc1ffd0, 0xc200da),
          (0xc251d3, 0xc252cd),
          (0xc54680, 0xc546af), (0xc51e60, 0xc51e86),
          (0xbc96f0, 0xbc9721), (0xbae650, 0xbae68d),
          (0xc04470, 0xc044dd)]


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def decode(clip, avatar):
    """Bounded scalar decoder; deliberately rejects other importer branches."""
    m = clip['m_MuscleClip']
    if any(type(m.get(k)) is not bool or m[k] != v for k, v in FLAGS.items()):
        raise ValueError('Unsupported root-extraction flags')
    if any(not finite(m.get(k)) or m[k] != 0 for k in
           ('m_StartTime', 'm_CycleOffset', 'm_OrientationOffsetY', 'm_Level')):
        raise ValueError('Unsupported source offsets')
    if clip.get('m_Legacy') is not False or clip.get('m_Compressed') is not False:
        raise ValueError('Unsupported clip encoding')
    if any(clip.get(k) for k in ('m_PositionCurves', 'm_RotationCurves',
                               'm_CompressedRotationCurves', 'm_ScaleCurves', 'm_PPtrCurves')):
        raise ValueError('Unexpected ordinary transform curves')
    ch = packed_channels(clip, avatar)
    human = [b for b in ch['bindings'] if b['kind'] == 'humanoid_scalar_unretargeted']
    attrs = [b['binding']['attribute'] for b in human]
    if (len(attrs) != 127 or set(attrs) != set(range(7, 134)) or
            any(b['kind'] != 'humanoid_scalar_unretargeted' and
                (b['kind'] != 'direct_transform' or not b['binding']['path']) for b in ch['bindings'])):
        raise ValueError('Require exact Root/goal/muscle binding closure, no Motion channels')
    if (not finite(ch['sample_rate']) or ch['sample_rate'] != 30 or
            not finite(ch['begin_time']) or abs(ch['begin_time']) > 1e-6 or
            not finite(ch['stop_time']) or not 0 < ch['stop_time'] <= 10 or
            not 2 <= ch['frame_count'] <= 302 or
            ch['begin_time']+(ch['frame_count']-1)/ch['sample_rate'] < ch['stop_time']):
        raise ValueError('Unsupported dense clock or missing terminal bracket')
    scale = avatar['m_Avatar']['m_Human']['data']['m_Scale']
    if not finite(scale) or not 0 < scale <= 10:
        raise ValueError('Invalid Avatar human scale')
    roots = {b['binding']['attribute']: [v[0] for v in b['samples']]
             for b in human if b['binding']['attribute'] < 14}
    return dict(name=clip['m_Name'], duration=ch['stop_time'], rate=ch['sample_rate'],
                begin=ch['begin_time'], count=ch['frame_count'], scale=scale, roots=roots)


def scalar(curve, attribute, seconds):
    if not finite(seconds) or not 0 <= seconds <= curve['duration']:
        raise ValueError('Authored time outside clip')
    values = curve['roots'][attribute]
    if len(values) == 1:
        return values[0]
    phase = min(max((seconds-curve['begin'])*curve['rate'], 0), curve['count']-1)
    i = int(phase)
    j = min(i+1, curve['count']-1)
    return values[i]*(1-(phase-i))+values[j]*(phase-i)


def candidate(curve, seconds):
    # Source RootT body input, projected XZ; no raw RootQ rotation is applied.
    # This is NOT a bone/palette-derived trajectory or an integrated seek delta.
    return [(scalar(curve, k, seconds)-scalar(curve, k, 0))*curve['scale'] if k else 0
            for k in (7, 0, 9)]


def compare(curve, samples):
    if len(samples) < 2:
        raise ValueError('Missing sequential samples')
    previous = -1
    errors = []
    for s in samples:
        t, pos = s['playableTime'], s['position']
        if (not finite(t) or t < 0 or t <= previous or len(pos) != 3 or
                any(not finite(x) for x in pos)):
            raise ValueError('Invalid sample clock/position')
        previous = t
        if t <= curve['duration']:
            predicted = candidate(curve, t)
            errors.append((math.dist(predicted, pos), t, predicted, pos))
    if samples[0]['playableTime'] != 0 or samples[-1]['playableTime'] <= curve['duration']:
        raise ValueError('Missing origin or measured terminal crossing')
    maximum = max(errors)
    before, after = next((a, b) for a, b in zip(samples, samples[1:])
                         if a['playableTime'] <= curve['duration'] < b['playableTime'])
    fraction = ((curve['duration']-before['playableTime']) /
                (after['playableTime']-before['playableTime']))
    endpoint = [a+(b-a)*fraction for a, b in zip(before['position'], after['position'])]
    predicted_end = candidate(curve, curve['duration'])
    return dict(compared_samples=len(errors), maximum_error_m=maximum[0],
                at_authored_seconds=maximum[1], candidate_xyz=maximum[2], observed_xyz=maximum[3],
                endpoint_interpolation_error_m=math.dist(predicted_end, endpoint),
                candidate_endpoint_xyz=predicted_end, interpolated_observed_endpoint_xyz=endpoint,
                endpoint_bracket_seconds=[before['playableTime'], after['playableTime']])


def native_evidence(binary):
    if digest(binary) != ELF_SHA:
        raise ValueError('Installed ELF differs from inspected branch evidence')
    raw = Path(binary).read_bytes()
    names = [(0x77b, 0x16fbbfc, 'm_LoopBlendOrientation'),
             (0x77c, 0x16fbc13, 'm_LoopBlendPositionY'),
             (0x77d, 0x16fbc28, 'm_LoopBlendPositionXZ'),
             (0x77e, 0x16fbc3e, 'm_KeepOriginalOrientation'),
             (0x77f, 0x16fbc58, 'm_KeepOriginalPositionY'),
             (0x780, 0x16fbc70, 'm_KeepOriginalPositionXZ')]
    for _, address, name in names:
        if raw[address-0x400000:address-0x400000+len(name)+1] != name.encode()+b'\0':
            raise ValueError('Native serialized field-name evidence differs')
    return dict(elf_sha256=ELF_SHA,
                ranges=[dict(begin=hex(a), end_exclusive=hex(b),
                             sha256=hashlib.sha256(raw[a-0x400000:b-0x400000]).hexdigest()) for a, b in RANGES],
                clip_flag_offsets={name: hex(offset) for offset, _, name in names},
                y_selection_mask=list(struct.unpack_from('<4I', raw, 0x16fc7b0-0x400000)),
                root_translation_indices=[dict(attribute=i, index_offset=hex(0x4f8+4*i)) for i in (7,8,9)],
                evidence='c1ffd0 samples RootT attributes7–9 through c2c470 and packs translation. Named serialized flags; humanoid branch copies baseline root quaternion and Y, preserves XZ. Animator humanScale resolves Human+0x1c0; c04492 loads this value and c044be multiplies a translation vector before c01b60 transform composition.',
                remaining_static_link='Final root-delta composition call-chain and transform basis have not been independently replayed; numerical comparison below covers the actual408 identity-clone case.')


def verify(game, capture):
    asset = game/'PE_Client_Data/sharedassets1.assets'
    if digest(asset) != ASSET_SHA or digest(capture) != CAPTURE_SHA:
        raise ValueError('Require pinned installed asset and complete original root06 oracle')
    native = native_evidence(game/'PE_Client.x86_64')
    oracle = analyze(capture, require_motion=True)
    if not oracle.get('clone_motion_qualified') or not oracle.get('all_authored_gaps_within_60hz'):
        raise ValueError('Root oracle did not pass unchanged original qualification')
    import UnityPy
    env = UnityPy.load(str(asset))
    file = next(iter(env.files.values()))
    avatar = file.objects[777].read_typetree()
    if avatar['m_Name'] != 'FemaleAvatar':
        raise ValueError('Avatar identity changed')
    curves, clips = {}, []
    for id, name in CLIPS.items():
        clip = file.objects[id].read_typetree()
        if clip['m_Name'] != name:
            raise ValueError('Clip identity changed')
        curve = curves[id] = decode(clip, avatar)
        events = [e['time'] for e in clip['m_Events'] if e['data'] == ('EndAction' if id == 408 else 'TP_EndAction')]
        if len(events) != 1:
            raise ValueError('Expected one source action-end event')
        clips.append(dict(id=id, name=name, duration=curve['duration'], dense_frames=curve['count'],
                          rate=curve['rate'], begin=curve['begin'], flags=FLAGS,
                          end_action=events[0], candidate_at_action_end=candidate(curve, events[0]),
                          candidate_at_clip_end=candidate(curve, curve['duration']),
                          raw_root_ranges={k: [min(v), max(v)] for k, v in curve['roots'].items()}))
    rows = [json.loads(line) for line in Path(capture).read_text().splitlines()]
    comparisons = [dict(trial=i, **compare(curves[408], [r for r in rows if r.get('type') == 'sample' and r['trial'] == i])) for i in (0, 1)]
    return dict(schema='pe-player-root-channel-verification-1', source_asset=str(asset),
                asset_sha256=ASSET_SHA, capture=str(capture), capture_sha256=CAPTURE_SHA,
                verifier_sha256=digest(__file__),
                dependency_sha256={name: digest(Path(__file__).with_name(name)) for name in
                    ('export_reference_player.py', 'analyze_reference_root_motion.py')}, native=native, avatar_id=777,
                human_scale=curves[408]['scale'], clips=clips, running_comparisons=comparisons,
                running_oracle_qualified=True, ordinary_clips_observed_sequentially=False,
                ordinary_runtime_motion_qualified=False,
                formula='(linearly_sampled_RootT.xz(t)-RootT.xz(0))*Avatar.Human.m_Scale; Y=0',
                scope='All four ordinary clips pass the exact bounded source-data predicate. Transfer of408 numerical agreement to them remains explicitly inferred until the complete extraction path is proven; this report creates no runtime asset.',
                limitations=['No layer/state-transition/root blend or source motor/collision evaluation.',
                             'Double-precision dense interpolation is diagnostic, not bit-identical Unity arithmetic.',
                             'Raw RootQ is a humanoid body input. Same sampled root quaternion is selected by bake-orientation branch; do not apply body yaw a second time.',
                             'No motion is inferred from root-removed meshes or paused-seek deltaPosition.'])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--capture', type=Path, default=Path.home()/'.local/share/planet-explorers-strafe-reference/pose-captures/female-running-root-06.jsonl')
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    game, output = args.game_dir.resolve(), args.output.resolve()
    if output.exists() or output == game or game in output.parents or ROOT/'rebuild/build/reference-assets' in output.parents:
        p.error('Output must be fresh and outside installed game and runtime assets')
    try:
        report = verify(game, args.capture.resolve())
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    except (OSError, ValueError, KeyError, ImportError) as e:
        p.error(str(e))
    print(output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
