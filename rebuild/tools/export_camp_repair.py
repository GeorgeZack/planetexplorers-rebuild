#!/usr/bin/env python3
"""Qualify one archived camp repair machine and ordinary durability profiles.

Offline only. UnityPy reads serialized prefabs; no engine, scene or desktop is
started. This contract does not enable a service or change native save data.
"""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import sys

from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
from export_adventure_settlement import decode_entities, transform, yird_records
from export_player_collider_center import read_prefab, pointer, vector
from export_town_wallets import PINS as ROOT_ARCHIVE_PINS
from export_town_interaction import ANNOT_SAVE
from read_reference_adventure import DEFAULT_SAVE, Reader, records

STATIC_PREFAB = 'Assets/Resources/Prefab/Item/Scene/RepairMachine_Static.prefab'
FURNITURE_PREFAB = 'Assets/Resources/Prefab/Item/Furniture/Building/RepairMachine.prefab'
BUNDLE = Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers/AssetBundles/Item/RepairMachine_Static.unity3d'
BUNDLE_SHA = 'e267a1b55e0e174da80af090dbefc8a39c6451a8e705a5637e182085acc570ee'
DATABASE_SHA = 'c3d63ee865a7e10e309e11e9ee561c93feef39c379a584dcf68722a523cd4376'
SOURCE_PINS = {'Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRepairWnd.cs': '1f78d87aefc87d7dee8ee8ff57a73dca39244801011cc80586130a412ac227ca',
 'Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/Widget/SelectItem_N.cs': '92f7ceab4c6858cc4deac9e480935a1f0c75eb70f5e9e96820514b89c7dfccce',
 'Assets/Scripts/Items/ItemObj.cs': 'd7704733cafc8fdb2a8ae853e089459df6185fb1498e904f22ee3b3dbeb2e00a',
 'Assets/Scripts/Items/ItemData.cs': '66d6203e0c910c7dcd805318dbddfa4bdb205b6e1d189d28de865670a7a8b2ab',
 'Assets/Scripts/Items/OperatableItem/OperatableItemRepairMachine.cs': 'b3d3a9fe60452f8e30c9611798d70b1c3a1c8e1afc66c64bfe156ccc6f439c2f',
 'Assets/Scripts/Items/OperatableItem/OperatableItem.cs': '2b1c548c2fa22ef587543d622a5fb6240eadc54f13fc595b654f0c6ee832c111',
 'Assets/Scripts/MousePicker/MousePickable.cs': '2786964932ead1336f4523f368b8877d1acffaed723874d9f921507949001e5a',
 'Assets/Scripts/MousePicker/MousePickableChildCollider.cs': 'd9bc02b5f5069ed83e0f02987d9698d453556d983589587477fcafd1c0b62f0d',
 'Assets/Scripts/Global/UTimer.cs': '66b5a2ac795c1e06ba02bf992cedd7f5315cd1dd6aac944fc2b8be530bb9a843',
 'Assets/Scripts/WuYiqiu/Colony/CounterScript.cs': '81fe1cd6eb3e161c82210fb01e95a18a6be9e6e258fdc955be6dc2c32af3bac0',
 'Assets/Scripts/Colony/ColonyBase.cs': 'e9a426c030ff101a7ba3945c55c82c3fb977149bdb77a0f442aaeea42ff097a2',
 'Assets/PeLauncher/PeArchiveMgrs.cs': '2e4bc73aa27385aee9d6396ca331629986158a78796a9c0de0be3d0cb4d79c0f',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/CSDataMgr.cs': 'a37fec228fd851e8559a507ffc9b64dbc5bc9e2ae17f2b17f0a6eaa1686c69b7',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/CSDataInst.cs': 'feb67d8c979eb45609681af7624646fdacd17e91ea0ee14cc62a62df15814ff8',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/CSRecordData.cs': 'b667d66f05f08871e80f7c3c0f0af1600087fadcf81fce66216b3c0a2a7537aa',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/CSConst.cs': 'f295f9d081fbd0c2ae1838a8bbb1467497e2bba21693e4daf4e6d0f88da713d0',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/CSClodMgr.cs': 'df5e2b948744569c637263d96a01cf3b5a8e3eabb883790baeae42fd53bba523',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/CSInfo.cs': 'ab5f160f66ed29010ff650e6e38553631e4076a03eecb94f03deb263a9503872',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/CSMain.cs': '1bbb573b5f07b8d780ec3f40b9f5d483d220f08539eda67914604852f2aba284',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/CSNoMgCreator.cs': 'f20710444ed904f81c44e2c8d19c9f567cf20d69c3861946872f15f380ffd893',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/Entities/CSRepair.cs': '00e1611d26cac6ba8afcf713e2f3a8fa2a61320574b8f4e1b6844a2349da1f41',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/Entities/CSCommon.cs': 'cd7757f15d58520e350f02e73a3b95d0301fde4fb69e53b92818859de89793a6',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/EntityObjects/CSEntityObject.cs': 'be0666d75858e4713044d6ec7b3cee26b2b8041c2720a62b845cad3bee9db3b3',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/EntityObjects/CSRepairObject.cs': '8358d34dbba2c4d1495d02bc344f50f1707f2e71bcc66bc20a41bcbe0014e90e',
 'Assets/Resources/Prefab/Item/Scene/RepairMachine_Static.prefab': '268d4ddbefaa4d3b8cc846b5fdd97be378716c1070c6d2c1cdbae47091a8d237',
 'Assets/Resources/Prefab/Item/Furniture/Building/RepairMachine.prefab': 'ab560f1ebd2c018c1db2d3432ed0624d5c16bf22115b60c399cab50ff3dd98f2',
 'Assets/Scripts/Items/OperatableItem/OperatableItemRepairMachine.cs.meta': 'e0abd3d5c5563e576ddf37d521dfa8d28df0baeb029e7f3d97edc6ab3da4998c',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/EntityObjects/CSRepairObject.cs.meta': '8c10afc03096653168ea5482a56fae4d8af83d93d5d2cb325171032506378f6a',
 'Assets/Scripts/Items/ItemManager.cs': '39de5d06becf84fd99b5af56f6121dee0ee1642b1e4e358af2e4e629ad409ade',
 'Assets/Scripts/PETools/PEMath.cs': '3f1aad65bb9928affcc3ad60c3ad7f4518abe3cf7f56f41aab9d4119433cbf3a',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/Entities/CSElectric.cs': '7d927252bce2b6a8933ab08e0abed20af7839268a2d939a30a3228c31f3774db',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/Entities/CSWorkerMachine.cs': '229644918554519e3f7cb767d2d0fda4720a47901031c05ff8049197c8c7261d',
 'Assets/Scripts/WuYiqiu/Colony System/Scripts/Entities/CSEntity.cs': '0b7da4e19cc61cad16b88c1498af8fe664f8a64547dd9da95bf9383ddd912093'}
