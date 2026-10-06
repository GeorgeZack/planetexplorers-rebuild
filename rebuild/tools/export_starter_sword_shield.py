#!/usr/bin/env python3
"""Guard/export modern Wood Sword1 + Wood Shield34 metadata, never runtime assets.

Requires optional UnityPy for repository prefabs. --game additionally verifies
installed ResourceManager bindings read-only; its local paths never enter output.
"""
import argparse
from contextlib import closing
import json
from pathlib import Path
import re
import sqlite3
import struct
import sys

from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
from export_starter_outfit import body, expect, require, uncomment

SWORD = 'Assets/Resources/Prefab/Item/Equip/Weapon/Sword/wood_sword.prefab'
SHIELD = 'Assets/Resources/Prefab/Item/Equip/Armor/Shield/wood_shield.prefab'
SCRIPTS = {
    'sword': 'Assets/Scripts/Items/PEEquipment/PeSword.cs',
    'shield': 'Assets/Scripts/Items/PEEquipment/PESheild.cs',
    'trigger': 'Assets/Scripts/Attack/PEAttackTrigger.cs',
    'hold': 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sheild.cs',
    'attack': 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs',
    'item': 'Assets/Scripts/Items/ItemObj.cs',
    'equipment': 'Assets/Scripts/PeEntity/Cmpt/EquipmentCmpt.cs',
    'alive': 'Assets/Scripts/SkillSystem/SkAliveEntity.cs',
    'hurt': 'Assets/Scripts/SkillSystem/SkExpFuncSet.cs',
    'condition': 'Assets/Scripts/SkillSystem/SkCond.cs',
    'relation': 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs',
    'attributes': 'Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs',
    'holdable': 'Assets/Scripts/Items/PEEquipment/PEHoldAbleEquipment.cs',
}
SOURCE_FILES = (SWORD, SHIELD, *SCRIPTS.values(), *(SCRIPTS[k]+'.meta' for k in ('sword','shield','trigger')))
SKILLS = {'m_SkillID':20110011, 'm_SkillInMove':20110014, 'm_SkillInSprint':20110013,
          'm_SkillInAir':20110012, 'm_SkillInWater':20110005}
BUFF_MODS = {
    30200087:'exp,25,target.buffPreAdd+=caster.sums[25]*target.sums[77]',
    30200093:'exp,27,target.buffPreAdd+=caster.sums[27]*target.sums[78];#exp,33,target.buffPreAdd+=caster.sums[33]',
    30200177:'exp,1,target.raws[70]=250;#exp,1,target.raws[71]=300;#exp,1,target.raws[72]=9999;#exp,1,target.raws[73]=9999',
}
HARE_MODS = 'exp,29,target.raws[29]=-1;#exp,1,target.expFunc.TryGetHurt(caster.sums[25]*caster.sums[25]/(target.sums[27]+caster.sums[25])*(target.sums[35]==1?(1-target.sums[33]):1)*(target.expFunc.RandIn(caster.sums[26])?2:1),0.1f)'


def equal(actual, expected, label):
    if actual != expected: raise ValueError(label+' source contract changed')


def f32(value): return struct.unpack('<f',struct.pack('<f',value))[0]


def vector(x,y,z): return dict(x=f32(x),y=f32(y),z=f32(z))


def parse_enum(text,name):
    result={};index=-1
    for entry in body(text,r'public\s+enum\s+'+name+r'\b').split(','):
        if not entry: continue
        pair=entry.split('=');index=int(pair[1]) if len(pair)==2 else index+1;result[pair[0]]=index
    return result


