#!/usr/bin/env python3
"""Recover archived Eitai or Westina's morphed appearance and source Female idle.

Optional UnityPy/Pillow are offline dependencies only. Inputs remain read-only;
output must be fresh. These are archived individuals, not a random NPC generator.
"""
import argparse
import hashlib
import json
import math
import sqlite3
from pathlib import Path
import tempfile
import zlib
import xml.etree.ElementTree as ET

import analyze_reference_bake as bake
from export_reference_assets import IDENTITY, multiply, trs, transform_point, transform_normal, property_map
from export_reference_actor import inverse_affine, skin_position
from export_reference_player import Assets, avatar_worlds, validate_skin, digest
from read_reference_adventure import ROOT, DEFAULT_SAVE, ARCHIVED_NPCS, decode


def morph_mesh(positions, normals, raw, child, weights):
    positions=[list(p[:3]) for p in positions];normals=[list(n[:3]) for n in normals]
    data=raw['m_Shapes'];channels=data['channels'];selected=[]
    span=next((span for name,span in [('head',(0,14)),('torso',(14,21)),('legs',(21,25)),('feet',(26,27)),('hand',(25,26))] if name in child),None)
    if not channels or span is None: return positions,normals,selected
    for global_index in range(*span):
        weight=weights[global_index];index=2*(global_index-span[0])+int(weight<=0)
        if index>=len(channels): continue # Source skips missing channel pairs.
        channel=channels[index]
        if channel['frameCount']!=1: raise ValueError('Only source single-frame morph channels supported')
        frame=channel['frameIndex']
        if not 0<=frame<len(data['shapes']) or data['fullWeights'][frame]!=100: raise ValueError('Unexpected blend shape frame weight')
        shape=data['shapes'][frame];begin=shape['firstVertex'];end=begin+shape['vertexCount']
        if not 0<=begin<=end<=len(data['vertices']): raise ValueError('Invalid morph delta range')
        seen=set()
        for delta in data['vertices'][begin:end]:
            vertex=delta['index']
            if not 0<=vertex<len(positions) or vertex in seen: raise ValueError('Invalid or duplicate morph vertex')
            seen.add(vertex)
            for axis,key in enumerate('xyz'):
                v=delta['vertex'][key];n=delta['normal'][key]
                if not math.isfinite(v) or not math.isfinite(n): raise ValueError('Nonfinite morph delta')
                positions[vertex][axis]+=abs(weight)*v
                if shape['hasNormals']: normals[vertex][axis]+=abs(weight)*n
        selected.append(dict(weight_index=global_index,channel=index,weight=weight,vertices=shape['vertexCount']))
    return positions,normals,selected


def tint_pixel(pixel, mask, color, skin, coefficient):
    rgb=[color[i]*pixel[i]/255 for i in range(3)]
    if mask:
        rgb=[(1-coefficient)*skin[i]*rgb[i]+coefficient*skin[i] for i in range(3)]
    return tuple(max(0,min(255,round(v*255))) for v in rgb)+(pixel[3],)


def source_bone_names(names, avatar):
    # Source #define IgnoreCaseOfBoneName maps male 'Bip01 mouth' to
    # 'Bip01 Mouth'. Asset spellings remain separately recorded in the rig.
    paths=[p for _,p in avatar['m_TOS']]
    result=[]
    for name in names:
        matches={p.rsplit('/',1)[-1] for p in paths if p.rsplit('/',1)[-1].lower()==name.lower()}
        if len(matches)>1: raise ValueError('Ambiguous case-insensitive source bone')
        result.append(next(iter(matches)) if matches else name)
    return result