SUB_PINS = {
    'starting': ('149860b7eaab236bd016b6b7a37242cdc10055f8aad60e5da6f70acf956b65ae',
                 'dcc22436b7c0652efb6174f9b135e2455a9ad11fdf3e1b1be51b7564cc3bfcf6',
                 '9266defc4ef289aa0846241db6e04090981bf4cce71db976d9bde9b6e483b075'),
    'route': ('d6113bc10bb71e511670a16a04b9ca90f0354a8887977343897ecd6d0803dd9f',
              '21da9cf0582805359f7d2da5ce714608181b06c6c741ea85c4db173154ca1668',
              '9a67861d6199b10acb78bd113ffc0a15e47d06ae961971f66c02424b413f9854'),
}
VERSION = 16102100
MAX_MATERIALS = 8
REPAIR_GUID = '054102e63e89ead43bb95aa118079982'
OPERATE_GUID = 'f2b95665b47ad85499b1d2cb1e2fb568'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def checked_file(path, expected):
    if path.is_symlink() or not path.is_file() or digest(path.read_bytes()) != expected:
        raise ValueError('Camp repair source changed: '+str(path))


def validate_sources(source_root=ROOT):
    for name, expected in SOURCE_PINS.items():
        checked_file(source_root/name, expected)
    return [source_root/p for p in SOURCE_PINS]


