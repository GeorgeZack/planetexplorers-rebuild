#!/usr/bin/env python3
"""Recover eight pinned Adventure vendor wallets; read-only original inputs."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys

from export_content import ROOT, describe_source
from export_starter_outfit import body, uncomment
from read_reference_adventure import DEFAULT_SAVE, Reader, records
from export_adventure_settlement import decode_entities
from export_town_interaction import ANNOT_SAVE

# Persistence order is entity order, independent of the existing stock order.
VENDORS = ((20002, 141), (20011, 151), (20014, 146), (20028, 153),
           (20031, 158), (20032, 163), (20033, 143), (20037, 148))
PINS = {
    'starting': {'index.arc': '939a80153919330eb873d555f944e70194c864ca7b7dcb7a0642efbadfe72cca',
                 'world.arc': '74e301a0744ee2e19cf93f2366b046a01f1413fd4be94664573d51d6765d9220'},
    'route': {'index.arc': '5c69435879e0c39953a1d95fcab30026195145b28e42c9a4e21853fd73bbd440',
              'world.arc': '0c015f99944af29f70aa665ef9004a36a53a9d220bc59fc16aacde166920309a'},
}
NPC = 'Assets/Scripts/PeEntity/Cmpt/NpcPackageCmpt.cs'
TIMER = 'Assets/Scripts/Global/UTimer.cs'
SLOTS = 'Assets/Scripts/Items/ItemPak/SlotList.cs'
# Whitespace/comment-independent complete region guards, not token-presence
# checks. Hashes bind refill order, serialization, no clamp and purchase credit.
GUARDS = (
    (NPC, r'public\s+class\s+AutoIncreaseMoney', 'a0006c650d2c884a22cacc578ea2fee5ead551136c15379d20e8ffc3f25502f0'),
    (NPC, r'public\s+override\s+void\s+Serialize\s*\(', 'd9b748f5f5042eff1689ef31d26045a2bf5b0db95f7fa0874f8f39a8d895faf8'),
    (NPC, r'public\s+override\s+void\s+Deserialize\s*\(', 'abf826a6088c9453bc1b8e083fa643b5719f6ccb13231b861a20bd91e9b90d5b'),
    (NPC, r'public\s+void\s+InitAutoIncreaseMoney\s*\(', 'ac7207d792c90a772871727a78f8d878d0c8b7fac541049cd1efb05d104d6f78'),
    (NPC, r'public\s+override\s+void\s+OnUpdate\s*\(', 'b53ed150d8618785db3ae2ed50462ec6f9013febcdc9e3617e9cd77e54cba418'),
    ('Assets/Scripts/PeEntity/Cmpt/PackageCmpt.cs', r'public\s+abstract\s+class\s+PackageCmpt', '9330e1ba270ff6fb2a4937a5847d002b5e8546e6d9885769f063e2f4e7d1e50e'),
    ('Assets/PeWorld/PeMoney.cs', r'public\s+class\s+Money', '6b6269eebaac9649372aa9575abc9b90029f0487d213a49d70a3c2f9ac814c55'),
    ('Assets/Scripts/Global/PETimer.cs', r'public\s+PETimer\s*\(', '5db685407051f625942a6518c4feba1ae3d2050c709e52c685ec8f0f52dd47ab'),
    (TIMER, r'public\s+UTimer\s*\(', 'daecba5bdf9442ae07f69cd3d46ade05bda430ad4c84c28e6c4e62bda86a3f27'),
    ('Assets/Scripts/Global/GameTime.cs', r'void\s+Start\s*\(', 'f0a9eceaad56f660afa3de3d2daf8a38764c4a8b37dc2e8833b8fa15eb5202e8'),
    ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs', r'void\s+Buy\s*\(', '31fa78e4fbbefc5cc03616ae420efe9142b625cf0aa4e07dd4dcc9cce71fab98'),
    (SLOTS, r'public\s+byte\[\]\s+Export\s*\(', '0603ff9e5af6ac88abf9424f62ca98bc0897906da920c9794b00edfce8766490'),
    ('Assets/PeLauncher/PlayerTypeLoader.cs', r'public\s+class\s+SinglePlayerTypeLoader', 'c316230bac314685515a602ab5ad0809216680a21dedaa281ba4de59614a5247'),
    ('Assets/PeLauncher/PeGameMgr.cs', r'public\s+enum\s+ESceneMode', '2167bffb57c7757abb7803fa2db9e1653d778f183a872d54bdd6bf7d114530d6'),
)
SOURCE_FILES = tuple(dict.fromkeys(p for p, _, _ in GUARDS))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def validate_sources(root=ROOT):
    paths = [root/p for p in SOURCE_FILES]
    before = {p: digest(p.read_bytes()) for p in paths}
    texts = {p: uncomment((root/p).read_text(encoding='utf-8-sig', errors='replace')) for p in SOURCE_FILES}
    for path, pattern, expected in GUARDS:
        if digest(body(texts[path], pattern).encode()) != expected:
            raise ValueError('Wallet source contract changed: '+path+' '+pattern)
    timer = re.sub(r'\s+', '', texts[TIMER])
    for fragment in ('publicUTimer(longday2Hour=24,longhour2Min=60,longmin2Sec=60)',
                     'publicdoubleSecondInDay{get{returnTimeInDay*Day2Sec;}}',
                     'publiclongHour2Sec{get{returnc_Hour2Min*c_Min2Sec;}}',
                     'publiclongDay2Sec{get{returnc_Day2Hour*Hour2Sec;}}'):
        if fragment not in timer:
            raise ValueError('Wallet calendar binding changed')
    if body(texts[TIMER], r'public\s+double\s+TimeInDay') != 'get{doubleday=Day;returnday-System.Math.Floor(day);}':
        raise ValueError('Wallet day phase changed')
    if body(texts[TIMER], r'public\s+double\s+Day') != 'get{return(double)(m_Tick*c_Tick2Sec)/Day2Sec;}set{m_Tick=(long)(value*Day2Sec*c_Sec2Tick);}':
        raise ValueError('Wallet day conversion changed')
    slots = re.sub(r'\s+', '', texts[SLOTS])
    if 'constintVERSION_0000=0;' not in slots or 'constintCURRENT_VERSION=VERSION_0000;' not in slots:
        raise ValueError('Wallet package slot version changed')
    if any(digest(p.read_bytes()) != value for p, value in before.items()):
        raise ValueError('Wallet source changed during inspection')
    return paths


def decode_package(data):
    """Parse the entire selected NpcPackageCmpt, including skipped item owners."""
    if not isinstance(data, bytes) or not 0 < len(data) <= 4096:
        raise ValueError('Invalid wallet component size')
    r = Reader(data)
    money = r.blob()
    balance = money.integer()
    money.end()
    seen = set()

    def slots(expected):
        s = r.blob()
        s.version(0)
        if s.count(64) != expected:
            raise ValueError('Unexpected wallet package capacity')
        for _ in range(expected):
            identity = s.integer()
            if identity != -1:
                if identity <= 0 or identity in seen:
                    raise ValueError('Invalid or aliased wallet package item')
                seen.add(identity)
        s.end()

    slots(15)
    slots(30)
    auto = r.blob()
    maximum, increase, added = auto.integer(), auto.integer(), auto.boolean()
    auto.end()
    slots(10)
    r.end()
    # Original Money uses signed32; these fixed profiles deliberately require
    # nonnegative values. Runtime overflow policy is a separate native bound.
    if not all(0 <= v <= 0x7fffffff for v in (balance, maximum, increase)):
        raise ValueError('Unsupported negative wallet profile')
    return dict(initial_balance=balance, serialized_max=maximum,
                daily_increase=increase, initial_added=added)


def read_archive(directory, profile):
    paths = sorted(directory.glob('*.arc'))
    if not 1 <= len(paths) <= 16:
        raise ValueError('Unexpected archive file count')
    if any(p.is_symlink() or not p.is_file() or not 28 <= p.stat().st_size <= 64*1024*1024 for p in paths):
        raise ValueError('Invalid wallet archive input')
    before = {p: digest(p.read_bytes()) for p in paths}
    for name, expected in PINS[profile].items():
        if before.get(directory/name) != expected:
            raise ValueError('Pinned wallet archive changed: '+profile+'/'+name)
    rows, inputs, guid = records(directory)
    if any(p not in before for p in inputs):
        raise ValueError('Wallet archive escaped inspected files')
    entities, _ = decode_entities(rows)
    session = decode_session(rows['Pathea.SinglePlayerTypeArchiveMgr'])
    if sorted(directory.glob('*.arc')) != paths or any(digest(p.read_bytes()) != value for p, value in before.items()):
        raise ValueError('Wallet archive changed during read')
    return entities, paths, guid, before, session


def decode_session(data):
    r = Reader(data)
    r.version(2)
    seconds, speed, play_seconds, mode = r.unpack('dfii')
    name, yird, uid = r.nullable(), r.nullable(), r.nullable()
    digital = r.boolean()
    r.end()
    if not math.isfinite(seconds) or seconds < 0 or not math.isfinite(speed) or speed < 0 or play_seconds < 0:
        raise ValueError('Invalid archived wallet clock')
    if mode != 1 or not digital or yird != 'MainAdventure':
        raise ValueError('Wallet requires archived single-player digital Adventure')
    return dict(seconds=seconds, speed=speed, play_seconds=play_seconds,
                scene_mode=mode, digital=digital, yird=yird, game_name=name, uid=uid)


def build_outputs(save=DEFAULT_SAVE, annot_save=ANNOT_SAVE, source_root=ROOT):
    sources = validate_sources(source_root)
    starting, first_files, first_guid, first_hashes, first_session = read_archive(save, 'starting')
    route, route_files, route_guid, route_hashes, route_session = read_archive(annot_save, 'route')
    profiles = []
    for index, (identity, prototype) in enumerate(VENDORS):
        selected = starting if index < 3 else route
        e = selected.get(identity)
        if not e or (e['prefab'], e['prototype_type'], e['prototype_id']) != ('EntityNpc', 1, prototype):
            raise ValueError('Wallet NPC binding changed')
        component = e['components']['NpcPackageCmpt']
        values = decode_package(component)
        if index < 3:
            other = route.get(identity)
            if not other or (other['prefab'], other['prototype_type'], other['prototype_id']) != ('EntityNpc', 1, prototype):
                raise ValueError('Route wallet NPC binding changed')
            if decode_package(other['components']['NpcPackageCmpt']) != values:
                raise ValueError('Starting wallet differs across pinned checkpoints')
        profiles.append(dict(entity_id=identity, prototype_id=prototype, **values,
                             archive_profile='starting' if index < 3 else 'route',
                             component_sha256=digest(component)))
    # Verify the runtime vendor set independently against the existing catalog.
    offers_path = ROOT/'rebuild/generated/pe_town_offers.json'
    offers = json.loads(offers_path.read_text())['offers']
    if {(r['entity_id'], r['prototype_id']) for r in offers} != set(VENDORS):
        raise ValueError('Wallet and shop entity bindings differ')
    for path, expected in {**first_hashes, **route_hashes}.items():
        if digest(path.read_bytes()) != expected:
            raise ValueError('Wallet archive changed before output')
    lines = ['/* Generated by tools/export_town_wallets.py; do not edit. */',
             '#ifndef PE_TOWN_WALLETS_H', '#define PE_TOWN_WALLETS_H', '#include "pe_town_wallet.h"', '#include "pe_game.h"',
             '#define PE_TOWN_WALLET_SOURCE_DAY_SECONDS 93600.0',
             '#define PE_TOWN_WALLET_SOURCE_HALF_DAY_SECONDS 46800.0',
             'static const PeTownWalletSource pe_town_wallet_sources[]={']
    for row in profiles:
        values = [str(row[k])+'u' for k in ('entity_id', 'initial_balance', 'serialized_max', 'daily_increase')]
        values.append('true' if row['initial_added'] else 'false')
        lines.append('    {'+','.join(values)+'},')
    lines += ['};', '_Static_assert(sizeof pe_town_wallet_sources/sizeof pe_town_wallet_sources[0]==PE_TOWN_WALLET_CAPACITY,"Town wallet persistence count");', '#endif', '']
    manifest = dict(schema=1, scope='Eight pinned Adventure vendor wallets; no sell/buyback or historical transaction reconstruction.',
        sources=[describe_source(p) for p in sources+first_files+route_files+[offers_path]],
        source_region_guards=[dict(path=p, pattern=pattern, canonical_body_sha256=value) for p, pattern, value in GUARDS],
        archives=dict(starting=dict(directory=str(save.resolve()), guid=first_guid, session=first_session), route=dict(directory=str(annot_save.resolve()), guid=route_guid, session=route_session)),
        profiles=profiles, clock=dict(day_seconds=93600, half_day_seconds=46800, unit='GameTime.Timer seconds, not realtime/scaled action clock',
            refill='If second-in-day < half-day, clear added; else add daily increase if not added, then set added. No elapsed-day catch-up.',
            new_entity_initial_added=True, archived_latch_preserved=True),
        semantics=dict(original_balance_type='signed int32; exported profiles nonnegative', serialized_max_applied=False,
            ordinary_purchase_credits_vendor=True, source_credit_clamped=False,
            source_wallet_and_refill_serialized=True, buyback_persistence_qualified=False),
        limitations=['This is source/archive extraction, not a new original or native gameplay observation.',
                     'Starting values are saved snapshots, not universal RNG results or current balances after later purchases.',
                     'Native overflow/old-save migration policies require explicit runtime adapters; old stock cannot reconstruct purchase history.'])
    return {'pe_town_wallets.h': '\n'.join(lines).encode(),
            'pe_town_wallets.json': (json.dumps(manifest, indent=2)+'\n').encode()}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT/'rebuild/generated')
    args = parser.parse_args(argv)
    try:
        for name, value in build_outputs().items():
            path = args.output/name
            if args.check:
                if not path.is_file() or path.read_bytes() != value:
                    raise ValueError('Stale wallet output '+name)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(value)
    except (OSError, ValueError, KeyError) as error:
        print(error, file=sys.stderr)
        return 1
    print('Town wallets checked' if args.check else 'Town wallets generated')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
