#!/usr/bin/env python3
"""Pin a validated observed six-hare cohort, preserving source agent anchors.

--from-report verifies a report against its unchanged closed raw capture and
writes bounded evidence first. Normal export/--check reads that checked-in
evidence only, so regeneration needs neither Steam nor the external raw file.
No runtime actor, random center, respawn sampling or inferred scale is created.
"""
from read_reference_adventure import source_label
import argparse
import hashlib
import json
import math
from pathlib import Path
from analyze_reference_hunt_encounter import analyze, entity_valid, finite, vector

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CAPTURE = ROOT / 'docs/reference/hunt-encounter-cohort.json'
DEFAULT_OUTPUT = ROOT / 'rebuild/generated'
PROBE = ROOT / 'rebuild/tools/ReferenceHuntEncounterProbe.cs'
DATABASE = ROOT / 'Assets/Resources/DataBase/localData.bytes'
SOURCE_PATHS = [PROBE, DATABASE, ROOT/'Assets/Scripts/Scene/SceneEntityMissionPointMonsterKill.cs',
                ROOT/'Assets/Scripts/Scene/SceneEntityPosAgent.cs', ROOT/'Assets/Scripts/Scene/MonsterEntityCreator.cs',
                ROOT/'Assets/Scripts/Scene/EntityMonsterBeacon.cs', ROOT/'Assets/PeWorld/PeEntityCreator.cs']


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def archive_report(report_path):
    report_bytes = report_path.read_bytes()
    report = json.loads(report_bytes)
    raw = Path(report['capture'])
    verified = analyze(raw)
    if verified != report:
        raise ValueError('Report differs from a fresh complete raw-capture analysis')
    selected = verified['first_complete_six_alive_frame']
    if selected is None:
        raise ValueError('No observed complete six-member cohort')
    raw_bytes = raw.read_bytes()
    if hashlib.sha256(raw_bytes).hexdigest() != verified['capture_sha256']:
        raise ValueError('Raw capture changed during evidence extraction')
    rows = [json.loads(line) for line in raw_bytes.splitlines()]
    first = selected['sample'] + 1
    if rows[first] != selected:
        raise ValueError('Selected frame disagrees with raw capture')
    evidence = dict(schema=1, kind='observed-original-hunt-cohort', mission=9126, target=1518,
                    raw_capture=dict(path=source_label(raw), sha256=verified['capture_sha256'], frames=verified['frames'], end=verified['end']),
                    report=dict(path=source_label(report_path), sha256=hashlib.sha256(report_bytes).hexdigest()),
                    header=verified['header'], observed_frames=rows[first:first+2],
                    source_sha256={str(path.relative_to(ROOT)):digest(path) for path in SOURCE_PATHS},
                    spawn_policy='Pinned source agentPosition/agentRotation, followed by authoritative native support settling; live entity pose is evidence only.',
                    center=None,
                    limitations=['This cohort is the observed reload instance, not the six actors from the initial arrival.',
                                 'No source random stream, general respawn policy, AI permissions, combat or kill progression is installed by this export.',
                                 'Bounds checks do not establish supported spawn terrain or complete movement coverage.',
                                 'Captured scale X supplies the scalar capsule/attribute scale; all three observed components remain in evidence.'])
    validate(evidence)
    return evidence