def validate_sources(source_root):
    text={k:uncomment((source_root/v).read_text(encoding='utf-8-sig',errors='replace')) for k,v in SCRIPTS.items()}
    compact={k:re.sub(r'\s+','',v) for k,v in text.items()}
    require(compact['sword'],['classPeSword:PEHoldAbleEquipment'],'Modern sword class')
    require(compact['shield'],['classPESheild:PECtrlAbleEquipment','HoldSheildAnim="SheildBlock";',
                               'AttachBoneName="BoneLForearm";','m_View.AttachObject(gameObject,AttachBoneName);'],'Shield attachment')
    require(body(text['holdable'],r'public\s+override\s+void\s+InitEquipment\s*\('),
            ['m_View.AttachObject(gameObject,m_HandChangeAttr.m_PutOffBone);'],'Initial stowed attachment')
    equal(body(text['hold'],r'public\s+override\s+bool\s+CanDoAction\s*\('),'returnnull!=sheild;','Shield admission')
    hold=body(text['hold'],r'public\s+override\s+void\s+DoAction\s*\(')
    equal(hold,'if(null==sheild||null==anim)return;anim.SetBool(PESheild.HoldSheildAnim,true);motionMgr.SetMaskState(PEActionMask.HoldShield,true);SkillSystem.SkEntity.MountBuff(skillCmpt,SheidBuffID,null,null);if(null!=move&&!motionMgr.IsActionRunning(PEActionType.EquipmentHold))move.style=MoveStyle.Sword;if(null!=skillCmpt)skillCmpt.SetAttribute(AttribType.EnableShieldBlock,1);if(null!=sheild&&hideEquipInactive)sheild.gameObject.SetActive(true);','Shield hold')
    equal(body(text['hold'],r'public\s+override\s+bool\s+Update\s*\('),'if(null==sheild||null==anim)returntrue;if(null!=move&&!motionMgr.IsActionRunning(PEActionType.EquipmentHold))move.style=MoveStyle.Sword;returnfalse;','Shield hold update')
    require(compact['hold'],['constintSheidBuffID=30200177;'],'Shield reaction buff')
    require(body(text['hold'],r'public\s+override\s+void\s+EndImmediately\s*\('),
            ['SetMaskState(PEActionMask.HoldShield,false);','anim.SetBool(PESheild.HoldSheildAnim,false);',
             'skillCmpt.SetAttribute(AttribType.EnableShieldBlock,0);','skillCmpt.CancelBuffById(SheidBuffID);'],'Shield release')
    require(body(text['attack'],r'void\s+UpdateAnimSpeed\s*\('),['floatanimSpeed=sword.m_AnimSpeed;',
            'if(stamina<=sword.m_AnimDownThreshold[i])animSpeed*=0.9f;',
            'animSpeed=Mathf.Clamp(animSpeed,0.5f,1.5f);'],'Weapon animation rate')
    require(body(text['attack'],r'void\s+CostStamina\s*\('),
            ['skillCmpt.GetAttribute(AttribType.Stamina)-sword.m_StaminaCost[attackModeIndex]*motionMgr.Entity.GetAttribute(AttribType.StaminaReducePercent)'],'Weapon stamina')
    equal(body(text['item'],r'public\s+override\s+bool\s+ExpendAttackDurability\s*\('),
          'returnChangeDurability(skEntity,-durabilityFactor);','Attack wear')
    equal(body(text['item'],r'public\s+override\s+bool\s+ExpendDefenceDurability\s*\('),
          'floatd=hpChange*durabilityFactor;d=Mathf.Clamp(d,0f,Mathf.Infinity);returnChangeDurability(skEntity,-d);','Defence wear')
    require(body(text['item'],r'public\s+bool\s+AddBuff\s*\('),['if(DurabilityExhaust()){returnfalse;}'],'Exhausted admission')
    require(body(text['item'],r'protected\s+bool\s+ChangeDurability\s*\('),['if(d.value.IsCurrentMin()){RemoveBuff(skEntity);}'],'Exhausted removal')
    require(body(text['alive'],r'public\s+override\s+void\s+OnHurtSb\s*\('),
            ['Entity.SendMsg(EMsg.Battle_EquipAttack,m_MotionEquip.Weapon.ItemObj);'],'Successful-hit wear')
    require(body(text['alive'],r'public\s+override\s+void\s+OnGetHurt\s*\('),
            ['Entity.SendMsg(EMsg.Battle_BeAttacked,dmg,inst.Caster);'],'Received-hit wear')
    require(body(text['equipment'],r'void\s+ReduceArmorDurability\s*\('),
            ['i<mItemList.Count','equipCmpt.ExpendDefenceDurability(mSkEntity,damage);'],'Every defence item wear')
    require(compact['equipment'],['equip.equipPos&existEquip.equipPos'],'Equipment coexistence')
    for event,call in [('Battle_EquipAttack','ReduceWeaponDurability((ItemObject)args[0]);'),
                       ('Battle_BeAttacked','ReduceArmorDurability((float)args[0],(SkEntity)args[1]);')]:
        section=compact['equipment'].split('caseEMsg.'+event+':',1)[1].split('break;',1)[0]
        require(section,['if(PeGameMgr.IsBuild)return;',call],'Build wear exclusion')
    hurt=body(text['hurt'],r'public\s+void\s+TryGetHurt\s*\(')
    require(hurt,['dmg*=skInst._colInfo.damageScale;','inst.Target.OnGetHurt(inst,dmg);'],'Resolver damage wear')
    if re.search(r'dmg=(?:Mathf|Math)\.(?:Min|Clamp)',hurt):raise ValueError('Resolver damage clamping changed')
    equal(body(text['condition'],r'internal\s+static\s+SkFuncInOutPara\s+TstColInfo\s*\('),
          'SkFuncInOutParafuncOut=newSkFuncInOutPara(inst,para);if(inst._colInfo==null)returnfuncOut;SkEntitycurTar=(SkEntity)inst._colInfo.hitTrans;if(curTar!=null){funcOut._para=newList<SkEntity>(){curTar};funcOut._ret=true;}returnfuncOut;','Hare collision condition')
    attributes=parse_enum(text['attributes'],'AttribType')
    for name,index in [('Atk',25),('Def',27),('ShieldMeleeProtect',33),('EnableShieldBlock',35),
                       ('ThresholdWhacked',70),('ThresholdRepulsed',71),('ThresholdWentfly',72),('ThresholdKnocked',73),
                       ('SwordMultiple',77),('ShieldMultiple',78)]:equal(attributes[name],index,'Attribute '+name)
    actions=parse_enum(text['relation'],'PEActionType')
    for name,index in [('Sprint',2),('EquipmentHold',13),('SwordAttack',14),('HoldShield',15),('EquipmentPutOff',16)]:
        equal(actions[name],index,'Action '+name)
    require(compact['relation'],['dependData.maskValue=(mask==1);'],'Action mask table interpretation')


