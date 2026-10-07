#!/usr/bin/env python3
"""Export source-qualified hare13 ground loot; no Unity runtime or save writes."""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import sqlite3
import sys
from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, numeric_rows, c_float

# Conservative whole-input guards preserve the audited active call chain,
# independent per-entry random draws, strict pickup/lifetime comparisons and
# complete-stack package transfer. A source change requires explicit re-audit.
SOURCE_SHA256 = {
    'Assets/PeWorld/PeEntityCreator.cs':
        '7808c7c9b0c71fdce5a83217e3d34a7e93c9cca8fd46085ad574b2687370a71b',
    'Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs':
        '33892866d1fcb4947e97cdbe844b5946600409a77001cf09bb94ccb1159f4975',
    'Assets/Scripts/LootItem/LootItemMgr.cs':
        'c05a0cd6a6e170bb4d1d3effe220f9923e6d48cb76c6eebff34b6e0fe331521c',
    'Assets/Scripts/Items/ItemDrop/ItemDropData.cs':
        'd8dc2478e40781c6e360e571bfadc47540da0ffe0a3003a540624428aad4e19e',
    'Assets/Scripts/MousePicker/MousePickableLootItem.cs':
        '4e6cf758f339424b73f04592f483f5e6246ffe76915c9c7370e3d40c0eb599c9',
    'Assets/Scripts/ZhangShunBo/GetSpecialItem.cs':
        'cd224d5dd22a788d57cc325967e42f80e9abffc21677d31049387b76af288727',
    'Assets/Scripts/Mission/PlayerMission.cs':
        '11ba6d3f1e1c7627a0d114febd2256d4482dc498010157cfd40437d957397c35',
    'Assets/Scripts/Items/ItemObj.cs':
        'd7704733cafc8fdb2a8ae853e089459df6185fb1498e904f22ee3b3dbeb2e00a',
    'Assets/Scripts/Items/ItemData.cs':
        '66d6203e0c910c7dcd805318dbddfa4bdb205b6e1d189d28de865670a7a8b2ab',
    'Assets/Scripts/PeEntity/Cmpt/PlayerPackageCmpt.cs':
        'c3668318f7d210ec0e3d7a435fa98a2f3382bffea3c4761883f115d49ed1a165',
    'Assets/Resources/Prefab/LootItem/LootItem.prefab':
        'eef0edbdbd82b7d2bc3cbcbaed645f3eef5b04dfc1d2b3cdb1b19962d7ec671b',
}
PREFAB = "Assets/Resources/Prefab/LootItem/LootItem.prefab"
ROLL = [(1011,.1),(1008,.2),(236,.4),(1026,.44),(1228,.45),(1742,.55),(0,1.)]
ITEMS = [(229,9999,"Synthetics"),(1011,9999,"Synthetics"),
         (1008,9999,"Synthetics"),(236,999,"Herb"),(1026,100,"Synthetics"),
         (1228,100,"Valuables"),(1742,9999,"Synthetics")]
PRESENTATION = dict(stayMinTime=1.,fadeSpeed=1.,effectID=223,lootSoundID=950,
                    getItemSoundID=949,m_MaxSpeed=15.,m_Acceleration=5.,
                    m_RotateSpeed=10.,selfRotateSpeed=180.)
QUEST_TARGETS = {9032:"4503",9072:"2531",9074:"0",9075:"4524",9076:"0",9126:"1518",9127:"4525"}


def validate_sources(root):
    for name,expected in SOURCE_SHA256.items():
        if hashlib.sha256((root/name).read_bytes()).hexdigest() != expected:
            raise ValueError("Hare loot source contract changed: "+name)


def expect(row, fields, label):
    for key,value in fields.items():
        if row[key] != str(value):
            raise ValueError(label+" source contract changed: "+key)


