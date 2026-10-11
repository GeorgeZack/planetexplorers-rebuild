#!/usr/bin/env python3
"""Recover optional PEAS1 material sidecars from qualified installed Unity materials.

PEAC, PNG and PEAS1 bytes are never changed. Run after installing/replacing actor
assets; missing metadata deliberately retains the old diffuse adapter. Unknown
attachment shaders retain that adapter explicitly rather than guessing from PNG
alpha or material names. UnityPy is an offline exporter dependency only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import shutil
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
STANDARD={'AvatarStdEntity','DoubleSideStdEntity','Standard','Standard (Specular setup)'}
SOURCE_DIR=ROOT/'Assets/Scripts/WuYiqiu/Resources/Shaders/Avatar Standard'


def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def fnv(data):
    result=14695981039346656037
    for byte in data:result=((result^byte)*1099511628211)&0xffffffffffffffff
    return result

def props(rows):return {k['name'] if isinstance(k,dict) else k:v for k,v in rows}

def policy(shader,keywords,saved,material):
    """Active Standard branch, not stale legacy fields stored in the material."""
    legacy=dict(mode='legacy',cutoff=0.,alpha=0.,two_sided=False)
    if shader not in STANDARD:return legacy
    floats=props(saved.get('m_Floats',[]));colors=props(saved.get('m_Colors',[]))
    mode=floats.get('_Mode',0.)
    if not isinstance(mode,(int,float)) or not math.isfinite(mode) or mode not in (0.,1.,2.,3.):
        raise ValueError('Invalid source material mode')
    if mode not in (0.,1.):return legacy
    flags=set(keywords.split())
    if bool(mode==1)!=('_ALPHATEST_ON' in flags) or flags.intersection({'_ALPHABLEND_ON','_ALPHAPREMULTIPLY_ON','_ALBEDO_ALPHATEST_ON'}):
        raise ValueError('Standard material mode/keywords disagree')
    two=shader=='DoubleSideStdEntity'
    if mode==0:return dict(mode='opaque',cutoff=0.,alpha=1.,two_sided=two)
    cutoff=floats.get('_Cutoff',.5)
    color=material.get('baked_color')
    alpha=color[3] if color is not None else colors.get('_Color',{'a':1})['a']
    if not all(isinstance(v,(int,float)) and math.isfinite(v) and 0<=v<=1 for v in (cutoff,alpha)):
        raise ValueError('Invalid source cutoff/alpha')
    return dict(mode='cutout',cutoff=float(cutoff),alpha=float(alpha),two_sided=two)


def check_source():
    files=[SOURCE_DIR/'PEUnityStandardInput.cginc',SOURCE_DIR/'PEUnityStandardCore.cginc',SOURCE_DIR/'DSUnityStandardCore.cginc',SOURCE_DIR/'DoubleSideStdEntity.shader',SOURCE_DIR/'AvatarStdEntity.shader']
    texts=[p.read_text() for p in files]
    if 'return tex2D(_MainTex, uv).a * _Color.a;' not in texts[0]:raise ValueError('Source texture alpha changed')
    for text in texts[1:3]:
        if 'clip (alpha - _Cutoff);' not in text or 'UNITY_OPAQUE_ALPHA(output.a);' not in text:raise ValueError('Source cutout/opaque output changed')
    if not re.search(r'(?m)^\s*Cull Off\s*$',texts[3]):raise ValueError('Source hair culling changed')
    replacement=ROOT/'Assets/Scripts/ZhouXun/ReplaceU3DShader/ReplaceShaderU3D.cs'
    table=ROOT/'Assets/Resources/shaderReplaceTbl.xml'
    replacement_text=replacement.read_text(encoding='utf-8-sig')
    pairs={p.attrib['ShellShader']:p.attrib['EntityShader'] for p in ET.parse(table).getroot().findall('./ShaderPairs/Pair')}
    if 'CorruptShaderReplacement.FindValidShader(mat.shader.name)' not in replacement_text or 'mat.shader = sh;' not in replacement_text or pairs.get('DoubleSideStdShell')!='DoubleSideStdEntity':raise ValueError('Source shader replacement changed')
    files.extend([replacement,table])
    return [dict(path=str(p.relative_to(ROOT)),sha256=digest(p)) for p in files]


def replacement_materials(records,component):
    """Only materials on renderer descendants of this attached component.

    Deliberately bounded to same-file hierarchy/material pointers; unfamiliar
    external hierarchy does not acquire replacement authority by inference.
    """
    def local(pointer):
        return pointer.get('m_PathID',0) if pointer.get('m_FileID')==0 else 0
    transforms={local(tree.get('m_GameObject',{})):identity for identity,(kind,tree) in records.items() if kind=='Transform'}
    root=transforms.get(local(component.get('m_GameObject',{})))
    if not root:return []
    result=[]
    for identity,(kind,tree) in records.items():
        if kind not in ('MeshRenderer','SkinnedMeshRenderer'):continue
        current=transforms.get(local(tree.get('m_GameObject',{})));chain=[]
        while current and current not in chain:
            chain.append(current)
            if current==root:
                result.extend((identity,local(material),chain[:]) for material in tree.get('m_Materials',[]) if local(material))
                break
            node=records.get(current)
            if not node or node[0]!='Transform':break
            current=local(node[1].get('m_Father',{}))
    return result


class Resolver:
    def __init__(self,data):
        from export_reference_player import Assets
        self.assets=Assets(data);self.cache={};self.replacements={}
        self.shader_pairs={p.attrib['ShellShader']:p.attrib['EntityShader'] for p in ET.parse(ROOT/'Assets/Resources/shaderReplaceTbl.xml').getroot().findall('./ShaderPairs/Pair')}
    def register_scene(self,scene):
        self.renderer_ids={}
        rig=scene.parent/'rig.json'
        if not rig.is_file():return
        record=json.loads(rig.read_text());bundle=record.get('npc',{}).get('bundle')
        if not bundle:return
        self.renderer_ids={i:p['renderer_id'] for i,p in enumerate(record['parts'])}
        path=Path(bundle)
        if path.is_absolute() or '..' in path.parts or path.parts[0]!='AssetBundles':raise ValueError('Invalid NPC bundle path')
        source=self.assets.data.parent/path
        self.register_bundle(source)
    def register_bundle(self,source):
        """Register only attached, enabled per-renderer shader replacements."""
        env=self.assets.UnityPy.load(str(source))
        for obj in env.objects:
            name=obj.assets_file.name
            self.assets.files.setdefault(name,(env,obj.assets_file))
        records={}
        for obj in env.objects:
            if obj.type.name in ('Transform','MonoScript','MonoBehaviour','MeshRenderer','SkinnedMeshRenderer'):
                records.setdefault(obj.assets_file.name,{})[obj.path_id]=(obj.type.name,obj.read_typetree())
        for obj in env.objects:
            if obj.type.name!='MonoBehaviour':continue
            tree=obj.read_typetree();script=tree.get('m_Script',{})
            local=records[obj.assets_file.name];script_record=local.get(script.get('m_PathID'))
            if tree.get('m_Enabled') and script.get('m_FileID')==0 and script_record and script_record[0]=='MonoScript' and script_record[1].get('m_ClassName')=='ReplaceShaderU3D':
                for renderer,material,chain in replacement_materials(local,tree):
                    self.replacements[(obj.assets_file.name,material,renderer)]=dict(component_path_id=obj.path_id,component_sha256=hashlib.sha256(obj.get_raw_data()).hexdigest(),renderer_path_id=renderer,transform_chain=chain)
        return env
    def material(self,material,renderer_id=None):
        key=(material.get('asset'),material.get('path_id'))
        if not isinstance(key[0],str) or not isinstance(key[1],int):raise ValueError('Missing recorded material identity')
        if key not in self.cache:
            if key[0] not in self.assets.files and not (self.assets.data/key[0]).is_file():
                if not key[0].startswith('source-'):raise ValueError('Missing source material asset: '+key[0])
                self.cache[key]=dict(shader='unqualified synthetic attachment',keywords='',saved={},material_sha256=None,shader_sha256=None)
            else:
                obj=self.assets.file(key[0]).objects[key[1]]
                if obj.type.name!='Material':raise ValueError('Source material identity changed')
                tree=obj.read_typetree()
                if tree['m_Name']!=material['name']:raise ValueError('Source material name changed')
                reference=tree['m_Shader']
                external=obj.assets_file.externals[reference['m_FileID']-1].path if reference['m_FileID'] else None
                shader=(self.assets.file('Resources/unity_builtin_extra').objects[reference['m_PathID']] if external=='resources/unity_builtin_extra' else self.assets.resolve(obj,reference))
                text=shader.read_typetree()['m_Script']
                match=re.match(r'Shader\s+"([^"]+)"',text)
                if not match:raise ValueError('Missing source shader name')
                # Stored legacy properties may differ after shader migration;
                # current installed shader and keywords identify the active rule.
                self.cache[key]=dict(shader=match[1],keywords=tree.get('m_ShaderKeywords',''),saved=tree['m_SavedProperties'],material_sha256=hashlib.sha256(obj.get_raw_data()).hexdigest(),shader_sha256=hashlib.sha256(shader.get_raw_data()).hexdigest())
        resolved=self.cache[key].copy()
        replacement=self.replacements.get((*key,renderer_id))
        if replacement:
            resolved['effective_shader']=self.shader_pairs.get(resolved['shader'],resolved['shader'])
            resolved['replacement']=replacement
        return resolved


def export_scene(scene,resolver):
    records=scene.read_text(encoding='ascii').splitlines()
    if records[0]!='PEAS1' or not 1<=len(records)-1<=64:raise ValueError('Invalid scene manifest')
    manifest=scene.parent/'recording-manifest.json'
    data=json.loads(manifest.read_text());parts={p['file']:p for p in data['parts']}
    lines=['PEAM1'];report=[]
    for line in records[1:]:
        names=line.split(' ')
        if len(names)!=2:raise ValueError('Invalid scene row')
        actor,texture=names;p=parts[actor]
        if p['texture']!=texture:raise ValueError('Recording/scene material mismatch')
        image=(scene.parent/texture).read_bytes()
        if hashlib.sha256(image).hexdigest()!=p['texture_sha256']:raise ValueError('Recorded texture hash changed')
        material=p['material'];resolved=resolver.material(material,getattr(resolver,'renderer_ids',{}).get(p.get('part_index')))
        rule=policy(resolved.get('effective_shader',resolved['shader']),resolved['keywords'],resolved['saved'],material)
        if p.get('generated_solid_diffuse') and rule['mode']=='cutout':rule['alpha']=1. # Solid PNG already contains _Color alpha.
        lines.append(f"{actor} {texture} {rule['mode']} {rule['cutoff']:.9g} {rule['alpha']:.9g} {int(rule['two_sided'])} {fnv(image):016x}")
        report.append(dict(actor=actor,texture=texture,texture_sha256=digest(scene.parent/texture),material_asset=material['asset'],material_path_id=material['path_id'],material_name=material['name'],**{k:v for k,v in resolved.items() if k!='saved'},**rule))
    return '\n'.join(lines)+'\n',dict(scene_sha256=digest(scene),recording_manifest_sha256=digest(manifest),parts=report)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--assets',type=Path,default=ROOT/'rebuild/build/reference-assets')
    parser.add_argument('--game-data',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers/PE_Client_Data')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--install',action='store_true')
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    if args.check and (args.output or args.install):parser.error('--check does not write outputs')
    if not args.check and (not args.output or args.output.exists()):parser.error('a fresh --output directory is required')
    sources=check_source();resolver=Resolver(args.game_data);scenes=sorted(args.assets.rglob('recording.peas'))
    if not scenes:raise ValueError('No installed actor scenes')
    prepared=[];report=dict(schema=1,source=sources,scenes={})
    for scene in scenes:
        resolver.register_scene(scene)
        text,evidence=export_scene(scene,resolver);relative=scene.relative_to(args.assets)
        print(str(relative),flush=True)
        target=Path(str(scene)+'.materials');prepared.append((scene,target,relative,text))
        report['scenes'][str(relative)]=evidence
        if args.check and (not target.is_file() or target.read_text()!=text):raise ValueError('Stale/missing actor material metadata: '+str(target))
    if args.check:print(f'Actor material metadata: {len(scenes)} scenes match source');return
    args.output.mkdir(parents=True)
    for scene,target,relative,text in prepared:
        staged=args.output/'sidecars'/Path(str(relative)+'.materials');staged.parent.mkdir(parents=True,exist_ok=True);staged.write_text(text,encoding='ascii')
    (args.output/'material-evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    if args.install:
        # All qualification completed before any install. Preserve original
        # manifests and preexisting sidecars, never overwrite actor/PNG bytes.
        for scene,target,relative,text in prepared:
            backup=args.output/'backup'/relative;backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(scene,backup)
            if target.exists():shutil.copy2(target,str(backup)+'.materials')
        for scene,target,relative,text in prepared:
            temporary=Path(str(target)+'.tmp');temporary.write_text(text,encoding='ascii');temporary.replace(target)
    print(f'Actor material metadata: {len(scenes)} scenes {"installed with backups" if args.install else "staged"} in {args.output}')


if __name__=='__main__':main()
