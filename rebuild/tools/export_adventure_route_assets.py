#!/usr/bin/env python3
"""Recover saved living town0/town4 buildings into one source-coordinate route.

Requires a newly saved archive after the original has loaded Annot. No placement
or missing building is invented, and output publication requires fresh paths.
"""
import argparse
from contextlib import closing
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import sqlite3
import tempfile

from export_adventure_settlement import decode_entities, transform, numeric_vector
from export_adventure_settlement_assets import recover, GAME
from read_reference_adventure import ROOT, DEFAULT_SAVE, Reader, records, source_label
from export_content import DEFAULT_DATABASE

ORIGIN=[-640.,0.,64.]


def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def town_buildings(data):
    r=Reader(data);r.version(20160902)
    for _ in range(r.count()):
        r.unpack('ii');values=r.unpack('dd')
        import math
        if not all(math.isfinite(v) for v in values):raise ValueError('Nonfinite town state')
    towns={};seen=set()
    for _ in range(r.count()):
        town=r.integer()
        if town<0 or town in towns:raise ValueError('Duplicate/invalid town')
        ids=[r.integer() for _ in range(r.count())]
        if any(i<=0 or i in seen for i in ids) or len(ids)!=len(set(ids)):raise ValueError('Duplicate/invalid building identity')
        seen.update(ids);towns[town]=ids
    r.end();return towns


