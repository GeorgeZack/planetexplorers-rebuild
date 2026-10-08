#!/usr/bin/env python3
"""Recover the pinned save0 population, exact appearances and matching-Avatar idles.

No procedural NPC generation, live AI, recruitment or schedule reconstruction.
Metadata uses only stdlib; --assets requires UnityPy/Pillow and closed idle bakes.
"""
import argparse
from contextlib import closing
import hashlib
import json
import math
from pathlib import Path
import sqlite3
import tempfile
import xml.etree.ElementTree as ET

from read_reference_adventure import ROOT, DEFAULT_SAVE, Reader, records, decode, source_label
from export_adventure_settlement import decode_entities, decode_progress, transform
from export_content import DEFAULT_DATABASE, c_string

DEFAULT_OUTPUT = ROOT/'rebuild/generated'
IDENTITIES = {20002:141,20008:17,20009:101,20010:106,20011:151,20012:111,
              20013:126,20014:146,20015:136,20016:22,20017:23,20018:24,20019:25}
BINDINGS = {1:20008,2:20016,5:20017,3:20018,4:20019}
FEMALE_BAKE = Path.home()/'.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl'


def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream,'sha256').hexdigest()


def build_manifest(save=DEFAULT_SAVE, database=DEFAULT_DATABASE):
    rows,sources,guid=records(save)
    sources += [database,ROOT/'Assets/Scripts/PeEntity/Cmpt/CommonCmpt.cs',
                ROOT/'Assets/Scripts/PeEntity/Cmpt/AvatarCmpt.cs',
                ROOT/'Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs',
                ROOT/'Assets/Scripts/SkillSystem/SkAliveEntity.cs',
                ROOT/'Assets/Scripts/SkillSystem/SkAttribs.cs']
    before={p:digest(p) for p in sources}
    entities,_=decode_entities(rows)
    roles,_=decode_progress(rows['ArchiveKeyMisRepository'])
    if roles!=BINDINGS: raise ValueError('Unexpected population role map')
    actual={i:e['prototype_id'] for i,e in entities.items() if e['prototype_type']==1}
    if actual!=IDENTITIES: raise ValueError('Archived population differs from pinned individuals')
    people=[]
    with closing(sqlite3.connect(database.resolve().as_uri()+'?mode=ro',uri=True)) as db:
        db.row_factory=sqlite3.Row
        for identity,prototype in sorted(IDENTITIES.items()):
            role=next((r for r,i in roles.items() if i==identity),0)
            entity=entities[identity]
            if entity['prefab']!='EntityNpc': raise ValueError('Unexpected NPC entity prefab')
            pose=transform(entity)
            alive=Reader(entity['components']['SkAliveEntity']);alive.version(3)
            values=alive.floats((len(alive.data)-4)//4);alive.end()
            if len(values)<2 or values[0]<=0 or values[1]<=0: raise ValueError('Expected living archived NPC')
            appearance=decode(save,identity,database,expected=dict(prototype_id=prototype,adventure_role=role))
            common=Reader(entity['components']['CommonCmpt']);common.boolean();sex=common.integer();common.unpack('iiii');common.end()
            if sex!=(2 if appearance['sex']=='male' else 1): raise ValueError('NPC sex mismatch')
            q=pose['rotation'];yaw=math.degrees(math.atan2(2*(q[3]*q[1]+q[0]*q[2]),1-2*(q[1]*q[1]+q[2]*q[2])))
            associations=[dict(r) for r in db.execute('SELECT * FROM AdRandNPCMission WHERE CAST(RNPC_ID AS INTEGER)=? ORDER BY CAST(ID AS INTEGER)',(prototype,))]
            scene=('adventure-welcome' if identity==20008 else 'adventure-supplier' if identity==20016 else f'adventure-population/{identity}')+'/recording.peas'
            people.append(dict(entity_id=identity,prototype_id=prototype,adventure_role=role,
                town_id=4 if role in (3,4) else 0,name=appearance['name'],sex=appearance['sex'],
                hp=values[1],hp_max=values[0],yaw_degrees=yaw,**pose,scene=scene,
                in_starting_crop=(-627<=pose['position'][0]<-563 and 323<=pose['position'][2]<451),
                role_candidates=associations,appearance=appearance))
    if any(digest(p)!=value for p,value in before.items()): raise ValueError('Population inputs changed while reading')
    return dict(schema=1,archive_guid=guid,npcs=people,
        sources=[dict(path=source_label(p),sha256=v) for p,v in before.items()],
        limitations=['Pinned living archived population, not procedural generation.',
                     'Poses preserve archived snapshots; no schedules, wandering or AI.',
                     'Database role candidates are provenance, not inferred saved mission assignments.',
                     'Jailekha20015 lies outside the original starting crop; never relocate to fit.',
                     'Annot roles3/4 use saved Male nude base model despite empty lazy-loaded prefab path.'])



ANNOT_IDENTITIES={20027:123,20028:153,20029:103,20030:113,20031:158,20032:163,
                  20033:143,20034:133,20035:173,20036:108,20037:148,20038:118}
DEFAULT_OBSERVATION=ROOT/'rebuild/build/reference-assets/adventure-route/terrain.pevx.json'


def fixed_point(entity):
    r=Reader(entity['components']['NpcCmpt']);r.version(12)
    r.unpack('iii') # master, revive time, medical state
    for _ in range(r.count(128)):r.integer()
    r.integer();point=r.floats(3);r.integer();r.floats(1);r.unpack('ii')
    for _ in range(3):r.boolean()
    r.unpack('ii');r.end();return point


def extended_manifest(save,database=DEFAULT_DATABASE,observation=DEFAULT_OBSERVATION):
    # Keep baseline identities, appearance and archived poses exactly as before.
    result=build_manifest(DEFAULT_SAVE,database)
    rows,paths,guid=records(save);entities,_=decode_entities(rows)
    observed=json.loads(observation.read_text())
    if not observed.get('complete') or observed.get('origin')!=[-640,0,64] or observed.get('size')!=[416,64,416]:
        raise ValueError('Population requires the complete observed route envelope')
    towns=[t for t in observed['towns'] if t['id']==4]
    if len(towns)!=1 or towns[0]['name']!='Annot':raise ValueError('Missing observed Annot bounds')
    town=towns[0];low=town['startXZ'];high=town['endXZ']
    def inside(p):return low[0]<=p[0]<=high[0] and low[1]<=p[2]<=high[1]
    if any(i not in entities or entities[i]['prototype_id']!=prototype or entities[i]['prototype_type']!=1 for i,prototype in IDENTITIES.items()):
        raise ValueError('Fresh archive changed a baseline NPC identity')
    selected={};excluded=[]
    for i,e in entities.items():
        if e['prototype_type']!=1 or i in IDENTITIES:continue
        pose=transform(e);anchor=fixed_point(e)
        if inside(anchor):selected[i]=e['prototype_id']
        else:excluded.append(dict(entity_id=i,prototype_id=e['prototype_id'],position=pose['position'],fixed_point=anchor,reason='outside observed Annot bounds'))
    if selected!=ANNOT_IDENTITIES:raise ValueError('Fresh Annot population differs from pinned observed individuals')
    paths += [observation,ROOT/'Assets/Scripts/PeEntity/Cmpt/NpcCmpt.cs']
    before={p:digest(p) for p in paths}
    with closing(sqlite3.connect(database.resolve().as_uri()+'?mode=ro',uri=True)) as db:
        db.row_factory=sqlite3.Row
        for identity,prototype in sorted(selected.items()):
            e=entities[identity];pose=transform(e);alive=Reader(e['components']['SkAliveEntity']);alive.version(3)
            values=alive.floats((len(alive.data)-4)//4);alive.end()
            if len(values)<2 or min(values[:2])<=0:raise ValueError('Expected living Annot resident')
            a=decode(save,identity,database,expected=dict(prototype_id=prototype,adventure_role=0))
            q=pose['rotation'];yaw=math.degrees(math.atan2(2*(q[3]*q[1]+q[0]*q[2]),1-2*(q[1]*q[1]+q[2]*q[2])))
            candidates=[dict(row) for row in db.execute('SELECT * FROM AdRandNPCMission WHERE CAST(RNPC_ID AS INTEGER)=? ORDER BY CAST(ID AS INTEGER)',(prototype,))]
            result['npcs'].append(dict(entity_id=identity,prototype_id=prototype,adventure_role=0,town_id=4,
                name=a['name'],sex=a['sex'],hp=values[1],hp_max=values[0],yaw_degrees=yaw,**pose,
                scene=f'adventure-population/{identity}/recording.peas',in_starting_crop=False,role_candidates=candidates,
                appearance=a,fixed_point=fixed_point(e),selection='saved fixed point inside observed Annot bounds'))
    if any(digest(p)!=sha for p,sha in before.items()):raise ValueError('Annot inputs changed while reading')
    result['sources'] += [dict(path=source_label(p),sha256=sha) for p,sha in before.items()]
    result['extension']=dict(save_directory=str(save.resolve()),observation=source_label(observation),archive_guid=guid,
                             town=town,excluded_npcs=excluded,baseline_preserved=True)
    result['limitations'].append('Twelve Annot residents appended from the newly observed save; baseline13 retain their original snapshot poses, despite later source wandering/streaming reset.')
    return result


def header(manifest):
    out=['/* Generated by export_adventure_population.py; archived individuals only. */',
         '#ifndef PE_ADVENTURE_POPULATION_DATA_H','#define PE_ADVENTURE_POPULATION_DATA_H',
         'typedef struct { int entity_id, prototype_id, adventure_role, town_id, male; const char *name, *scene; float position[3], yaw_degrees; } PeAdventurePopulationSource;',
         '#define PE_ADVENTURE_POPULATION_COUNT '+str(len(manifest['npcs'])),
         'static const PeAdventurePopulationSource pe_adventure_population_sources[PE_ADVENTURE_POPULATION_COUNT] = {']
    def number(v): return format(float(v),'.9f')+'f'
    for n in manifest['npcs']:
        out.append('    {%d, %d, %d, %d, %d, %s, %s, {%s}, %s},' % (
            n['entity_id'],n['prototype_id'],n['adventure_role'],n['town_id'],n['sex']=='male',
            c_string(n['name']),c_string(n['scene']),', '.join(map(number,n['position'])),number(n['yaw_degrees'])))
    return '\n'.join(out+['};','#endif',''])


def export_actor(game, output, appearance, capture):
    import analyze_reference_bake as bake
    from export_adventure_supplier import recover
    from export_adventure_npc_collider import source_capsule
    if output.exists(): raise ValueError('Actor output must be fresh')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.population-',dir=output.parent) as tmp:
        staging=Path(tmp);research=staging/'source';research.mkdir()
        rig=recover(game,research,appearance,str(appearance['entity_id']))
        rig_path=research/'rig.json';rig_path.write_text(json.dumps(rig,separators=(',',':'),allow_nan=False)+'\n')
        report,samples=bake.analyze(capture,rig,retain=True)
        if report['rig_mapping']['missing_bones']: raise ValueError('Idle lacks required source bones')
        clip=next(c for c in report['clips'] if c['name']=='NormalIdle')
        for mount in rig.get('attachment_mounts',[]):
            mapped=report['rig_mapping']['bones'][mount['name']]
            if mapped['relative_path']!=mount['path']: raise ValueError('Source/bake equipment mount path differs')
            node=report['skeleton']['nodes'][mapped['index']]
            bake.pose.close_matrix(bake.pose.matrix(node['initialLocal'],'baked mount'),mount['local'],'source equipment mount')
        result=bake.export_clip(staging,rig,rig_path,samples['NormalIdle'],report,clip)
        binary,collider=source_capsule(appearance['sex']);(staging/'collider.pesc').write_bytes(binary)
        (research/'bake-report.json').write_text(json.dumps(report,indent=2)+'\n')
        result.update(appearance=appearance,avatar=rig['avatar']['m_Name'],collider=collider,
                      sources=rig['sources']+appearance['sources']+[dict(path=str(capture),sha256=digest(capture))])
        (staging/'population-manifest.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
        staging.rename(output)
    return dict(entity_id=appearance['entity_id'],parts=len(result['parts']),frames=result['frames'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--save-dir',type=Path);p.add_argument('--database',type=Path,default=DEFAULT_DATABASE)
    p.add_argument('--output',type=Path,default=DEFAULT_OUTPUT);p.add_argument('--check',action='store_true')
    p.add_argument('--observation',type=Path,default=DEFAULT_OBSERVATION)
    p.add_argument('--assets',type=Path);p.add_argument('--entity',type=int,action='append')
    p.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--female-bake',type=Path,default=FEMALE_BAKE);p.add_argument('--male-bake',type=Path)
    a=p.parse_args()
    try:
        save=a.save_dir
        # A generated extension records its reproducible local inputs; preserve
        # that scope during regeneration unless --save-dir explicitly changes it.
        # The pinned identity/pose validation still runs against both archives.
        if save is None:
            existing=a.output/'pe_adventure_population.json'
            if existing.is_file():
                extension=json.loads(existing.read_text()).get('extension')
                if extension:save=Path(extension['save_directory']);a.observation=ROOT/extension['observation']
        a.save_dir=(save or DEFAULT_SAVE).resolve()
        manifest=(build_manifest(a.save_dir,a.database) if a.save_dir==DEFAULT_SAVE.resolve()
                  else extended_manifest(a.save_dir,a.database,a.observation))
        if a.assets:
            if a.check: raise ValueError('--check is metadata-only')
            selected=a.entity or [n['entity_id'] for n in manifest['npcs'] if n['entity_id'] not in (20008,20016)]
            if len(set(selected))!=len(selected) or not set(selected)<={n['entity_id'] for n in manifest['npcs']}: raise ValueError('Unknown/duplicate actor ID')
            config=a.save_dir.parents[1]/'Config/SystemSaveData.xml'
            settings=ET.parse(config).getroot().find('HideHeadgear')
            if settings is None or settings.get('value')!='False': raise ValueError('Expected archived HideHeadgear=False')
            for n in manifest['npcs']:
                if n['entity_id'] not in selected: continue
                capture=a.male_bake if n['sex']=='male' else a.female_bake
                if capture is None: raise ValueError('Male actors require an exact MaleAvatar idle bake')
                appearance=n['appearance'];appearance['hide_headgear']=False
                appearance['sources'].append(dict(path=str(config),sha256=digest(config)))
                target=a.assets.resolve()/str(n['entity_id'])
                for source in (a.save_dir.resolve(),a.game_dir.resolve()):
                    if target==source or source in target.parents or target in source.parents: raise ValueError('Output overlaps protected input')
                print(json.dumps(export_actor(a.game_dir,target,appearance,capture)),flush=True)
        else:
            outputs={'pe_adventure_population.h':header(manifest),'pe_adventure_population.json':json.dumps(manifest,indent=2,ensure_ascii=False,allow_nan=False)+'\n'}
            for name,body in outputs.items():
                path=a.output/name
                if a.check:
                    if not path.is_file() or path.read_text()!=body: raise ValueError('Stale population output: '+name)
                else: path.parent.mkdir(parents=True,exist_ok=True);path.write_text(body)
            print('Verified' if a.check else 'Exported',len(manifest['npcs']),'archived living NPCs')
    except (OSError,ValueError,KeyError,IndexError,ImportError,sqlite3.Error,ET.ParseError) as e: p.exit(1,'Population export: '+str(e)+'\n')

if __name__=='__main__': main()
