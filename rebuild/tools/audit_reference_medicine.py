#!/usr/bin/env python3
"""Read-only native Adventure medicine checkpoint evidence, with byte offsets."""
import argparse
import hashlib
import json
from pathlib import Path
from read_reference_adventure import ROOT, Reader, file_bytes, records
from export_adventure_settlement import decode_entities


def hashes(directory):
    return {str(p.relative_to(directory)):hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(directory.rglob('*')) if p.is_file()}


def inspect(directory):
    rows,_,guid=records(directory)
    _,_,index=file_bytes(directory/'index.arc');ranges={}
    for _ in range(index.count()):
        key,name,yird,begin,size=index.string(),index.string(),index.boolean(),*index.unpack('qq')
        if yird:raise ValueError('Unexpected subworld index')
        ranges[key]=dict(file=name+'.arc',offset=begin,size=size)
    index.end()
    _,player=decode_entities(rows)
    r=Reader(rows['Pathea.MainPlayer']);r.version(0);r.integer();r.string();length=r.count(64*1024*1024)
    entity_start=r.at;body=Reader(r.take(length));r.end();body.version(2);body.unpack('ii');component_offsets={}
    for _ in range(body.count(128)):
        key=body.string().rsplit('.',1)[-1];length=body.count(64*1024*1024)
        component_offsets[key]=ranges['Pathea.MainPlayer']['offset']+entity_start+body.at
        if body.take(length)!=player['components'][key]:raise ValueError('Component offset mismatch')
    body.end()
    r=Reader(player['components']['ReplicatorCmpt']);r.version(-1);known=[]
    for _ in range(r.count()):
        offset=component_offsets['ReplicatorCmpt']+r.at
        known.append(dict(formula=r.integer(),new_flag=r.boolean(),file_offset=offset))
    running=r.boolean()
    if running:raise ValueError('Expected no running replication')
    r.end()
    # Source ItemMgr archive maps instance identity to prototype identity.
    r=Reader(rows['ArchiveKeyItemAsset']);r.blob();objects={}
    for _ in range(r.count(100000)):
        item=r.blob();prototype,instance=item.unpack('ii');item.blob();item.end()
        if instance in objects:raise ValueError('Duplicate item instance')
        objects[instance]=prototype
    r.integer();r.end()
    r=Reader(player['components']['PlayerPackageCmpt']);money=r.blob();money_value=money.integer();money.end()
    package=r.blob();mission_package=r.blob();r.blob();r.end()
    def slots(reader):
        result=[]
        for category in range(4):
            row=reader.blob();row.version(0);ids=[row.integer() for _ in range(row.count())];row.end()
            present=[i for i in ids if i!=-1]
            if any(i not in objects for i in present):raise ValueError('Unresolved inventory instance')
            result.append(dict(category=category,capacity=len(ids),occupied=len(present),
                               script877_instances=[i for i in present if objects[i]==877]))
        reader.end();return result
    packages=dict(player=slots(package),mission=slots(mission_package))
    # The opening prefix is PlayerMission.Export; the remaining archive also
    # includes story state and is deliberately outside this bounded decode.
    r=Reader(rows['ArchiveKeyMisRepository']);rewards=[r.integer() for _ in range(r.count())]
    targets=[r.unpack('ii') for _ in range(r.count())];states={}
    for _ in range(r.count()):
        identity,state=r.unpack('ii');states[identity]=state
    missions={}
    for _ in range(r.count()):
        offset=ranges['ArchiveKeyMisRepository']['offset']+r.at;identity=r.integer()
        missions[identity]=dict(file_offset=offset,flags=[(r.string(),r.string()) for _ in range(r.count())])
    return dict(archive_guid=guid,player_entity_id=player['entity_id'],money=money_value,
                records={key:ranges[key] for key in ['Pathea.MainPlayer','ArchiveKeyMisRepository','ArchiveKeyItemAsset']},
                components={key:dict(file_offset=component_offsets[key],size=len(player['components'][key]))
                            for key in ['ReplicatorCmpt','PlayerPackageCmpt']},
                known_formulas=known,running_replication=running,
                medicine_mission=missions.get(9072),completed_mission_states=states,
                medicine_acceptance_grant_entries=rewards.count(9072),target_states=targets,
                mission_prefix_decoded_bytes=r.at,mission_record_tail_decoded=False,
                packages=packages,item_registry_count=len(objects),
                script877_instances_in_entire_item_registry=[i for i,p in objects.items() if p==877])


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--save-dir',type=Path,required=True)
    p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    before={str(path.resolve()):hashes(path) for path in [a.baseline,a.save_dir]}
    baseline=inspect(a.baseline);current=inspect(a.save_dir)
    known={row['formula'] for row in current['known_formulas']};old={row['formula'] for row in baseline['known_formulas']}
    if not {337,388,390}<=known or {337,388,390}&old:raise ValueError('Expected newly learned medicine formulas')
    if current['medicine_mission'] is None or current['medicine_acceptance_grant_entries']!=1:raise ValueError('Expected one accepted medicine mission')
    if current['script877_instances_in_entire_item_registry']:raise ValueError('Medicine script still exists')
    if any(hashes(Path(path))!=values for path,values in before.items()):raise ValueError('Archive changed during audit')
    source_paths=['Assets/Scripts/PeEntity/Cmpt/ReplicatorCmpt.cs','Assets/Scripts/PeEntity/Cmpt/PlayerPackageCmpt.cs',
                  'Assets/Scripts/PeEntity/Cmpt/PackageCmpt.cs','Assets/Scripts/Items/ItemPak/ItemPackage.cs',
                  'Assets/Scripts/Items/ItemPak/SlotList.cs','Assets/Scripts/Mission/PlayerMission.cs']
    result=dict(schema=1,evidence='Observed original native-client save, decoded read-only from source serializers.',
                baseline_directory=str(a.baseline.resolve()),save_directory=str(a.save_dir.resolve()),
                input_sha256=before,archives_unchanged_during_audit=True,
                source_sha256={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in source_paths},
                baseline=baseline,current=current,newly_learned_formulas=sorted(known-old),
                conclusions=['Mission9072 is accepted and active; its acceptance grant is recorded once.',
                             'Formulas337,388,390 are newly learned relative to the Annot checkpoint.',
                             'No item877 instance remains in the player/mission packages or global item registry.',
                             'This checkpoint does not establish crafting or medicine turn-in completion.'],
                offset_convention='Zero-based absolute byte offsets into each named archive file; formula offsets identify the signed32 ID preceding its boolean new flag.')
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
    print('Verified original medicine acceptance, learned337/388/390, consumed877; archives unchanged.')

if __name__=='__main__':main()