def read_prefab(path):
    import UnityPy
    env=UnityPy.load(str(path));file,=env.files.values()
    return {'objects':{o.path_id:dict(type=o.type.name,data=o.read_typetree()) for o in env.objects},
            'external_guids':[bytes(x.guid).hex() for x in file.externals]}


def guid_from_serialized(value):
    # Unity5 editor GUID stores each byte's hex digits swapped against .meta.
    return ''.join(value[i+1]+value[i] for i in range(0,len(value),2))


def script_binding(prefab,component,source_root,key):
    ref=component['m_Script'];equal(ref['m_PathID'],11500000,key+' script path')
    if not 1<=ref['m_FileID']<=len(prefab['external_guids']):raise ValueError('Missing external script')
    meta=(source_root/(SCRIPTS[key]+'.meta')).read_text()
    match=re.search(r'^guid: ([a-f0-9]{32})$',meta,re.M)
    if not match:raise ValueError('Missing script GUID')
    guid=guid_from_serialized(prefab['external_guids'][ref['m_FileID']-1])
    equal(guid,match[1],key+' modern script binding')
    return dict(script=SCRIPTS[key],guid=guid,source_script_reference=ref)


def validate_prefabs(sword,shield,source_root=ROOT):
    def get(prefab,identity,kind):
        obj=prefab['objects'][identity];equal(obj['type'],kind,'Prefab object type');return obj['data']
    s=get(sword,11400004,'MonoBehaviour');b=get(shield,11400000,'MonoBehaviour')
    t=get(sword,11412292,'MonoBehaviour')
    bindings={key:script_binding(p,ob,source_root,key) for key,p,ob in [('sword',sword,s),('shield',shield,b),('trigger',sword,t)]}
    for ob in (s,b,t):equal(ob['m_Enabled'],1,'Modern component enabled')
    equal(get(sword,100002,'GameObject')['m_Name'],'wood_sword','Sword root')
    equal(get(shield,100000,'GameObject')['m_Name'],'wood_shield','Shield root')
    equal(s['m_GameObject']['m_PathID'],100002,'Sword component owner')
    equal(b['m_GameObject']['m_PathID'],100000,'Shield component owner')
    attachment=s['m_HandChangeAttr']
    for k,v in {'m_Layers':[],'m_PutOnBone':'mountMain','m_PutOffBone':'mountBack','m_PutOnAnim':'SwordPutOn',
                'm_PutOffAnim':'SwordPutOff','m_ActiveActionType':13,'m_UnActiveActionType':16,
                'm_HoldActionMask':13,'m_BaseMoveStyle':0,'m_MoveStyle':1}.items():equal(attachment[k],v,'Sword '+k)
    equal(s['m_RemoveEndAction'],[14],'Sword removal');equal(b['m_RemoveEndAction'],[],'Shield removal')
    equal(s['m_AttackSkill'],[SKILLS],'Modern sword skills')
    equal(s['m_StaminaCost'],[5.0],'Sword stamina');equal(s['m_AnimSpeed'],1.0,'Sword rate')
    equal(s['m_AnimDownThreshold'],[50.0,20.0],'Sword low stamina')
    capsule=dict(trans={'m_FileID':0,'m_PathID':400004},radius=f32(.2),heigh=f32(1.8),offset=vector(0,0,-.1),axis=2,TestHit=0)
    equal(t['attackParts'],[dict(attackForm=0,capsule=capsule)],'Sword capsule')
    equal(t['attackLayer']['m_Bits'],34154496,'Sword layer mask')
    equal(t['m_GameObject']['m_PathID'],100004,'Blade trigger owner')
    equal(get(sword,100004,'GameObject')['m_Name'],'SingleHandSword','Blade event binding')
    blade=get(sword,400004,'Transform');equal(blade['m_GameObject']['m_PathID'],100004,'Blade transform owner')
    for p,i in ((sword,400002),(sword,400004),(sword,400008),(shield,400000)):
        tr=get(p,i,'Transform')
        equal(tr['m_LocalPosition'],vector(0,0,0),'Identity attachment position')
        equal(tr['m_LocalRotation'],dict(x=0.,y=0.,z=0.,w=1.),'Identity attachment rotation')
        equal(tr['m_LocalScale'],vector(1,1,1),'Identity attachment scale')
    equal(blade['m_Father'],{'m_FileID':0,'m_PathID':400002},'Sword blade ancestry')
    box=get(sword,6500000,'BoxCollider')
    equal(box['m_GameObject']['m_PathID'],100004,'Sword broadphase owner')
    for k,v in {'m_IsTrigger':1,'m_Enabled':0,'m_Size':vector(.6,.6,2.5),'m_Center':vector(0,0,1)}.items():equal(box[k],v,'Sword broadphase '+k)
    if any('Collider' in x['type'] for x in shield['objects'].values()):raise ValueError('Shield gained physical collider')
    equal(sum(x['type']=='MonoBehaviour' for x in shield['objects'].values()),1,'Shield component set')
    mesh=get(shield,400002,'Transform')
    equal(mesh['m_Father'],{'m_FileID':0,'m_PathID':400000},'Shield mesh ancestry')
    equal(mesh['m_LocalPosition'],vector(-.191,-.052,.006),'Shield mesh position')
    equal(mesh['m_LocalRotation'],dict(x=-3.248115731935286e-08,y=.979924738407135,z=-.1993679404258728,w=-1.5965001409767865e-07),'Shield mesh rotation')
    equal(mesh['m_LocalScale'],vector(1,1,1),'Shield mesh scale')
    return dict(bindings=bindings,sword_component=s,shield_component=b,attack_trigger=t,
                capsule=capsule,broadphase_box=box,shield_mesh_transform=mesh)


