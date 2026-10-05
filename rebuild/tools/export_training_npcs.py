#!/usr/bin/env python3
"""Recover actual tutorial NPC skins/materials and source prefab rest geometry.

Requires optional UnityPy1.25.4. No humanoid retargeting is claimed: rig.json is
an input for separately validated original-Avatar pose sampling. Originals stay
read-only; output must be fresh. No source animation is replaced by player poses.
"""
import argparse
from contextlib import closing
import hashlib
import json
import math
from pathlib import Path
import sqlite3
import tempfile

from export_reference_assets import IDENTITY, multiply, trs, transform_normal, material_symbols, property_map, floats
from export_reference_actor import inverse_affine, skin_position
from export_reference_player import validate_skin
from export_training_gathering_assets import checked_mesh

ROOT=Path(__file__).resolve().parents[2]
DATABASE=ROOT/'Assets/Resources/DataBase/localData.bytes'


def roster(database=DATABASE):
    with closing(sqlite3.connect(database.resolve().as_uri()+'?mode=ro',uri=True)) as connection:
        connection.row_factory=sqlite3.Row
        rows=[]
        for row in connection.execute("SELECT NPC_ID,NPC_name,PrototypeNPC,training_pos FROM npc WHERE training_pos != '0,0,0'"):
            try: entity_id=int(row['NPC_ID']);prototype=int(row['PrototypeNPC']);position=[float(x) for x in row['training_pos'].split(',')]
            except ValueError: continue # The table includes a metadata description row.
            if len(position)!=3 or not all(math.isfinite(x) for x in position):raise ValueError('Invalid NPC placement')
            source=connection.execute('SELECT id,gender,Bonepath,Bonepath_Editor,path FROM PrototypeNPC WHERE CAST(id AS INTEGER)=?',(prototype,)).fetchone()
            if source is None:raise ValueError('Missing NPC prototype')
            rows.append(dict(entity_id=entity_id,prototype_id=prototype,name=row['NPC_name'],position=position,
                             rotation=[0.,0.,0.,1.],scale=[1.,1.,1.],gender=int(source['gender']),
                             bundle='AssetBundles/'+source['Bonepath'],source_prefab='Assets/Resources/'+source['Bonepath_Editor'],
                             appearance_resource=source['path']))
    return sorted(rows,key=lambda r:r['entity_id'])


def skin_rest(positions,normals,skin,palette):
    posed=[];posed_normals=[]
    validate_skin(skin,len(palette))
    for position,normal,row in zip(positions,normals,skin):
        weights=[row[f'weight[{i}]'] for i in range(4)];joints=[row[f'boneIndex[{i}]'] for i in range(4)]
        posed.append(skin_position(position,weights,joints,palette))
        n=[0.,0.,0.]
        for weight,joint in zip(weights,joints):
            value=transform_normal(palette[joint],normal)
            for axis in range(3):n[axis]+=weight*value[axis]
        length=math.sqrt(sum(v*v for v in n))
        if length<1e-8:raise ValueError('Degenerate NPC normal')
        posed_normals.append([v/length for v in n])
    return posed,posed_normals


