#!/usr/bin/env python3
"""Validate bounded original player observations; never derive a native root asset."""
import argparse
import hashlib
import json
import math
from pathlib import Path

MAX_BYTES = 32*1024*1024
MAX_LINE = 131072


def pairs(items):
    result = {}
    for k, v in items:
        if k in result:
            raise ValueError('Duplicate JSON key')
        result[k] = v
    return result


def finite(v):
    if isinstance(v, dict):
        for x in v.values(): finite(x)
    elif isinstance(v, list):
        for x in v: finite(x)
    elif isinstance(v, float) and not math.isfinite(v):
        raise ValueError('Nonfinite value')


def number(v, lo=None):
    if type(v) not in (int, float) or not math.isfinite(v) or (lo is not None and v < lo):
        raise ValueError('Invalid numeric value')
    return v


def integer(v, lo=0):
    if type(v) is not int or v < lo:
        raise ValueError('Invalid integer')
    return v


def vec(v, n=3):
    if not isinstance(v, list) or len(v) != n:
        raise ValueError('Invalid vector')
    for x in v: number(x)
    return v


def boolean(v):
    if type(v) is not bool: raise ValueError('Invalid boolean')


def read(path):
    path = Path(path)
    if path.stat().st_size > MAX_BYTES: raise ValueError('Capture byte bound')
    rows = []
    with path.open('rb') as f:
        while True:
            line = f.readline(MAX_LINE+1)
            if not line: break
            if len(line) > MAX_LINE: raise ValueError('Record byte bound')
            row = json.loads(line, object_pairs_hook=pairs)
            finite(row); rows.append(row)
            if len(rows) > 7202: raise ValueError('Record count bound')
    return rows


def clips(a):
    layers = a['layers']
    if not isinstance(layers, list) or not 1 <= len(layers) <= 32: raise ValueError('Layer bound')
    result = []
    for layer in layers:
        if not isinstance(layer['name'], str): raise ValueError('Layer name')
        number(layer['weight'], 0); boolean(layer['transition'])
        for key in ('current', 'next'):
            s = layer[key]
            for k in ('normalizedTime', 'length', 'speed', 'speedMultiplier'): number(s[k])
            for k in ('fullPathHash', 'shortNameHash'):
                if type(s[k]) is not int: raise ValueError('Invalid hash')
            boolean(s['loop'])
        for key in ('clips', 'nextClips'):
            c = layer[key]
            if not isinstance(c, list) or len(c) > 64: raise ValueError('Clip bound')
            for entry in c:
                if not isinstance(entry['name'], str): raise ValueError('Unavailable clip')
                number(entry['weight'], 0); number(entry['length'], 0)
                if layer['weight'] > 0 and entry['weight'] > 0:
                    state = layer['current' if key == 'clips' else 'next']
                    result.append(dict(layer=layer['name'], branch=key, name=entry['name'], weight=entry['weight'],
                                       normalized_time=state['normalizedTime'], state_length=state['length'],
                                       state_speed=state['speed'], speed_multiplier=state['speedMultiplier']))
    return result


