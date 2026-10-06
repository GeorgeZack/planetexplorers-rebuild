#!/usr/bin/env python3
"""Read-only Adventure hunt archive inspection; no fabricated random targets."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from read_reference_adventure import Reader, records
from export_adventure_settlement import yird_records


def decode_random_missions(data):
    """AdRMRepository.Export, bounded known target-layout variants only."""
    r=Reader(data);missions={}
    for _ in range(r.count(256)):
        record_offset=r.at;identity=r.integer()
        if identity in missions:raise ValueError('Duplicate random mission')
        name=r.string();npc,reply,kind,max_count=r.unpack('iiii');description=r.string();targets=[]
        for _ in range(r.count(128)):
            target_id=r.integer();flag=r.integer();target=dict(id=target_id,kind=flag)
            if flag in (3,4,5,6):
                present=r.integer()
                if present not in (0,1):raise ValueError('Invalid target presence')
                target['present']=bool(present)
                if present:
                    if flag in (3,6):target['npcs']=[r.integer() for _ in range(r.count())]
                    target['position_record_offset']=r.at;target['position']=r.floats(3)
                    if any(not math.isfinite(v) for v in target['position']):raise ValueError('Nonfinite target position')
            elif flag!=0:raise ValueError('Unsupported target layout')
            targets.append(target)
        player_talk=[r.integer() for _ in range(r.count(32))]
        item_lists={key:[list(r.unpack('ii')) for _ in range(r.count())]
                    for key in ('get_demand','get_delete','get_mission','reward','selection_reward','complete_remove')}
        talks={key:[r.integer() for _ in range(r.count())]
               for key in ('opening_talk','opening_quests','ongoing_talk','ongoing_quests','ending_talk','ending_quests')}
        missions[identity]=dict(record_offset=record_offset,name=name,npc=npc,reply_npc=reply,
                                type=kind,max_count=max_count,description=description,targets=targets,
                                player_talk=player_talk,items=item_lists,**talks)
    r.end();return missions


def active_missions(data):
    r=Reader(data)
    for _ in range(r.count()):r.integer()
    for _ in range(r.count()):r.unpack('ii')
    completed={}
    for _ in range(r.count()):
        identity,count=r.unpack('ii')
        if identity in completed or count<0:raise ValueError('Invalid completed mission entry')
        completed[identity]=count
    active={}
    for _ in range(r.count()):
        identity=r.integer()
        if identity in active:raise ValueError('Duplicate active mission')
        active[identity]=[(r.string(),r.string()) for _ in range(r.count())]
    return active,completed,r.at


def audit(save):
    files=sorted(p for p in save.rglob('*') if p.is_file())
    before={str(p.relative_to(save)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    top,_,guid=records(save);sub,_,subguid=yird_records(save/'MainAdventure')
    if guid!=subguid:raise ValueError('Top-level and subworld GUID differ')
    active,completed,prefix=active_missions(top['ArchiveKeyMisRepository'])
    missions=decode_random_missions(sub['ArchiveKeyAdRMRepository'])
    if set(active)!=set(missions):raise ValueError('Active quest IDs differ between mission archives')
    target=None
    if 9075 in missions:
        quest=missions[9075]
        if quest['npc']!=20018 or len(quest['targets'])!=1:raise ValueError('Expected Naoko hunt mission')
        target=quest['targets'][0]
        if target['id']!=4524 or target['kind']!=4 or not target.get('present'):raise ValueError('Expected saved discovery4524')
        x,y,z=target['position'];target=dict(target)
        target['inside_installed_route_bounds']=(-640<=x< -224 and 0<=y<64 and 64<=z<480)
        target['arrival_disk_inside_route_xz']=(-640+15<=x< -224-15 and 64+15<=z<480-15)
        target['twenty_one_meter_encounter_margin_inside_route_xz']=(-640+21<=x< -224-21 and 64+21<=z<480-21)
    if any(hashlib.sha256((save/name).read_bytes()).hexdigest()!=value for name,value in before.items()):raise ValueError('Save changed during read-only audit')
    return dict(schema=1,evidence='Read-only original archive decode; no native quest installed.',
                save_directory=str(save.resolve()),archive_guid=guid,input_sha256=before,archives_unchanged=True,
                active_missions=active,completed_missions=completed,top_mission_prefix_decoded_bytes=prefix,
                random_missions=missions,hunt_target=target,
                limitations=['Offsets are relative to ArchiveKeyAdRMRepository record, not whole archive file.',
                             'Bounds checks do not establish traversability, support, collision or complete spawn-area coverage.',
                             'No target is inferred if original9075 is not active.'])


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--save-dir',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--require-hunt',action='store_true');a=p.parse_args()
    result=audit(a.save_dir)
    if a.require_hunt and result['hunt_target'] is None:raise ValueError('Save has no accepted9075 discovery target')
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
    print('Decoded source random missions:',','.join(str(x) for x in result['active_missions']))
    print('Hunt target:',result['hunt_target'])

if __name__=='__main__':main()