def recover_npc(game,output,entry):
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler
    bundle=game/entry['bundle'];env=UnityPy.load(str(bundle));objects={o.path_id:o for o in env.objects}
    def ref(pointer):
        if pointer['m_FileID'] or not pointer['m_PathID'] in objects:raise ValueError('Missing/external NPC bundle reference')
        return objects[pointer['m_PathID']]
    def identity(obj):return dict(file=obj.assets_file.name,path_id=obj.path_id,sha256=hashlib.sha256(obj.get_raw_data()).hexdigest())
    transforms={i:o.read_typetree() for i,o in objects.items() if o.type.name=='Transform'}
    gos={i:o.read_typetree() for i,o in objects.items() if o.type.name=='GameObject'}
    got={t['m_GameObject']['m_PathID']:i for i,t in transforms.items()}
    asset,= [o for o in objects.values() if o.type.name=='AssetBundle']
    roots=asset.read_typetree()['m_Container'];container,=roots;root=ref(container[1]['asset']);rootid=got[root.path_id]
    animators=[o for o in objects.values() if o.type.name=='Animator'];animator,=animators
    animator_tree=animator.read_typetree();modelid=got[animator_tree['m_GameObject']['m_PathID']]
    avatar_obj=ref(animator_tree['m_Avatar']);avatar=avatar_obj.read_typetree()
    def ancestry(tid):
        chain=[];seen=set()
        while tid:
            if tid in seen or len(seen)>=256:raise ValueError('Cyclic NPC hierarchy')
            seen.add(tid);chain.append(tid);tid=transforms[tid]['m_Father']['m_PathID']
        return chain[::-1]
    def name(tid):return gos[transforms[tid]['m_GameObject']['m_PathID']]['m_Name']
    def relative_path(tid):
        ids=ancestry(tid);ids=ids[ids.index(modelid)+1:];labels=[]
        for child in ids:
            parent=transforms[child]['m_Father']['m_PathID']
            siblings=[p['m_PathID'] for p in transforms[parent]['m_Children'] if name(p['m_PathID'])==name(child)]
            label=name(child)
            if len(siblings)>1:label+='#'+str(siblings.index(child))
            labels.append(label)
        return '/'.join(labels)
    world={}
    def matrix(tid):
        if tid not in world:
            t=transforms[tid];local=trs(t['m_LocalPosition'],t['m_LocalRotation'],t['m_LocalScale']);parent=t['m_Father']['m_PathID']
            world[tid]=multiply(matrix(parent),local) if parent else local
        return world[tid]
    root_inverse=inverse_affine(matrix(modelid));model_nodes=[i for i in transforms if modelid in ancestry(i)]
    renderers=[]
    for o in objects.values():
        if o.type.name!='SkinnedMeshRenderer':continue
        d=o.read_typetree()
        if d['m_Mesh']['m_PathID'] and modelid in ancestry(got[d['m_GameObject']['m_PathID']]):renderers.append(o)
    if not 1<=len(renderers)<=32:raise ValueError('NPC model renderer count outside bounds')
    parts=[];materials={}
    for renderer_obj in sorted(renderers,key=lambda o:o.path_id):
        renderer=renderer_obj.read_typetree();mesh_obj=ref(renderer['m_Mesh']);raw=mesh_obj.read_typetree()
        mesh=MeshHandler(mesh_obj.read());mesh.process()
        positions=[v[:3] for v in mesh.m_Vertices];normals=[v[:3] for v in mesh.m_Normals];uvs=[v[:2] for v in mesh.m_UV0];triangles=list(mesh.get_triangles())
        checked_mesh(positions,normals,uvs,triangles)
        skin=raw['m_Skin'];bones=[ref(p).path_id for p in renderer['m_Bones']]
        if any(i not in model_nodes for i in bones):raise ValueError('Skin bone outside animated NPC model')
        if len({name(b) for b in bones})!=len(bones):raise ValueError('Duplicate skin bone name')
        binds=[[[b[f'e{r}{c}'] for c in range(4)] for r in range(4)] for b in raw['m_BindPose']]
        if len(bones)!=len(binds) or len(skin)!=len(positions):raise ValueError('NPC skin counts disagree')
        palette=[multiply(multiply(root_inverse,matrix(b)),bind) for b,bind in zip(bones,binds)]
        for p in palette:inverse_affine(p)
        posed,posed_normals=skin_rest(positions,normals,skin,palette)
        mids=[]
        for p in renderer['m_Materials']:
            material=ref(p);key=(material.assets_file.name,material.path_id);materials[key]=material;mids.append(key)
        if len(mids)!=len(triangles):raise ValueError('NPC material slots disagree')
        parts.append(dict(resource=entry['appearance_resource'],renderer_id=renderer_obj.path_id,mesh_id=mesh_obj.path_id,
                          mesh=identity(mesh_obj),bone_ids=bones,bone_names=[name(b) for b in bones],
                          bone_paths=[relative_path(b) for b in bones],
                          inverse_binds=binds,positions=positions,normals=normals,uv=uvs,skin=skin,triangles=triangles,
                          materials=mids,default_positions=posed,default_normals=posed_normals))
    symbols=material_symbols(sorted(materials));mtl=['# Source material slots; diffuse-only preview conversion\n'];material_rows=[]
    for key,material in sorted(materials.items()):
        d=material.read_typetree();saved=d['m_SavedProperties'];color=property_map(saved['m_Colors']).get('_Color',dict(r=1.,g=1.,b=1.,a=1.))
        mtl.extend([f'newmtl {symbols[key]}\n','Kd '+floats([color[k] for k in 'rgb'])+'\n','d '+floats([color['a']])+'\n','illum 1\n'])
        row=dict(asset=key[0],path_id=key[1],name=d['m_Name'],symbol=symbols[key],properties=saved,identity=identity(material))
        main=property_map(saved['m_TexEnvs']).get('_MainTex')
        if main and main['m_Texture']['m_PathID']:
            texture=ref(main['m_Texture']);image=texture.read().image
            if image.width<1 or image.height<1 or image.width*image.height>16777216:raise ValueError('NPC texture too large')
            filename=f'texture-{symbols[key]}.png';image.save(output/filename);row.update(diffuse_file=filename,texture=identity(texture));mtl.append(f'map_Kd {filename}\n')
        mtl.append('\n');material_rows.append(row)
    (output/'model.mtl').write_text(''.join(mtl))
    lines=['# Baked Unity XYZ meters; original vertex/normal coordinate convention\n','mtllib model.mtl\n'];offset=1
    for part in parts:
        for tag,channel in [('v',part['default_positions']),('vt',part['uv']),('vn',part['default_normals'])]:lines.extend(tag+' '+floats(v)+'\n' for v in channel)
        for key,triangles in zip(part['materials'],part['triangles']):
            lines.append('usemtl '+symbols[key]+'\n')
            for triangle in triangles:lines.append('f '+' '.join(f'{i+offset}/{i+offset}/{i+offset}' for i in triangle)+'\n')
        offset+=len(part['positions'])
    (output/'model.obj').write_text(''.join(lines))
    skeleton=[]
    for tid in sorted(model_nodes,key=lambda i:(len(ancestry(i)),i)):
        t=transforms[tid];a=ancestry(tid);relative=a[a.index(modelid)+1:]
        skeleton.append(dict(path_id=tid,name=name(tid),relative_path=relative_path(tid),
                             parent=t['m_Father']['m_PathID'] if tid!=modelid else 0,local=t,
                             model_matrix=multiply(root_inverse,matrix(tid))))
    rig=dict(schema=1,sex='female' if entry['gender']==1 else 'male',npc=entry,pose='serialized NPC Model hierarchy rest pose; not evaluated idle',animated=False,
             avatar_id=avatar_obj.path_id,avatar=avatar,avatar_identity=identity(avatar_obj),model_root_id=modelid,
             parts=parts,materials=material_rows,skeleton=skeleton)
    (output/'rig.json').write_text(json.dumps(rig,separators=(',',':'),allow_nan=False)+'\n')
    idle=[]
    for o in objects.values():
        if o.type.name=='AnimationClip' and o.peek_name()=='NormalIdle':
            d=o.read_typetree();bindings=d['m_ClipBindingConstant']['genericBindings']
            idle.append(dict(**identity(o),name=d['m_Name'],duration=d['m_MuscleClip']['m_StopTime'],
                             frame_rate=d['m_SampleRate'],loop=d['m_MuscleClip']['m_LoopTime'],
                             humanoid_bindings=sum(b['classID']==95 and b['customType']==8 for b in bindings),
                             evaluated=False))
    controller=ref(animator_tree['m_Controller']);controller_tree=controller.read_typetree()
    effective=[r for r in idle if r['path_id']==7422444173399996218]
    if controller.type.name=='AnimatorOverrideController':
        for override in controller_tree['m_Clips']:
            original=ref(override['m_OriginalClip'])
            if original.read_typetree()['m_Name']=='NormalIdle' and override['m_OverrideClip']['m_PathID']:
                effective=[r for r in idle if r['path_id']==override['m_OverrideClip']['m_PathID']]
    if len(effective)!=1:raise ValueError('Cannot resolve effective NPC idle')
    colliders=[dict(**identity(o),fields=o.read_typetree()) for o in objects.values() if o.type.name=='CapsuleCollider' and o.read_typetree()['m_GameObject']['m_PathID']==animator_tree['m_GameObject']['m_PathID']]
    return dict(**entry,bundle_sha256=hashlib.sha256(bundle.read_bytes()).hexdigest(),
                source_prefab_sha256=hashlib.sha256((ROOT/entry['source_prefab']).read_bytes()).hexdigest(),
                model='model.obj',rig='rig.json',avatar_name=avatar['m_Name'],avatar=identity(avatar_obj),
                model_root_id=modelid,model_capsules=colliders,idle_candidates=idle,effective_idle=effective[0],
                controller=dict(**identity(controller),name=controller_tree['m_Name'],type=controller.type.name),animated=False,
                vertices=offset-1,triangles=sum(len(t) for p in parts for t in p['triangles']),
                materials=len(material_rows),bounds=[[fn(v[a] for p in parts for v in p['default_positions']) for a in range(3)] for fn in (min,max)])