def validate(evidence):
    if (evidence.get('schema') != 1 or evidence.get('kind') != 'observed-original-hunt-cohort'
            or evidence.get('mission') != 9126 or evidence.get('target') != 1518 or evidence.get('center') is not None):
        raise ValueError('Unexpected cohort evidence contract')
    header = evidence.get('header', {})
    if (header.get('kind') != 'observed-original-hunt-agents' or header.get('mission') != 9126
            or header.get('target') != 1518 or header.get('probeSourceSha256') != digest(PROBE)):
        raise ValueError('Observer source/mission provenance differs')
    hashes = evidence.get('source_sha256', {})
    for path in SOURCE_PATHS:
        if hashes.get(str(path.relative_to(ROOT))) != digest(path):
            raise ValueError('Source input changed: ' + str(path.relative_to(ROOT)))
    raw = evidence.get('raw_capture', {})
    sha = raw.get('sha256', '')
    if (not isinstance(sha, str) or len(sha) != 64 or any(c not in '0123456789abcdef' for c in sha)
            or raw.get('end', {}).get('reason') not in ('stop-file', 'capture-limit', 'time-limit')
            or raw.get('end', {}).get('frames') != raw.get('frames')):
        raise ValueError('Missing complete raw capture provenance')
    frames = evidence.get('observed_frames')
    if not isinstance(frames, list) or len(frames) != 2:
        raise ValueError('Two consecutive cohort observations required')
    identities = []
    for number, frame in enumerate(frames):
        if (frame.get('type') != 'frame' or frame.get('missionActive') is not True
                or frame.get('missionPointPresent') is not True or frame.get('counter') != '13_0'
                or type(frame.get('sample')) is not int or type(frame.get('frame')) is not int
                or not all(finite(frame.get(k)) for k in ('time', 'realtime')) or not entity_valid(frame.get('player'))):
            raise ValueError('Expected active zero-kill encounter observation')
        if number and (frame['sample'] != frames[0]['sample']+1 or frame['frame'] <= frames[0]['frame']):
            raise ValueError('Observations are not consecutive/increasing')
        agents = frame.get('agents')
        if not isinstance(agents, list) or len(agents) != 6:
            raise ValueError('Exactly six source mission agents required')
        pairs = []
        for index, actor in enumerate(agents):
            entity = actor.get('entity')
            if (actor.get('sourceOrder') != index or type(actor.get('agentId')) is not int or actor['agentId'] <= 0
                    or actor.get('prototype') != 13 or actor.get('step') != 'Created' or actor.get('canRide') is not False
                    or not entity_valid(entity) or entity['prototype'] != 13 or entity['prototypeType'] != 3
                    or entity['hp'] <= 0 or entity['id'] <= 0 or entity['id'] > 0xffffffff):
                raise ValueError('Invalid observed mission actor identity/state')
            pairs.append((actor['agentId'], entity['id']))
            if entity['id'] == frame['player']['id']:
                raise ValueError('Actor identity aliases player')
            if not vector(actor.get('agentPosition'), 3) or not vector(actor.get('agentRotation'), 4):
                raise ValueError('Missing source anchor/rotation')
            scale = entity['scale'][0]
            if not .8 <= scale <= 1.3 or max(entity['scale'])-min(entity['scale']) > .0001:
                raise ValueError('Observed scale is outside the source uniform-scale contract')
            if actor.get('agentScale') != [1, 1, 1] or actor['agentRotation'] != [0, 0, 0, 1]:
                raise ValueError('Unexpected source mission agent transform')
            # Inclusive capsule clearance inside the installed source route.
            x, y, z = actor['agentPosition'];radius, height = .6*scale, 1.5*scale
            if not (-640+radius <= x < -224-radius and 64+radius <= z < 480-radius and 0 <= y < 64-height):
                raise ValueError('Observed anchor/capsule is outside installed route; never clamp or reroll')
            multiplier = entity['maxHp']/(212*scale)
            if not any(abs(multiplier-m) < .00001 for m in (1, .5)):
                raise ValueError('Observed maximum HP is not normal/Easy source scaling')
            if abs(entity['attack']-64*scale*multiplier) > .001 or abs(entity['defense']-29*scale*multiplier) > .001:
                raise ValueError('Observed combat stats disagree with source scaling')
            if (entity['campId'], entity['damageId'], entity['defaultPlayerId']) != (45, 45, 4):
                raise ValueError('Unexpected observed faction/permission IDs')
        if len({p[0] for p in pairs}) != 6 or len({p[1] for p in pairs}) != 6:
            raise ValueError('Duplicate cohort identity')
        identities.append(pairs)
    if identities[0] != identities[1]:
        raise ValueError('Cohort identities changed between observations')
    for first, second in zip(frames[0]['agents'], frames[1]['agents']):
        if first['agentPosition'] != second['agentPosition'] or first['agentRotation'] != second['agentRotation']:
            raise ValueError('Source anchor changed across cohort observations')
    return frames[0]['agents']


