#!/usr/bin/env python3
"""Recover Adventure sell/buyback prices and component-selected condition factors."""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import sys

from export_content import (ROOT, DEFAULT_DATABASE, DEFAULT_LOCALIZATION, describe_source,
                            integer, numeric_rows, build_outputs as content_outputs)
from export_starter_outfit import body, uncomment

ITEM = 'Assets/Scripts/Items/ItemObj.cs'
SHOP = 'Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs'
STORY = 'Assets/Scripts/Mission/StroyManager.cs'
DATA = 'Assets/Scripts/Items/ItemData.cs'
RANGE = 'Assets/Scripts/Items/ItemManager.cs'
GUARDS = (
    ('Assets/Scripts/Items/ItemObj.cs', 'static\\s+int\\s+GetPrice\\s*\\(', 'd5bb11a27985a17152863607d6f588a6c770e19e142ce235dd18390f427364ae'),
    ('Assets/Scripts/Items/ItemObj.cs', 'public\\s+int\\s+GetSellPrice\\s*\\(', '2db73a42e0888726e11a77e0ace7602a8a5b180001561d6c5303a9557f628e30'),
    ('Assets/Scripts/Items/ItemObj.cs', 'public\\s+float\\s+priceFactor', '27b1659c6092d1252c466b5e572d1f9502dfc652358c77ba9c2e3342c70c21d4'),
    ('Assets/Scripts/Items/ItemObj.cs', 'public\\s+class\\s+Repair', 'a510f912c085e8a26f95c890d380f7dc50ea13c821aebffa1f541c827a640252'),
    ('Assets/Scripts/Items/ItemObj.cs', 'public\\s+class\\s+Durability', '3652cb248ac51f84285260e46d2b707dd9d06c5a3e579ce74d72963ec89aed78'),
    ('Assets/Scripts/Items/ItemObj.cs', 'public\\s+class\\s+LifeLimit', 'bbbc98401408697be3ba2330d7a8309439e2143804ed8f52fbfd1354df8734ab'),
    ('Assets/Scripts/Items/ItemObj.cs', 'static\\s+void\\s+LoadItemType\\s*\\(', '37c5b4fdb6053d04f7255b58860437cadbce72f34eed70613c3ae9d63cee9d33'),
    ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs', 'public\\s+void\\s+Sell\\s*\\(', 'c0527b07418361b13a0b444f9e622ed44033de21d5d32468912496c3fc0cbc64'),
    ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs', 'void\\s+Buy\\s*\\(', '31fa78e4fbbefc5cc03616ae420efe9142b625cf0aa4e07dd4dcc9cce71fab98'),
    ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs', 'public\\s+void\\s+AddRepurchase\\s*\\(', 'dd1b49613ac23e1e882d75d4efa8eec6040740e23f16cbb26d379e1f00637f81'),
    ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs', 'public\\s+void\\s+RemoveRepurchase\\s*\\(', '6e8934515feccba70a2dfbca9d1883e66449a8c0557619de8a09f4326413ea3d'),
    ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs', 'protected\\s+override\\s+void\\s+OnHide\\s*\\(', '0ea58a733fdf5ef8e3a838fe6bcb58d1fcd223307f3d040611aa2dda4e12c352'),
    ('Assets/Scripts/Mission/StroyManager.cs', 'public\\s+bool\\s+BuyItem\\s*\\(', 'f1dca9282259f3a22229b939e3ce0291c71d39dc2035680c5791a92ec91e9bb8'),
    ('Assets/Scripts/Items/ItemData.cs', 'public\\s+int\\s+currency\\s*\\{', '07850e74863c6ed75f8728cb75ca13e364724f07480d4cc4b6e62d1755a06a5a'),
    ('Assets/Scripts/Items/ItemManager.cs', 'public\\s+float\\s+percent', '1d71a837ccb0efa3fd38ec555b4741d58fd954b993d7e91ebce63775c7dd6334'),
)


def validate_sources(root=ROOT):
    texts = {p: uncomment((root/p).read_text(encoding='utf-8-sig', errors='replace'))
             for p in dict.fromkeys(p for p, _, _ in GUARDS)}
    for path, pattern, expected in GUARDS:
        actual = hashlib.sha256(body(texts[path], pattern).encode()).hexdigest()
        if actual != expected:
            raise ValueError('Town trade source contract changed: '+path+' '+pattern)
    data = texts[DATA]
    if 'protoItem.itemClassId = Convert.ToInt32(reader.GetString(reader.GetOrdinal("_function")));'.replace(' ', '') not in ''.join(data.split()):
        raise ValueError('Town trade component class binding changed')
    return [root/p for p in texts]


def price_row(identity, row, classes):
    cls = integer(row['_function'], 'item class')
    components = [] if cls not in classes else str(classes[cls]['classname']).split(',')
    factor = 'FULL'
    if 'Repair' in components:
        if 'Durability' in components:
            factor = 'DURABILITY'
            integer(row['_durability'], 'durability max', 1)
        elif 'LifeLimit' in components:
            factor = 'LIFE_LIMIT'
    return dict(item_id=identity, base_currency=integer(row['currency_value2'], 'Adventure currency'),
                factor=factor, component_class=cls, components=components)


def build_outputs(database=DEFAULT_DATABASE, root=ROOT):
    sources = validate_sources(root)
    catalog = json.loads(content_outputs(database, DEFAULT_LOCALIZATION)['content_manifest.json'])
    ids = sorted(catalog['selected_item_ids'])
    if not ids or len(ids) != len(set(ids)):
        raise ValueError('Invalid content item IDs')
    with sqlite3.connect(database.resolve().as_uri()+'?mode=ro', uri=True) as db:
        db.row_factory = sqlite3.Row
        rows, _ = numeric_rows(db, 'PrototypeItem', 'id')
        classes, _ = numeric_rows(db, 'item_class', 'id', allow_duplicates=True)
        prices = [price_row(identity, rows[identity], classes) for identity in ids]
    lines = ['/* Generated by export_town_trade_prices.py; original input IDs preserved. */',
             '#ifndef PE_TOWN_TRADE_PRICES_GENERATED_H', '#define PE_TOWN_TRADE_PRICES_GENERATED_H',
             'typedef enum { PE_TOWN_PRICE_FULL=0, PE_TOWN_PRICE_DURABILITY=1,',
             '               PE_TOWN_PRICE_LIFE_LIMIT=2 } PeTownPriceFactor;',
             'typedef struct { int item_id; unsigned base_currency; PeTownPriceFactor factor; } PeTownTradePrice;',
             '#define PE_TOWN_TRADE_PRICE_COUNT '+str(len(prices))+'u',
             'static const PeTownTradePrice pe_town_trade_prices[PE_TOWN_TRADE_PRICE_COUNT] = {']
    lines += ['    {%du,%du,PE_TOWN_PRICE_%s},' % (r['item_id'],r['base_currency'],r['factor']) for r in prices]
    lines += ['};', '#endif', '']
    manifest = dict(format_version=1, scope='Current native catalog; Adventure source sell and buyback metadata.',
                    sources=[describe_source(database)]+[describe_source(p) for p in sources],
                    contract=dict(currency_column='currency_value2', condition='Repair: Durability before LifeLimit; absent factor gives1',
                                  unit_price='(int)(base * (0.8f * factor + 0.2f))',
                                  quantity_order='truncate unit price before multiplying quantity',
                                  buyback_vendor_credit='unadjusted base currency times quantity',
                                  buyback_persistence_qualified=False),
                    prices=prices,
                    limitations=['LifeLimit requires unsupported instance HP and must not substitute native durability.',
                                 'No original or native runtime observation is established by this export.'])
    return {'pe_town_trade_prices.h': '\n'.join(lines).encode(),
            'pe_town_trade_prices.json': (json.dumps(manifest, indent=2)+'\n').encode()}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--output',type=Path,default=ROOT/'rebuild/generated')
    args=parser.parse_args(argv)
    try:
        for name,data in build_outputs().items():
            path=args.output/name
            if args.check:
                if not path.is_file() or path.read_bytes()!=data:
                    raise ValueError('Stale town trade output '+name)
            else:
                path.parent.mkdir(parents=True,exist_ok=True)
                path.write_bytes(data)
    except (OSError,ValueError,KeyError,sqlite3.Error) as error:
        print(error,file=sys.stderr)
        return 1
    print('Town trade prices checked' if args.check else 'Town trade prices generated')
    return 0


if __name__=='__main__':
    raise SystemExit(main())
