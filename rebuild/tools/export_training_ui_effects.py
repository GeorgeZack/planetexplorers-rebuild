#!/usr/bin/env python3
"""Recover the seven original HUD/dialogue effect masks and shader provenance."""
import argparse,json,re,shutil,tempfile
from pathlib import Path
from export_training_ui import ROOT,digest
PREFAB=ROOT/'Assets/Scripts/Chenzhi/NewUI/Resources/Prefab/GameUI/GameMain.prefab'
WINDOWS=[('ItemPackgeWnd',11400144,'inventory'),('WarehouseWnd',11441708,'storage'),('PlayerInfoWnd',11408934,'character'),('CompoundWnd',11400532,'replicator'),('MissionWnd',11400174,'mission'),('WarehouseWnd',11430420,'storage-scan')]
ROLES={11436526:'dialogue',11400316:'frame',11492306:'health',11400330:'map-left',11400328:'map-right',11400336:'hud-scan',11400322:'map-scan'}

def recover(output):
    import UnityPy
    from PIL import Image
    output.mkdir(exist_ok=True)
    env=UnityPy.load(str(PREFAB));objects={o.path_id:o.read_typetree() for o in env.objects};asset=next(iter(env.objects)).assets_file
    guid_paths={}
    for meta in (ROOT/'Assets').rglob('*.meta'):
        match=re.search(r'^guid: (\w+)',meta.read_text(errors='replace'),re.M)
        if match:guid_paths[match[1]]=Path(str(meta)[:-5])
    def resolve(pointer):
        if not pointer['m_FileID'] and not pointer['m_PathID']:return None
        raw=asset.externals[pointer['m_FileID']-1].guid.hex();return guid_paths[''.join(raw[i:i+2][::-1] for i in range(0,32,2))]
    sources={str(PREFAB.relative_to(ROOT)):digest(PREFAB)};textures={};effects=[]
    entries=[(PREFAB,identity,role) for identity,role in ROLES.items()]+[(PREFAB.parent/(name+'.prefab'),identity,role) for name,identity,role in WINDOWS]
    current=PREFAB
    for prefab,identity,role in entries:
        if prefab!=current:
            env=UnityPy.load(str(prefab));objects={o.path_id:o.read_typetree() for o in env.objects};asset=next(iter(env.objects)).assets_file;current=prefab
        sources[str(prefab.relative_to(ROOT))]=digest(prefab)
        fields=objects[identity];go=fields['m_GameObject']['m_PathID'];transform=next(d for d in objects.values() if 'm_LocalPosition'in d and d['m_GameObject']['m_PathID']==go)
        row=dict(role=role,prefab=str(prefab.relative_to(ROOT)),path_id=identity,fields=fields,transform=transform,textures={})
        for key in ('MainTex','SpecularMask','RandomTex','RandomTex2','AlphabetTex','SpecShader','HoloShader','m_Script'):
            if key not in fields:continue
            source=resolve(fields[key])
            if source is None:row['textures'][key]=None;continue
            sources[str(source.relative_to(ROOT))]=digest(source)
            if source.suffix!='.png':continue
            name='effect-'+source.name
            if name not in textures:
                with Image.open(source) as im:im.verify();width,height=im.size
                meta=Path(str(source)+'.meta');sources[str(meta.relative_to(ROOT))]=digest(meta)
                metadata=meta.read_text();filter_mode=int(re.search(r'filterMode: (-?\d+)',metadata)[1]);wrap_mode=int(re.search(r'wrapMode: (-?\d+)',metadata)[1])
                shutil.copyfile(source,output/name);textures[name]=dict(source=str(source.relative_to(ROOT)),width=width,height=height,filter_mode=filter_mode,wrap_mode=wrap_mode)
            row['textures'][key]=name
        effects.append(row)
    result=dict(format='PE-training-ui-effects-1',effects=effects,textures=textures,sources=sources,
                limitations=['Source PNG bytes are preserved; Unity build texture compression may differ.',
                             'Specular shader null MainTex uses white. Holographic null RandomTex uses shader bump default red=.5; null alphabet uses white.',
                             'UIEffectAlpha and refresh flag are caller-controlled; original game-time phase is not synchronized.'])
    (output/'manifest.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/ui/effects');args=parser.parse_args();output=args.output.resolve()
    if output.exists() or ROOT/'Assets' in output.parents:raise ValueError('Fresh output outside original Assets required')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.ui-effects-',dir=output.parent) as temp:
        result=recover(Path(temp));Path(temp).rename(output)
    print(f'Exported {len(result["effects"])} source UI effects and {len(result["textures"])} textures to {output}')
if __name__=='__main__':main()
