# Shop quantity entry and bounded item details

October7,2026, US/Central. **Source-derived:** editable shop quantities, component-driven item tooltips, displayed durability units and the first-formula learned marker. **Implemented in C:** safe numeric quantity entry and shared item detail presentation across inventory, character, shop and camp repair. No original-game launch, desktop input, new textures or save-version change was needed. A later private-display draw-only capture checks the staged native presentation; it does not establish original-game or live host acceptance.

## Active source paths

[UIShopWnd](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs) declares `UIInput mOpNumLabel`. `PreSell` and `OnLeftMouseCliked` bind the selected exact item/grid, set quantity1 and populate unit/total prices. `Update` parses entered text and clamps the result to1..`mOpGrid.Item.GetCount()`; blank text means1. When `UICamera.inputHasFocus` is false, it writes the normalized quantity and current total back to the labels. `OnOpOkBtn` calls `Buy` or `Sell` using that selected quantity. Buyback uses the same buy path; the existing native trade owner retains its documented condition-adjusted debit versus full-prototype vendor credit.

The source additionally accelerates held +/- buttons and has a separate right-click Buy All confirmation. Those controls are not implemented by this increment. The native +/- controls remain fresh-click steps.

[Grid_N.OnTooltip](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/Widget/Grid_N.cs) calls `Item.GetTooltip`, adds suit context and passes the result to `ToolTipsMgr`. [ItemObject.GetTooltip](../../Assets/Scripts/Items/ItemObj.cs) begins with the localized description template, processes its item components, then replaces sell/buy price markers. The `Durability.ProcessTooltip` component inserts `ceil(current / 100f)` and `ceil(maximum / 100f)`. These are display units: raw durability remains the gameplay, pricing, repair and save value. `pe_item_has_durability` qualifies exact selected-item `_function` → `item_class.classname` component membership from the guarded content exporter. A positive prototype `_durability` default alone is insufficient: scripts and consumables must not acquire a fabricated condition label. The table adds no item/save layout field.

`ReplicatorFormula.ProcessTooltip` checks **only `formulaId[0]`**, appending localized marker4000001 when that formula is known. The native learned status follows this first-formula test; it does not claim that every formula in a multi-formula script is known. Scanner items retain separate Scan wording.

[UIRepairWnd.UpdateItem](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRepairWnd.cs) uses the distinct float expression `ceil(current * .01f)` and shows the missing durability as an additional amount. Native repair details retain their existing current/maximum layout, now using that multiplication expression for their displayed units. This layout is a native adapter; the source's missing-amount label is not claimed. The two float expressions are preserved separately rather than assuming they are identical at every boundary.

[The focused source guard](../../rebuild/tests/test_shop_details_source.py) pins complete SHA256 hashes of these four active files and checks the specific input/commit/component expressions. The reference sources remain unchanged. These are source checks, not an asset export or Unity dependency.

## Native quantity behavior

[PeSandboxUI](../../rebuild/src/pe_sandbox_ui.h) holds only transient edit text, the prior quantity and an exact selected-row snapshot. Click the quantity field to edit; the first typed digit replaces the previous value. Backspace removes a digit, and only ASCII digits enter the buffer. Empty/zero text resolves to1. Overflow-safe parsing clamps to the current stock/stack count; nonnumeric input and oversized pasted values cannot overflow or issue commands. The edited value and total remain visible.

Enter or a click outside the quantity field accepts the edit **without trading or activating that outside target**. A subsequent fresh Enter/click confirms the transaction. Escape cancels just the edit and restores its previous quantity; focus loss also restores it. A following Escape closes the shop through its existing route. These explicit two-step input rules and safe parsing are native adapters, not a claim to reproduce NGUI focus exactly.

The edit binds vendor, mode, row index, bag/store identity, complete item/count/instance/wear value, unit price and eligibility. A changed row or stock resets quantity to1 and consumes the current input, including changes after accepting the edit but before confirming the trade. It never silently trades a replacement instance. The existing atomic shop/sell/buyback owners remain authoritative for capacity, funds, price and ownership. UI quantities, snapshots and focus are not saved.

Shop input still owns quickslot/conversation digits, save/load shortcuts and pointer held/release frames. Quantity text never changes ordinary quickslot selection. The shop's moved/scaled quantity hit rectangle is shared with rendering, and long edit strings are clipped to the field.

## Shared details and truthful boundaries

[Headless UI helpers](../../rebuild/src/pe_sandbox_ui_input.c) format item name, current/maximum displayed condition (or quantity), first-formula learned status and a qualified camp repair material quote when an actual supported instance can supply it. Shop hover additionally uses that exact row's current transaction unit price, or its actual ineligibility reason. Inventory/character hints describe the modeled operation; supported live-player medicines use the same core capability helper as action-only quickslot dispatch, including Bandage924.

A fresh purchase row has stock metadata but no item instance. Its tooltip uses a **non-owning presentation copy** with full prototype durability, matching the new instances created by inventory insertion. It does not manufacture an instance ID, modify stock or make the goods appear broken because the synthetic row has zero wear. Sell/buyback hover uses the actual owned instance and its actual worn condition. The character's equipped-condition text uses the same display-unit helper as its hover details; camp repair uses its source-specific multiplication helper.