def cfloat(value):
    s = format(float(value), '.9g')
    return s + ('f' if '.' in s or 'e' in s else '.0f')


def generated(evidence, evidence_path):
    actors = validate(evidence)
    lines = ['/* Generated by tools/export_adventure_hunt_encounter.py; do not edit. */',
             '#ifndef PE_ADVENTURE_HUNT_ENCOUNTER_SOURCE_H', '#define PE_ADVENTURE_HUNT_ENCOUNTER_SOURCE_H', '#include <stdint.h>',
             '#define PE_HUNT_ENCOUNTER_REFERENCE_AVAILABLE 1', '#define PE_HUNT_ENCOUNTER_ACTOR_COUNT 6',
             'typedef struct {', '    uint32_t source_agent_id,source_entity_id; int prototype;',
             '    float position[3],rotation[4],yaw_degrees,scale,hp,max_hp,attack,defense;',
             '    int camp_id,damage_id,default_player_id;', '} PeHuntEncounterSourceActor;',
             'static const PeHuntEncounterSourceActor pe_hunt_encounter_source_actors[PE_HUNT_ENCOUNTER_ACTOR_COUNT] = {']
    for actor in actors:
        entity = actor['entity'];q = actor['agentRotation']
        yaw = math.degrees(math.atan2(2*(q[3]*q[1]+q[0]*q[2]), 1-2*(q[1]*q[1]+q[2]*q[2])))
        scalar = [yaw, entity['scale'][0], entity['hp'], entity['maxHp'], entity['attack'], entity['defense']]
        lines.append('    {'+str(actor['agentId'])+'u,'+str(entity['id'])+'u,13,{'+','.join(map(cfloat,actor['agentPosition']))+'},{'+
                     ','.join(map(cfloat,q))+'},'+','.join(map(cfloat,scalar))+',45,45,4},')
    lines += ['};', '#endif', '']
    manifest = dict(schema=1, capture=dict(path=str(evidence_path.relative_to(ROOT)) if evidence_path.is_relative_to(ROOT) else str(evidence_path),
                                          sha256=digest(evidence_path)),
                    mission=9126, target=1518, actor_count=6, source_sha256=evidence['source_sha256'],
                    raw_capture=evidence['raw_capture'], sample=evidence['observed_frames'][0]['sample'],
                    source_agent_anchors=[a['agentPosition'] for a in actors],
                    spawn_policy=evidence['spawn_policy'], limitations=evidence['limitations'])
    return {'pe_adventure_hunt_encounter_source.h':'\n'.join(lines).encode(),
            'pe_adventure_hunt_encounter_source.json':(json.dumps(manifest,indent=2)+'\n').encode()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, default=DEFAULT_CAPTURE)
    parser.add_argument('--from-report', type=Path)
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.from_report:
        if args.check:
            parser.error('--check cannot create evidence from a report')
        evidence = archive_report(args.from_report)
        args.capture.parent.mkdir(parents=True, exist_ok=True)
        with args.capture.open('x') as stream:
            json.dump(evidence, stream, indent=2);stream.write('\n')
    else:
        evidence = json.loads(args.capture.read_text())
    for name, content in generated(evidence, args.capture.resolve()).items():
        path = args.output/name
        if args.check:
            if not path.is_file() or path.read_bytes() != content:
                raise ValueError('Stale generated '+name)
        else:
            args.output.mkdir(parents=True, exist_ok=True);path.write_bytes(content)
    print('Observed six-hare source cohort '+('checked' if args.check else 'exported')+'; no native runtime encounter installed.')


if __name__ == '__main__':
    main()
