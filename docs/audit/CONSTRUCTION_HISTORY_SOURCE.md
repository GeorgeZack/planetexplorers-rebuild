# Construction undo/redo source audit

This October 6 audit is **source-derived** and required no original-game launch. The inspected code establishes the history capacity, operation ordering, resource arithmetic and standalone keyboard bindings. It does not establish physical native acceptance or exact original behavior across every scene/load lifecycle.

## Source owners

| Owner | Contract |
|---|---|
| [BSHistory.cs](<../../Assets/Scripts/WuYiqiu/Building System/Scripts/Action/BSHistory.cs>) | Five actions; undo/redo lists; new action clears redo; explicit Clear empties both lists. |
| [BSAction.cs](<../../Assets/Scripts/WuYiqiu/Building System/Scripts/Action/BSAction.cs>) | Undo modifications in reverse order, redo in forward order, stop on first failed modification; Do delegates to Redo. |
| [BSVoxelModify.cs](<../../Assets/Scripts/WuYiqiu/Building System/Scripts/Action/BSVoxelModify.cs>) | Own indices plus old/new voxel arrays. Precheck before writing; undo swaps arrays and reverses the mode passed to the check. |
| [PEBuildingMan.cs](<../../Assets/Scripts/WuYiqiu/Building System/Scripts/PEBuildingMan.cs>) | `Awake` subscribes `OnCheckVoxelModify`, around lines 173 and 302. This check owns resource debit/refund, capacity admission and the pre-write notification. |
| [BuildingMan.cs](<../../Assets/Scripts/WuYiqiu/Building System/Scripts/BuildingMan.cs>) | Update checks Undo before Redo, around lines 203–209; Clear destroys the active brush and clears history, around line 161. |
| [BSInput.cs](<../../Assets/Scripts/WuYiqiu/Building System/Scripts/Tools/BSInput.cs>) | Standalone Ctrl+Z / Ctrl+X; Unity Editor Shift+Z / Shift+X; input-field and voxel-creation-editor guards, lines 83–98. |
| [BSPointBrush.cs](<../../Assets/Scripts/WuYiqiu/Building System/Scripts/Brush/BSPointBrush.cs>) and [BSBoxBrush.cs](<../../Assets/Scripts/WuYiqiu/Building System/Scripts/Brush/BSBoxBrush.cs>) | Ordinary point/box edits add an action only if its initial Do succeeds. |

## Resource arithmetic is recomputed on every inverse operation

For the supported nonextended half-meter block cells, the check counts cells **per material in each modification**. Addition uses the proposed voxel array; subtraction uses the old voxel array and ignores empty cells. The selected compile branch in [Block45Constants.cs](../../Assets/Terrain/Voxelform2/Voxelform/Source/Scripts/Voxel/Block45/Block45Constants.cs) defines half-meter cells and `_scaleInverted=2`. [BSDataSource.cs](<../../Assets/Scripts/WuYiqiu/Building System/Scripts/BSDataSource.cs>) exposes that value; the resource divisor in PEBuildingMan is `1 << 2`, which is **4**, not the inverse cell volume of 8.

| Request | Effective resource operation for each material count n |
|---|---|
| Original addition / redo addition | Debit `ceil(n / 4)` |
| Undo addition | Refund `floor(n / 4)` |
| Original subtraction / redo subtraction | Refund `floor(n / 4)` |
| Undo subtraction | Debit `ceil(n / 4)` |

Undo is therefore not an exact reversal of the original inventory transaction. Adding one cell costs one item; undoing it returns zero; redoing it costs another item. Adding four same-material cells costs one and undo refunds one. Two separate two-cell batches each cost one and each refund zero when undone. Mixing two wood and two stone cells in one batch costs one of each and refunds none. Counts are not pooled across materials, actions or separate modifications within a source action.

`BSVoxelModify.Undo(Add)` passes the old voxels as proposed values, new voxels as old values, and `Subtract` to `onModifyCheck`. Undo(Subtract) swaps arrays and passes Add. Redo passes the original mode/arrays. `opType` is zero for undo and one for redo; the single-player resource branches choose arithmetic from effective mode, not this operation number. The check immediately mutates inventory before the voxel write. The source `IsGod` field defaults false and bypasses the check only when explicitly enabled; do not treat Adventure's B-key construction or Single Player Build as an automatic god-mode switch.

