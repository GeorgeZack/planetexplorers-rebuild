#!/usr/bin/env python3
"""Export source tutorial gathering data for review; no runtime/mission changes.
Requires optional UnityPy 1.25.4; output is a JSON audit fixture, not render assets.
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import re
import sqlite3
import tempfile

from export_reference_actor import local_matrix
from export_reference_assets import multiply, transform_point

ROOT = Path(__file__).resolve().parents[2]
PREFAB = ROOT/'Assets/Scripts/Jaluca/TrainingField/Resources/TrainingManager_New.prefab'
TERRAIN = ROOT/'Assets/Scripts/Jaluca/TrainingField/TerrainDigTask.cs'
DATABASE = ROOT/'Assets/Resources/DataBase/localData.bytes'
SELECTED = {11405434:'HoloherbTask',11473516:'HolotreeTask',11496244:'HolotreeCutter',11480128:'TerrainDigTask',
            11438508:'HoloherbGather',11405274:'HoloherbGather',11434286:'HoloherbGather',11409872:'HoloherbGather',11412708:'HoloherbGather'}


def terrain_samples(text):
    """Interpret only the bounded literals/loops/SafeWrite grammar used here."""
    body = text.split('void CreatTerrian()',1)[1].split('IEnumerator FindTerrian()',1)[0].strip()
    if not body.startswith('{') or not body.endswith('}'):raise ValueError('Unexpected terrain method')
    body=body[1:-1];volumes={};loops=[];pending=None;out={}
    for line in body.splitlines():
        line=line.strip()
        if not line:continue
        m=re.fullmatch(r'VFVoxel (vol\d+) = new VFVoxel\(\(byte\)(\d+), (\d+)\);',line)
        if m:
            volumes[m[1]]=(int(m[2]),int(m[3]));continue
        m=re.fullmatch(r'for \(int ([ij]) = (-?\d+); \1 <= (-?\d+); \1\+\+\)',line)
        if m:
            if pending or len(loops)>=3 or int(m[3])-int(m[2]) not in range(64):raise ValueError('Unbounded terrain loop')
            pending=(m[1],range(int(m[2]),int(m[3])+1));continue
        if line=='{' and pending:loops.append(pending);pending=None;continue
        if line=='}' and loops:loops.pop();continue
        m=re.fullmatch(r'VFVoxelTerrain.self.Voxels.SafeWrite\((-?\d+|[ij]), (-?\d+|[ij]), (-?\d+|[ij]), (vol\d+)\);',line)
        if m:
            for values in itertools.product(*(item[1] for item in loops)):
                env=dict(zip((item[0] for item in loops),values));xyz=tuple(env[x] if x in env else int(x) for x in m.group(1,2,3))
                if xyz in out:raise ValueError('Duplicate terrain sample')
                out[xyz]=volumes[m[4]]
            continue
        raise ValueError('Unsupported terrain source statement: '+line)
    if pending or loops or len(out)>10000:raise ValueError('Incomplete terrain grammar')
    return [list(xyz)+list(out[xyz]) for xyz in sorted(out)]


def build():
    import UnityPy
    environment=UnityPy.load(str(PREFAB));objects={obj.path_id:obj for obj in environment.objects}
    transforms={pid:o.read_typetree() for pid,o in objects.items() if o.type.name=='Transform'}
    world={}
    def matrix(pid,seen=()):
        if pid in world:return world[pid]
        if pid in seen or len(seen)>64:raise ValueError('Invalid transform hierarchy')
        t=transforms[pid];m=local_matrix([t['m_LocalPosition'][k] for k in 'xyz'],[t['m_LocalRotation'][k] for k in 'xyzw'],[t['m_LocalScale'][k] for k in 'xyz'])
        parent=t['m_Father']['m_PathID']
        if parent:m=multiply(matrix(parent,seen+(pid,)),m)
        world[pid]=m;return m
    props=[]
    for pid,kind in SELECTED.items():
        obj=objects[pid];data=obj.read_typetree();go=objects[data['m_GameObject']['m_PathID']].read_typetree()
        references=[(x['component'] if isinstance(x,dict) else x[1])['m_PathID'] for x in go['m_Component']]
        transform=next(x for x in references if x in transforms)
        script=data['m_Script'];raw=obj.assets_file.externals[script['m_FileID']-1].guid.hex()
        guid=''.join(raw[i+1]+raw[i] for i in range(0,len(raw),2))
        meta=(ROOT/'Assets/Scripts/Jaluca/TrainingField'/f'{kind}.cs.meta').read_text()
        if 'guid: '+guid not in meta:raise ValueError('Source component changed')
        colliders=[dict(path_id=x,type=objects[x].type.name,fields=objects[x].read_typetree()) for x in references if objects[x].type.name.endswith('Collider')]
        props.append(dict(class_name=kind,component_id=pid,game_object_id=data['m_GameObject']['m_PathID'],name=go['m_Name'],
                          transform_id=transform,world_matrix=matrix(transform),position=transform_point(matrix(transform),[0,0,0]),
                          fields={k:v for k,v in data.items() if not k.startswith('m_')},colliders=colliders))
    db=sqlite3.connect(DATABASE.as_uri()+'?mode=ro',uri=True);db.row_factory=sqlite3.Row
    def rows(table,key,ids):
        return [dict(r) for r in db.execute(f'SELECT * FROM "{table}" WHERE CAST("{key}" AS INTEGER) IN ({",".join("?" for _ in ids)})',ids)]
    missions=rows('Quest_List','ID',[719,748,757,721,751,752]);plots=rows('Plot','ID',[432,433,434,435,436,437]);targets={}
    for table,ids in [('Quest_EnterArea',[4066,4122,4123,4105]),('Quest_LootItem',[2093,2089,2032]),('Quest_UseItem',[5023])]:targets[table]=rows(table,'TargetID',ids)
    talks=set([300241,4078])
    for m in missions:
        for field in ('TalkOP','TalkED'):
            talks.update(int(x) for x in m[field].split(':')[0].split(',') if x!='0')
    for p in plots:talks.update(int(x) for x in p['ScenarioID'].split(',') if x!='0')
    dialogue=[]
    for r in rows('Scenario','talk_id',sorted(talks)):
        translation_id=int(r['talk_content'].split(':')[0]);text=rows('Translation','ID',[translation_id])[-1]['English']
        dialogue.append(dict(id=int(r['talk_id']),localization_id=translation_id,speaker=r['npc_id'],english=text))
    recipes=rows('synthesis_skill','ID',[1]);resources=rows('resource','ID',[1]);items=rows('PrototypeItem','id',[12,406,1527,268,279]);db.close()
    sources=[PREFAB,TERRAIN,DATABASE,*[ROOT/'Assets/Scripts/Jaluca/TrainingField'/f'{name}.cs' for name in sorted(set(SELECTED.values()))]]
    return dict(format='PE-training-gathering-audit-1',runtime_integrated=False,
                sources=[dict(path=str(p.relative_to(ROOT)),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in dict.fromkeys(sources)],
                source_prefab_props=props,missions=missions,plots=plots,targets=targets,dialogue=dialogue,recipes=recipes,resources=resources,items=items,
                terrain_columns=['x','y','z','density','material'],terrain_coordinates='absolute Unity world voxels, not prefab-local',
                terrain_samples=terrain_samples(TERRAIN.read_text(encoding='utf-8-sig')))


def compare_installed(game, report):
    """Resolve selected resources; never deserialize unrelated meshes/textures."""
    import UnityPy
    from UnityPy.helpers.TypeTreeNode import TypeTreeNode
    from export_reference_player import Assets
    source=UnityPy.load(str(PREFAB));objects={o.path_id:o for o in source.objects}
    schemas={kind:objects[pid].serialized_type.node for pid,kind in SELECTED.items()}
    assets=Assets(game/'PE_Client_Data')
    manager=next(o for o in assets.file('mainData').objects.values() if o.type.name=='ResourceManager')
    containers=manager.read_typetree()['m_Container']
    def resource(name,kind):
        matches=[assets.resolve(manager,p) for n,p in containers if n==name]
        matches=[o for o in matches if o.type.name==kind]
        if len(matches)!=1:raise ValueError('Ambiguous installed resource '+name)
        return matches[0]
    root=resource('trainingmanager_new','GameObject');props=[];visited=set()
    def walk(go,parent=None,path=''):
        identity=(go.assets_file.name,go.path_id)
        if identity in visited or len(visited)>=5000:raise ValueError('Invalid installed hierarchy')
        visited.add(identity)
        transform,=(assets.components(go,'Transform') or assets.components(go,'RectTransform'))
        t=transform.read_typetree();local=local_matrix([t['m_LocalPosition'][k] for k in 'xyz'],[t['m_LocalRotation'][k] for k in 'xyzw'],[t['m_LocalScale'][k] for k in 'xyz'])
        world=multiply(parent,local) if parent else local;name=go.peek_name();path=path+'/'+name
        for component in assets.components(go,'MonoBehaviour'):
            header=component.read_typetree(check_read=False)
            kind=assets.resolve(component,header['m_Script']).read_typetree()['m_ClassName']
            if kind not in schemas:continue
            # Installed MonoBehaviour trees omit custom fields. Combine the
            # actual player header with source custom fields, requiring exact
            # byte coverage; never silently accept a different layout.
            node=TypeTreeNode.from_list([{k:v for k,v in n.items() if k!='m_Children'} for n in component._get_typetree_node().to_dict_list()])
            node.m_Children=list(node.m_Children)+[n for n in schemas[kind].m_Children if not n.m_Name.startswith('m_')]
            data=component.read_typetree(nodes=node)
            expected=next(p for p in report['source_prefab_props'] if p['class_name']==kind and p['name']==name)
            position=transform_point(world,[0,0,0])
            props.append(dict(class_name=kind,path=path,component_id=component.path_id,position=position,
                              source_component_id=expected['component_id'],position_delta=[a-b for a,b in zip(position,expected['position'])],
                              serialized_counters={k:data[k] for k in ('herbCount','MaxHP','HP') if k in data}))
        for pointer in t['m_Children']:
            child=assets.resolve(transform,pointer);walk(assets.resolve(child,child.read_typetree()['m_GameObject']),world,path)
    walk(root)
    database=resource('database/localdata','TextAsset').read().m_Script
    if isinstance(database,str):database=database.encode('utf-8','surrogateescape')
    installed_hash=hashlib.sha256(database).hexdigest()
    localization=sqlite3.connect((game/'i18n.db').as_uri()+'?mode=ro',uri=True)
    differences=[]
    for row in report['dialogue']:
        found=localization.execute('SELECT English FROM Translation WHERE ID=?',(str(row['localization_id']),)).fetchall()
        if not found or found[-1][0]!=row['english']:differences.append(row['id'])
    localization.close()
    return dict(game_directory=str(game),resource_root_id=root.path_id,props=props,
                database_sha256=installed_hash,database_byte_identical=installed_hash==hashlib.sha256(DATABASE.read_bytes()).hexdigest(),
                dialogue_english_differences=differences,behavior_executed=False,
                note='Custom fields decoded with source schemas and strict full-byte consumption; installed managed-code behavior was not decompiled.')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/training-gathering.json')
    parser.add_argument('--installed-game',type=Path,help='Optional read-only installed resource/database comparison');args=parser.parse_args()
    result=build()
    if args.installed_game:
        game=args.installed_game.expanduser().resolve()
        if args.output.resolve()==game or game in args.output.resolve().parents:raise ValueError('Output must be outside original installation')
        result['installed_comparison']=compare_installed(game,result)
    payload=json.dumps(result,indent=2,sort_keys=True)+'\n';args.output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile('w',dir=args.output.parent,prefix='.gathering-',delete=False) as f:f.write(payload);temporary=Path(f.name)
    temporary.replace(args.output);print(f'Exported {len(result["source_prefab_props"])} props, {len(result["terrain_samples"])} terrain samples to {args.output}')
if __name__=='__main__':main()