def source_prefabs(source_root=ROOT):
    static, furniture = read_prefab(source_root/STATIC_PREFAB), read_prefab(source_root/FURNITURE_PREFAB)
    def component(objects, guid):
        rows = [(i,o['data']) for i,o in objects.items() if o['kind']=='MonoBehaviour' and o['script_guid']==guid]
        if len(rows)!=1 or rows[0][1]['m_Enabled']!=1:
            raise ValueError('Missing or disabled repair component')
        return rows[0]
    repair_id, repair = component(static, REPAIR_GUID)
    operate_id, operate = component(static, OPERATE_GUID)
    game_id = pointer(repair['m_GameObject'])
    if pointer(operate['m_GameObject'])!=game_id or repair['m_Type']!=5 or repair['m_ItemID']!=0 or repair['m_WorkTrans'] or repair['m_Power']!=0:
        raise ValueError('Camp machine component binding changed')
    game = static[game_id]['data']
    if game['m_Name']!='RepairMachine_Static' or not game['m_IsActive'] or game['m_Layer']!=13:
        raise ValueError('Camp machine root changed')
    attached = {pointer(v) for _,v in game['m_Component']}
    if not {repair_id,operate_id}<=attached:
        raise ValueError('Repair components detached from root')
    colliders = [(i,o['data']) for i,o in static.items() if o['kind']=='BoxCollider' and pointer(o['data']['m_GameObject'])==game_id]
    if len(colliders)!=1 or not colliders[0][1]['m_Enabled'] or colliders[0][1]['m_IsTrigger']:
        raise ValueError('Expected source static repair box collider')
    collider_id, collider = colliders[0]
    _, ordinary = component(furniture, REPAIR_GUID)
    if ordinary['m_ItemID']!=1130 or ordinary['m_Type']!=repair['m_Type']:
        raise ValueError('Ordinary repair prototype/type binding changed')
    return dict(game_object_path_id=game_id,repair_component_path_id=repair_id,operate_component_path_id=operate_id,
        scripts=dict(repair=REPAIR_GUID,operate=OPERATE_GUID),object_type=5,item_id_field=0,
        ordinary_repair_prototype=1130,work_transforms=0,power_field=0,layer=13,
        collider=dict(path_id=collider_id,kind='BoxCollider',center=vector(collider['m_Center']),size=vector(collider['m_Size']),trigger=False))


def installed_prefab(bundle=BUNDLE):
    import UnityPy
    checked_file(bundle,BUNDLE_SHA)
    env=UnityPy.load(str(bundle)); objects={o.path_id:(o.type.name,o.read_typetree()) for o in env.objects if o.type.name in ('MonoBehaviour','MonoScript','GameObject')}
    found={}
    for identity,(kind,data) in objects.items():
        if kind!='MonoBehaviour':continue
        script=objects[pointer(data['m_Script'])][1]['m_ClassName']
        if script in ('CSRepairObject','OperatableItemRepairMachine'):
            if script in found or data['m_Enabled']!=1:raise ValueError('Ambiguous installed repair component')
            found[script]=(identity,data)
    if set(found)!={'CSRepairObject','OperatableItemRepairMachine'}:raise ValueError('Installed machine missing repair behavior')
    repair_id,repair=found['CSRepairObject'];operate_id,operate=found['OperatableItemRepairMachine']
    root=pointer(repair['m_GameObject'])
    if root!=pointer(operate['m_GameObject']) or objects[root][1]['m_Name']!='RepairMachine_Static' or repair['m_Type']!=5 or repair['m_ItemID']!=0 or repair['m_WorkTrans'] or repair['m_Power']!=0:
        raise ValueError('Installed repair machine differs from qualified source')
    return dict(game_object_path_id=root,repair_component_path_id=repair_id,operate_component_path_id=operate_id,object_type=5,item_id_field=0,work_transforms=0)


