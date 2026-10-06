#!/usr/bin/env python3
"""Export guarded source starter clothing; no original writes or runtime gender feature."""
import argparse
from contextlib import closing
import json
from pathlib import Path
import re
import sqlite3
import sys
from export_content import ROOT,DEFAULT_DATABASE,connect_readonly,describe_source,numeric_rows

SOURCE_FILES=('Assets/PeWorld/SingleGameInitData.cs','Assets/Scripts/PeEntity/Cmpt/CommonCmpt.cs',
              'Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs','Assets/Scripts/Items/ItemData.cs','Assets/Scripts/Items/ItemObj.cs',
              'Assets/Scripts/PeEntity/Cmpt/EquipmentCmpt.cs','Assets/Scripts/SkillSystem/SkExpFuncSet.cs','Assets/Scripts/SkillSystem/SkAliveEntity.cs')
FEMALE=(95,131,192)
MALE=(113,149,210)
BUFFS=(30200097,30200098,30200100)
MODS='exp,27,target.buffPreAdd+=caster.sums[27];#exp,9,target.buffPreAdd+=caster.sums[9];#exp,10,target.buffPreAdd+=caster.sums[10]'


def expect(row,values,label):
    for key,value in values.items():
        if row[key]!=str(value):raise ValueError(f'{label} source contract changed: {key}')