def qualify_tables(db):
    monsters,_=numeric_rows(db,"PrototypeMonster","id")
    drops,_=numeric_rows(db,"loot","id")
    items,_=numeric_rows(db,"PrototypeItem","id")
    classes,_=numeric_rows(db,"item_class","id",allow_duplicates=True)
    quests,_=numeric_rows(db,"AdRandomQuest_List","ID")
    collect,_=numeric_rows(db,"AdQuest_LootItem","TargetID")
    targets,_=numeric_rows(db,"AdQuest_KillMonster","TargetID")
    expect(monsters[13],dict(Loot=11,DeleteTime=30),"Hare13")
    expect(drops[11],dict(meat="22;35",loot="5;1011_0.1,1008_0.2,236_0.4,1026_0.44,1228_0.45,1742_0.55,0_1"),"Hare loot11")
    # Class0 is intentionally absent. Default prototype _durability=100 is
    # not a component: ItemObject.Create returns early when class is absent.
    if 0 in classes:
        raise ValueError("Hare loot item component class0 is no longer absent")
    qualified=[]
    for identity,stack,category in ITEMS:
        row=items[identity]
        expect(row,dict(_function=0,stacking_num=stack,category=category,tab=2,
                        is_formula=0,learn=0,bundle=0,skillId=0),"Loot item"+str(identity))
        qualified.append(dict(item_id=identity,max_stack=stack,category=category,
                              inventory_tab=2,component_class=0,components=[],
                              has_durability=False,icon=str(row["_iconId"]).split(",")[0]))
    for identity,expected in QUEST_TARGETS.items():
        expect(quests[identity],dict(TargetIDList=expected),"Supported quest"+str(identity))
        for target in (int(s) for s in expected.split(",") if s != "0"):
            if target in collect:
                # 2531 medicine is plain possession, not monster special loot.
                expect(collect[target],dict(Type=0,TargetItemID=0,MaxNum=0,Chance=0),"Supported collect"+str(target))
    expect(targets[1518],dict(MonsterID="13_4",PathIDtemp="0_13_6"),"Hunt target1518")
    return qualified


def qualify_prefab(fields):
    if fields.get("m_Enabled") != 1 or any(fields.get(k) != v for k,v in PRESENTATION.items()):
        raise ValueError("Hare loot presentation prefab changed")


def validate_prefab(root):
    import UnityPy
    matches=[]
    for obj in UnityPy.load(str(root/PREFAB)).objects:
        if obj.type.name == "MonoBehaviour":
            fields=obj.read_typetree()
            if "m_MaxSpeed" in fields and "stayMinTime" in fields:
                matches.append(fields)
    if len(matches) != 1:
        raise ValueError("Expected one loot presentation component")
    qualify_prefab(matches[0])


def recover(root=ROOT,database=None,verify_prefab=False):
    validate_sources(root)
    database=database or root/"Assets/Resources/DataBase/localData.bytes"
    with closing(connect_readonly(database)) as db:
        items=qualify_tables(db)
    if verify_prefab: validate_prefab(root)
    return dict(schema="pe-hunt-loot-1",prototype_id=13,drop_id=11,
        meat=dict(item_id=229,min_count=22,max_count_exclusive=35),trials=5,
        entries=[dict(item_id=i,probability=p) for i,p in ROLL],items=items,
        pickup_distance=4.,generate_distance=48.,icon_distance=10.,lifetime_game_seconds=93600.,presentation=PRESENTATION,
        source_database_sha256=hashlib.sha256(database.read_bytes()).hexdigest(),
        sources=[dict(path=p,sha256=h) for p,h in SOURCE_SHA256.items()],
        contracts=dict(death="PESkEntity.DispatchDeathEvent -> LootItemMgr.RequestCreateLootItem; old ItemDropPeEntity attachment is commented out.",
          roll="System.Random.Next(22,35); five trials; fresh float-cast NextDouble per ordered entry; strict less-than; first success ends trial; ID0 gives nothing; merge duplicate IDs.",
          pickup="LootItemMgr.Update strict squared root distance <16 and living player; AddItemToPlayer transfers a complete stack or retains it; successful stackable transfer destroys the world instance.",
          lifetime="CheckItemRemove: non-Quest Item category expires only when GameTime.Timer.Second-dropTime >93600.",
          persistence="LootItemData archives item instance identity, position and double dropTime independently of the originating monster; ItemMgr owns prototype/count.",
          components="All seven _function0; class0 absent; ItemObject.Create returns with no components. Prototype _durability defaults do not establish Durability.",
          special_items="MonsterItemAdd consults active special-collection registrations. Supported quest targets are guarded; only medicine2531 is collect and its Type0/zero target/chance is not monster loot. Arbitrary side missions remain excluded.",
          native_stackables="Existing C inventory requires instance_id=0 and durability=0 for stackables; separately owned world-drop identity must not be copied into bag cells."),
        limitations=["No observed original random seed; native RNG is an explicit adapter preserving ordered decisions and saved outcomes.",
          "No corpse window, corpse pose, mission-special drop activation or new enemy populations are authorized.",
          "Prefab fields are source-derived; full glow, ballistic flight, particles and audio presentation are not established by this metadata.",
          "Native pinned cohort/reload and bounded storage remain separate implementation policies; generate only from actual positive-to-zero deaths, never disappearance or saved quest credit."])