def database_contract(database):
    with closing(connect_readonly(database)) as db:
        items,_=numeric_rows(db,'PrototypeItem','id');buffs,_=numeric_rows(db,'skBuff','_id')
        rows={}
        for identity,values in [(1,dict(_modelPath='Prefab/Item/Equip/Weapon/Sword/wood_sword',_position=16,_function=5,
                                        _WeaponType=8,Atk=10,Def=0,_durability=2369,durDec=5,buffId=30200087,ShieldMeleeProtect=0)),
                                (34,dict(_modelPath='Prefab/Item/Equip/Armor/Shield/wood_shield',_position=512,_function=18,
                                         _WeaponType=256,Atk=0,Def=9,_durability=4312,durDec='.1',buffId=30200093,ShieldMeleeProtect='.22'))]:
            row=dict(items[identity]);expect(row,dict(values,_replacePos=0,skillId=0,stacking_num=1),f'Item{identity}');rows[identity]=row
        classes=[]
        for identity,suffix in [(5,'EquipAttack'),(18,'EquipDefence')]:
            found=[dict(r) for r in db.execute('select * from item_class where id=?',(str(identity),))]
            if len(found)!=1:raise ValueError('Missing/ambiguous equipment class')
            expect(found[0],{'classname':'Property,Strengthen,Repair,RecycleReplicate,Durability,Instantiate,'+suffix},'Equipment class')
            classes+=found
        buff_rows=[]
        for identity,kind in [(30200087,65513),(30200093,65514),(30200177,10003)]:
            row=dict(buffs[identity]);expect(row,{'_type':kind,'_stackLimit':1,'_timeActive':-1,'_timeInterval':0,'_mods':BUFF_MODS[identity]},f'Buff{identity}');buff_rows.append(row)
        actions,_=numeric_rows(db,'ActionRelation','ActionType')
        expect(actions[14],{'Name':'SwordAttack','EndImmediately':'1,2,3,15,31','HoldEquip':1,'HoldShield':0},'Sword action relation')
        expect(actions[15],{'Name':'HoldShield','EndImmediately':2,'SwordAttack':2,'Sprint':2,'HoldEquip':0},'Shield action relation')
        triggers,_=numeric_rows(db,'skTriggerEvent','_id');hare=dict(triggers[10100])
        expect(hare,{'_cond':'colinfo','_modsTarget':HARE_MODS},'Hare damage')
        defaults,_=numeric_rows(db,'initProp','id');player=dict(defaults[1])
        expect(player,{'Atk':10,'Def':0,'SwordMultiple':1,'ShieldMultiple':1,'EnableShieldBlock':0,'ShieldMeleeProtect':0},'Initial player')
        skills,_=numeric_rows(db,'skMain','_id')
        for identity,effect,triggers in [(20110011,8,'493;502;503;237'),(20110014,42,'237')]:
            expect(skills[identity],{'_effMainEachTime':effect,'_triggerEvents':triggers,'_cond':'CondTstFunc,SwordAttack;CondTstFunc,SwordAttack;CondTstFunc,SwordAttack;False'},'Sword skill')
    return dict(items=rows,item_classes=classes,buffs=buff_rows,action_relations=[dict(actions[i]) for i in (14,15)],
                hare_trigger=hare,player_defaults={k:player[k] for k in ('id','Atk','Def','SwordMultiple','ShieldMultiple','EnableShieldBlock','ShieldMeleeProtect')},
                skills=[dict(skills[i]) for i in SKILLS.values()])


