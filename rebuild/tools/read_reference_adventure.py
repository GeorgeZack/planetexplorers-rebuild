#!/usr/bin/env python3
"""Bounded read-only decoder for one archived Adventure NPC appearance."""
from contextlib import closing
import hashlib
import math
import os
from pathlib import Path
import sqlite3
import struct

ROOT = Path(__file__).resolve().parents[2]


def source_label(path):
    """Repository-relative label for a source inside this repository, else the
    absolute path. Not resolved: the Unity original is reached through root
    links, and a checkout location must not leak into generated manifests."""
    logical = Path(os.path.abspath(path))
    return logical.relative_to(ROOT).as_posix() if logical.is_relative_to(ROOT) else str(logical)
DEFAULT_SAVE = Path.home()/'.local/share/planet-explorers-sandbox-reference/PlanetExplorers/GameSave/save0'
ARCHIVED_NPCS = {
    'supplier': dict(entity_id=20016, prototype_id=22, adventure_role=2),
    'welcome': dict(entity_id=20008, prototype_id=17, adventure_role=1),
}

class Reader:
    def __init__(self, data): self.data, self.at = data, 0
    def take(self, n):
        if n < 0 or self.at+n > len(self.data): raise ValueError('Truncated archive record')
        out=self.data[self.at:self.at+n];self.at+=n;return out
    def unpack(self, fmt): return struct.unpack('<'+fmt,self.take(struct.calcsize('<'+fmt)))
    def integer(self): return self.unpack('i')[0]
    def count(self, cap=4096):
        n=self.integer()
        if not 0 <= n <= cap: raise ValueError('Archive count outside bound')
        return n
    def boolean(self):
        n=self.take(1)[0]
        if n not in (0,1): raise ValueError('Invalid archive boolean')
        return bool(n)
    def string(self):
        n=0
        for shift in range(0,35,7):
            byte=self.take(1)[0];n|=(byte&127)<<shift
            if not byte&128: break
        else: raise ValueError('Invalid string length')
        if n>16384: raise ValueError('Oversized archive string')
        try: return self.take(n).decode('utf-8')
        except UnicodeError as error: raise ValueError('Invalid archive UTF-8') from error
    def nullable(self): return None if self.boolean() else self.string()
    def blob(self): return Reader(self.take(self.count(64*1024*1024)))
    def version(self, expected):
        if self.integer()!=expected: raise ValueError('Unsupported archive record version')
    def floats(self,n):
        out=list(self.unpack('f'*n))
        if not all(math.isfinite(v) for v in out): raise ValueError('Nonfinite archive values')
        return out
    def end(self):
        if self.at!=len(self.data): raise ValueError('Trailing archive record data')

def file_bytes(path, cap=64*1024*1024):
    if path.is_symlink() or not path.is_file() or not 28<=path.stat().st_size<=cap: raise ValueError('Invalid archive input')
    data=path.read_bytes();r=Reader(data);r.version(5);guid=r.take(16)
    if r.unpack('q')[0]!=len(data): raise ValueError('Archive file length differs')
    return data,guid,r

def records(directory):
    index,guid,r=file_bytes(directory/'index.arc'); rows={}; sources=[directory/'index.arc'];files={}
    for _ in range(r.count(4096)):
        key,name,yird,begin,length=r.string(),r.string(),r.boolean(),*r.unpack('qq')
        if not name or not name.isalnum() or yird or key in rows: raise ValueError('Unsupported or duplicate archive index')
        path=directory/(name+'.arc')
        if name not in files:
            data,other,_=file_bytes(path)
            if other!=guid: raise ValueError('Archive GUID differs')
            files[name]=data;sources.append(path)
        data=files[name]
        if begin<28 or length<0 or begin+length>len(data): raise ValueError('Archive record range outside file')
        rows[key]=data[begin:begin+length]
    r.end();return rows,sources,guid.hex()

