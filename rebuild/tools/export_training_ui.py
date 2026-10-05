#!/usr/bin/env python3
"""Recover original NGUI atlases, slice borders, bitmap fonts and icon mappings.

Optional UnityPy/Pillow are required to read serialized prefab metadata and crop
original bitmap-font sheets. No redraws, generated art or original-file writes.
Native manifest: PEUI1, then sprite FILE x y w h L T R B NAME(remainder).
Coordinates and borders are top-left PNG pixels. JSON preserves source padding,
conflicts and the two invalid legacy border repairs rather than hiding them.
"""
import argparse
from contextlib import closing
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import sqlite3
import tempfile

ROOT=Path(__file__).resolve().parents[2]
ATLAS_ROOT=ROOT/'Assets/Scripts/Chenzhi/NewUI/Resources/_Atlas'
DATABASE=ROOT/'Assets/Resources/DataBase/localData.bytes'
MINIMAP=ROOT/'Assets/Scripts/Jaluca/TrainingField/Texture/TrainingRoomMinimap.png'
ATLAS_NAMES=[('NewUIAtlas','ui-atlas.png','ui'),('NewIcon','icons-atlas.png','icons'),('VCE Atlas','vce-atlas.png','vce')]
FONT_NAMES=['ui_Font_14','ui_Font_14_bold','ui_Font_18','ui_Font_18_bold','ui_Font_18_talk','ui_Font_26','ui_Font_27_endbold','ui_Font_32']


def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def rectangle(raw,width,height):
    values=[raw[k] for k in ('x','y','width','height')]
    if any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) or v!=int(v) for v in values):raise ValueError('Invalid integer sprite rectangle')
    x,y,w,h=map(int,values)
    if x<0 or y<0 or w<=0 or h<=0 or x+w>width or y+h>height:raise ValueError('Sprite outside atlas')
    return [x,y,w,h]


def sprite_row(raw,width,height,filename,name):
    outer=rectangle(raw['outer'],width,height);inner=raw['inner'];x,y,w,h=outer
    source_border=[inner['x']-x,inner['y']-y,x+w-inner['x']-inner['width'],y+h-inner['y']-inner['height']]
    if any(not math.isfinite(v) or v!=int(v) for v in source_border):raise ValueError('Invalid sprite border')
    border=[max(0,int(v)) for v in source_border]
    if border[0]+border[2]>w or border[1]+border[3]>h:raise ValueError('Sprite borders overlap')
    padding={k:raw[k] for k in ('paddingLeft','paddingTop','paddingRight','paddingBottom')}
    if any(not math.isfinite(v) or v<0 for v in padding.values()):raise ValueError('Invalid sprite padding')
    return dict(name=name,source_name=raw['name'],file=filename,rect=outer,border=border,
                source_border=source_border,border_clamped=border!=source_border,padding=padding)


def bitmap_fields(line):
    return {k:v.strip('"') for k,v in re.findall(r'(\w+)=("[^"]*"|\S+)',line)}


def validate_font(text,width,height):
    lines=text.splitlines();common=next((bitmap_fields(l) for l in lines if l.startswith('common ')),None)
    if not common or (int(common['scaleW']),int(common['scaleH']),int(common['pages']))!=(width,height,1):raise ValueError('Bitmap font dimensions/pages differ')
    declared=next((int(bitmap_fields(l)['count']) for l in lines if l.startswith('chars ')),None)
    glyphs=[];seen=set()
    for line in lines:
        if not line.startswith('char '):continue
        raw=bitmap_fields(line);g={k:int(raw[k]) for k in ('id','x','y','width','height','xoffset','yoffset','xadvance','page','chnl')}
        if g['id'] in seen or not 0<=g['id']<=0x10ffff or g['page']!=0 or min(g['x'],g['y'],g['width'],g['height'])<0 or g['x']+g['width']>width or g['y']+g['height']>height:raise ValueError('Invalid bitmap glyph')
        seen.add(g['id']);glyphs.append(g)
    if not glyphs or len(glyphs)!=declared or len(glyphs)>65536:raise ValueError('Bitmap font glyph count differs')
    return dict(common={k:int(v) for k,v in common.items()},glyphs=glyphs)