def analyze(path):
    rows = read(path)
    if len(rows) < 3: raise ValueError('No observations or incomplete capture')
    h, end = rows[0], rows[-1]
    if (h.get('type'),h.get('schema'),h.get('kind'),h.get('phase')) != ('header',1,'observed-original-running-attack','WaitForEndOfFrame'):
        raise ValueError('Wrong capture identity')
    if not isinstance(h.get('unity'), str) or not h['unity'].startswith('5.2.4'): raise ValueError('Unexpected Unity version')
    source = h.get('probeSourceSha256')
    if not isinstance(source,str) or len(source)!=64 or any(c not in '0123456789abcdef' for c in source): raise ValueError('Missing source hash')
    if h.get('maxBytes') != MAX_BYTES or not 1<=integer(h['maxFrames'],1)<=7200 or not 1<=number(h['maxSeconds'],1)<=120:
        raise ValueError('Capture limits differ')
    if end.get('type')!='end' or end.get('reason') not in ('stop-file','time-limit','capture-limit'): raise ValueError('Capture did not close normally')
    data=rows[1:-1]
    if integer(end['frames'])!=len(data) or not len(data)<=integer(end['observedLoops'])<=h['maxFrames']: raise ValueError('Frame count mismatch')
    episodes={}; thresholds=set(); controllers=set(); avatars=set(); previous=None; running_samples=0; max_gap=0; player=None; max_delta_error=0
    for i,f in enumerate(data):
        if f.get('type')!='frame' or integer(f['sample'])!=i: raise ValueError('Sample order mismatch')
        integer(f['frame']); integer(f['playerId'],1)
        if player is not None and player!=f['playerId']: raise ValueError('Player changed')
        player=f['playerId']
        for k in ('time','realtime','dt','fixedDt'): number(f[k],0)
        if previous:
            if f['frame']<=previous['frame'] or f['realtime']<=previous['realtime'] or f['time']<previous['time']: raise ValueError('Frame clocks reversed')
            max_gap=max(max_gap,f['time']-previous['time'])
        for k in ('position','modelPosition','velocity','lastMove'):vec(f[k])
        for k in ('rotation','modelRotation','lastRotation'):vec(f[k],4)
        if f['motorVelocity'] is not None:vec(f['motorVelocity'])
        boolean(f['grounded']);number(f['stamina'],0)
        action=f['action'];number(action['moveThreshold'],0);number(action['sprintThreshold'],0)
        if action['moveThreshold']>=action['sprintThreshold']:raise ValueError('Unordered thresholds')
        thresholds.add((action['moveThreshold'],action['sprintThreshold']))
        for k in ('firstPerson','useStamina','m_AttackInAir','m_AttackInWater','m_AttackInMove','m_AttackInSprint','m_EndAction'):boolean(action[k])
        if action['weaponPrototype'] is not None:integer(action['weaponPrototype'],1)
        integer(action['combo']);vec(action['direction']);vec(action['targetDirection'])
        a=f['animator'];controllers.add(a['controller']);avatars.add(a['avatar']);integer(a['id'], -2147483648);number(a['speed'],0);boolean(a['applyRootMotion'])
        for k in ('deltaPosition','rootPosition'):vec(a[k])
        for k in ('deltaRotation','rootRotation'):vec(a[k],4)
        observed_clips=clips(a)
        max_delta_error=max(max_delta_error,math.dist(f['lastMove'],a['deltaPosition']))
        sk=action['skill']
        if sk is not None:
            sid=integer(sk['instance'],1);skill=integer(sk['id'],1);integer(sk['guide'],-99);integer(sk['coolStat']);number(sk['startTime'])
            boolean(sk['active']);boolean(sk['execExtEnable'])
            if sid not in episodes:
                if len(episodes)>=1024:raise ValueError('Skill identity bound')
                episodes[sid]=dict(instance=sid,skill=skill,start_time=sk['startTime'],samples=0,first_sample=i,last_sample=i,
                    first_time=f['time'],last_time=f['time'],first_position=f['position'],last_position=f['position'],
                    first_velocity=f['velocity'],previous_observed_velocity=None if previous is None else previous['velocity'],
                    previous_observed_time=None if previous is None else previous['time'],
                    first_flags={k:action[k] for k in ('m_AttackInAir','m_AttackInWater','m_AttackInMove','m_AttackInSprint')},
                    weapon_prototypes=[],clip_names=[],guides=[],running_clip_samples=0,clip_clock_ranges={},transitions=[])
            ep=episodes[sid]
            if ep['skill']!=skill or ep['start_time']!=sk['startTime']:raise ValueError('Skill identity reused')
            ep.update(samples=ep['samples']+1,last_sample=i,last_time=f['time'],last_position=f['position'])
            status=dict(guide=sk['guide'],active=sk['active'],contact_enabled=sk['execExtEnable'],combo=action['combo'],end_action=action['m_EndAction'],clips=sorted({c['name'] for c in observed_clips}))
            if not ep['transitions'] or ep['transitions'][-1]['status']!=status:
                ep['transitions'].append(dict(sample=i,frame=f['frame'],time=f['time'],status=status,clip_clocks=observed_clips))
            for value,key in ((action['weaponPrototype'],'weapon_prototypes'),(sk['guide'],'guides')):
                if value not in ep[key]:ep[key].append(value)
            for c in observed_clips:
                if c['name'] not in ep['clip_names']:ep['clip_names'].append(c['name'])
                key=c['layer']+'/'+c['branch']+'/'+c['name'];t=c['normalized_time']
                lo,hi=ep['clip_clock_ranges'].get(key,[t,t]);ep['clip_clock_ranges'][key]=[min(lo,t),max(hi,t)]
            running=skill==20110014 and sk['active'] and action['weaponPrototype']==33 and f['grounded'] and not action['m_AttackInAir'] and not action['m_AttackInWater'] and not action['m_AttackInSprint'] and action['m_AttackInMove'] and not action['firstPerson'] and any(c['name']=='RunSwordAttack' and c['branch']=='clips' for c in observed_clips)
            if running:running_samples+=1;ep['running_clip_samples']+=1
        previous=f
    if data[-1]['realtime']-data[0]['realtime']>h['maxSeconds']+.001:raise ValueError('Observation duration exceeds bound')
    manifest_path=Path(path).with_suffix('.launch.json');provenance=None
    if manifest_path.exists():
        if manifest_path.stat().st_size>65536:raise ValueError('Manifest bound')
        provenance=json.loads(manifest_path.read_text(),object_pairs_hook=pairs)
        if provenance.get('schema')!='pe-running-attack-launch-1' or provenance.get('probe_source_sha256')!=source or Path(provenance['capture']).resolve()!=Path(path).resolve():raise ValueError('Launch provenance mismatch')
        for key in ('assembly_sha256','shim_sha256','engine_sha256','game_assembly_sha256','asset_sha256'):
            v=provenance.get(key)
            if not isinstance(v,str) or len(v)!=64 or any(c not in '0123456789abcdef' for c in v):raise ValueError('Missing provenance hash')
    with Path(path).open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
    return dict(schema='pe-running-attack-report-1',capture=str(Path(path).resolve()),capture_sha256=digest,
        frames=len(data),closure=end['reason'],player_id=player,controllers=sorted(controllers),avatars=sorted(avatars,key=str),thresholds=[dict(move=x,sprint=y) for x,y in sorted(thresholds)],
        running_knife_observed_samples=running_samples,max_observed_game_time_gap=max_gap,
        last_move_vs_animator_delta_max_error=max_delta_error,skills=list(episodes.values()),launch_provenance=provenance,
        exact_selection_velocity_observed=False,exact_motor_order_observed=False,root_curve_qualified=False,
        note='End-of-frame snapshots can miss short stages. Prior velocity is an observation, not the instantaneous selection velocity. No clip clock or root trajectory is inferred from wall time.')


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path);p.add_argument('--require-running',action='store_true');p.add_argument('--report',type=Path);a=p.parse_args(argv)
    try:
        result=analyze(a.capture)
        if a.report:
            a.report.parent.mkdir(parents=True,exist_ok=True)
            with a.report.open('x') as f:json.dump(result,f,indent=2);f.write('\n')
        print(json.dumps({k:result[k] for k in ('frames','closure','running_knife_observed_samples','thresholds','max_observed_game_time_gap')},indent=2))
        if a.require_running and (not result['running_knife_observed_samples'] or not result['launch_provenance']):raise ValueError('No provenance-backed grounded running Knife33 sample')
        return 0
    except (OSError,ValueError,KeyError,TypeError,OverflowError) as e:
        print('Running attack analysis: '+str(e));return 1


if __name__=='__main__':raise SystemExit(main())
