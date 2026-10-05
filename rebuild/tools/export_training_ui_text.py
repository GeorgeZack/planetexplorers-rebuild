#!/usr/bin/env python3
"""Export original tutorial mission titles and tracker objectives, without Unity.

Read-only Quest_List/target/localization queries; supports --check for deterministic
checked-in outputs. Display counters remain the host's responsibility. Zero source
title/description IDs produce empty strings, never invented tutorial instructions.
"""
from __future__ import annotations
import argparse
from contextlib import closing
import json
from pathlib import Path
import re
import sqlite3
import sys
from export_content import (ROOT, DEFAULT_DATABASE, DEFAULT_LOCALIZATION, c_string,
                            connect_readonly, describe_source, integer, numeric_rows)

MISSIONS = (739, 747, 718, 740, 741, 742, 743, 744, 749, 745, 746,
            719, 748, 757, 721, 751, 752, 753, 756)
TABLES = {1: 'Quest_KillMonster', 2: 'Quest_LootItem',
          4: 'Quest_EnterArea', 5: 'Quest_UseItem'}
DEFAULT_OUTPUT = ROOT / 'rebuild/generated'


def build_outputs(database: Path, localization: Path | None) -> dict[str, bytes]:
    with closing(connect_readonly(database)) as db:
        quests, _ = numeric_rows(db, 'Quest_List', 'ID')
        items, _ = numeric_rows(db, 'PrototypeItem', 'id')
        monsters, _ = numeric_rows(db, 'PrototypeMonster', 'id')
        targets = {kind: numeric_rows(db, table, 'TargetID')[0]
                   for kind, table in TABLES.items()}
        translations, _ = numeric_rows(db, 'Translation', 'ID', allow_duplicates=True)
        english = {i: row['English'] for i, row in translations.items()}
        if localization is not None:
            with closing(connect_readonly(localization)) as external:
                rows, _ = numeric_rows(external, 'Translation', 'ID', allow_duplicates=True)
                english.update({i: row['English'] for i, row in rows.items() if row['English']})

        def text(identity: int) -> str:
            if identity == 0:
                return ''
            value = english.get(identity)
            if not isinstance(value, str) or not value or '\0' in value:
                raise ValueError(f'missing/invalid English translation {identity}')
            return value

        mission_rows, objective_rows = [], []
        for mission_id in MISSIONS:
            if mission_id not in quests:
                raise ValueError(f'missing tutorial mission {mission_id}')
            q = quests[mission_id]
            title_id = integer(q['MissionName'], 'mission title')
            description_id = integer(q['Description'], 'mission description')
            objective_first = len(objective_rows)
            ids = str(q['TargetIDList']).split(',')
            if ids == ['0']:
                ids = []
            seen = set()
            for value in ids:
                target_id = integer(value, 'target ID', 1)
                if target_id in seen:
                    raise ValueError(f'duplicate target {target_id} in mission {mission_id}')
                seen.add(target_id)
                kind = target_id // 1000
                if kind not in targets or target_id not in targets[kind]:
                    raise ValueError(f'missing/unsupported target {target_id}')
                target = targets[kind][target_id]
                description = integer(target['Desc'], 'target description')
                raw = rendered = text(description)
                item_id = maximum = 0
                if kind in (2, 5):
                    item_id = integer(target['ItemID'], 'target item', 1)
                    maximum = integer(target['ItemNum' if kind == 2 else 'UseNum'], 'target count', 1)
                    if item_id not in items:
                        raise ValueError(f'missing target item {item_id}')
                    item_name = text(integer(items[item_id]['_nameID'], 'item name', 1))
                    rendered = rendered.replace('"itemid%"', item_name).replace('"itemnum%"', str(maximum))
                elif kind == 1:
                    # This deliberately bounded tutorial catalog has one monster
                    # group. Reject a wider schema instead of guessing its UI.
                    match = re.fullmatch(r'([0-9]+)_([0-9]+)_([0-9]+)', str(target['MonsterID']))
                    if not match:
                        raise ValueError(f'unsupported monster group at {target_id}')
                    item_id = integer(match[1], 'monster prototype', 1)
                    maximum = integer(match[2], 'monster count', 1)
                    if item_id not in monsters:
                        raise ValueError(f'missing target monster {item_id}')
                    monster_name = text(integer(monsters[item_id]['TranslationID'], 'monster name', 1))
                    rendered = rendered.replace('"monsterid%"', monster_name).replace('"monsternum%"', str(maximum))
                if re.search(r'\"[^\"]*%\"', rendered):
                    raise ValueError(f'unresolved target placeholder {target_id}: {rendered}')
                objective_rows.append(dict(target_id=target_id, description_id=description, kind=kind,
                    item_id=item_id, max_count=maximum, text=rendered, source_template=raw,
                    source_table=TABLES[kind]))
            mission_rows.append(dict(mission_id=mission_id,
                mission_type=integer(q['Type'], 'mission type', 1), title_id=title_id,
                description_id=description_id, title=text(title_id), description=text(description_id),
                objective_first=objective_first, objective_count=len(objective_rows)-objective_first))
    header = ['/* Generated by tools/export_training_ui_text.py; do not edit. */',
        '#ifndef PE_TRAINING_UI_TEXT_H', '#define PE_TRAINING_UI_TEXT_H', '#include <stddef.h>',
        'typedef struct {', '    int target_id, description_id, kind, item_id, max_count;',
        '    const char *text;', '} PeTrainingUiObjectiveText;',
        'typedef struct {', '    int mission_id, mission_type, title_id, description_id;',
        '    const char *title, *description;', '    size_t objective_first, objective_count;',
        '} PeTrainingUiMissionText;',
        '/* Kinds match original target thousands: 1 kill, 2 collect, 4 event/area, 5 use.',
        ' * item_id is the monster prototype for kind1. max_count0 means no count text.',
        ' * Strings have original placeholder expansion; append dynamic counters separately. */',
        'static const PeTrainingUiObjectiveText pe_training_ui_objective_texts[] = {']
    for row in objective_rows:
        header.append('    {%d,%d,%d,%d,%d,%s},' % (row['target_id'], row['description_id'],
            row['kind'], row['item_id'], row['max_count'], c_string(row['text'])))
    header.extend(['};', 'static const PeTrainingUiMissionText pe_training_ui_mission_texts[] = {'])
    for row in mission_rows:
        header.append('    {%d,%d,%d,%d,%s,%s,%d,%d},' % (row['mission_id'], row['mission_type'],
            row['title_id'], row['description_id'], c_string(row['title']), c_string(row['description']),
            row['objective_first'], row['objective_count']))
    header.extend(['};',
        'static inline const PeTrainingUiMissionText *pe_training_ui_mission_text(int id) {',
        '    for(size_t i=0;i<sizeof pe_training_ui_mission_texts/sizeof pe_training_ui_mission_texts[0];i++)',
        '        if(pe_training_ui_mission_texts[i].mission_id==id)return &pe_training_ui_mission_texts[i];',
        '    return NULL;', '}',
        'static inline const PeTrainingUiObjectiveText *pe_training_ui_objective_text(int id) {',
        '    for(size_t i=0;i<sizeof pe_training_ui_objective_texts/sizeof pe_training_ui_objective_texts[0];i++)',
        '        if(pe_training_ui_objective_texts[i].target_id==id)return &pe_training_ui_objective_texts[i];',
        '    return NULL;', '}',
        'static inline const PeTrainingUiObjectiveText *pe_training_ui_mission_objective(int mission_id,size_t index) {',
        '    const PeTrainingUiMissionText *m=pe_training_ui_mission_text(mission_id);',
        '    return m&&index<m->objective_count?&pe_training_ui_objective_texts[m->objective_first+index]:NULL;',
        '}', '#endif', ''])
    manifest = dict(schema=1, source=describe_source(database),
        localization=describe_source(localization) if localization is not None else None,
        source_expansion='Assets/Scripts/Mission/MissionScript/MissionManager.cs:2186-2575',
        missions=mission_rows, objectives=objective_rows)
    return {'pe_training_ui_text.h': '\n'.join(header).encode(),
            'pe_training_ui_text.json': (json.dumps(manifest, ensure_ascii=False, indent=2)+'\n').encode()}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database', type=Path, default=DEFAULT_DATABASE)
    parser.add_argument('--localization', type=Path, default=DEFAULT_LOCALIZATION)
    parser.add_argument('--embedded-only', action='store_true')
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    try:
        outputs = build_outputs(args.database, None if args.embedded_only else args.localization)
        if args.check:
            stale = [name for name, data in outputs.items()
                     if not (args.output/name).is_file() or (args.output/name).read_bytes()!=data]
            if stale:
                raise ValueError('stale generated UI text: '+', '.join(stale))
        else:
            args.output.mkdir(parents=True, exist_ok=True)
            for name, data in outputs.items():
                (args.output/name).write_bytes(data)
        print('Tutorial UI text checked' if args.check else 'Tutorial UI text generated')
        return 0
    except (ValueError, OSError, sqlite3.Error) as exc:
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