def icon_mappings(names):
    items=[];portraits=[]
    with closing(sqlite3.connect(DATABASE.as_uri()+'?mode=ro',uri=True)) as connection:
        for identity,icons in connection.execute('SELECT id,_iconId FROM PrototypeItem'):
            if not re.fullmatch(r'[0-9]+',identity):continue
            name=icons.split(',')[0]
            if name in names:items.append(dict(item_id=int(identity),sprite=names[name]))
        for identity,name,small,large in connection.execute('SELECT id,default_name,npc_icon,npc_bigicon FROM PrototypeNPC'):
            if not re.fullmatch(r'[0-9]+',identity):continue
            if small in names or large in names:portraits.append(dict(prototype_id=int(identity),name=name,small=names.get(small),large=names.get(large)))
    return sorted(items,key=lambda r:r['item_id']),sorted(portraits,key=lambda r:r['prototype_id'])


def recover(output):
    import UnityPy
    from PIL import Image
    from export_training_ui_effects import recover as recover_effects
    sources={};atlases=[];sprites=[];by_atlas={};images={};used=set();lines=['PEUI1\n'];skipped=[]
    def record(path):sources[str(path.relative_to(ROOT))]=digest(path)
    def mono(path):
        record(path);env=UnityPy.load(str(path));obj,=[o for o in env.objects if o.type.name=='MonoBehaviour'];return obj.read_typetree()
    for source_name,filename,namespace in ATLAS_NAMES:
        base=(ROOT/'Assets/Scripts/ZhouXun/Voxel Creation/Resources/GUI/Atlas') if source_name=='VCE Atlas' else ATLAS_ROOT/'_UIBase'
        prefab=base/(source_name+'.prefab');png=base/(source_name+'.png')
        data=mono(prefab);record(png);image=Image.open(png);image.load();width,height=image.size
        if data['mCoordinates']!=0 or data['mPixelSize']!=1 or data['mReplacement']['m_PathID']:raise ValueError('Unsupported atlas coordinate/replacement mode')
        if width*height>16777216 or image.mode!='RGBA':raise ValueError('Unexpected atlas format')
        shutil.copyfile(png,output/filename);images[source_name]=image;by_atlas[source_name]={}
        for raw in data['sprites']:
            name=raw['name']
            if source_name=='VCE Atlas' and name not in ('female_icon','male_icon'):continue
            if not name:skipped.append(dict(atlas=source_name,reason='Empty sprite name',source=raw));continue
            if len(name.encode())>256 or any(c in name for c in '\r\n\t'):raise ValueError('Invalid sprite name')
            native_name=name
            if native_name in used:
                native_name=namespace+':'+name;duplicate=1
                while native_name in used:native_name=namespace+':'+name+':duplicate'+str(duplicate);duplicate+=1
            used.add(native_name);row=sprite_row(raw,width,height,filename,native_name);row['atlas']=source_name
            by_atlas[source_name].setdefault(name,row);sprites.append(row)
            lines.append('sprite '+filename+' '+' '.join(map(str,row['rect']+row['border']))+' '+native_name+'\n')
        atlases.append(dict(source=source_name,file=filename,width=width,height=height,source_sprite_count=len(data['sprites'])))
    if len(sprites)>4096:raise ValueError('Native sprite cap exceeded')
    (output/'atlas.peui').write_text(''.join(lines),encoding='utf-8')
    fonts=[]
    for name in FONT_NAMES:
        prefab=ATLAS_ROOT/'_Font'/(name+'.prefab');text_path=ATLAS_ROOT/'_Font'/((name if name!='ui_Font_18_talk' else 'ui_Font_18_bold')+'.txt')
        data=mono(prefab);font=data['mFont'];sprite=by_atlas['NewUIAtlas'].get(font['mSpriteName'])
        if not sprite:raise ValueError('Missing original bitmap-font atlas sprite')
        x,y,w,h=sprite['rect']
        if (w,h)!=(font['mWidth'],font['mHeight']):raise ValueError('Font sheet/atlas crop mismatch')
        record(text_path);text=text_path.read_text(encoding='gb18030' if name=='ui_Font_27_endbold' else 'utf-8');metrics=validate_font(text,w,h)
        if len(font['mSaved'])!=len(metrics['glyphs']):raise ValueError('Prefab/text font counts differ')
        for a,b in zip(sorted(font['mSaved'],key=lambda g:g['index']),sorted(metrics['glyphs'],key=lambda g:g['id'])):
            for source_key,key in [('index','id'),('x','x'),('y','y'),('width','width'),('height','height'),('offsetX','xoffset'),('offsetY','yoffset'),('advance','xadvance')]:
                if a[source_key]!=b[key]:raise ValueError('Prefab/text font metrics differ')
        png=name+'.png';fnt=name+'.fnt';images['NewUIAtlas'].crop((x,y,x+w,y+h)).save(output/png)
        text=re.sub(r'^page id=0 file="[^"]+"',f'page id=0 file="{png}"',text,flags=re.MULTILINE)
        (output/fnt).write_text(text)
        fonts.append(dict(name=name,file=fnt,texture=png,sprite=font['mSpriteName'],size=font['mSize'],base=font['mBase'],
                          spacing_x=data['mSpacingX'],spacing_y=data['mSpacingY'],**metrics))
    ttf=[]
    for source_name,target in [('Orbitron Light.ttf','orbitron-light.ttf'),('Orbitron Bold.ttf','orbitron-bold.ttf'),('Arial_TowerTip.ttf','arial-towertip.ttf')]:
        path=ROOT/'Assets/Resources/Font'/source_name;record(path);shutil.copyfile(path,output/target);ttf.append(dict(file=target,source=str(path.relative_to(ROOT))))
    names={name:row['name'] for name,row in by_atlas['NewIcon'].items()};items,portraits=icon_mappings(names);record(DATABASE)
    record(MINIMAP)
    with Image.open(MINIMAP) as minimap_image:
        minimap_image.verify()
        minimap_size=minimap_image.size
    shutil.copyfile(MINIMAP,output/'training-minimap.png')
    minimap=dict(file='training-minimap.png',source=str(MINIMAP.relative_to(ROOT)),width=minimap_size[0],height=minimap_size[1],
                 coordinates='Original PNG orientation preserved; world-to-map transform is separate runtime metadata')
    recovered_effects=recover_effects(output/'effects')
    result=dict(format='PE-training-UI-assets-1',native_manifest='atlas.peui',coordinates='Top-left PNG pixels; borders L,T,R,B; NGUI padding preserved separately',
                atlases=atlases,sprites=sprites,fonts=fonts,ttf=ttf,item_icons=items,portraits=portraits,sources=sources,skipped=skipped,
                minimap=minimap,
                effects=dict(manifest='effects/manifest.json',count=len(recovered_effects['effects'])),
                limitations=['Repository NGUI source assets; installed atlas equality is not asserted.',
                             'Ten cross-atlas sprite name conflicts retain UI name and namespace icon name with icons: prefix.',
                             'Duplicate within-atlas names are explicitly namespaced; source record order preserved.',
                             'Two invalid negative legacy border values clamp to zero; source_border preserves original values.',
                             'Sprite rectangles are trimmed source bounds; manifest padding is needed for full NGUI geometry parity.',
                             'Recovering atlas/font assets alone does not reproduce layouts, shader blending, animations or interaction logic.'])
    (output/'manifest.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/ui');args=parser.parse_args();output=args.output.expanduser().resolve()
    if output.exists() or output==ROOT/'Assets' or ROOT/'Assets' in output.parents:raise ValueError('Output must be fresh and outside source Assets')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.ui-assets-',dir=output.parent) as temporary:
        staging=Path(temporary);result=recover(staging);staging.rename(output)
    print(f'Exported {len(result["sprites"])} sprites, {len(result["fonts"])} bitmap fonts to {output}')

if __name__=='__main__':main()