Subtraction preflights all returned materials with `package.CanAdd`; zero refunds are skipped when adding. Addition tests all needed counts before destroying materials. Terrain-voxel data uses divisor one. Extendable blocks have separate root/body counting and packed material rules, outside the bounded native cube/ramp history scope.

## History and failure ordering

Before adding, undoing or redoing, `BSHistory` drops null modifications and empty actions. `BSVoxelModify.IsNull` treats an edit outside the current data source's LOD0 bounds as null and also rejects empty old/new arrays. This is source streaming lifetime, not a general native player-distance rule.

When adding an action at capacity five, the oldest undo action is removed. The new action is appended and the redo list is cleared. Undo selects the last undo action; redo selects the last redo action. A `BSAction` can hold multiple modifications and processes reverse order on undo, forward order on redo.

The source history owner **ignores the action's returned success flag** and moves the action to the opposite list anyway. A multi-modification action can also fail after earlier modifications succeeded; no general rollback appears in this owner. The native rebuild should explicitly retain its safer all-or-nothing policy: check geometry, expected cell contents, resources, capacity and history integrity before committing, and preserve the history cursor after a failed attempt. This avoids copying source partial-failure defects while retaining source successful-operation arithmetic.

The source is a cell modification journal, not a complete inventory/world snapshot. Native history must not restore stale wallets, quest progress, health, equipment or unrelated later item changes. Recompute the debit/refund against the current inventory and validate the expected changed cells before applying the inverse.

## Notifications and excluded work

`onAfterDo` is declared and invoked by BSVoxelModify after the write; undo passes the **original** mode to this event even though its precheck passed the reversed mode. The inspected repository contains no subscribers to `onAfterDo`. Resource accounting is in `PEBuildingMan.OnCheckVoxelModify`, not a post-write handler. `onVoxelMotify` is raised from that successful precheck before the writes; mission observers are a separate source behavior.

The ordinary digging tool path in `DigTerrainManager`/`PEDigTool` does not create BSHistory actions. Native direct mining/removal must therefore not acquire an undo journal accidentally because it shares a low-level removal helper with B-key construction. Terrain deformation, tree removal, legacy full-sized objects and source selected-box/UI restoration remain outside this bounded block-cell journal.

## Input and lifecycle

Standalone source accepts either Control key held plus a fresh Z press for undo, or a fresh X press for redo. When both are set, BuildingMan's `if`/`else if` gives undo priority. `UICamera.inputHasFocus` and `VCEditor.s_Active` block both shortcuts. Source checks no `s_MouseOnUI` condition for these two keys. Unity Editor Shift variants are development-environment behavior and should not replace the installed game's Control bindings.

The inspected source has no dedicated undo/redo toolbar handler; visible native buttons/labels would be a native usability addition. Ctrl+X must own the native X edge so that it cannot also issue ordinary construction removal. Retain native modal/release ownership, focus and death guards.

[UIBuildBlock.QuitBlock](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/Widget/UIBuildBlock.cs), around line 234, disables its UI and creates a null brush but does not clear history. [Action_Build](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs), around line 389, toggles the build mask and UI mode message. BuildingMan.Update itself does not test this action mask; component activation outside the inspected C# chain remains unqualified. Restricting native undo/redo input to B-key construction is an explicit host adapter.

`BuildingMan.Clear` explicitly empties both lists. A direct caller tying that helper to every world transition was not established, and no history archive owner was found. Preserve history across normal exit/reentry to construction; native clearing on successful load, new game or world replacement is an explicit safe lifetime policy. Failed loads should not discard the live session's journal. Do not claim source reload persistence.

## Extraction and verification

[export_build_history.py](../../rebuild/tools/export_build_history.py) emits [the source constants header](../../rebuild/generated/pe_build_history_source.h) and [the manifest](../../rebuild/generated/pe_build_history_source.json). Complete-region guards bind list/failure semantics, array/mode inversion, pre-write accounting, exact input branches and priority, the active half-meter compile selection, clear behavior and successful point/box admission. The native batch bound of 64 and world bound of 1024 cells are existing rebuild limits, not source history constants.

```sh
python3 rebuild/tools/export_build_history.py --check
python3 -m unittest discover -s rebuild/tests -p test_build_history_export.py -v
```

Fourteen focused Python checks pass with no skips, including changed debit/refund rounding, inverse mode, failed-action history movement, standalone key/modifier, compile selection, capacity, god-mode default, close behavior, accepted whitespace/comments and read-only stale-output rejection. No new physical observation is claimed. Native safe-placement/reach checks, atomic retry and runtime regression evidence belong in the implementation record.