def build_manifest(save,database=DEFAULT_DATABASE,base_save=DEFAULT_SAVE,require_annot=True):
    rows,paths,guid=records(save);entities,_=decode_entities(rows)
    towns=town_buildings(rows['VArtifactTownArchiveMgr'])
    if not towns.get(0) or (require_annot and not towns.get(4)):raise ValueError('Archive must contain loaded living town0 and Annot town4')
    base_rows,base_paths,base_guid=records(base_save)
    base_entities,_=decode_entities(base_rows)
    base_towns=town_buildings(base_rows['VArtifactTownArchiveMgr'])
    if set(towns[0])!=set(base_towns[0]):raise ValueError('Starting living building set changed')
    paths=list(dict.fromkeys(paths+base_paths+[database]))
    before={p:digest(p) for p in paths};buildings=[]
    with closing(sqlite3.connect(database.resolve().as_uri()+'?mode=ro',uri=True)) as db:
        db.row_factory=sqlite3.Row
        for town in (0,4):
            for identity in sorted(towns.get(town,[])):
                e=entities[identity]
                if e['prefab']!='EntityDoodad' or e['prototype_type']!=5:raise ValueError('Alive building is not a doodad')
                pose=transform(e)
                alive=Reader(e['components']['SkAliveEntity']);alive.version(3)
                values=alive.floats((len(alive.data)-4)//4);alive.end()
                if len(values)<2 or values[0]<=0 or values[1]<=0:raise ValueError('Alive building list contains a dead building')
                if town==0 and (e['prototype_id']!=base_entities[identity]['prototype_id'] or pose!=transform(base_entities[identity])):raise ValueError('Starting building source pose changed')
                matches=db.execute('SELECT * FROM Townhouse WHERE CAST(PrototypeDoodad_Id AS INTEGER)=? ORDER BY CAST(ID AS INTEGER)',(e['prototype_id'],)).fetchall()
                prototypes=db.execute('SELECT asset_path FROM PrototypeDoodad WHERE CAST(id AS INTEGER)=?',(e['prototype_id'],)).fetchall()
                if not matches or len(prototypes)!=1:raise ValueError('Missing/ambiguous source doodad '+str(e['prototype_id']))
                resource=prototypes[0]['asset_path']
                if {h['FilePath'] for h in matches}!={resource}:raise ValueError('Townhouse/doodad resource disagrees')
                if not resource.startswith('Item/') or not resource.endswith('.unity3d') or '..' in Path(resource).parts:raise ValueError('Invalid building resource path')
                # Different procedural layout rows can share one exact doodad
                # bundle. Preserve candidates without inventing a layout size.
                candidates=[]
                for house in matches:
                    dimensions=numeric_vector(house['Size'],2)
                    if any(v<=0 for v in dimensions):raise ValueError('Invalid building source size')
                    candidates.append(dict(townhouse_id=int(house['ID']),dimensions=dimensions))
                buildings.append(dict(entity_id=identity,prototype_id=e['prototype_id'],town_id=town,
                                      townhouse_candidates=candidates,resource=resource,hp_max=values[0],hp=values[1],**pose))
    if len(buildings)>128:raise ValueError('Route building bound exceeded')
    if any(digest(p)!=sha for p,sha in before.items()):raise ValueError('Route sources changed during read')
    return dict(schema=1,archive_guid=guid,baseline_archive_guid=base_guid,crop_origin=ORIGIN,buildings=buildings,
                sources=[dict(path=source_label(p),sha256=sha) for p,sha in before.items()])


def same_apart_from_checkout(installed,fresh):
    """Whether an installed manifest equals a fresh one except that it labels an
    in-repository source by the absolute checkout path it was installed from.
    Hashes and every other value must match; the installed record is not rewritten."""
    if isinstance(fresh,dict):return isinstance(installed,dict) and installed.keys()==fresh.keys() and all(same_apart_from_checkout(installed[k],v) for k,v in fresh.items())
    if isinstance(fresh,list):return isinstance(installed,list) and len(installed)==len(fresh) and all(map(same_apart_from_checkout,installed,fresh))
    if installed==fresh and type(installed) is type(fresh):return True
    return (isinstance(installed,str) and isinstance(fresh,str) and os.path.isabs(installed)
            and not os.path.isabs(fresh) and (ROOT/fresh).is_file() and installed.endswith('/'+fresh))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--save-dir',type=Path,required=True)
    p.add_argument('--base-save-dir',type=Path,default=DEFAULT_SAVE);p.add_argument('--database',type=Path,default=DEFAULT_DATABASE)
    p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/adventure-route')
    p.add_argument('--check',action='store_true');p.add_argument('--metadata-only',action='store_true')
    a=p.parse_args()
    try:
        manifest=build_manifest(a.save_dir.resolve(),Path(os.path.abspath(a.database)),a.base_save_dir.resolve())
        if a.metadata_only:print(json.dumps(manifest,indent=2));return 0
        if importlib.metadata.version('UnityPy')!='1.25.4':raise ValueError('Requires UnityPy1.25.4')
        output=a.output.resolve();game=a.game_dir.resolve()
        for source in (game,a.save_dir.resolve(),a.base_save_dir.resolve()):
            if output==source or source in output.parents or output in source.parents:raise ValueError('Output overlaps input')
        # The route terrain may already exist; publish only these bounded names.
        names=['route.obj','route-collision.obj','route-manifest.json','settlement.mtl']
        if not a.check and output.exists() and any((output/name).exists() for name in names):raise ValueError('Route geometry output already exists; use a fresh directory or --check')
        output.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.route-assets-',dir=output.parent) as tmp:
            staging=Path(tmp);report=recover(game,staging,manifest,ORIGIN,compact=True)
            report['visual_vertex_policy']='Only referenced vertices per material submesh; exact original triangle positions/normals/UVs retained'
            (staging/'settlement.obj').rename(staging/'route.obj')
            (staging/'settlement-collision.obj').rename(staging/'route-collision.obj')
            report['buildings']=manifest['buildings'];report['archive_guid']=manifest['archive_guid']
            report['files']={path.name:dict(bytes=path.stat().st_size,sha256=digest(path)) for path in sorted(staging.iterdir())}
            (staging/'route-manifest.json').write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
            if a.check:
                for path in staging.iterdir():
                    installed=output/path.name
                    if not installed.is_file():raise ValueError('Stale route asset '+path.name)
                    if path.read_bytes()==installed.read_bytes():continue
                    if path.name!='route-manifest.json' or not same_apart_from_checkout(json.loads(installed.read_text()),json.loads(path.read_text())):raise ValueError('Stale route asset '+path.name)
            else:
                output.mkdir(exist_ok=True)
                for path in staging.iterdir():
                    if (output/path.name).exists():raise ValueError('Route output collision '+path.name)
                for path in sorted(staging.iterdir(),key=lambda path:path.name=='route-manifest.json'):path.rename(output/path.name)
        print(json.dumps(dict(output=str(output),buildings=len(manifest['buildings']),visual=report['visual'],collision=report['collision'])))
    except (OSError,ValueError,KeyError,ImportError,sqlite3.Error,importlib.metadata.PackageNotFoundError) as e:p.exit(1,'Route asset export: '+str(e)+'\n')

if __name__=='__main__':main()
