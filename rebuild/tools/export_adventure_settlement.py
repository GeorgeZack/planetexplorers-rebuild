#!/usr/bin/env python3
"""Export one archived Adventure starting lot; never invent terrain or map sites."""
from contextlib import closing
import argparse
import hashlib
import json
import math
from pathlib import Path
import sqlite3
import sys
import xml.etree.ElementTree as ET

from read_reference_adventure import ROOT, DEFAULT_SAVE, Reader, file_bytes, records
from export_content import DEFAULT_DATABASE, c_string, connect_readonly, describe_source

DEFAULT_XML = ROOT/'Assets/Resources/RandomTown/VArtifactTown.xml'
DEFAULT_OUTPUT = ROOT/'rebuild/generated'
# Archived reference identity and authored slot mapping, not procedural allocation.
BUILDINGS = ((20001,163,21,0),(20003,164,22,1),(20004,162,20,2),
             (20005,161,19,3),(20006,145,63,4),(20007,146,64,5))
ROLES = {1:(20008,17),2:(20016,22)}


def yird_records(directory):
    """Validate every subworld archive range, including records not exported."""
    _, guid, r = file_bytes(directory/'index.arc')
    rows, files, sources = {}, {}, [directory/'index.arc']
    for _ in range(r.count()):
        key, name, yird, begin, size = r.string(),r.string(),r.boolean(),*r.unpack('qq')
        if not key or not name or not name.isalnum() or not yird or key in rows:
            raise ValueError('Unsupported or duplicate subworld archive index')
        if name not in files:
            path = directory/(name+'.arc')
            data, other, _ = file_bytes(path)
            if other != guid: raise ValueError('Subworld archive GUID differs')
            files[name] = data
            sources.append(path)
        data = files[name]
        if begin < 28 or size < 0 or begin+size > len(data):
            raise ValueError('Subworld archive record range outside file')
        rows[key] = data[begin:begin+size]
    r.end()
    return rows, sources, guid.hex()


def entity(r):
    identity, prefab, data = r.integer(), r.string(), r.blob()
    data.version(2)
    kind, prototype = data.unpack('ii')
    components = {}
    for _ in range(data.count(128)):
        key, value = data.string().rsplit('.',1)[-1], data.blob().data
        if key in components: raise ValueError('Duplicate entity component')
        components[key] = value
    data.end()
    return dict(entity_id=identity,prefab=prefab,prototype_type=kind,
                prototype_id=prototype,components=components)


def transform(e):
    r = Reader(e['components']['PeTrans'])
    r.version(1)
    position, rotation = r.floats(3), r.floats(4)
    fast_travel, fast_position = r.boolean(), r.floats(3)
    r.end()
    if fast_travel: raise ValueError('Pending fast travel is not a stable archived pose')
    if abs(sum(x*x for x in rotation)-1) > 1e-5:
        raise ValueError('Invalid archived rotation quaternion')
    return dict(position=position,rotation=rotation)


def decode_entities(rows):
    outer = Reader(rows['ArchiveKeyCreature'])
    outer.version(0)
    r = outer.blob()
    outer.end()
    r.version(0)
    entities = {}
    for _ in range(r.count()):
        e = entity(r)
        if e['entity_id'] in entities: raise ValueError('Duplicate entity ID')
        entities[e['entity_id']] = e
    r.end()
    r = Reader(rows['Pathea.MainPlayer'])
    r.version(0)
    player = entity(r)
    r.end()
    if (player['entity_id'],player['prefab'],player['prototype_type'],player['prototype_id']) != (20000,'EntityPlayer',0,-1):
        raise ValueError('Unexpected archived main player')
    return entities, player