def read_colony(data):
    """Bounded COMPLETE decoder for these two empty-job colony snapshots.

    Reject other schemas, personnel, farming and active jobs; this is evidence
    extraction, not a general importer or an alleged mid-repair observation.
    """
    if not isinstance(data,bytes) or not 0<len(data)<=4096:raise ValueError('Invalid colony record size')
    r=Reader(data);r.version(VERSION)
    if r.count(2)!=2:raise ValueError('Expected managed and nonmanaged creators')
    creators=[];seen=set()
    for _ in range(2):
        identity=r.integer();r.version(VERSION)
        if identity in seen:raise ValueError('Duplicate creator')
        seen.add(identity)
        siege=(r.integer(),*r.floats(2));kind=r.integer()
        if siege!=(0,0.,0.) or (identity,kind) not in ((0,0),(10000,1)):raise ValueError('Unsupported colony creator')
        objects=[];object_ids=set()
        for _ in range(r.count(2)):
            dtype=r.integer()
            if dtype not in (5,34):raise ValueError('Unsupported archived camp object')
            obj=dict(data_type=dtype,entity_id=r.integer(),item_id=r.integer(),alive=r.boolean(),name=r.string(),position=r.floats(3),durability=r.floats(1)[0],maintenance=r.floats(5),bounds=r.floats(6))
            if obj['entity_id'] in object_ids or r.count(0)!=0:raise ValueError('Invalid camp object ownership')
            object_ids.add(obj['entity_id'])
            if dtype==5:
                obj['job']=dict(item_instance_id=r.integer(),current_seconds=r.floats(1)[0],final_seconds=r.floats(1)[0])
                if obj['job']!=dict(item_instance_id=-1,current_seconds=0.,final_seconds=-1.):raise ValueError('Expected qualified empty repair job')
            elif r.count(0)!=0:raise ValueError('Unexpected solar item owner')
            objects.append(obj)
        for label in ('personnel','treatment','trade NPC','store'):
            if r.count(0)!=0:raise ValueError('Unsupported '+label)
        money=r.integer()
        if money!=5000:raise ValueError('Unexpected colony purse')
        creators.append(dict(id=identity,type=kind,siege=list(siege),objects=objects,colony_money=money))
    if seen!={0,10000}:raise ValueError('Missing camp creator')
    # Obsolete empty clod manager, then one empty managed clod owner0.
    if r.unpack('iiiiiiii')!=(15042418,0,0,VERSION,1,0,VERSION,0):raise ValueError('Unexpected archived clod owners')
    if r.integer()!=0:raise ValueError('Unexpected archived clod chunks')
    r.end()
    managed=next(c for c in creators if c['id']==0);camp=next(c for c in creators if c['id']==10000)
    if managed['objects'] or {(o['entity_id'],o['data_type']) for o in camp['objects']}!={(20006,5),(20007,34)}:raise ValueError('Unexpected camp machine identity')
    repair=next(o for o in camp['objects'] if o['entity_id']==20006)
    if repair['item_id']!=0 or repair['position']!=[-613.5,29.5,404.] or repair['durability']!=3500 or repair['maintenance']!=[0,-1,0,0,-1] or repair['bounds']!=[0]*6 or repair['name']!='Repair Machine':raise ValueError('Archived repair fields changed')
    return dict(version=VERSION,creators=creators,repair=repair)


def archive(directory,profile):
    before={p:digest(p.read_bytes()) for p in sorted(directory.glob('*.arc'))+sorted((directory/'MainAdventure').glob('*.arc'))}
    for name,sha in ROOT_ARCHIVE_PINS[profile].items():checked_file(directory/name,sha)
    for name,sha in zip(('index.arc','world.arc'),SUB_PINS[profile][:2]):checked_file(directory/'MainAdventure'/name,sha)
    root,root_files,guid=records(directory);sub,sub_files,sub_guid=yird_records(directory/'MainAdventure')
    if guid!=sub_guid:raise ValueError('Camp root/subworld GUID mismatch')
    colony=sub['ArchiveKeyCSDataMgr']
    if digest(colony)!=SUB_PINS[profile][2]:raise ValueError('Archived camp record changed')
    decoded=read_colony(colony);entities,_=decode_entities(root);entity=entities[20006]
    if (entity['prefab'],entity['prototype_type'],entity['prototype_id'])!=('EntityDoodad',5,145):raise ValueError('Repair doodad binding changed')
    pose=transform(entity)
    if pose['position']!=decoded['repair']['position']:raise ValueError('Doodad and repair job anchors differ')
    resource=Reader(entity['components']['BiologyViewCmpt']);path=resource.string();resource.end()
    if path!='Item/RepairMachine_Static.unity3d':raise ValueError('Archived repair model changed')
    if any(p not in before or digest(p.read_bytes())!=before[p] for p in root_files+sub_files):raise ValueError('Archive changed during read')
    return dict(profile=profile,guid=guid,entity_id=20006,doodad_prototype=145,pose=pose,resource=path,record_sha256=digest(colony),record_bytes=len(colony),**decoded),root_files+sub_files


