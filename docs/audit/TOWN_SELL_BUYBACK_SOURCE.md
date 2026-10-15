# Vendor selling and buyback: source contract

This October 6 audit is **source-derived**. It did not launch the original or the native game. The active single-player paths answer pricing, transaction ownership and shop reopening without another gameplay capture. Runtime integration and physical acceptance are separate evidence.

## Active call chain

| Behavior | Local source |
|---|---|
| Clicking an ordinary bag item while the shop is open calls `PreSell`; the separate mission package is excluded | [UIItemPackageCtrl.cs](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/GameWnd/UIItemPackageCtrl.cs), `OnLeftMouseCliked`, lines 975–1000; `OnMissionBtn`, line 419 |
| Compute unit value, test vendor funds, debit vendor, remove the bag item, append buyback, credit player | [UIShopWnd.cs](../../Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs), `Sell`, line 187 |
| Select Adventure currency | [ItemData.cs](../../Assets/Scripts/Items/ItemData.cs), `currency`, line 584; database fields loaded around 809–818 |
| Compute condition-adjusted unit price | [ItemObj.cs](../../Assets/Scripts/Items/ItemObj.cs), `priceFactor`, `GetPrice`, `GetSellPrice`, lines 399–420 |
| Connect condition only through Repair; prefer Durability over LifeLimit | Same file, `Repair.Init`, line 1270, and `Repair.GetValue`, line 1335 |
| Charge buyback and return items | [StroyManager.cs](../../Assets/Scripts/Mission/StroyManager.cs), `BuyItem`, line 279; UI `Buy`, line 503 |
| Preserve per-vendor buyback on shop close/reopen | UI `InitNpcShop` around lines 388–397, `OnHide` around 836–857; manager `m_SellInfo`, line 180 |

The source separates ordinary inventory tabs 0–3 from its mission package. `isMission` is not an item prototype tab number. No additional universal sale whitelist, binding restriction or positive-price requirement appears in the inspected path. The original moves equipped items outside ordinary bag ownership; rejecting currently selected native held/shield instances until unequipped is a necessary explicit adapter to the native bag-backed equipment representation.

## Prices and components

Adventure uses `PrototypeItem.currency_value2`; Story uses `currency_value`. This branch depends on scene mode, not the `Money.Digital` flag. The prototype's component class comes from **`_function`**, not its unrelated `_typeId` classification. `ItemObject.LoadItemType` reads `item_class` and uses last-row assignment for duplicate class IDs.

For each item:

```text
factor = 1 if no Repair-supplied range
otherwise factor = (current - min) / (max - min)
unit = truncate_to_int(base * (0.8f * factor + 0.2f))
total = unit * quantity
```

Source float arithmetic occurs before truncation; truncation occurs per item, before quantity multiplication. `Repair.GetValue` selects Durability first, then LifeLimit. Energy/EnergySmall does not supply the sale-price factor. A Durability component alone is insufficient. `OneFloat.CreateValue` supplies minimum zero; Durability obtains its maximum from `_durability`, while LifeLimit uses `HpMax`. Strengthening and general mutable maxima remain outside current native support.

At this audit's checkpoint the catalog had 320 entries: 249 full-price, 69 durability-priced, and two LifeLimit-priced items, 365 and 370. Native state does not model these two towers' HP, so their sale must remain unavailable rather than substituting durability. Torch 10 has Durability but lacks Repair and therefore sells for its full base 56 even when worn. Knife 33 has base 14 and uses durability. Juice 916 and Medpack 925 have base zero; the source permits zero-value sales. Normal purchase prices are separate store values and must not be reused for selling.

[export_town_trade_prices.py](../../rebuild/tools/export_town_trade_prices.py) regenerates [the header](../../rebuild/generated/pe_town_trade_prices.h) and [the provenance manifest](../../rebuild/generated/pe_town_trade_prices.json) from the source database and the current content catalog selection. It guards complete active price/transaction/condition regions, currency and range selection, plus the component-class binding. No UnityPy, engine or recovered runtime asset is required.

## Identity and atomicity

Selling an unstackable (`GetStackMax()==1`) removes and retains the exact `ItemObject`. Selling a stack creates a new source item object, destroys the selected quantity in the player's original stack, sets the sold object's quantity and appends one buyback entry. Repeated sales append separate entries; there is no source merge of buyback entries.

Buying back an unstackable returns the same object, retaining wear. Buying back stackables uses `package.CanAdd` followed by `package.Add(protoId,count)`; the player bag's stack identity is not preserved. A partial buyback decrements the listed quantity; a full buyback removes the entry. The native stack-instance-zero representation is compatible with these source ownership semantics, while durable objects require exact IDs and wear.

There is a deliberate asymmetry in the inspected source: `StroyManager.BuyItem(..., bReduce=false)` charges the player the adjusted sell price, while `UIShopWnd.Buy` credits the vendor **unadjusted prototype currency × quantity**. For example, buying back a worn Knife can credit the vendor more than the player pays. This is source behavior, not an inferred balancing rule.

The original updates some owners before later failure checks and can partially process multi-item purchases. Native all-or-nothing transactions are an explicit safer adapter: preflight item identity/count, capacity, funds, overflow and buyback space before committing inventory, buyback and both balances. Source prices and transaction ownership do not require reproducing partial-failure defects. Zero-value sales still transfer items and occupy buyback space.

## Buyback lifetime and limits

The source `m_SellInfo` dictionary is keyed by NPC entity ID. Opening copies its list into the shop; hiding copies the current list back. Thus ordinary close/reopen and visiting a different vendor preserve separate buyback lists during that manager's lifetime.

A repository-wide search finds `m_SellInfo` only at its declaration and these UI open/close references. No buyback archive owner was established. This does **not** prove engine lifetime behavior across every load or menu transition. A bounded native transient owner that survives panel close/reopen but clears after successful load or a new game is an explicit lifecycle adapter; do not introduce saved buyback records while claiming recovered persistence. A failed load must leave the current session's list intact. A fixed capacity with a clean sale refusal is also a native bound; the inspected original uses an unbounded list.

## Verification

The focused exporter suite checks exact regeneration, all 320 catalog IDs, known digital prices, Repair-only condition binding, Durability-before-LifeLimit precedence, explicit unsupported LifeLimit items, Energy independence, malformed/overflowing metadata, whole-region source drift, tolerated whitespace/comments and read-only stale-output checks. It runs without optional dependencies or skips:

```sh
python3 -m unittest discover -s rebuild/tests -p test_town_trade_export.py -v
python3 rebuild/tools/export_town_trade_prices.py --check
```

These tests verify extraction and source guards. They do not establish shop layout, pointer behavior or an observed sale/buyback in either executable.