def recover(game, output, appearance, npc='supplier'):
    from UnityPy.helpers.MeshHelper import MeshHandler
    from PIL import Image
    assets=Assets(game/'PE_Client_Data');manager=next(o for o in assets.file('mainData').objects.values() if o.type.name=='ResourceManager')
    container=manager.read_typetree()['m_Container'];materials={};stream_sources=set()
    def resource(path,kind):
        found=[assets.resolve(manager,p) for n,p in container if n==path.lower()]
        found=[o for o in found if o.type.name==kind]
        if len(found)!=1: raise ValueError('Ambiguous resource '+path+'/'+kind)
        return found[0]
    sex=appearance['sex']
    if sex not in ('female','male'): raise ValueError('Unsupported humanoid sex')
    avatar_obj=resource('Model/PlayerModel/'+sex.title(),'Avatar');avatar=avatar_obj.read_typetree();worlds,by_name=avatar_worlds(avatar)
    rig=dict(schema=1,sex=sex,appearance='archived-adventure-'+npc,avatar=avatar,avatar_id=avatar_obj.path_id,avatar_worlds=worlds,parts=[],materials=[],selection=appearance)
    def world(transform,seen=None):
        seen=set() if seen is None else seen
        key=(transform.assets_file.name,transform.path_id)
        if key in seen or len(seen)>256: raise ValueError('Cyclic transform')
        seen.add(key);t=transform.read_typetree();m=trs(t['m_LocalPosition'],t['m_LocalRotation'],t['m_LocalScale'])
        return multiply(world(assets.resolve(transform,t['m_Father']),seen),m) if t['m_Father']['m_PathID'] else m
    def mesh_data(owner,renderer,mesh_obj):
        raw=mesh_obj.read_typetree();mesh=MeshHandler(mesh_obj.read());mesh.process()
        keys=[]
        for reference in renderer['m_Materials']:
            material=assets.resolve(owner,reference);key=(material.assets_file.name,material.path_id);keys.append(key);materials[key]=material
        triangles=list(mesh.get_triangles())
        if len(keys)!=len(triangles): raise ValueError('Material slots differ')
        return raw,mesh,keys,triangles
    # AvatarData.GetParts preserves slot order but emits each resource once.
    for path in dict.fromkeys(appearance['slots']):
        resource_path,child=path.rsplit('-',1)
        root=resource(resource_path,'GameObject')
        try: go=assets.child(root,child)
        except ValueError as error:
            if not str(error).startswith('Missing child '): raise
            # AppearBuilder.GetPartInfos omits CustomPartInfo with null Smr.
            rig.setdefault('skipped_slots',[]).append(dict(resource=path,reason='missing child; source null-Smr omission'))
            continue
        renderers=assets.components(go,'SkinnedMeshRenderer')
        if not renderers:
            rig.setdefault('skipped_slots',[]).append(dict(resource=path,reason='source null-Smr omission'))
            continue
        owner,=renderers;renderer=owner.read_typetree();mesh_obj=assets.resolve(owner,renderer['m_Mesh']);raw,mesh,keys,triangles=mesh_data(owner,renderer,mesh_obj)
        positions,normals,morphs=morph_mesh(mesh.m_Vertices,mesh.m_Normals,raw,child,appearance['weights'])
        bones=[assets.resolve(owner,p) for p in renderer['m_Bones']];names=[assets.resolve(b,b.read_typetree()['m_GameObject']).peek_name() for b in bones]
        original_names=list(names)
        names=source_bone_names(names,avatar)
        binds=[[[b[f'e{r}{c}'] for c in range(4)] for r in range(4)] for b in raw['m_BindPose']]
        skin=raw['m_Skin'];validate_skin(skin,len(bones))
        if not len(positions)==len(normals)==len(mesh.m_UV0)==len(skin) or len(binds)!=len(bones): raise ValueError('Mesh channel/bone lengths differ')
        # BakeMesh uses the resource renderer's current pose before transferring
        # the resulting geometry and original bind poses to the shared avatar.
        transform,=assets.components(go,'Transform');inverse_renderer=inverse_affine(world(transform))
        palette=[multiply(multiply(inverse_renderer,world(b)),bind) for b,bind in zip(bones,binds)]
        residual=max(abs(p[r][c]-IDENTITY[r][c]) for p in palette for r in range(4) for c in range(4))
        if morphs:
            positions=[skin_position(p,[row[f'weight[{i}]'] for i in range(4)],[row[f'boneIndex[{i}]'] for i in range(4)],palette) for p,row in zip(positions,skin)]
            baked=[]
            for normal,row in zip(normals,skin):
                n=[0.,0.,0.]
                for i in range(4):
                    value=transform_normal(palette[row[f'boneIndex[{i}]']],normal)
                    for a in range(3):n[a]+=row[f'weight[{i}]']*value[a]
                length=math.sqrt(sum(v*v for v in n))
                if length<1e-8:raise ValueError('Degenerate morphed normal')
                baked.append([v/length for v in n])
            normals=baked
        if renderer['m_BlendShapeWeights'] and any(renderer['m_BlendShapeWeights']): raise ValueError('Nonzero resource default shape weights unsupported')
        rig['parts'].append(dict(resource=path,mesh_id=mesh_obj.path_id,mesh_asset=mesh_obj.assets_file.name,mesh_sha256=hashlib.sha256(mesh_obj.get_raw_data()).hexdigest(),bone_names=names,inverse_binds=binds,
            source_bone_names=original_names,positions=positions,normals=normals,uv=[list(v[:2]) for v in mesh.m_UV0],skin=skin,triangles=triangles,materials=keys,morphs=morphs,resource_skin_identity_error=residual))
    body_part_count=len(rig['parts'])
    # Shield36 always attaches to Bone L Forearm (PESheild.InitEquipment).
    equipment={row['item_id']:row for row in appearance['equipment']}
    if any(not row['replace_mask'] and item not in (3,5,12,28,33,35,36,38,43,45,54,55) for item,row in equipment.items()):
        raise ValueError('Unsupported archived equipment attachment')
    attachment_sources=[]
    for item_id,mount in ((35,'Bone L Forearm'),(36,'Bone L Forearm'),(38,'Bone L Forearm'),(3,'mountBack'),(5,'mountBack'),(12,'mountBack'),(28,'mountWaist'),(43,'Bow'),(45,'Bow'),(54,'Pistol'),(55,'Long_Gun')):
        if item_id not in equipment: continue
        path=equipment[item_id]['resource'];root=resource(path,'GameObject');rt,=assets.components(root,'Transform')
        mount_path=None
        if item_id not in (35,36,38):
            import UnityPy
            source=ROOT/('Assets/Resources/'+path+'.prefab')
            objects=list(UnityPy.load(str(source)).objects)
            root_transforms=[o.read_typetree() for o in objects if o.type.name=='Transform' and not o.read_typetree()['m_Father']['m_PathID']]
            if len(root_transforms)!=1:raise ValueError('Ambiguous equipment prefab root')
            source_root=root_transforms[0]
            if trs(source_root['m_LocalPosition'],source_root['m_LocalRotation'],source_root['m_LocalScale'])!=IDENTITY:
                raise ValueError('Equipment source root requires transform handling')
            equipment_trees=[o.read_typetree() for o in objects if o.type.name=='MonoBehaviour']
            attachment=next(t['m_HandChangeAttr'] for t in equipment_trees if 'm_HandChangeAttr' in t)
            if attachment['m_PutOffBone']!=mount:raise ValueError('Equipment stowed source mount differs')
            model=ROOT/('Assets/Resources/Prefab/PlayerPrefab/'+sex.title()+'Model.prefab')
            model_objects={o.path_id:o for o in UnityPy.load(str(model)).objects}
            transforms={i:o.read_typetree() for i,o in model_objects.items() if o.type.name=='Transform'}
            names={i:model_objects[t['m_GameObject']['m_PathID']].read_typetree()['m_Name'] for i,t in transforms.items()}
            def below_model(index):
                while index:
                    if names[index]=='Model':return True
                    index=transforms[index]['m_Father']['m_PathID']
                return False
            matches=[i for i,n in names.items() if n==mount and below_model(i)]
            if len(matches)!=1:raise ValueError('Ambiguous source equipment mount')
            current=matches[0];local=transforms[current];chain=[]
            while names[current]!='Model':
                chain.append(names[current]);current=transforms[current]['m_Father']['m_PathID']
            mount_path='/'.join(reversed(chain))
            rig.setdefault('attachment_mounts',[]).append(dict(name=mount,path=mount_path,local=trs(local['m_LocalPosition'],local['m_LocalRotation'],local['m_LocalScale']),equipment=attachment))
            attachment_sources += [source,model,ROOT/'Assets/Scripts/Items/PEEquipment/PEHoldAbleEquipment.cs']
        def add_rigid(go,matrix):
            for filt in assets.components(go,'MeshFilter'):
                owner,=assets.components(go,'MeshRenderer');renderer=owner.read_typetree();mesh_obj=assets.resolve(filt,filt.read_typetree()['m_Mesh']);raw,mesh,keys,triangles=mesh_data(owner,renderer,mesh_obj)
                skin={**{f'weight[{i}]':float(i==0) for i in range(4)},**{f'boneIndex[{i}]':0 for i in range(4)}}
                part=dict(resource=path,mesh_id=mesh_obj.path_id,mesh_asset=mesh_obj.assets_file.name,mesh_sha256=hashlib.sha256(mesh_obj.get_raw_data()).hexdigest(),bone_names=[mount],inverse_binds=[IDENTITY],positions=[transform_point(matrix,p) for p in mesh.m_Vertices],normals=[transform_normal(matrix,p) for p in mesh.m_Normals],uv=[list(v[:2]) for v in mesh.m_UV0],skin=[dict(skin) for _ in mesh.m_Vertices],triangles=triangles,materials=keys,attachment=mount)
                if mount_path:part['bone_paths']=[mount_path]
                rig['parts'].append(part)
            t,=assets.components(go,'Transform')
            for ref in t.read_typetree()['m_Children']:
                ct=assets.resolve(t,ref);tree=ct.read_typetree();cg=assets.resolve(ct,tree['m_GameObject']);add_rigid(cg,multiply(matrix,trs(tree['m_LocalPosition'],tree['m_LocalRotation'],tree['m_LocalScale'])))
        root_tree=rt.read_typetree()
        if trs(root_tree['m_LocalPosition'],root_tree['m_LocalRotation'],root_tree['m_LocalScale'])!=IDENTITY: raise ValueError('Equipment root requires source reset audit')
        previous=len(rig['parts'])
        add_rigid(root,IDENTITY)
        if len(rig['parts'])==previous: raise ValueError('Equipment has no supported rigid visual: '+path)
    for index,(key,obj) in enumerate(sorted(materials.items())):
        tree=obj.read_typetree();saved=tree['m_SavedProperties'];name=tree['m_Name'];colors=property_map(saved['m_Colors']);floats=property_map(saved['m_Floats']);textures=property_map(saved['m_TexEnvs'])
        color=[colors.get('_Color',dict(r=1,g=1,b=1,a=1))[c] for c in 'rgba'];skin=[colors.get('_SkinColor',dict(r=1,g=1,b=1,a=1))[c] for c in 'rgba'];is_body=any(key in p['materials'] for p in rig['parts'][:body_part_count])
        if is_body:
            if 'eye' in name:skin=appearance['colors']['eye']
            elif 'hair' in name:color=appearance['colors']['hair']
            elif 'head' in name:color=appearance['colors']['skin']
            elif 'Helmet' not in name:skin=appearance['colors']['skin']
        main=textures.get('_MainTex')
        if not main or not main['m_Texture']['m_PathID']:raise ValueError('Missing supplier diffuse')
        tex=assets.resolve(obj,main['m_Texture']);image=tex.read().image.convert('RGBA')
        for texture_ref in (main, textures.get('_MaskTex')):
            if texture_ref and texture_ref['m_Texture']['m_PathID']:
                texture=assets.resolve(obj,texture_ref['m_Texture']);stream=texture.read_typetree().get('m_StreamData',{}).get('path')
                if stream:stream_sources.add(assets.data/Path(stream).name)
        if image.width*image.height>16777216:raise ValueError('Oversized supplier texture')
        mask_ref=textures.get('_MaskTex');mask=None
        if is_body and mask_ref and mask_ref['m_Texture']['m_PathID']:
            mask_tex=assets.resolve(obj,mask_ref['m_Texture']);mask=mask_tex.read().image.convert('RGB')
            if mask.size!=image.size: mask=mask.resize(image.size,Image.Resampling.BILINEAR)
        coefficient=floats.get('_SkinCoef',0.)
        if not 0<=coefficient<=1:raise ValueError('Unsupported skin coefficient')
        pixels=list(image.get_flattened_data() if hasattr(image,'get_flattened_data') else image.getdata());masks=list(mask.get_flattened_data() if hasattr(mask,'get_flattened_data') else mask.getdata()) if mask else [None]*len(pixels)
        image.putdata([tint_pixel(p,bool(m and m[0]),color,skin,coefficient) for p,m in zip(pixels,masks)])
        filename=f'{npc}-material-{index}.png';image.save(output/filename)
        rig['materials'].append(dict(asset=key[0],path_id=key[1],name=name,properties=saved,diffuse_file=filename,baked_color=color,baked_skin=skin,skin_coefficient=coefficient,mask_applied=mask is not None,texture_asset=tex.assets_file.name,texture_id=tex.path_id))
    # Reuse the separately verified exact source knife visual and stowed mount.
    if 33 in equipment:
        from export_baked_knife import knife_rig,source_file
        source=ROOT/'rebuild/build/reference-assets';visual=json.loads((source/'training-knife-manifest.json').read_text());collision=json.loads((source/'knife-collision.json').read_text())
        mesh=source_file(source,visual,'training-knife.obj','.obj');knife,mount,_=knife_rig(rig,collision,visual,mesh)
        for part in knife['parts']:part['bone_paths']=[mount]
        for material in knife['materials']:
            filename=material.get('diffuse_file')
            if filename:(output/filename).write_bytes(source_file(source,visual,filename,'.png'))
        rig['parts']+=knife['parts'];rig['materials']+=knife['materials']
    source_paths=[assets.data/name for name in sorted(assets.files)]+sorted(stream_sources)+attachment_sources
    source_paths += [Path(__file__).resolve(),ROOT/'rebuild/tools/read_reference_adventure.py',ROOT/'Assets/AppearBuilder/AppearBuilder.cs',ROOT/'Assets/AppearBuilder/CustomCharactor.cs',ROOT/'Assets/Scripts/PeEntity/Cmpt/AvatarCmpt.cs',ROOT/'Assets/Scripts/Items/PEEquipment/PESheild.cs',ROOT/'Assets/Scripts/ZhouXun/Voxel Creation/Scripts/WhiteCat/PE-VC/BoneCollector.cs',ROOT/'Assets/Scripts/WuYiqiu/Resources/Shaders/Avatar Standard/PEUnityStandardInput.cginc',ROOT/'Assets/Resources/Prefab/Item/Equip/Armor/Shield/copper_shield.prefab']
    rig['sources']=[dict(path=str(p),sha256=digest(p)) for p in source_paths]
    return rig


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--npc',choices=tuple(ARCHIVED_NPCS),default='supplier')
    parser.add_argument('--save-dir',type=Path,default=DEFAULT_SAVE)
    parser.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    parser.add_argument('--bake',type=Path,default=Path.home()/'.local/share/planet-explorers-bake-reference/pose-captures/female-clips-02.jsonl')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args();npc=args.npc;output=(args.output or ROOT/f'rebuild/build/reference-assets/adventure-{npc}').expanduser().resolve();game=args.game_dir.expanduser().resolve();save=args.save_dir.expanduser().resolve()
    if output.exists() or game==output or game in output.parents or save==output or save in output.parents:parser.error('Fresh output outside original installation/save required')
    output.parent.mkdir(parents=True,exist_ok=True)
    try:
        appearance=decode(save,ARCHIVED_NPCS[npc]['entity_id'])
        config=save.parents[1]/'Config/SystemSaveData.xml'
        settings=ET.parse(config).getroot().find('HideHeadgear')
        if settings is None or settings.get('value')!='False':raise ValueError('This archived appearance requires HideHeadgear=False')
        appearance['sources'].append(dict(path=str(config),sha256=digest(config)))
        appearance['hide_headgear']=False
        with tempfile.TemporaryDirectory(prefix=f'.{npc}-',dir=output.parent) as temporary:
            staging=Path(temporary);research=staging/'source';research.mkdir();rig=recover(game,research,appearance,npc);rig_path=research/f'{npc}-rig.json';rig_path.write_text(json.dumps(rig,separators=(',',':'),allow_nan=False)+'\n')
            report,samples=bake.analyze(args.bake,rig,retain=True);clip=next(c for c in report['clips'] if c['name']=='NormalIdle')
            for mount in rig.get('attachment_mounts',[]):
                mapped=report['rig_mapping']['bones'][mount['name']]
                if mapped['relative_path']!=mount['path']:raise ValueError('Source/bake equipment mount path differs')
                node=report['skeleton']['nodes'][mapped['index']]
                bake.pose.close_matrix(bake.pose.matrix(node['initialLocal'],'baked mount local'),mount['local'],'source equipment mount local')
            result=bake.export_clip(staging,rig,rig_path,samples['NormalIdle'],report,clip)
            from export_adventure_npc_collider import source_capsule
            binary,collider=source_capsule();(staging/f'{npc}-collider.pesc').write_bytes(binary)
            (research/'bake-report.json').write_text(json.dumps(report,indent=2)+'\n')
            manifest=dict(schema=1,appearance=appearance,rig=f'source/{npc}-rig.json',collider=collider,parts=len(result['parts']),frames=result['frames'],clip='NormalIdle',sources=rig['sources']+appearance['sources']+[dict(path=str(args.bake),sha256=hashlib.sha256(args.bake.read_bytes()).hexdigest())],
                limitations=['Exact archived modular selection, base morph weights and colors; not a general random NPC generator.',
                    'Offline reconstruction of source single-frame morph blending/BakeMesh; no live archived NPC BakeMesh comparison or transient abnormal-condition submorphs.',
                    'Diffuse Avatar albedo tint/mask baked to PNG; no original PBR/specular/normal lighting, shader filtering or gamma equivalence.',
                    'Source FemaleAvatar NormalIdle reused from isolated player clip bake; NPC controller, IK, dialogue gestures and root motion omitted.',
                    'Weapons use source stowed mounts; shield source forearm mount. Archived equipment ownership does not preserve transient hand/action visibility.'])
            (staging/f'{npc}-manifest.json').write_text(json.dumps(manifest,indent=2,allow_nan=False)+'\n');staging.rename(output)
        print(json.dumps(dict(name=appearance['name'],parts=manifest['parts'],frames=manifest['frames'],output=str(output))))
    except (ValueError,KeyError,OSError,ImportError,IndexError,sqlite3.Error,ET.ParseError) as error:parser.exit(1,f'Adventure NPC export failed: {error}\n')
if __name__=='__main__':main()