Tooltips cover inventory cells, supported worn/selected equipment, visible shop rows, repair candidate rows and the reserved repair slot. They are suppressed while dragging, editing a quantity, confirming repair, or interacting with another foreground modal. The resolver uses the current visible row and shared hit geometry; it does not retain a copied item owner or stale tooltip cache. Hover does not itself consume gameplay input. Its box accounts for multiline content and is clamped to the viewport independently of the owner window.

The generated content does not yet contain complete localized item-description templates or every source component. Accordingly, this increment does not invent damage/defense/use-effect statistics, suit bonuses, creation descriptions, LifeLimit details or unsupported equipment actions. English labels and the compact layout are native presentation. Raw durability, learned recipes, funds, inventory and repair state remain unchanged by formatting.

## Verification

Four source guard tests pass with zero skips. The focused [quantity/details routes](../../rebuild/tests/test_shop_quantity_details_ui.c) and existing [shop trade regression](../../rebuild/tests/test_sandbox_shop_trade_ui.c) pass optimized. The focused quantity/details target also passes ASan/UBSan with leak detection, including the post-review removed-final-row regression. The new route tests cover real Buy/Sell/Buyback transactions; accept-versus-trade separation; empty, zero, nonnumeric and oversized input; cancellation/focus loss; exact wear/stock changes before and after acceptance, including disappearance of the final selected row; held/release ownership; new/full versus worn display; learned script877; repair quotes; viewport clamping at1280×720,640×480 and320×240; no fabricated durability for877/916/924/925; and healing-consumable924 success, cooldown and empty-shortcut action-only input.

One initial fixture-development run asserted because it incorrectly expected vendor20002 to stock at least three stackable items. Its actual source quantity3 offers are tools. The fixture now exercises a three-tool purchase, and the new test reports ordinary exit1 diagnostics rather than SIGABRT on failures. That development failure is not a runtime game crash or part of the passing evidence.

```sh
python rebuild/tests/test_shop_details_source.py
make -C rebuild shop-quantity-details-ui-test sandbox-shop-trade-ui-test
```

## Isolated staged render check

[render_shop_details_ui.c](../../rebuild/tests/render_shop_details_ui.c) links the actual UI renderer/assets/shader and core, but does not invoke the sandbox host, gameplay frame loop, saves or original game. It initializes one reference-route game fixture and stages source-stock quantity entry, full-condition purchased goods, worn sale details, learned script877 and a genuine reserved repair transaction. The screenshot background intentionally contains no world scene or actors. Asset load succeeds without skips.

[The runner](../../rebuild/tests/run_private_ui_capture.py) starts its own Xvfb child, obtains that child's display number through `-displayfd`, overrides `DISPLAY`, removes inherited `WAYLAND_DISPLAY` and forces software GL/llvmpipe. It never connects the harness to the user's existing display or injects desktop input. Raylib's normal private-window event handling remains internal to this isolated process. The runner terminates its Xvfb child in `finally`; no system package or display configuration is changed.

The dependency is privately extracted under `rebuild/build/private-xvfb`, not installed. This run used the configured repository package `xorg-server-xvfb-21.1.24-1-x86_64.pkg.tar.zst` from `https://stable-mirror.omarchy.org/extra/os/x86_64/`, SHA256 `7f2116f869aedf51eb899dcfee4cf1f3bf6f9f42c71e089dcdbc0907d529e985`. `ldd` resolved every dependency against existing libraries. The renderer uses the existing `build/reference-assets/ui` atlas, fonts and effects.

Reproduce from the repository root, with those recovered assets already installed:

```sh
mkdir -p rebuild/build/private-xvfb rebuild/build/compiler-tmp
curl --fail --location --output rebuild/build/private-xvfb/xvfb.pkg.tar.zst https://stable-mirror.omarchy.org/extra/os/x86_64/xorg-server-xvfb-21.1.24-1-x86_64.pkg.tar.zst
sha256sum rebuild/build/private-xvfb/xvfb.pkg.tar.zst
# Check the hash above before extracting this exact package.
bsdtar -xf rebuild/build/private-xvfb/xvfb.pkg.tar.zst -C rebuild/build/private-xvfb
TMPDIR="$PWD/rebuild/build/compiler-tmp" make -C rebuild shop-details-render-capture
```

The optional Make target compiles only the draw harness, core and UI sources, then invokes the private-display runner. It is not part of ordinary build/test targets. Captures and process logs remain ignored local evidence under `rebuild/build/ui-details-captures`: five named states at1280×720,640×480 and320×240. The first inspection found the script's positive default durability being presented as a real component; the qualification helper and explicit no-condition tests correct that error. It also prompted a higher-contrast instruction line above quantity entry.

The final optional capture target exits0 and writes all15 images after the component and contrast fixes. Inspection confirms the script now shows quantity/learned status without invented durability, fresh equipment shows full condition, worn sale/repair details preserve their actual values, and the quantity instruction is visible. These staged images check panel/tooltip bounds and visible labels. At320×240 the existing scale makes text too small for usability acceptance even though bounds fit; this increment does not redesign overall scaling. Live movement/combat around the UI, actual input ergonomics, equipment animation and original-game visual parity remain outside this draw-only evidence.