def materials(text):
    if text in ('','0'):return []
    result=[];seen=set()
    for token in text.split(';'):
        parts=token.split(',')
        if len(parts)!=2:raise ValueError('Invalid repair material tuple')
        identity,count=map(int,parts)
        if identity<=0 or count<=0 or identity in seen or len(result)>=MAX_MATERIALS:raise ValueError('Invalid repair material values')
        seen.add(identity);result.append(dict(item_id=identity,count=count))
    return result


def catalog_profiles(database=DEFAULT_DATABASE,catalog_path=ROOT/'rebuild/generated/content_manifest.json'):
    checked_file(database,DATABASE_SHA)
    catalog=json.loads(catalog_path.read_text());selected=set(catalog['selected_item_ids'])
    runtime={r['id']:r for r in catalog['items']}
    if len(selected)!=len(catalog['selected_item_ids']) or set(runtime)!=selected:raise ValueError('Malformed runtime catalog selection')
    with closing(connect_readonly(database)) as db:
        items,_=numeric_rows(db,'PrototypeItem','id');classes,_=numeric_rows(db,'item_class','id',allow_duplicates=True)
        rows=[];qualified=[];excluded=[]
        for identity in sorted(selected):
            row=dict(items[identity]);component=classes.get(int(row['_function']));names=str(component['classname']).split(',') if component else []
            if 'Repair' not in names:continue
            parts=materials(row['_repairMax']);maximum=int(row['_durability'])
            if maximum!=runtime[identity]['durability_max']:raise ValueError('Runtime/source durability mismatch')
            reason=None
            if 'Durability' not in names or 'LifeLimit' in names:reason='unmodeled LifeLimit'
            elif any('Creation' in name for name in names):reason='creation requires separately qualified classification'
            elif not parts:reason='source camp UI requires a nonempty repair material list'
            elif any(p['item_id'] not in selected for p in parts):reason='missing runtime material'
            elif maximum<=0 or runtime[identity]['stack_limit']!=1:reason='unsupported durable instance profile'
            record=dict(item_id=identity,name=runtime[identity]['name'],durability_max=maximum,component_class=int(row['_function']),components=names,materials=parts,missing_materials=[p['item_id'] for p in parts if p['item_id'] not in selected],eligible=reason is None,exclusion=reason,source_row_sha256=digest(json.dumps(row,sort_keys=True).encode()))
            rows.append(record)
            (excluded if reason else qualified).append(record)
        colony=[dict(r) for r in db.execute('SELECT * FROM colonyinfo WHERE CAST(id AS INTEGER)=1130')]
        if len(colony)!=1 or colony[0]['_property']!='60' or colony[0]['workerscnt']!='0' or colony[0]['durability']!='3500':raise ValueError('Repair colony info changed')
        return dict(catalog_item_count=len(selected),repair_component_count=len(rows),qualified=qualified,excluded=excluded,colony_info=colony[0],repair_prototype=dict(items[1130]))


