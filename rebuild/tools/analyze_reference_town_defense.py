#!/usr/bin/env python3
"""Validate a closed beacon observation; no inferred actors, spawn positions or save policy."""
import argparse
import hashlib
import json
from pathlib import Path
from analyze_reference_hunt_encounter import entity_valid, finite, vector

MAX_BYTES = 16 * 1024 * 1024


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def analyze(path):
    raw = path.read_bytes()
    if not raw or len(raw) > MAX_BYTES:
        raise ValueError('Capture empty or exceeds16MiB')
    lines = raw.splitlines()
    if any(len(line) > 65536 for line in lines):
        raise ValueError('Line exceeds64KiB')
    rows = [json.loads(line) for line in lines]
    if len(rows) < 2 or any(not isinstance(row, dict) for row in rows):
        raise ValueError('Missing structured header/end')
    header, end = rows[0], rows[-1]
    if any(header.get(k) != v for k, v in dict(type='header', schema=1, kind='observed-original-town-defense',
                                               mission=9077, target=7507, phase='WaitForEndOfFrame').items()):
        raise ValueError('Wrong observation contract')
    digest = header.get('probeSourceSha256', '')
    if not isinstance(digest, str) or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
        raise ValueError('Missing probe source hash')
    frames = rows[1:-1]
    if (end.get('type') != 'end' or end.get('reason') not in ('stop-file', 'time-limit', 'capture-limit', 'wait-limit')
            or type(end.get('frames')) is not int or end['frames'] != len(frames) or len(frames) > 3600):
        raise ValueError('Missing clean closure or wrong frame count')
    epochs, changes = [], []
    current = None
    previous_frame, previous_time, previous_real = -1, -1., -1.
    previous_signature = None
    for index, frame in enumerate(frames):
        if frame.get('type') != 'frame' or type(frame.get('sample')) is not int or frame['sample'] != index:
            raise ValueError('Nonsequential sample')
        if not integer(frame.get('frame'), previous_frame + 1, 2147483647):
            raise ValueError('Nonincreasing Unity frame')
        if not all(finite(frame.get(k)) for k in ('time', 'realtime')) or frame['realtime'] < previous_real:
            raise ValueError('Invalid/nonmonotonic realtime clock')
        previous_frame, previous_time, previous_real = frame['frame'], frame['time'], frame['realtime']
        if any(type(frame.get(k)) is not bool for k in ('missionActive', 'missionPointPresent')):
            raise ValueError('Invalid mission flags')
        if frame.get('player') is not None and not entity_valid(frame['player']):
            raise ValueError('Invalid observed player')
        counter = frame.get('counter')
        if counter is not None and (not isinstance(counter, str) or len(counter) > 64):
            raise ValueError('Invalid observed counter')
        npcs = frame.get('protectedNpcs')
        if not isinstance(npcs, list) or len(npcs) > 64:
            raise ValueError('Invalid protected list')
        npc_ids = set()
        for npc in npcs:
            if not isinstance(npc, dict) or not integer(npc.get('id'), 1, 2147483647) or npc['id'] in npc_ids:
                raise ValueError('Invalid/duplicate protected identity')
            npc_ids.add(npc['id'])
            if npc.get('entity') is not None and (not entity_valid(npc['entity']) or npc['entity']['id'] != npc['id']):
                raise ValueError('Protected entity identity mismatch')
        agents = frame.get('agents')
        if not isinstance(agents, list) or len(agents) > 64:
            raise ValueError('Invalid bounded agent list')
        identities, entity_ids = set(), set()
        for order, agent in enumerate(agents):
            if not isinstance(agent, dict) or type(agent.get('sourceOrder')) is not int or agent['sourceOrder'] != order:
                raise ValueError('Invalid source order')
            if not integer(agent.get('agentId'), 0, 2147483647) or (agent['agentId'] and agent['agentId'] in identities):
                raise ValueError('Invalid/duplicate agent identity')
            identities.add(agent['agentId'])
            if (type(agent.get('prototype')) is not int or agent.get('step') not in ('NotCreated','InCreating','Created','Destroyed')
                    or type(agent.get('canRide')) is not bool or not vector(agent.get('agentPosition'),3)
                    or not vector(agent.get('agentScale'),3) or not vector(agent.get('agentRotation'),4)):
                raise ValueError('Invalid agent geometry/metadata')
            entity = agent.get('entity')
            if entity is not None:
                if not entity_valid(entity) or entity['id'] in entity_ids:
                    raise ValueError('Invalid/duplicate resolved entity')
                entity_ids.add(entity['id'])
        beacon = frame.get('beacon')
        if beacon is None:
            if agents:
                raise ValueError('Agents without beacon')
            current = None
        else:
            if (not isinstance(beacon,dict) or not integer(beacon.get('id'),1,2147483647)
                    or not integer(beacon.get('waveIndex'),0,2) or not finite(beacon.get('prepTime'))
                    or not 0 <= beacon['prepTime'] <= 30 or not vector(beacon.get('position'),3)
                    or not vector(beacon.get('forward'),3) or not frame['missionPointPresent']):
                raise ValueError('Invalid observed beacon')
            ui = beacon.get('ui')
            if ui is not None and (not isinstance(ui,dict) or ui.get('mission') != 9077
                    or not integer(ui.get('count'),0,1000000) or ui.get('goal') != 15
                    or not integer(ui.get('remainingWaves'),0,2) or ui.get('totalWaves') != 2):
                raise ValueError('Invalid source tower UI')
            wave = beacon['waveIndex']
            if wave == 0 and agents:
                raise ValueError('Agents before first wave')
            if (current is None or current['beacon_id'] != beacon['id'] or wave < current['last_wave_index']
                    or len(agents) < current['last_agent_count']):
                current = dict(epoch=len(epochs),beacon_id=beacon['id'],first_sample=index,last_sample=index,
                               last_wave_index=wave,last_agent_count=len(agents),phase_frames={},members={},wave0_count=None)
                epochs.append(current)
            current.update(last_sample=index,last_wave_index=wave,last_agent_count=len(agents))
            current['phase_frames'].setdefault(str(wave),frame)
            if wave == 1:
                if current['wave0_count'] not in (None,len(agents)):
                    raise ValueError('First wave membership changed without wave transition')
                current['wave0_count'] = len(agents)
            for agent in agents:
                order=agent['sourceOrder']; old=current['members'].get(order)
                if old is not None and old['agent']['agentId'] and agent['agentId'] and old['agent']['agentId'] != agent['agentId']:
                    raise ValueError('Beacon source order changed identity')
                # Keep the first actual resolved pose/anchor, not an uncreated placeholder or a later moving pose.
                if old is None or (old['agent']['entity'] is None and agent['entity'] is not None):
                    current['members'][order]=dict(sample=index,wave_index_when_observed=wave,agent=agent)
        signature=(frame['missionActive'],counter,None if beacon is None else beacon['id'],None if beacon is None else beacon['waveIndex'],len(agents))
        if signature != previous_signature:
            changes.append(dict(sample=index,time=frame['time'],realtime=frame['realtime'],mission_active=frame['missionActive'],
                                counter=counter,beacon=beacon,agent_count=len(agents),protected_npcs=npcs))
            previous_signature=signature
    qualified=[]
    for epoch in epochs:
        n0=epoch['wave0_count'];members=epoch.pop('members'); ordered=[members[k] for k in sorted(members)]
        epoch['members']=ordered
        epoch['wave_membership_observed']=n0 is not None and '2' in epoch['phase_frames']
        counts=[{},{}]
        for record in ordered:
            agent=record['agent'];order=agent['sourceOrder'];entity=agent['entity']
            wave=0 if n0 is not None and order<n0 else 1 if n0 is not None else None
            record['source_wave']=wave
            if wave is not None and entity is not None:
                key=str(entity['prototype']);counts[wave][key]=counts[wave].get(key,0)+1
        epoch['resolved_species_by_wave']=counts
        expected=(counts[0].get('13') in (8,9) and counts[0].get('4')==2 and set(counts[0])=={'13','4'}
                  and counts[1].get('7') in (8,9) and counts[1].get('4')==2 and set(counts[1])=={'7','4'})
        epoch['complete_two_wave_membership']=bool(epoch['wave_membership_observed'] and expected
            and len(ordered)==sum(sum(c.values()) for c in counts)
            and all(r['agent']['agentId'] and not r['agent']['canRide'] and r['agent']['entity'] is not None
                    and r['agent']['entity']['prototypeType']==3 for r in ordered))
        if epoch['complete_two_wave_membership']:qualified.append(epoch['epoch'])
    return dict(schema=1,evidence='Closed read-only beacon observation; no native defense installation.',capture=str(path.resolve()),
                capture_sha256=hashlib.sha256(raw).hexdigest(),header=header,end=end,frames=len(frames),changes=changes,
                beacon_epochs=epochs,complete_two_wave_epochs=qualified,
                limitations=['Source wave assignment requires a directly observed first-wave list and later appended second-wave list in one beacon epoch.',
                 'Encoded agent prototype is not the resolved entity species; no quest counter is an entity ID.',
                 'First resolved anchor/scale/stat snapshot need not precede damage; no all-full-HP claim or inferred original spawn pose.',
                 'Missing creation, species, timer, killer or protected NPC observations remain missing; no save/reload semantics inferred.',
                 'No inputs, gameplay mutation, RNG, spawn, damage, death callbacks or saves are issued by the observer.'])


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path);parser.add_argument('--output',type=Path)
    parser.add_argument('--require-waves',action='store_true',help='require complete resolved membership of both source waves in one beacon epoch')
    args=parser.parse_args(argv)
    try:
        report=analyze(args.capture)
        if args.require_waves and not report['complete_two_wave_epochs']:
            raise ValueError('No complete two-wave membership observation; capture remains negative/incomplete evidence')
        text=json.dumps(report,indent=2,allow_nan=False)+'\n'
        if args.output:args.output.write_text(text)
        else:print(text,end='')
    except (OSError,ValueError,KeyError,TypeError) as error:
        parser.exit(1,str(error)+'\n')
    return 0


if __name__=='__main__':raise SystemExit(main())