def header(r):
    lines=["/* Generated by export_hunt_loot.py; do not edit. */","#ifndef PE_HUNT_LOOT_SOURCE_H","#define PE_HUNT_LOOT_SOURCE_H",
      "#define PE_HUNT_LOOT_PROTOTYPE 13","#define PE_HUNT_LOOT_DROP_ID 11",
      "#define PE_HUNT_LOOT_MEAT_ITEM 229","#define PE_HUNT_LOOT_MEAT_MIN 22u",
      "#define PE_HUNT_LOOT_MEAT_MAX_EXCLUSIVE 35u","#define PE_HUNT_LOOT_TRIALS 5u",
      "#define PE_HUNT_LOOT_ENTRY_COUNT 7u","#define PE_HUNT_LOOT_ITEM_COUNT 7u",
      "#define PE_HUNT_LOOT_PICKUP_DISTANCE 4.0f","#define PE_HUNT_LOOT_GENERATE_DISTANCE 48.0f",
      "#define PE_HUNT_LOOT_ICON_DISTANCE 10.0f","#define PE_HUNT_LOOT_LIFETIME_SECONDS 93600.0",
      "#define PE_HUNT_LOOT_STAY_MIN_SECONDS 1.0f","#define PE_HUNT_LOOT_FADE_SPEED 1.0f",
      "#define PE_HUNT_LOOT_MAX_SPEED 15.0f","#define PE_HUNT_LOOT_ACCELERATION 5.0f",
      "#define PE_HUNT_LOOT_ROTATE_SPEED 10.0f","#define PE_HUNT_LOOT_SELF_ROTATE_SPEED 180.0f",
      "#define PE_HUNT_LOOT_EFFECT_ID 223","#define PE_HUNT_LOOT_FLIGHT_SOUND_ID 950",
      "#define PE_HUNT_LOOT_RECEIVED_SOUND_ID 949",
      "typedef struct {int item_id; float probability;} PeHuntLootRollEntry;",
      "static const PeHuntLootRollEntry pe_hunt_loot_roll_entries[PE_HUNT_LOOT_ENTRY_COUNT]={"]
    lines.extend("{%d,%s},"%(e["item_id"],c_float(e["probability"])) for e in r["entries"])
    lines += ["};","typedef struct {int item_id; unsigned max_stack;} PeHuntLootItemDef;",
              "static const PeHuntLootItemDef pe_hunt_loot_items[PE_HUNT_LOOT_ITEM_COUNT]={"]
    lines.extend("{%d,%du},"%(e["item_id"],e["max_stack"]) for e in r["items"])
    return "\n".join(lines+["};","#endif",""])


def build_outputs(root=ROOT,database=None,verify_prefab=False):
    result=recover(root,database,verify_prefab)
    return {"pe_hunt_loot_source.h":header(result),
            "pe_hunt_loot_source.json":json.dumps(result,indent=2,sort_keys=True)+"\n"}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--database",type=Path,default=DEFAULT_DATABASE)
    p.add_argument("--output",type=Path,default=ROOT/"rebuild/generated")
    p.add_argument("--check",action="store_true")
    p.add_argument("--verify-prefab",action="store_true",help="Additional offline UnityPy serialized-field qualification")
    args=p.parse_args()
    try:
        if args.output.resolve()==(ROOT/"Assets").resolve() or (ROOT/"Assets").resolve() in args.output.resolve().parents:
            raise ValueError("Output must not modify reference Assets")
        for name,text in build_outputs(database=args.database,verify_prefab=args.verify_prefab).items():
            destination=args.output/name
            if args.check:
                if not destination.is_file() or destination.read_text()!=text:
                    raise ValueError("Stale generated "+name)
            else:
                args.output.mkdir(parents=True,exist_ok=True)
                destination.write_text(text)
        print("Hare loot source "+("checked" if args.check else "exported"))
        return 0
    except (OSError,ValueError,KeyError,sqlite3.Error,ImportError) as error:
        print(error,file=sys.stderr);return 1

if __name__=="__main__":raise SystemExit(main())
