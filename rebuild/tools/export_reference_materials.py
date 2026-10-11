#!/usr/bin/env python3
"""Source-bound material sidecars for the two installed settlement OBJ groups.

Preserves every OBJ/MTL/PNG and archived manifest. Stages all policies before
optional installation, retaining prior sidecars and current input manifests.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

from export_actor_materials import ROOT,Resolver,check_source,digest,fnv,policy


def material_order(mtl):
    rows=[]
    for line in mtl.read_text(encoding='ascii').splitlines():
        tokens=line.split()
        if tokens and tokens[0]=='newmtl':
            if len(tokens)!=2 or any(r[0]==tokens[1] for r in rows):raise ValueError('Invalid/duplicate MTL symbol')
            rows.append([tokens[1],None])
        elif tokens and tokens[0]=='map_Kd':
            if len(tokens)!=2 or not rows or rows[-1][1]:raise ValueError('Invalid/duplicate MTL texture')
            rows[-1][1]=tokens[1]
    if not 1<=len(rows)<=256:raise ValueError('Invalid MTL count')
    return rows


def sidecar(obj,mtl,materials,rules):
    by_symbol={m['symbol']:m for m in materials}
    if len(by_symbol)!=len(materials):raise ValueError('Duplicate recorded symbol')
    rows=material_order(mtl)
    if set(by_symbol)!={r[0] for r in rows}:raise ValueError('MTL/manifest symbols differ')
    text=[f'PERM1 {fnv(obj.read_bytes()):016x} {fnv(mtl.read_bytes()):016x}']
    for symbol,texture in rows:
        material=by_symbol[symbol];rule=rules[symbol]
        if texture!=material['texture_file']:raise ValueError('MTL/manifest texture differs')
        if texture and (Path(texture).name!=texture or texture.startswith('.') or '..' in texture):raise ValueError('Invalid texture basename')
        image=obj.parent/texture if texture else None
        hash_value=fnv(image.read_bytes()) if image else 0
        text.append(f"{symbol} {texture or '-'} {rule['mode']} {rule['cutoff']:.9g} {rule['alpha']:.9g} {int(rule['two_sided'])} {hash_value:016x}")
    return '\n'.join(text)+'\n'


def recover(obj,manifest,resolver):
    report=json.loads(manifest.read_text());mtl=obj.parent/'settlement.mtl'
    for name in (obj.name,mtl.name,*[m['texture_file'] for m in report['materials'] if m['texture_file']]):
        path=obj.parent/name
        if Path(name).name!=name or path.is_symlink() or digest(path)!=report['files'][name]['sha256']:
            raise ValueError('Recorded model/texture hash changed: '+name)
    references={};sources=[]
    for asset in report['assets']:
        source=Path(asset['source'])
        if source.is_symlink() or digest(source)!=asset['sha256']:raise ValueError('Recorded source bundle hash changed')
        env=resolver.register_bundle(source)
        objects={o.path_id:o for o in env.objects}
        renderers={}
        for obj_reader in objects.values():
            if obj_reader.type.name in ('MeshRenderer','SkinnedMeshRenderer'):
                tree=obj_reader.read_typetree()
                renderers.setdefault(tree['m_GameObject']['m_PathID'],[]).append((obj_reader,tree))
        for part in report['parts']:
            if part['entity_id']!=asset['entity_id'] or part['collision']:continue
            candidates=renderers.get(part['game_object_id'],[])
            if len(candidates)!=1:raise ValueError('Ambiguous source renderer')
            renderer,tree=candidates[0]
            ids=[p['m_PathID'] for p in tree['m_Materials']]
            if not tree.get('m_Enabled') or any(p['m_FileID'] for p in tree['m_Materials']) or ids!=[p[1] for p in part['materials']]:
                raise ValueError('Recorded renderer/material relationship changed')
            for identity in ids:
                references.setdefault((asset['entity_id'],identity),[]).append((objects[identity].assets_file.name,renderer.path_id))
        sources.append(dict(entity_id=asset['entity_id'],path=str(source),sha256=asset['sha256']))
    rules={};evidence=[]
    for material in report['materials']:
        refs=references[(material['entity_id'],material['path_id'])];selected=None;resolved_rows=[]
        for asset_name,renderer_id in refs:
            resolved=resolver.material(dict(asset=asset_name,path_id=material['path_id'],name=material['name']),renderer_id)
            if json.loads(json.dumps(resolved['saved']))!=material['properties']:raise ValueError('Recorded material properties changed')
            rule=policy(resolved.get('effective_shader',resolved['shader']),resolved['keywords'],resolved['saved'],{})
            if selected is not None and selected!=rule:raise ValueError('Shared material has conflicting renderer policies')
            selected=rule
            resolved_rows.append(dict(renderer_id=renderer_id,asset=asset_name,**{k:v for k,v in resolved.items() if k!='saved'}))
        rules[material['symbol']]=selected
        evidence.append(dict(symbol=material['symbol'],entity_id=material['entity_id'],material_path_id=material['path_id'],name=material['name'],**selected,renderers=resolved_rows))
    return sidecar(obj,mtl,report['materials'],rules),dict(manifest_sha256=digest(manifest),obj_sha256=digest(obj),mtl_sha256=digest(mtl),sources=sources,materials=evidence)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets',type=Path,default=ROOT/'rebuild/build/reference-assets')
    parser.add_argument('--game-data',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers/PE_Client_Data')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--install',action='store_true')
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    if args.check and (args.output or args.install):parser.error('--check does not install')
    if not args.check and (not args.output or args.output.exists()):parser.error('a fresh --output is required')
    sources=check_source();resolver=Resolver(args.game_data);prepared=[];report=dict(schema=1,source=sources,models={})
    for group,obj_name,manifest_name in [('adventure-settlement','settlement.obj','manifest.json'),('adventure-route','route.obj','route-manifest.json')]:
        obj=args.assets/group/obj_name;manifest=obj.parent/manifest_name
        text,evidence=recover(obj,manifest,resolver);relative=obj.relative_to(args.assets)
        target=Path(str(obj)+'.materials');prepared.append((target,manifest,relative,text));report['models'][str(relative)]=evidence
        if args.check and (not target.is_file() or target.read_text()!=text):raise ValueError('Stale/missing reference materials: '+str(relative))
    if args.check:print('Reference materials: both installed groups match source');return
    args.output.mkdir(parents=True)
    for target,manifest,relative,text in prepared:
        staged=args.output/'sidecars'/Path(str(relative)+'.materials');staged.parent.mkdir(parents=True,exist_ok=True);staged.write_text(text,encoding='ascii')
    (args.output/'material-evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    if args.install:
        for target,manifest,relative,text in prepared:
            backup=args.output/'backup'/relative.parent;backup.mkdir(parents=True,exist_ok=True);shutil.copy2(manifest,backup/manifest.name)
            if target.exists():shutil.copy2(target,backup/target.name)
        for target,manifest,relative,text in prepared:
            temporary=Path(str(target)+'.tmp');temporary.write_text(text,encoding='ascii');temporary.replace(target)
    print('Reference materials: both groups '+('installed with backups' if args.install else 'staged'))


if __name__=='__main__':main()