def verify_installed(game,prefabs):
    """Verify modern ResourceManager references, never a same-named legacy bundle."""
    from export_reference_player import Assets
    assets=Assets(game/'PE_Client_Data')
    manager,=[o for o in assets.file('mainData').objects.values() if o.type.name=='ResourceManager']
    result={}
    for key,resource,classname in [('sword','prefab/item/equip/weapon/sword/wood_sword','PeSword'),
                                    ('shield','prefab/item/equip/armor/shield/wood_shield','PESheild')]:
        matches=[assets.resolve(manager,p) for n,p in manager.read_typetree()['m_Container'] if n==resource]
        root,=[o for o in matches if o.type.name=='GameObject']
        components=assets.components(root,'MonoBehaviour');classes=[]
        for component in components:
            header=component.read_typetree(check_read=False)
            classes.append(assets.resolve(component,header['m_Script']).read_typetree()['m_ClassName'])
        equal(classes,[classname],key+' installed modern class')
        result[key]=dict(root=root.path_id,component=components[0].path_id,resource=resource)
        if key=='sword':
            blade=assets.child(root,'SingleHandSword');transform,=assets.components(blade,'Transform')
            triggers=[]
            for component in assets.components(blade,'MonoBehaviour'):
                h=component.read_typetree(check_read=False)
                if assets.resolve(component,h['m_Script']).read_typetree()['m_ClassName']=='PEAttackTrigger':triggers.append(component)
            trigger,=triggers;raw=trigger.get_raw_data();equal(len(raw),80,'Installed trigger layout')
            actual=struct.unpack_from('<IIiiqff3fi',raw,32)
            equal(actual,(34154496,1,0,0,transform.path_id,f32(.2),f32(1.8),0.,0.,f32(-.1),2),'Installed sword capsule')
            box,=assets.components(blade,'BoxCollider');box=box.read_typetree()
            for k in ('m_IsTrigger','m_Enabled','m_Size','m_Center'):equal(box[k],prefabs['broadphase_box'][k],'Installed broadphase')
        else:
            def walk(go):
                if any(assets.components(go,t) for t in ('BoxCollider','CapsuleCollider','SphereCollider','MeshCollider')):raise ValueError('Installed shield gained collider')
                tr,=assets.components(go,'Transform')
                for child in tr.read_typetree()['m_Children']:
                    ch=assets.resolve(tr,child);walk(assets.resolve(ch,ch.read_typetree()['m_GameObject']))
            walk(root)
    return result


