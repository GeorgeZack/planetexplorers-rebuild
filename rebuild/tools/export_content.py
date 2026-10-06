#!/usr/bin/env python3
"""Export a deliberately small, source-backed C content catalog without Unity.

Only Python's standard library is needed. Input databases are opened read-only.
Regenerate with this script; --check checks both outputs without writing them.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sqlite3
import sys

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DATABASE = ROOT / 'Assets/Resources/DataBase/localData.bytes'
DEFAULT_LOCALIZATION = ROOT / 'i18n.db'
DEFAULT_OUTPUT = ROOT / 'rebuild/generated'
RECIPE_IDS = (1, 7, 8, 19, 20, 58, 152, 13)
ANNOT_VENDOR_PROTOTYPES = (153, 158, 163, 143, 148)
RESOURCE_IDS = (268, 271, 273, 279, 280)
# Catalog presentation only. Equipment identity/defense/wear are owned by the
# separate guarded export_starter_outfit contract, not generic bag equip logic.
STARTER_OUTFIT_CATALOG_IDS = (95, 131, 192)
TOOL_SOURCES = (ROOT / 'Assets/Scripts/Items/PEEquipment/PEDigTool.cs',
                ROOT / 'Assets/Scripts/Items/PEEquipment/PEAxe.cs')
TOOL_OVERRIDES = {12: (4.0, 'Assets/Resources/Prefab/Item/Equip/Weapon/Spade/wood_spade.prefab'),
                  19: (4.0, 'Assets/Resources/Prefab/Item/Equip/Weapon/Hoe/wood_hoe.prefab'),
                  26: (4.0, 'Assets/Resources/Prefab/Item/Equip/Weapon/Ax/wood_ax.prefab'),
                  1527: (4.0, 'Assets/Resources/Prefab/Item/Equip/Weapon/Ax/safety_axe.prefab')}


def pairs(value, separator, field):
    """Decode source fixed grant data; reject malformed/duplicate grants."""
    result = []
    tokens = str(value).split(separator)
    if separator == ',':
        if len(tokens) % 2:
            raise ValueError(f'{field}: incomplete grant pair')
        tokens = ['_'.join(tokens[i:i+2]) for i in range(0, len(tokens), 2)]
    for token in tokens:
        parts = token.split('_')
        if len(parts) != 2:
            raise ValueError(f'{field}: invalid grant')
        identity, count = (integer(v, field, 1) for v in parts)
        if any(row['item_id'] == identity for row in result):
            raise ValueError(f'{field}: duplicate grant')
        result.append({'item_id': identity, 'count': count})
    return result


def integer(value: object, field: str, minimum: int = 0) -> int:
    text = str(value)
    if not re.fullmatch(r'[0-9]+', text):
        raise ValueError(f'{field}: expected an unsigned integer, got {value!r}')
    result = int(text)
    if result < minimum or result > 2147483647:
        raise ValueError(f'{field}: out-of-range integer {result}')
    return result


def real(value: object, field: str) -> float:
    result = float(value)
    if not math.isfinite(result) or result < 0 or result > 3.402823466e38:
        raise ValueError(f'{field}: invalid nonnegative float {value!r}')
    return result


def c_float(value: float) -> str:
    text = format(value, '.9g')
    return text + ('f' if '.' in text or 'e' in text else '.0f')


def numeric_rows(connection: sqlite3.Connection, table: str, id_column: str, allow_duplicates=False):
    """Skip spreadsheet metadata; localization alone allows last-row-wins IDs."""
    result = {}
    skipped = 0
    for row in connection.execute(f'SELECT * FROM "{table}" ORDER BY rowid'):
        value = str(row[id_column])
        if not re.fullmatch(r'[0-9]+', value):
            skipped += 1
            continue
        identity = integer(value, f'{table}.{id_column}')
        if identity in result and not allow_duplicates:
            raise ValueError(f'{table}: duplicate ID {identity}')
        result[identity] = row
    return result, skipped


def connect_readonly(path: Path) -> sqlite3.Connection:
    connection = sqlite3.connect(path.resolve().as_uri() + '?mode=ro', uri=True)
    connection.row_factory = sqlite3.Row
    return connection


def c_string(value: str) -> str:
    """Use UTF-8 octal escapes for portable, unambiguous C string literals."""
    return '"' + ''.join(
        chr(byte) if 32 <= byte < 127 and byte not in (34, 92)
        else f'\\{byte:03o}' for byte in value.encode('utf-8')
    ) + '"'


def describe_source(path: Path) -> dict:
    try:
        label = path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        label = str(path.resolve())
    return {'path': label, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def build_outputs(database: Path, localization: Path | None) -> dict[str, bytes]:
    stamina_costs = []
    for path in TOOL_SOURCES:
        match = re.search(r'm_StaminaCost\s*=\s*([0-9.]+)f\s*;', path.read_text(encoding='utf-8-sig'))
        if not match:
            raise ValueError(f'{path.name}: missing numeric stamina default')
        stamina_costs.append(real(match.group(1), f'{path.name} stamina default'))
    db = connect_readonly(database)
    try:
        item_rows, skipped_items = numeric_rows(db, 'PrototypeItem', 'id')
        recipe_rows, skipped_recipes = numeric_rows(db, 'synthesis_skill', 'ID')
        embedded_rows, _ = numeric_rows(db, 'Translation', 'ID', allow_duplicates=True)
        names = {identity: row['English'] for identity, row in embedded_rows.items()}
        external_rows = {}
        external_source = None
        if localization is not None and localization.exists():
            external = connect_readonly(localization)
            try:
                external_rows, _ = numeric_rows(external, 'Translation', 'ID', allow_duplicates=True)
                # External English has precedence; blank/missing values use the
                # embedded translation and finally PrototypeItem._engName.
                names.update({identity: row['English'] for identity, row in external_rows.items()
                              if row['English']})
            finally:
                external.close()
            external_source = describe_source(localization)

        bundle = str(item_rows[1289]['bundle']).split(';')
        if len(bundle) != 2 or bundle[0] != '0':
            raise ValueError('Adventure starter must be a fixed bundle')
        starter = pairs(bundle[1], ',', 'Adventure starter1289')
        quests, _ = numeric_rows(db, 'AdRandomQuest_List', 'ID')
        # Quest grants separate underscore ID/count pairs with commas.
        supplies = []
        for token in str(quests[9032]['Get_MissionItem']).split(','):
            row = pairs(token, ';', 'Adventure supplies9032')[0]
            if any(v['item_id'] == row['item_id'] for v in supplies):
                raise ValueError('Adventure supplies9032: duplicate grant')
            supplies.append(row)
        medicine_grants = {}
        for field in ('Get_MissionItem', 'RewardItem', 'CoRemoveItem'):
            medicine_grants[field] = [pairs(token, ';', 'Medicine9072 '+field)[0]
                                      for token in str(quests[9072][field]).split(',')]
        hunt_return_rewards = [pairs(token, ';', 'Hunt9127 RewardItem')[0]
                               for token in str(quests[9127]['RewardItem']).split(',')]
        if hunt_return_rewards != [{'item_id': identity, 'count': 1} for identity in (592,593,594,595)]:
            raise ValueError('Hunt9127 reward contract changed')
        stores, _ = numeric_rows(db, 'store', 'store_id')
        npc_stores, _ = numeric_rows(db, 'NPCstore', 'store_id')
        vendor_items = set()
        for prototype in ANNOT_VENDOR_PROTOTYPES:
            matching = [r for r in npc_stores.values() if int(r['storenpc_id']) == prototype]
            if len(matching) != 1:
                raise ValueError('Missing or ambiguous Annot vendor')
            for value in str(matching[0]['adven_single']).split(','):
                vendor_items.add(integer(stores[integer(value, 'store ID', 1)]['item_id'], 'store item', 1))
        item_classes, _ = numeric_rows(db, 'item_class', 'id', allow_duplicates=True)
        skills, _ = numeric_rows(db, 'skMain', '_id')
        triggers, _ = numeric_rows(db, 'skTriggerEvent', '_id')
        medpack_skill = skills[20150028]
        medpack_trigger = triggers[2]
        if (item_rows[925]['_function'] != '27' or item_rows[925]['skillId'] != '20150028'
                or item_classes[27]['classname'] != 'Property,Consume'
                or medpack_skill['_triggerEvents'] != '2' or medpack_skill['_cond'] != 'False'
                or any(medpack_skill[key] != '0' for key in ('_pretimeOfPrepare','_postimeOfPrepare',
                    '_pretimeOfMain','_timeOfMain','_postimeOfMain','_pretimeOfEnding','_postimeOfEnding'))
                or medpack_trigger['_cond'] != 'True' or medpack_trigger['_modsCaster'] != '0'
                or medpack_trigger['_modsTarget'] != 'mad,1,1,350'
                or medpack_skill['_coolingTime'] != '10' or medpack_skill['_coolingTimeShared'] != '10'
                or medpack_skill['_coolingTimeType'] != '9' or skills[20150023]['_coolingTimeType'] != '11'):
            raise ValueError('Portable Medpack925 healing/cooldown source contract changed')
        medpack = {'item_id':925,'skill_id':20150028,'component_class':27,'trigger_id':2,
                   'heal':real(medpack_trigger['_modsTarget'].split(',')[3],'medpack heal'),
                   'cooldown_seconds':real(medpack_skill['_coolingTime'],'medpack cooldown'),
                   'cooldown_group':9,'juice_cooldown_group':11,
                   'target_effect_id':integer(medpack_trigger['_effOnHitTarget'],'medpack effect')}
        formula_classes = {identity for identity, row in item_classes.items()
                           if 'ReplicatorFormula' in str(row['classname']).split(',')}
        selected_ids = set(RESOURCE_IDS) | {v['item_id'] for v in starter + supplies}
        selected_ids |= vendor_items | {v['item_id'] for rows in medicine_grants.values() for v in rows}
        selected_ids |= {v['item_id'] for v in hunt_return_rewards}
        selected_ids.update(STARTER_OUTFIT_CATALOG_IDS)
        starter_outfit_catalog = []
        for identity in STARTER_OUTFIT_CATALOG_IDS:
            row = item_rows[identity]
            component_class = integer(row['_function'], f'item {identity} component class')
            starter_outfit_catalog.append({'item_id': identity, 'component_class': component_class,
                'components': str(item_classes[component_class]['classname']).split(',')})
        recipe_ids = list(RECIPE_IDS)
        for identity in sorted(selected_ids):
            row = item_rows.get(identity)
            if row is not None and integer(row['_function'], 'item component class') in formula_classes:
                for value in str(row['learn']).split(','):
                    formula = integer(value, f'book {identity} formula', 1)
                    if formula not in recipe_ids:
                        recipe_ids.append(formula)
        material_items = [-1] * 256
        material_rows, _ = numeric_rows(db, 'buildblock', 'matID')
        for material, row in material_rows.items():
            if material > 255:
                raise ValueError(f'buildblock: material ID {material} exceeds byte range')
            material_items[material] = integer(row['itemID'], f'material {material} item', 1)
            selected_ids.add(material_items[material])
        material_durations = [0.0] * 256
        resource_rows, _ = numeric_rows(db, 'resource', 'ID')
        terrain_resources = {}
        for material, row in resource_rows.items():
            if material <= 255:
                material_durations[material] = real(row['durability'], f'resource {material} durability')
                primary = str(row['production']).split(';')[0].split(',')
                drops = []
                if primary != ['0']:
                    if len(primary) % 2 or len(primary) > 4:
                        raise ValueError(f'resource {material}: unsupported primary drop list')
                    previous = 0.0
                    for offset in range(0, len(primary), 2):
                        identity = integer(primary[offset], f'resource {material} drop ID', 1)
                        threshold = real(primary[offset+1], f'resource {material} drop threshold')
                        if threshold < previous or threshold > 100:
                            raise ValueError(f'resource {material}: invalid cumulative drop threshold')
                        previous = threshold
                        drops.append({'item_id': identity, 'threshold': threshold})
                        selected_ids.add(identity)
                terrain_resources[material] = {
                    'fixed_count': real(row['fixed_num'], f'resource {material} fixed_num'),
                    'self_count': real(row['self_num'], f'resource {material} self_num'),
                    'drops': drops,
                    'original_production': str(row['production']),
                }
        player_rows, _ = numeric_rows(db, 'initProp', 'id')
        player_fields = {'stamina_max': 'StaminaMax', 'stamina_recover_speed': 'StaminaRecoverSpeed',
                         'stamina_recover_delay': 'StaminaRecoverDelay', 'stamina_reduce_percent': 'StaminaReducePercent',
                         'resource_range': 'ResRange', 'resource_damage': 'ResDamage', 'attack': 'Atk',
                         'cut_damage': 'CutDamage', 'cut_bonus': 'CutBouns', 'hp': 'Hp', 'hp_max': 'HpMax', 'defense': 'Def'}
        player_defaults = {key: real(player_rows[1][column], f'player {column}')
                           for key, column in player_fields.items()}
        recipes = []
        for identity in recipe_ids:
            if identity not in recipe_rows:
                raise ValueError(f'synthesis_skill: missing required recipe {identity}')
            row = recipe_rows[identity]
            output = integer(row['object_ID'], f'recipe {identity} output', 1)
            output_count = integer(row['num_each_product'], f'recipe {identity} output count', 1)
            seconds = float(row['need_time'])
            if not math.isfinite(seconds) or seconds <= 0 or seconds > 3.402823466e38:
                raise ValueError(f'recipe {identity}: invalid craft duration {seconds}')
            ingredients = []
            for slot in range(1, 7):
                column = 'materia4' if slot == 4 else f'material{slot}'
                item_id = integer(row[column], f'recipe {identity} {column}')
                count = integer(row[f'num{slot}'], f'recipe {identity} num{slot}')
                if (item_id == 0) != (count == 0):
                    raise ValueError(f'recipe {identity}: incomplete ingredient {slot}')
                if item_id:
                    ingredients.append({'item_id': item_id, 'count': count})
                    selected_ids.add(item_id)
            if not ingredients:
                raise ValueError(f'recipe {identity}: no ingredients')
            selected_ids.add(output)
            recipes.append({'id': identity, 'output_id': output, 'output_count': output_count,
                            'seconds': seconds,
                            'workspace': integer(row['workspace'], f'recipe {identity} workspace'),
                            'ingredients': ingredients})

        items = []
        for identity in sorted(selected_ids):
            if identity not in item_rows:
                raise ValueError(f'PrototypeItem: missing required item {identity}')
            row = item_rows[identity]
            name_id = integer(row['_nameID'], f'item {identity} name ID')
            name = names.get(name_id) or row['_engName']
            if not isinstance(name, str) or not name or '\0' in name:
                raise ValueError(f'item {identity}: invalid English name')
            item_type = integer(row['_typeId'], f'item {identity} type')
            kind = {11: 'PE_TOOL_SHOVEL', 12: 'PE_TOOL_PICKAXE', 13: 'PE_TOOL_AXE'}.get(item_type, 'PE_TOOL_NONE')
            icons = str(row['_iconId']).split(',')
            if len(icons) != 3 or any('\0' in icon for icon in icons):
                raise ValueError(f'item {identity}: invalid icon triple')
            learn_kind = ('PE_LEARN_RECIPE' if integer(row['_function'], 'item component class') in formula_classes else 'PE_LEARN_SCAN' if item_type == 27 else 'PE_LEARN_NONE') if str(row['learn']) != '0' else 'PE_LEARN_NONE'
            learns = [] if learn_kind == 'PE_LEARN_NONE' else [integer(v, f'item {identity} learn ID', 1) for v in str(row['learn']).split(',')]
            if len(learns) > 16 or len(set(learns)) != len(learns):
                raise ValueError(f'item {identity}: invalid learn list')
            items.append({'id': identity, 'name': name,
                          'stack_limit': integer(row['stacking_num'], f'item {identity} stack', 1),
                          'item_type': item_type,
                          'durability_max': integer(row['_durability'], f'item {identity} durability'),
                          'attack': real(row['Atk'], f'item {identity} Atk'),
                          'resource_damage': real(row['ResDamage'], f'item {identity} ResDamage'),
                          'resource_range': real(row['ResRange'], f'item {identity} ResRange'),
                          'skill_id': integer(row['skillId'], f'item {identity} skillId'),
                          'durability_cost': real(row['durDec'], f'item {identity} durDec'),
                          'stamina_cost': TOOL_OVERRIDES[identity][0] if identity in TOOL_OVERRIDES else ((stamina_costs[1] if kind == 'PE_TOOL_AXE' else stamina_costs[0]) if kind != 'PE_TOOL_NONE' else 0.0),
                          'tool_kind': kind,
                          'cut_damage': real(row['CutDamage'], f'item {identity} CutDamage'),
                          'cut_bonus': real(row['CutBouns'], f'item {identity} CutBouns'),
                          'tab': integer(row['tab'], f'item {identity} tab'),
                          'equip_position': integer(row['_position'], f'item {identity} position'),
                          'icons': ['' if v == '0' else v for v in icons],
                          'learn_kind': learn_kind, 'learn_ids': learns})
    finally:
        db.close()

    source = describe_source(database)
    manifest = {
        'format_version': 4,
        'scope': 'Legacy subset plus Adventure1289 starter,9032 supplies,9072 medicine,9127 hunt rewards, fixed female starter clothing presentation and all five archived Annot purchase catalogs with formula ingredient/output closure; equipment/deployable/consumable behavior remains bounded.',
        'adventure_starter': starter, 'adventure_supplies': supplies,
        'medicine_grants': medicine_grants, 'annot_vendor_item_ids': sorted(vendor_items),
        'hunt_return_rewards': hunt_return_rewards,
        'starter_outfit_catalog': starter_outfit_catalog,
        'portable_medpack': medpack,
        'formula_component_classes': sorted(formula_classes),
        'source_tool_overrides': {str(k): {'stamina_cost': v[0], 'source': describe_source(ROOT/v[1]), 'evidence': 'Serialized wood_hoe PEDigTool11466528; see SANDBOX_PICKAXE audit.' if k == 19 else 'Serialized wood_ax PEAxe11484812; see SANDBOX_ACTIONS audit.' if k == 26 else 'Serialized source/installed fields verified in TRAINING_MINING/TRAINING_GATHERING audits.'} for k,v in TOOL_OVERRIDES.items()},
        'source_database': source,
        'source_tool_defaults': [describe_source(path) for path in TOOL_SOURCES],
        'source_localization': external_source,
        'duplicate_localization_ids': 'Last row by SQLite rowid wins, matching legacy dictionary assignment.',
        'localization_order': ['external English (when present)', 'embedded English', 'PrototypeItem._engName'],
        'source_counts': {'valid_items': len(item_rows), 'skipped_item_metadata': skipped_items,
                          'valid_recipes': len(recipe_rows), 'skipped_recipe_metadata': skipped_recipes,
                          'embedded_unique_translations': len(embedded_rows), 'external_unique_translations': len(external_rows),
                          'construction_materials': len(material_rows), 'terrain_resources': len(terrain_resources)},
        'selected_item_ids': sorted(selected_ids),
        'selected_recipe_ids': recipe_ids,
        'player_defaults': player_defaults,
        'terrain_material_items': material_items,
        'terrain_material_durability': material_durations,
        'terrain_resources': terrain_resources,
        'stat_sources': {
            'item_stats': 'PrototypeItem columns; zero resource stats are preserved, not replaced with attack.',
            'tool_kind': 'PrototypeItem._typeId: 11 shovel, 12 pickaxe, 13 axe (itemtype localization).',
            'stamina_cost': 'Verified serialized overrides12/19/26/1527=4; other selected tools retain PEDigTool/PEAxe class default5 pending prefab audit.',
            'player_defaults': 'initProp row id=1; equipment buffs and skill modifiers are not evaluated here.',
            'terrain_material_items': 'buildblock.matID -> itemID; placement material mapping, not complete resource drop probabilities.',
            'terrain_material_durability': 'resource.ID -> durability; NaturalRes.GetTerrainResData matches ID directly.',
            'terrain_resources': 'resource first production segment cumulative primary thresholds plus fixed_num/self_num. Extra and special drops remain in manifest original_production and are not implemented by this catalog.',
            'cut_damage': 'PrototypeItem.CutDamage and CutBouns; original tree resource duration/scale is applied separately.',
        },
        'items': items,
        'recipes': recipes,
        'asset_terms': 'Original repository README.md; original data remains subject to its terms.',
    }
    lines = ['/* Generated by tools/export_content.py. Do not edit.',
             f' * Source SQLite SHA-256: {source["sha256"]}',
             ' * See content_manifest.json and original README.md for provenance/terms. */',
             '#include "pe_content.h"', '', 'const PeItemDef pe_items[] = {']
    for item in items:
        lines.append('    {' + ', '.join([
            str(item['id']), c_string(item['name']), str(item['stack_limit']),
            str(item['item_type']), str(item['durability_max']),
            c_float(item['attack']), c_float(item['resource_damage']), c_float(item['resource_range']),
            str(item['skill_id']), c_float(item['durability_cost']), c_float(item['stamina_cost']),
            item['tool_kind'], c_float(item['cut_damage']), c_float(item['cut_bonus']),
            str(item['tab']), str(item['equip_position']), *[c_string(v) for v in item['icons']],
            item['learn_kind'], str(len(item['learn_ids'])), '{' + (', '.join(map(str,item['learn_ids'])) or '0') + '}']) + '},')
    lines += ['};', 'const size_t pe_item_count = sizeof(pe_items) / sizeof(pe_items[0]);', '',
              'const PeRecipeDef pe_recipes[] = {']
    for recipe in recipes:
        inputs = ', '.join(f'{{{i["item_id"]}, {i["count"]}}}' for i in recipe['ingredients'])
        duration = format(recipe['seconds'], '.9g')
        if '.' not in duration and 'e' not in duration:
            duration += '.0'
        lines.append(f'    {{{recipe["id"]}, {recipe["output_id"]}, {recipe["output_count"]}, '
                     f'{duration}f, {recipe["workspace"]}, {len(recipe["ingredients"])}, {{{inputs}}}}},')
    lines += ['};', 'const size_t pe_recipe_count = sizeof(pe_recipes) / sizeof(pe_recipes[0]);', '',
              'const PeItemDef *pe_item_def(int id)', '{',
              '    for (size_t i = 0; i < pe_item_count; ++i)',
              '        if (pe_items[i].id == id) return &pe_items[i];',
              '    return NULL;', '}', '', 'const PeRecipeDef *pe_recipe_def(int id)', '{',
              '    for (size_t i = 0; i < pe_recipe_count; ++i)',
              '        if (pe_recipes[i].id == id) return &pe_recipes[i];',
              '    return NULL;', '}', '']
    for name, grants in [('starter', starter), ('supplies', supplies)]:
        lines += [f'const PeIngredient pe_adventure_{name}[] = {{' + ', '.join(f'{{{v["item_id"]}, {v["count"]}}}' for v in grants) + '};',
                  f'const size_t pe_adventure_{name}_count = sizeof(pe_adventure_{name}) / sizeof(pe_adventure_{name}[0]);', '']
    lines += ['const PePlayerDefaults pe_player_defaults = {' +
              ', '.join(c_float(value) for value in player_defaults.values()) + '};', '',
              'const int pe_terrain_material_items[256] = {']
    for start in range(0, 256, 16):
        lines.append('    ' + ', '.join(map(str, material_items[start:start+16])) + ',')
    lines += ['};', '', 'const float pe_terrain_material_durability[256] = {']
    for start in range(0, 256, 8):
        lines.append('    ' + ', '.join(c_float(value) for value in material_durations[start:start+8]) + ',')
    lines += ['};', '', 'int pe_terrain_material_item(int material)', '{',
              '    return material >= 0 && material < 256 ? pe_terrain_material_items[material] : -1;',
              '}', '', 'float pe_terrain_material_duration(int material)', '{',
              '    return material >= 0 && material < 256 ? pe_terrain_material_durability[material] : 0.0f;',
              '}', '']
    lines += ['const PeTerrainResourceDef pe_terrain_resources[256] = {']
    for material, resource in sorted(terrain_resources.items()):
        drops = ', '.join('{' + str(d['item_id']) + ', ' + c_float(d['threshold']) + '}'
                          for d in resource['drops']) or '{0, 0.0f}'
        lines.append(f'    [{material}] = {{{c_float(resource["fixed_count"])}, '
                     f'{c_float(resource["self_count"])}, {len(resource["drops"])}, {{{drops}}}}},')
    lines += ['};', '', 'const PeTerrainResourceDef *pe_terrain_resource(int material)', '{',
              '    return pe_terrain_material_duration(material) > 0.0f ? &pe_terrain_resources[material] : NULL;',
              '}', '']
    lines += [f'const float pe_medpack_heal = {c_float(medpack["heal"])};',
              f'const float pe_medpack_cooldown_seconds = {c_float(medpack["cooldown_seconds"])};', '']
    return {'pe_content.c': '\n'.join(lines).encode('utf-8'),
            'content_manifest.json': (json.dumps(manifest, indent=2, ensure_ascii=False) + '\n').encode('utf-8')}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='fail if generated files differ; do not write')
    parser.add_argument('--database', type=Path, default=DEFAULT_DATABASE)
    parser.add_argument('--localization', type=Path, default=DEFAULT_LOCALIZATION)
    parser.add_argument('--output-dir', type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args(argv)
    try:
        outputs = build_outputs(args.database, args.localization)
        if args.check:
            stale = [name for name, data in outputs.items()
                     if not (args.output_dir / name).is_file() or (args.output_dir / name).read_bytes() != data]
            if stale:
                print('Generated content is stale or missing: ' + ', '.join(stale), file=sys.stderr)
                return 1
            print('Generated content matches source databases.')
        else:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            for name, data in outputs.items():
                (args.output_dir / name).write_bytes(data)
            print('Exported original recipes, Adventure grants/book dependencies, player stats and material tables.')
    except (OSError, sqlite3.Error, ValueError, KeyError, IndexError, TypeError) as error:
        print(f'Content export failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