def recover(game,output):
    entries=roster();reports=[]
    if len(entries)!=9:raise ValueError('Expected nine source tutorial NPCs')
    for entry in entries:
        directory=output/str(entry['entity_id']);directory.mkdir();print('Recovering '+entry['name'],flush=True)
        reports.append(recover_npc(game,directory,entry))
    result=dict(format='PE-training-NPC-assets-1',npcs=reports,database_sha256=hashlib.sha256(DATABASE.read_bytes()).hexdigest(),
                coordinates='Unity model-root local XYZ; entity placements separate, no room transform baked',
                limitations=['Serialized source model rest pose only; humanoid idle requires original-Avatar runtime sampling.',
                             'Dedicated original NPC mesh/outfit/materials; diffuse shader approximation, no live animation or AI.',
                             'NPC weapons, facial morphs, eye/hair shader effects and runtime equipment are not reproduced.'])
    (output/'manifest.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    plan=dict(schema=1,npcs=[])
    for npc in reports:
        idle=npc['effective_idle']
        plan['npcs'].append(dict(npc_id=npc['entity_id'],model=Path(npc['source_prefab']).stem,
            avatar=npc['avatar_name'],controller=npc['controller']['name'],gender='Female' if npc['gender']==1 else 'Male',
            asset=str((game/npc['bundle']).resolve()),asset_sha256=npc['bundle_sha256'],
            clip=dict(name=idle['name'],path_id=idle['path_id'],length=idle['duration'],frame_rate=idle['frame_rate'],
                      loop=idle['loop'],humanoid_bindings=idle['humanoid_bindings'])))
    (output/'npc-plan.json').write_text(json.dumps(plan,indent=2,allow_nan=False)+'\n')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    parser.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/npcs');args=parser.parse_args()
    game=args.game_dir.expanduser().resolve();output=args.output.expanduser().resolve()
    if output.exists() or output==game or game in output.parents:raise ValueError('Output must be fresh and outside original game')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.npc-assets-',dir=output.parent) as temporary:
        staging=Path(temporary);report=recover(game,staging);staging.rename(output)
    print('Exported '+str(len(report['npcs']))+' original NPCs to '+str(output))

if __name__=='__main__':main()