def build_outputs(database=DEFAULT_DATABASE,source_root=ROOT):
    validate_sources(source_root)
    prefabs=validate_prefabs(read_prefab(source_root/SWORD),read_prefab(source_root/SHIELD),source_root)
    db=database_contract(database)
    manifest=dict(schema=1,database=describe_source(database),sources=[describe_source(source_root/p) for p in SOURCE_FILES],
                  sword=db['items'][1],shield=db['items'][34],prefabs=prefabs,
                  **{k:v for k,v in db.items() if k!='items'},
                  semantics=dict(sword_wear='5 per successful OnHurtSb notification, not per swing; Build bypass.',
                                 shield_wear='max(0,resolver_damage*.1) for each received hit; Build bypass.',
                                 exhausted='Equipment property buff is refused/removed at zero durability.',
                                 block='Holding toggles attribute35 and reaction buff30200177, without hold stamina cost.',
                                 block_eligibility='SwordAttack ends HoldShield; HoldShield requires no SwordAttack or Sprint.',
                                 mitigation='Only matching source damage expressions, including hare10100, apply1-attribute33 while attribute35==1.',
                                 geometry='Shield has no physical collider. Sword capsule is event-gated, distinct from broadphase.',
                                 captures='No Sword locomotion, masked block or draw/stow pose qualification is supplied by metadata.'),
                  scope='Source metadata only; no runtime equipment support, poses, root motion or generated assets.')
    sword=db['items'][1];shield=db['items'][34];s=prefabs['sword_component'];capsule=prefabs['capsule']
    # Header owns only numeric item/source contracts. No dependency on a runtime vector or equipment type.
    header='''/* Generated by export_starter_sword_shield.py; do not edit. */
#ifndef PE_STARTER_SWORD_SHIELD_SOURCE_H
#define PE_STARTER_SWORD_SHIELD_SOURCE_H
typedef struct {
    int item_id, equip_mask, replace_mask, buff_id;
    float attack, durability, wear_per_damage_notification;
    float animation_speed, stamina_cost, stamina_thresholds[2];
    int stationary_skill, running_skill, sprint_skill, air_skill, water_skill;
    const char *held_bone, *stowed_bone;
} PeStarterSwordSource;
typedef struct {
    int item_id, equip_mask, replace_mask, buff_id, hold_buff_id;
    float defense, melee_protect, durability, wear_factor;
    const char *attachment_bone;
} PeStarterShieldSource;
typedef struct {
    unsigned layer_mask;
    int form, axis;
    float radius, height, offset[3];
} PeStarterSwordCapsuleSource;
'''
    header+='static const PeStarterSwordSource pe_starter_sword_source = {\n    %d, %d, %d, %d, %.1ff, %.1ff, %.1ff, %.1ff, %.1ff, {%.1ff, %.1ff},\n    %s, "mountMain", "mountBack"\n};\n'%(
        int(sword['id']),int(sword['_position']),int(sword['_replacePos']),int(sword['buffId']),float(sword['Atk']),float(sword['_durability']),float(sword['durDec']),s['m_AnimSpeed'],s['m_StaminaCost'][0],*s['m_AnimDownThreshold'],', '.join(str(v) for v in SKILLS.values()))
    header+='static const PeStarterShieldSource pe_starter_shield_source = {\n    %d, %d, %d, %d, 30200177, %.1ff, %.2ff, %.1ff, %.1ff, "Bone L Forearm"\n};\n'%(
        int(shield['id']),int(shield['_position']),int(shield['_replacePos']),int(shield['buffId']),float(shield['Def']),float(shield['ShieldMeleeProtect']),float(shield['_durability']),float(shield['durDec']))
    header+='static const PeStarterSwordCapsuleSource pe_starter_sword_capsule_source = {\n    34154496u, 0, %d, %.9gf, %.9gf, {0.0f, 0.0f, %.9gf}\n};\n#endif\n'%(capsule['axis'],capsule['radius'],capsule['heigh'],capsule['offset']['z'])
    return {'pe_starter_sword_shield_source.h':header.encode(),
            'pe_starter_sword_shield_source.json':(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()}


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--database',type=Path,default=DEFAULT_DATABASE)
    p.add_argument('--output',type=Path,default=ROOT/'rebuild/generated');p.add_argument('--check',action='store_true')
    p.add_argument('--game',type=Path,help='Optional installed game directory for read-only modern binding checks')
    args=p.parse_args(argv)
    try:
        outputs=build_outputs(args.database)
        if args.game:
            manifest=json.loads(outputs['pe_starter_sword_shield_source.json'])
            verify_installed(args.game,manifest['prefabs'])
        # Validate all output bytes before any write; --check is always nonmutating.
        if args.check:
            for name,data in outputs.items():
                path=args.output/name
                if not path.is_file() or path.read_bytes()!=data:raise ValueError('Stale generated '+name)
        else:
            args.output.mkdir(parents=True,exist_ok=True)
            for name,data in outputs.items():(args.output/name).write_bytes(data)
        print('Starter sword/shield metadata '+('checked' if args.check else 'generated')+('; installed bindings verified' if args.game else ''));return 0
    except (OSError,ValueError,KeyError,IndexError,sqlite3.Error,ImportError) as error:
        print(error,file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())