def uncomment(text):
    # Only source-contract regions are parsed. Preserve literals while removing comments.
    return re.sub(r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|//[^\n]*|/\*[\s\S]*?\*/',lambda m:m.group(1) or '',text)


def body(text,pattern):
    match=re.search(pattern,text)
    if not match:raise ValueError('Missing source method '+pattern)
    start=text.index('{',match.end());depth=1;end=start+1
    while end<len(text) and depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    if depth:raise ValueError('Unclosed source contract method')
    return re.sub(r'\s+','',text[start+1:end-1])


def require(text,parts,label):
    for part in parts:
        if part not in text:raise ValueError(label+' source contract changed: '+part)


def validate_sources(source_root):
    sources={p:uncomment((source_root/p).read_text(encoding='utf-8-sig',errors='replace')) for p in SOURCE_FILES}
    init=sources[SOURCE_FILES[0]]
    cloth=body(init,r'static\s+void\s+AddDefaultClothToPlayer\s*\(\s*\)')
    if cloth!='int[]initEquip=newint[]{113,149,210,95,131,192};InitEquipment(PeCreature.Instance.mainPlayer,initEquip);':
        raise ValueError('Starter source initialization selection changed')
    adventure=body(init,r'public\s+static\s+void\s+AddAdventureInitData\s*\(\s*\)')
    if adventure!='AddDefaultClothToPlayer();AddAdventurePresent();':raise ValueError('Adventure clothing initialization changed')
    equip=body(init,r'static\s+void\s+InitEquipment\s*\(')
    require(equip,['PeSexentitySex=entity.ExtGetSex();','if(!PeGender.IsMatch(itemProto.equipSex,entitySex)){continue;}',
                   'ItemAsset.ItemMgr.Instance.CreateItem(equipmentItemProtoId)','equipmentCmpt.PutOnEquipment(itemObj);'],'Sex-filtered equipment')
    gender=sources[SOURCE_FILES[1]]
    require(re.sub(r'\s+','',gender),['Undefined=0,Female=1,Male=2'],'Sex enumeration')
    if body(gender,r'public\s+static\s+bool\s+IsMatch\s*\(')!='if(sex==PeSex.Undefined){returntrue;}if(sex==require){returntrue;}returnfalse;':
        raise ValueError('Sex matching contract changed')
    attributes=body(sources[SOURCE_FILES[2]],r'public\s+enum\s+AttribType')
    enum={};value=-1
    for entry in attributes.split(','):
        if not entry:continue
        pair=entry.split('=');value=int(pair[1]) if len(pair)==2 else value+1;enum[pair[0]]=value
    if any(enum.get(name)!=index for name,index in [('Def',27),('ComfortReducePercent',9),('ComfortRecoverSpeed',10)]):
        raise ValueError('Clothing buff attribute indices changed')
    items=re.sub(r'\s+','',sources[SOURCE_FILES[3]])
    require(items,['protoItem.durabilityFactor=Convert.ToSingle(reader.GetString(reader.GetOrdinal("durDec")));',
                   'protoItem.durabilityMax=Convert.ToInt32(reader.GetString(reader.GetOrdinal("_durability")));',
                   'protoItem.equipSex=Pathea.PeGender.Convert(Convert.ToInt32(reader.GetString(reader.GetOrdinal("sex"))));'],'Item metadata binding')
    obj=sources[SOURCE_FILES[4]]
    require(body(obj,r'public\s+class\s+Property\s*:'),['floatmFactor=1f;'],'Initial item property multiplier')
    require(body(obj,r'public\s+class\s+Durability\s*:'),['returnprotoData.durabilityMax;'],'Durability maximum binding')
    if body(obj,r'protected\s+void\s+CreateValue\s*\(')!='floatinitLife=valueMax;if(null!=mValue){initLife=mValue.current;}mValue=newPeFloatRangeNum(initLife,0f,valueMax);':
        raise ValueError('Fresh equipment durability initialization changed')
    require(body(obj,r'public\s+bool\s+AddBuff\s*\('),['if(DurabilityExhaust()){returnfalse;}','property.CreateSkBuff(skEntity)'],'Exhausted equipment buff')
    require(body(obj,r'protected\s+bool\s+ChangeDurability\s*\('),['d.value.Change(v);','if(d.value.IsCurrentMin()){RemoveBuff(skEntity);}'],'Durability removal')
    wear=body(obj,r'public\s+override\s+bool\s+ExpendDefenceDurability\s*\(')
    if wear!='floatd=hpChange*durabilityFactor;d=Mathf.Clamp(d,0f,Mathf.Infinity);returnChangeDurability(skEntity,-d);':
        raise ValueError('Defense wear expression changed')
    require(body(obj,r'public\s+SkillSystem.SkBuffInst\s+CreateSkBuff\s*\('),['valueList.Add(p.value*mFactor);','SkillSystem.SkEntity.MountBuff(skEntity,buffId,indexList,valueList)'],'Item buff properties')
    equipment=re.sub(r'\s+','',sources[SOURCE_FILES[5]])
    require(body(sources[SOURCE_FILES[5]],r'void\s+ReduceArmorDurability\s*\('),['i<mItemList.Count','equipCmpt.ExpendDefenceDurability(mSkEntity,damage);'],'Every equipped defense item wear')
    case=equipment.split('caseEMsg.Battle_BeAttacked:',1)[1].split('break;',1)[0]
    require(case,['if(PeGameMgr.IsBuild)return;','ReduceArmorDurability((float)args[0],(SkEntity)args[1]);'],'Build exclusion')
    for name in ['GetHurt','TryGetHurt']:
        hurt=body(sources[SOURCE_FILES[6]],r'public\s+void\s+'+name+r'\s*\(')
        require(hurt,['dmg*=skInst._colInfo.damageScale;','inst.Target.OnGetHurt(inst,dmg);'],'Unclamped resolver damage')
        if re.search(r'dmg=(?:Mathf|Math)\.(?:Min|Clamp)',hurt):raise ValueError('Resolver damage clamping changed')
    require(body(sources[SOURCE_FILES[7]],r'public\s+override\s+void\s+OnGetHurt\s*\('),['Entity.SendMsg(EMsg.Battle_BeAttacked,dmg,inst.Caster);'],'Wear damage notification')
    return [describe_source(source_root/p) for p in SOURCE_FILES]


def build_outputs(database=DEFAULT_DATABASE,source_root=ROOT):
    source_hashes=validate_sources(source_root)
    with closing(connect_readonly(database)) as db:
        items,_=numeric_rows(db,'PrototypeItem','id');buffs,_=numeric_rows(db,'skBuff','_id')
        class_rows=[]
        for identity in (2,15,16):
            matches=[dict(r) for r in db.execute('SELECT * FROM item_class WHERE id=?',(str(identity),))]
            if len(matches)!=1:raise ValueError(f'Ambiguous clothing class{identity}')
            row=matches[0]
            expect(row,{'classname':'Property,Strengthen,Repair,RecycleReplicate,Durability,Instantiate,EquipDefence'},f'ClothingClass{identity}')
            class_rows.append(row)
        outfits={}
        for sex,identities,sex_id in [('female',FEMALE,1),('male',MALE,2)]:
            rows=[]
            for index,identity in enumerate(identities):
                row=dict(items[identity]);defense=(7,4,1)[index];durability=(2156,2156,862)[index];slot=(2,64,128)[index]
                part=('torso','legs','feet')[index]
                expect(row,{'sex':sex_id,'Def':defense,'_durability':durability,'durDec':'.1','buffId':BUFFS[index],
                            '_position':slot,'_replacePos':slot,'_function':(2,15,16)[index],
                            '_modelPath':f'Model/PlayerModel/{sex}01-{part}','stacking_num':1,
                            'ComfortReducePercent':0,'ComfortRecoverSpeed':0},f'Clothing{identity}')
                rows.append(dict(item_id=identity,sex=sex_id,defense=float(row['Def']),durability=float(row['_durability']),
                                 wear_factor=float(row['durDec']),buff_id=int(row['buffId']),equip_position=slot,replace_position=slot,
                                 name=row['_engName'],model_path=row['_modelPath'],source_row=row))
            outfits[sex]=rows
        buff_rows=[]
        for identity,kind in zip(BUFFS,(65517,65518,65520)):
            row=dict(buffs[identity]);expect(row,{'_type':kind,'_stackLimit':1,'_timeActive':-1,'_timeInterval':0,'_mods':MODS},f'ClothingBuff{identity}');buff_rows.append(row)
    lines=['/* Generated by export_starter_outfit.py; do not edit. */','#ifndef PE_STARTER_OUTFIT_SOURCE_H','#define PE_STARTER_OUTFIT_SOURCE_H',
           '#define PE_STARTER_OUTFIT_SOURCE_COUNT 3',
           'typedef struct { int item_id; float defense, durability, wear_factor; int buff_id; } PeStarterOutfitSource;',
           '/* Fixed female appearance contract; male source bindings remain in JSON. */',
           'static const PeStarterOutfitSource pe_starter_outfit_sources[PE_STARTER_OUTFIT_SOURCE_COUNT] = {']
    for row in outfits['female']:
        lines.append('    {%d, %.1ff, %.1ff, %.1ff, %d},'%(row['item_id'],row['defense'],row['durability'],row['wear_factor'],row['buff_id']))
    lines+=['};','#endif','']
    manifest=dict(schema=1,source=describe_source(database),source_files=source_hashes,
                  initialization=dict(method='SingleGameInitData.AddAdventureInitData -> AddDefaultClothToPlayer -> InitEquipment',
                                      original_order=[113,149,210,95,131,192],selection='PeGender.IsMatch(itemProto.equipSex, entity.ExtGetSex())',
                                      creation='CreateItem then PutOnEquipment: actual independent instances, not base-stat changes'),
                  outfits=outfits,buffs=buff_rows,item_classes=class_rows,
                  semantics=dict(buff_indices={'Def':27,'ComfortReducePercent':9,'ComfortRecoverSpeed':10},
                                 other_buff_values='Both comfort fields are0 for all six starter items; item Hp/HpMax columns are not applied by these buffs.',
                                 effective_defense='base + sum(item.Def for equipped nonexhausted source instances)',
                                 intact_outfit_defense=12,wear='Each equipped defense item loses max(0, resolver_damage * item.durDec).',
                                 notification='Battle_BeAttacked receives source OnGetHurt damage after collision scaling, not remaining-HP-clamped damage.',
                                 exhausted='AddBuff rejects already exhausted durability; ChangeDurability removes its buff at minimum.',
                                 build_mode='Battle_BeAttacked returns before armor wear when PeGameMgr.IsBuild.',
                                 healing='HP recovery is not equipment durability repair.'),
                  scope='Generated source metadata only. C header targets fixed female appearance; male bindings are provenance, not runtime gender/equipment replacement support.')
    return {'pe_starter_outfit_source.h':'\n'.join(lines).encode(),
            'pe_starter_outfit_source.json':(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()}


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--database',type=Path,default=DEFAULT_DATABASE)
    p.add_argument('--output',type=Path,default=ROOT/'rebuild/generated');p.add_argument('--check',action='store_true');a=p.parse_args(argv)
    try:
        for name,data in build_outputs(a.database).items():
            path=a.output/name
            if a.check:
                if not path.is_file() or path.read_bytes()!=data:raise ValueError('Stale generated '+name)
            else:a.output.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        print('Starter outfit metadata checked' if a.check else 'Starter outfit metadata generated');return 0
    except (OSError,ValueError,KeyError,IndexError,sqlite3.Error) as error:print(error,file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())