def decode_progress(data):
    r = Reader(data)
    rewards = [r.integer() for _ in range(r.count())]
    targets = [r.unpack('ii') for _ in range(r.count())]
    states = [r.unpack('ii') for _ in range(r.count())]
    missions = {}
    for _ in range(r.count()):
        key = r.integer()
        if key in missions: raise ValueError('Duplicate saved mission')
        missions[key] = [(r.string(),r.string()) for _ in range(r.count())]
    for _ in range(r.count()): r.floats(3)
    for _ in range(r.count()): r.integer(); r.floats(3)
    r.version(10)
    r.integer()
    for _ in range(r.count()): r.integer(); r.string()
    r.integer()
    buildings = {}
    for _ in range(r.count()):
        key, value = r.integer(), r.floats(3)
        if key in buildings: raise ValueError('Duplicate saved building position')
        buildings[key] = value
    for _ in range(r.count()): r.unpack('iii')
    for _ in range(r.count()): r.integer(); r.floats(3)
    for _ in range(r.count()): r.integer()
    for _ in range(r.count()): r.unpack('iii')
    roles = {}
    for _ in range(r.count()):
        key, value = r.unpack('ii')
        if key in roles: raise ValueError('Duplicate saved Adventure role')
        roles[key] = value
    for _ in range(r.count()): r.floats(3)
    r.integer()
    r.end()
    if missions != {9032:[('STEP','0')]} or dict(states) != {i:1 for i in range(9027,9032)} or targets or rewards != [9032]*18:
        raise ValueError('Expected post-supplies archived mission state')
    if roles != {1:20008,2:20016,5:20017,3:20018,4:20019}:
        raise ValueError('Unexpected archived Adventure role bindings')
    if set(buildings) != {0}: raise ValueError('Expected start-building saved front door')
    return roles, buildings[0]


def decode_target(data):
    r = Reader(data)
    if r.count() != 1 or r.integer() != 9032: raise ValueError('Expected sole mission9032')
    title = r.string()
    if r.unpack('iiii') != (20016,20016,1,1): raise ValueError('Unexpected supply mission binding')
    description = r.string()
    if r.count() != 1 or r.unpack('iii') != (4503,4,1): raise ValueError('Expected discovery4503')
    position = r.floats(3)
    # PlayerTalk, six item lists, and OP/IN/ED talk and next-mission lists.
    for _ in range(r.count()): r.integer()
    for _ in range(6):
        for _ in range(r.count()): r.unpack('ii')
    for _ in range(6):
        for _ in range(r.count()): r.integer()
    r.end()
    return dict(mission_id=9032,target_id=4503,town_id=4,title=title,
                description=description,position=position,completion_radius=15,
                track_radius=60,strict=True,axes='XZ',not_for_dungeon=True)


def numeric_vector(value, count):
    out = [float(v) for v in value.split(',')]
    if len(out) != count or not all(math.isfinite(v) for v in out):
        raise ValueError('Invalid source vector')
    return out


def db_row(db, table, key, identity):
    # Table/column names come only from fixed call sites below.
    rows = db.execute(f'SELECT * FROM {table} WHERE CAST({key} AS INTEGER)=?',(identity,)).fetchall()
    if len(rows) != 1: raise ValueError(f'Missing or duplicate {table} {identity}')
    return dict(rows[0])