def decode(directory, entity_id=20016, database=ROOT/'Assets/Resources/DataBase/localData.bytes', *, expected=None):
    expected=expected or next((npc for npc in ARCHIVED_NPCS.values() if npc['entity_id']==entity_id),None)
    if expected is None: raise ValueError('Expected archived supplier or welcome entity')
    rows,sources,guid=records(directory)
    outer=Reader(rows['ArchiveKeyCreature']);outer.version(0);c=outer.blob();outer.end();c.version(0);entity=None
    for _ in range(c.count(4096)):
        ident,prefab,data=c.integer(),c.string(),c.blob()
        if ident!=entity_id: continue
        if entity is not None: raise ValueError('Duplicate entity ID')
        data.version(2);proto_type,proto_id=data.unpack('ii');components={}
        for _ in range(data.count(128)):
            key,body=data.string(),data.blob().data
            if key in components: raise ValueError('Duplicate component')
            components[key]=body
        data.end();entity=dict(entity_id=ident,prefab=prefab,prototype_type=proto_type,prototype_id=proto_id,components=components)
    c.end()
    if entity is None or entity['prototype_type']!=1 or entity['prototype_id']!=expected['prototype_id']: raise ValueError('Expected archived RandomNpc identity')
    components=entity.pop('components')
    def component(name):
        matches=[value for key,value in components.items() if key.rsplit('.',1)[-1]==name]
        if len(matches)!=1: raise ValueError('Missing or duplicate '+name)
        return Reader(matches[0])
    info=component('EntityInfoCmpt');info.version(0);name=info.blob();full,family,given=[name.nullable() for _ in range(3)];name.end()
    entity['name']=full or ' '.join(v for v in (given,family) if v)
    avatar=component('AvatarCmpt');entity['prefab_path']=avatar.string();avatar.version(0);appear=avatar.blob();nude=avatar.blob();avatar.end()
    if entity['prefab_path'] not in ('', 'Prefab/PlayerPrefab/FemaleModel', 'Prefab/PlayerPrefab/MaleModel'): raise ValueError('Unknown archived humanoid rig')
    appear.version(1)
    if appear.count(27)!=27: raise ValueError('Expected 27 source morph weights')
    entity['weights']=appear.floats(27)
    if any(abs(v)>1 for v in entity['weights']): raise ValueError('Morph weight outside source range')
    entity['colors']={key:appear.floats(4) for key in ('eye','lip','skin','hair')};appear.end()
    if any(not 0<=v<=1 for color in entity['colors'].values() for v in color): raise ValueError('Color outside source range')
    nude.version(150911);count=nude.count(8);entity['base_model']=nude.string();slots={}
    if entity['base_model'] not in ('Prefab/PlayerPrefab/FemaleModel', 'Prefab/PlayerPrefab/MaleModel'): raise ValueError('Unknown nude avatar base model')
    if entity['prefab_path'] and entity['base_model']!=entity['prefab_path']: raise ValueError('Nude avatar base model differs')
    entity['sex']='male' if entity['base_model'].endswith('/MaleModel') else 'female'
    if not entity['prefab_path']:
        entity['serialized_prefab_path']=''
        entity['prefab_path']=entity['base_model']
    if 'CommonCmpt' in {key.rsplit('.',1)[-1] for key in components}:
        common=component('CommonCmpt');common.boolean();sex=common.integer()
        if sex != (2 if entity['sex']=='male' else 1): raise ValueError('Common sex disagrees with avatar base model')
    for _ in range(count):
        index,path=nude.integer(),nude.string()
        if not 0<=index<8 or index in slots: raise ValueError('Invalid avatar slot')
        slots[index]=path
    nude.end()
    if set(slots)!=set(range(8)): raise ValueError('Incomplete nude avatar slots')
    equipment=component('EquipmentCmpt');equipment.version(1);instance_ids=[equipment.integer() for _ in range(equipment.count(32))];equipment.end()
    items=Reader(rows['ArchiveKeyItemAsset']);items.blob();objects={}
    for _ in range(items.count(100000)):
        item=items.blob();proto,ident=item.unpack('ii');item.blob();item.end()
        if ident in objects: raise ValueError('Duplicate item instance')
        objects[ident]=proto
    items.integer();items.end()
    trans=component('PeTrans');trans.version(1);entity['saved_position']=trans.floats(3);entity['saved_rotation']=trans.floats(4)
    entity['equipment']=[];slot_masks={1024:0,1:1,2048:2,4096:3,2:4,4:5,64:6,128:7}
    with closing(sqlite3.connect(database.resolve().as_uri()+'?mode=ro',uri=True)) as connection:
        if expected['adventure_role']:
            role=connection.execute('SELECT RNPC_ID FROM AdRandNPCMission WHERE CAST(ID AS INTEGER)=?',(expected['adventure_role'],)).fetchall()
            if len(role)!=1 or int(role[0][0])!=entity['prototype_id']: raise ValueError('Archive prototype disagrees with Adventure role')
        for ident in instance_ids:
            proto=objects[ident]
            row=connection.execute('SELECT _modelPath,sex,_replacePos FROM PrototypeItem WHERE CAST(id AS INTEGER)=?',(proto,)).fetchone()
            if row is None: raise ValueError('Unknown equipped item')
            path,sex,mask=row[0],int(row[1]),int(row[2])
            if sex not in (0,2 if entity['sex']=='male' else 1): raise ValueError('Equipment sex disagrees')
            entity['equipment'].append(dict(instance_id=ident,item_id=proto,resource=path,replace_mask=mask))
            if mask:
                if mask<0 or mask & ~sum(slot_masks): raise ValueError('Unsupported appearance replacement mask')
                for bit,slot in slot_masks.items():
                    if mask & bit: slots[slot]=path
    entity.update(slots=[slots[i] for i in range(8)],adventure_role=expected['adventure_role'],archive_guid=guid,
                  sources=[dict(path=source_label(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sources+[database]])
    return entity