def build_outputs(source_root=ROOT,database=DEFAULT_DATABASE,catalog_path=ROOT/'rebuild/generated/content_manifest.json',save=DEFAULT_SAVE,route_save=ANNOT_SAVE,bundle=BUNDLE):
    sources=validate_sources(source_root);prefab=source_prefabs(source_root);installed=installed_prefab(bundle)
    starting,first_files=archive(save,'starting');route,route_files=archive(route_save,'route')
    profiles=catalog_profiles(database,catalog_path)
    if starting['repair']['alive'] or not route['repair']['alive']:raise ValueError('Qualified saved camp lifecycle flags changed')
    qualified=profiles['qualified']
    header=['/* Generated by export_camp_repair.py; do not edit. */','#ifndef PE_CAMP_REPAIR_SOURCE_H','#define PE_CAMP_REPAIR_SOURCE_H',
        '#define PE_CAMP_REPAIR_ENTITY_ID 20006','#define PE_CAMP_REPAIR_CREATOR_ID 10000','#define PE_CAMP_REPAIR_DATA_TYPE 5',
        '#define PE_CAMP_REPAIR_BASE_SECONDS 60.0f','#define PE_CAMP_REPAIR_TICKS_PER_SECOND 100000',
        '#define PE_CAMP_REPAIR_MATERIAL_CAPACITY 8',f'#define PE_CAMP_REPAIR_SOURCE_ITEM_COUNT {len(qualified)}',
        'typedef struct { int item_id,count; } PeCampRepairMaterial;',
        'typedef struct { int item_id; float durability_max; unsigned material_count; PeCampRepairMaterial materials[PE_CAMP_REPAIR_MATERIAL_CAPACITY]; } PeCampRepairItemSource;',
        'static const PeCampRepairItemSource pe_camp_repair_items[PE_CAMP_REPAIR_SOURCE_ITEM_COUNT] = {']
    for item in qualified:
        pairs=','.join('{'+str(p['item_id'])+','+str(p['count'])+'}' for p in item['materials'])
        header.append('    {'+f"{item['item_id']},{item['durability_max']}.0f,{len(item['materials'])},"+'{'+pairs+'}},')
    header+=['};','#endif','']
    all_sources=list(dict.fromkeys(sources+first_files+route_files+[database,catalog_path,bundle]))
    manifest=dict(schema=1,scope='One archived camp repair machine; source-qualified metadata, no gameplay service or PECS format change.',
        sources=[describe_source(p) for p in all_sources],source_file_guards=SOURCE_PINS,
        source_prefab=prefab,installed_prefab=installed,archives=[starting,route],catalog=profiles,
        semantics=dict(creator_id=10000,data_type=5,ordinary_repair_prototype=1130,base_seconds=60,qualified_workers=0,
            cost='ceil(base material count * 1 * (1 - durability/max)); active UI has no epsilon early return',
            start_gate='Repair exists and !PeFloatRangeNum.IsCurrentMax(); absolute deficit < PEMath.Epsilon counts as full',
            time='base * pow(.82,worker_count) * product(1-worker_skill); qualified camp has zero assigned workers',
            ticks_per_second=100000,timer='Per enabled CounterScript display Update: remaining ticks += trunc(float(-1 * dt * 100000)); completion at <=0; independent of accelerated world clock',
            insert='Transfer the same instance out of its former owner; busy machine rejects insertion/extraction; source equipment origin can unequip',
            replace='Same category exchanges instances; otherwise return old only after CanAdd succeeds, then insert new',
            cancel='StopCounter sets current0/final-1, destroys counter, retains item and does not refund consumed materials',
            close='OnClose clears UI binding only; item and job remain owned by machine; distance>8 closes UI',
            complete='Optional UI callback then Repairing->Repair.Do; same instance set to maximum, remains in machine; repeated Do is idempotent',
            saved='CSDataMgr MainAdventure creator10000 -> CSRepairData entity20006: independent item instance ID/current/final; ItemMgr separately saves actual item',
            restore='CSRepair.CreateData resolves stored instance via ItemMgr and restarts nonnegative final counter; empty -1 final does not start',
            limitation='Both archived jobs are empty. Source establishes restart path; no observed nonempty/partial job save or exact cross-MonoBehaviour update order is claimed.'),
        exclusions=['No LifeLimit, creation, worker colony, arbitrary repair machine placement, service UI, inventory transfer or save integration is enabled.',
                    'Knife33 and SafetyAxe1527 have no source repair materials and must not receive free repairs.',
                    'Native atomic preflight/commit, bag-only admission and pause/clock scheduling require explicit implementation policies.',
                    'Source and installed static machine components agree; rendered active machine animation remains outside this metadata extraction.'])
    # Validate again after decoding so concurrent edits cannot produce mixed source evidence.
    validate_sources(source_root);checked_file(database,DATABASE_SHA);checked_file(bundle,BUNDLE_SHA)
    return {'pe_camp_repair_source.h':'\n'.join(header).encode(),'pe_camp_repair_source.json':(json.dumps(manifest,indent=2)+'\n').encode()}


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');p.add_argument('--output',type=Path,default=ROOT/'rebuild/generated');a=p.parse_args(argv)
    try:
        for name,data in build_outputs().items():
            path=a.output/name
            if a.check:
                if not path.is_file() or path.read_bytes()!=data:raise ValueError('Stale repair output '+name)
            else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    except (OSError,ValueError,KeyError,ImportError) as error:
        print(error,file=sys.stderr);return 1
    print('Camp repair metadata '+('checked' if a.check else 'generated'));return 0

if __name__=='__main__':raise SystemExit(main())