def build_manifest(save=DEFAULT_SAVE,database=DEFAULT_DATABASE,town_xml=DEFAULT_XML):
    rows, sources, guid = records(save)
    sub, sub_sources, sub_guid = yird_records(save/'MainAdventure')
    if guid != sub_guid: raise ValueError('Root/subworld GUID differs')
    source_paths = sources+sub_sources+[database,town_xml]
    before = {p:hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths}
    entities, player = decode_entities(rows)
    roles, front_door = decode_progress(rows['ArchiveKeyMisRepository'])
    target = decode_target(sub['ArchiveKeyAdRMRepository'])
    markers = {}
    for key in ('PeMap.StaticPoint+Mgr','ArchiveKeyMapUserLabel','PeMap.TowerMark+Mgr'):
        r = Reader(sub[key])
        markers[key] = r.count()
        r.end()
        if markers[key] != 0: raise ValueError('Expected empty saved map label list')
    xml = ET.parse(town_xml).getroot().find('VAStartTown')
    if xml is None or xml.get('tid') != '-1': raise ValueError('Missing start town XML')
    units = xml.findall('ArtifactUnitArray/ArtifactUnit')
    if len(units) != 1 or units[0].attrib != dict(id='11',pos='0,0',rot='0',type='0'):
        raise ValueError('Unsupported start lot XML')
    unit = units[0]
    xml_buildings = {int(b.get('bid')):(int(b.get('num')),int(b.get('posIndex','-1')))
                     for b in unit.findall('BuildingNumArray/BuildingNum')}
    if xml_buildings != {20:(1,-1),19:(1,-1),21:(1,0),22:(1,1),63:(1,4),64:(1,5)}:
        raise ValueError('Unexpected authored building allocation')
    first_npc = unit.find('NPCArray/NPC')
    if first_npc is None or first_npc.attrib != dict(nid='1',num='1'):
        raise ValueError('Unexpected start mission NPC')
    with closing(connect_readonly(database)) as db:
        db.row_factory = sqlite3.Row
        lot = db_row(db,'Town_artifacts','Id',11)
        size = numeric_vector(lot['Size'],3)
        if lot['Name'] != 'town1_8' or size != [55,100,14]: raise ValueError('Unexpected starting lot dimensions')
        slots = []
        for value in lot['B_position'].split('_'):
            pos, yaw = value.split(';')
            slots.append(dict(position=numeric_vector(pos,3),yaw=float(yaw)))
        if len(slots) != 6 or not all(math.isfinite(s['yaw']) for s in slots):
            raise ValueError('Invalid building slots')
        first = transform(entities[20001])
        origin = [a-b for a,b in zip(first['position'],slots[0]['position'])]
        buildings = []
        for identity,prototype,house,slot in BUILDINGS:
            e = entities[identity]
            if (e['prefab'],e['prototype_type'],e['prototype_id']) != ('EntityDoodad',5,prototype):
                raise ValueError('Unexpected archived building identity')
            pose = transform(e)
            expected = [a+b for a,b in zip(origin,slots[slot]['position'])]
            angle = slots[slot]['yaw']*math.pi/360
            expected_q = [0,math.sin(angle),0,math.cos(angle)]
            if any(abs(a-b)>1e-5 for a,b in zip(pose['position'],expected)) or min(
                    max(abs(a-b) for a,b in zip(pose['rotation'],expected_q)),
                    max(abs(a+b) for a,b in zip(pose['rotation'],expected_q)))>1e-5:
                raise ValueError('Archived building disagrees with authored lot slot')
            row = db_row(db,'Townhouse','ID',house)
            if int(row['PrototypeDoodad_Id']) != prototype: raise ValueError('Townhouse prototype mismatch')
            dimensions = numeric_vector(row['Size'],2)
            if any(v<=0 for v in dimensions): raise ValueError('Invalid building size')
            buildings.append(dict(entity_id=identity,prototype_id=prototype,townhouse_id=house,
                slot=slot,resource=row['FilePath'],dimensions=dimensions,
                yaw_degrees=slots[slot]['yaw'],**pose))
        npcs = []
        for role,(identity,prototype) in ROLES.items():
            e = entities[identity]
            row = db_row(db,'AdRandNPCMission','ID',role)
            if roles[role] != identity or int(row['RNPC_ID']) != prototype or (e['prefab'],e['prototype_type'],e['prototype_id']) != ('EntityNpc',1,prototype):
                raise ValueError('Adventure NPC role/prototype mismatch')
            npcs.append(dict(role=role,entity_id=identity,prototype_id=prototype,**transform(e)))
        row = db_row(db,'AdQuest_EnterArea','TargetID',4503)
        if row['DistPos'] != '2_4_0_15' or row['TrackRadius'] != '60' or row['NotForDungeon'] != '1':
            raise ValueError('Discovery4503 source contract changed')
        npc_born = numeric_vector(lot['NPC_born'].split('_')[0],3)
    spawn = [origin[i]+npc_born[i]+[0,1,-1][i] for i in range(3)]
    result = dict(schema=1,scope='One archived reference lot, source-world coordinates; no terrain or invented markers.',
        archive_guid=guid,sources=[describe_source(p) for p in source_paths],
        lot=dict(artifact_id=11,template_id=-1,name=lot['Name'],size_x_z_y=size,
                 origin=origin,rotation_degrees=0,source_npc_born=npc_born),
        buildings=buildings,npcs=npcs,
        player=dict(entity_id=player['entity_id'],saved_pose=transform(player),
                    initial_spawn=dict(position=spawn,evidence='Derived from source InitTownHeight: lot origin + NPC_born[0] + (0,1,-1); not saved pose.')),
        start_building_front_door=front_door,destination=target,
        colony_candidates=dict(town_ids=[1,2,3],coordinates=None,marker_visibility='Unverified; inspected source excludes empty-town labels.'),
        saved_label_counts=markers)
    if any(hashlib.sha256(p.read_bytes()).hexdigest()!=value for p,value in before.items()):
        raise ValueError('Reference input changed during export')
    return result


def c_numbers(values):
    def number(value):
        out = format(value,'.17g')
        return out if '.' in out or 'e' in out else out+'.0'
    return '{'+','.join(number(v) for v in values)+'}'


def build_outputs(save=DEFAULT_SAVE,database=DEFAULT_DATABASE,town_xml=DEFAULT_XML):
    m = build_manifest(save,database,town_xml)
    lines = ['/* Generated by tools/export_adventure_settlement.py; do not edit. */',
        '#ifndef PE_ADVENTURE_SETTLEMENT_H','#define PE_ADVENTURE_SETTLEMENT_H',
        '#define PE_ADVENTURE_SETTLEMENT_BUILDING_COUNT 6',
        '#define PE_ADVENTURE_SETTLEMENT_NPC_COUNT 2',
        'typedef struct { int entity_id,prototype_id,townhouse_id,slot; double position[3],rotation[4],size[2],yaw_degrees; const char *resource; } PeAdventureSettlementBuilding;',
        'typedef struct { int role,entity_id,prototype_id; double position[3],rotation[4]; } PeAdventureSettlementNpc;',
        'static const PeAdventureSettlementBuilding pe_adventure_settlement_buildings[6] = {']
    for b in m['buildings']:
        lines.append('    {%d,%d,%d,%d,%s,%s,%s,%.17g,%s},'%(b['entity_id'],b['prototype_id'],b['townhouse_id'],b['slot'],c_numbers(b['position']),c_numbers(b['rotation']),c_numbers(b['dimensions']),b['yaw_degrees'],c_string(b['resource'])))
    lines += ['};','static const PeAdventureSettlementNpc pe_adventure_settlement_npcs[2] = {']
    for n in m['npcs']:
        lines.append('    {%d,%d,%d,%s,%s},'%(n['role'],n['entity_id'],n['prototype_id'],c_numbers(n['position']),c_numbers(n['rotation'])))
    lines += ['};']
    for name,values in [('lot_origin',m['lot']['origin']),('lot_size_x_z_y',m['lot']['size_x_z_y']),
            ('initial_spawn',m['player']['initial_spawn']['position']),('saved_player_position',m['player']['saved_pose']['position']),
            ('saved_player_rotation',m['player']['saved_pose']['rotation']),('town4_destination',m['destination']['position']),
            ('start_front_door',m['start_building_front_door'])]:
        lines.append('static const double pe_adventure_settlement_%s[%d] = %s;'%(name,len(values),c_numbers(values)))
    lines += ['#define PE_ADVENTURE_SETTLEMENT_DESTINATION_MISSION 9032',
        '#define PE_ADVENTURE_SETTLEMENT_DESTINATION_TARGET 4503',
        '#define PE_ADVENTURE_SETTLEMENT_DESTINATION_RADIUS 15.0',
        '#define PE_ADVENTURE_SETTLEMENT_DESTINATION_TRACK_RADIUS 60.0',
        '/* Candidate sites1..3 have no recovered coordinates or verified marker visibility. */',
        '#endif','']
    return {'pe_adventure_settlement.h':'\n'.join(lines).encode(),
            'pe_adventure_settlement.json':(json.dumps(m,ensure_ascii=False,indent=2)+'\n').encode()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--save',type=Path,default=DEFAULT_SAVE)
    parser.add_argument('--database',type=Path,default=DEFAULT_DATABASE)
    parser.add_argument('--town-xml',type=Path,default=DEFAULT_XML)
    parser.add_argument('--output',type=Path,default=DEFAULT_OUTPUT)
    parser.add_argument('--check',action='store_true')
    args = parser.parse_args()
    try:
        outputs = build_outputs(args.save,args.database,args.town_xml)
        if args.check:
            for name,data in outputs.items():
                if not (args.output/name).is_file() or (args.output/name).read_bytes()!=data:
                    raise ValueError('Stale generated '+name)
        else:
            args.output.mkdir(parents=True,exist_ok=True)
            for name,data in outputs.items(): (args.output/name).write_bytes(data)
        print('Adventure settlement checked' if args.check else 'Adventure settlement generated')
        return 0
    except (OSError,ValueError,KeyError,TypeError,sqlite3.Error,ET.ParseError) as error:
        print(error,file=sys.stderr)
        return 1


if __name__ == '__main__': raise SystemExit(main())
