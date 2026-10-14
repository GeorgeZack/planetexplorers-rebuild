# Vendor selling and buyback

October 6, 2026. Source-derived implementation alongside the [wallet owner](TOWN_WALLETS.md); no original or native graphics session was run for this increment. See [source extraction](TOWN_SELL_BUYBACK_SOURCE.md) for original call sites, component selection and generated metadata.

## Runtime contract

`pe_town_trade` owns atomic bag-to-vendor sales and repurchases for the eight archived vendors. Quotes use the Adventure prototype currency and source single-precision condition calculation, truncated per unit before multiplying quantity. Sale debits the vendor and credits the player. Buyback debits the player's condition-adjusted price but credits the vendor's full prototype value, retaining the asymmetry in the active original code. Normal catalog purchases retain their separate offer prices and stock owner.

Unstackable items transfer their existing instance ID and durability. Stack sales remove the selected bag slot's requested quantity, creating a separate buyback entry; repeated sales do not silently merge repurchase rows. Stack buyback uses normal bag merging. Exact-instance shortcuts clear when their item is sold; type shortcuts survive depletion. Equipped primary or shield instances require unequipping before sale. Clothing and deployed objects are not bag sale candidates.

Both purses, all affected inventory slots and the repurchase owner commit together. Insufficient currency, invalid quantities, capacity failures, ownership aliases or signed32 currency overflow cannot partially transfer an item or currency. Unsupported condition-dependent items fail closed; native storage does not yet represent the source LifeLimit health needed to quote some towers.

## Session and persistence

`PeGame.town_buyback` is a bounded transient owner: eight vendors with at most32 entries each. Full lists reject another sale rather than discarding an earlier item. Closing/reopening shops, visiting another vendor, cloning/moving the active game and world replacement retain these entries. A successful load or a new game clears them. Failed loads preserve the active session. Saving alone does not clear live buyback entries.

Source `m_SellInfo` and the shop's repurchase list establish per-vendor, in-memory close/reopen behavior; no buyback serialization owner was found. The explicit successful-load/new-game boundary and32-row limit are native adapters. Existing PECSv17 bytes remain unchanged: player bag/money and vendor purses persist, while sold objects do not reappear after load. Save validation rejects malformed transient ownership before writing. No recovered assets or user saves are altered.

## Shop controls and acceptance

The existing draggable shop gains Buy, Sell and Buyback tabs, eight scrolling rows, quantity controls and an action button. Player and vendor balances are visible. Input remains modal through closing-button release. The buyback panel explains that loading or starting a new game clears its list.

Optimized build/full tests, content and23 CLI checks pass; full core and focused UI ASan/UBSan with leak detection pass. New price-exporter tests pass11/11 with no skips; the broader Python run retains51 optional skips, which are not passes. Exact commands and scope are recorded in [PLAYTEST](../PLAYTEST.md). These October 6 tests did not cover graphical layout or ordinary vendor interaction; the latter is now [observed](#ordinary-native-acceptance). No live hunt completion is implied by them.

When manual testing is available, use a disposable checkpoint for a short native check:

1. Open a vendor and verify all three tabs and both currency labels at the usual window size, then at a smaller size. Drag the panel and scroll long Buy/Sell lists.
2. Try selling selected equipment; verify the unequip message. Unequip a worn item, sell it, close/reopen the shop and buy it back. Check the same wear and the displayed transaction prices.
3. Sell part of a stack and buy back part of it. Verify quantities, purse changes and that conversation digits do not activate a quickslot.
4. Close the shop while holding its close-button press; no world action should occur through release.
5. Save after a sale, then reload: the sale's money/inventory changes remain, but the buyback list clears. This deliberate session boundary is shown in the panel.

Parts of this list are now observed in ordinary native play ([below](#ordinary-native-acceptance)): refusal of an equipped sale; a worn exact-instance sale, close/reopen and buyback with the same wear and prices; a partial stack sale and partial buyback with count and both-purse checks; and a save, then load, that clears buyback while keeping money and inventory. Still unchecked: conversation digits not activating a quickslot (part of item 3), the smaller window size, dragging the panel and scrolling long lists by drag (item 1), and holding the close-button press (item 4).

## Ordinary native acceptance

**October 14, observed in the native production runtime with ordinary input;
no original-game comparison.** Evidence: [manifest, input journal and lossless
crops](../reference/trade-native-20261014/checkpoints.json). Full captures and
saves stay in `rebuild/build/trade-private-20261014-01`.

*Setup.*
- Runtime `e72816b`, binary SHA256 `f5c07847…a8e`, private display `:115`.
- Baseline: the earned food save `food-private-20261014-02/reloaded.pesave`
  (SHA256 `40e47365…e114`), unchanged after the session.
- Starting state: money 237, Xing Zhang's (20028) purse 4480, Wood Shovel 12 as
  instance 10 at full 3553 durability, Wood 279 ×200, and Knife 33 (instance 1)
  equipped.
- No fixture, cheat, teleport, save edit or grant. Sword and Shield were not
  touched.

*Observed, in order:*

| Step | Input | UI result | Saved state (inspector) |
|---|---|---|---|
| Wear | Inventory: right-click the shovel to equip it; hold the left button on the ground for 2.5 s | dig target shown | `dug`: shovel instance 10 at 3543, still equipped; money and purse unchanged |
| Equipped refusal | Shop, Sell tab, select the equipped Wood Shovel, press Sell | row greyed with price `--`; "Unequip this item before selling."; button disabled; 237 / 4480 unchanged | — |
| Unequip | Inventory: right-click the Knife (re-equips it) | Sell row now 41 (tooltip "Sell unit price: 41"); the Knife row is greyed | — |
| Sell | Sell | "Sold 1 Wood Shovel for 41."; 278 / 4439 | — |
| Close/reopen | Close, talk again, Shop, Buyback | Wood Shovel ×1 at 41 still listed | — |
| Buy back | Buy back | "Bought back 1 Wood Shovel for 41."; 237 / 4481 | — |
| Partial sale | Sell tab, Wood, + ×9, Sell | "Sold 10 Wood for 40."; Wood ×190; 277 / 4441 | — |
| Partial buyback | Buyback, Wood row, + ×3, Buy back | "Bought back 4 Wood for 16."; row ×6 remains; 261 / 4457 | — |
| Close/reopen | Close, talk again, Shop, Buyback | Wood ×6 still listed | — |
| Save | Pause → Save | — | `traded`: money 261, purse 4457, Wood 194, shovel instance 10 in slot 425 at 3543, Knife instance 1 equipped |
| Distinguishing sale | Sell 1 Wood | "Sold 1 Wood for 4."; a second, separate buyback row ×1; 265 / 4453 | — |
| Load | Pause → Load, Resume, Shop, Buyback | "No items to buy back."; 261 / 4457; in-game clock back to the saved time | — |
| Re-save | Pause → Load again, then Save while paused | — | `reloaded`: byte-identical to `traded` (SHA256 `21a80c12…1ac0`), identical inspection |

*Prices against the source rule.* With durability 3543 of 3553, the float32
arithmetic gives 42 × (0.8 × 0.997185 + 0.2) = 41.905, truncated to 41; that is
the price shown and charged in both directions. On buyback the vendor was
credited the full base price 42 while the player paid 41: the active source's
asymmetry, observed. Wood was full price 4 each way. Purse arithmetic:
4480 − 41 + 42 − 40 + 16 = 4457, and money 237 + 41 − 41 + 40 − 16 = 261.

*Identity.* The repurchased shovel came back as the same instance (10), in the
same bag slot, with the same 3543 durability. The distinguishing sale created
a separate buyback row rather than merging, as the source does. Only the
inspector reads exact IDs and durability; screenshots show the tooltip's
rounded display ("36 / 36") and are not used for wear.

*Limits.* One vendor (Xing Zhang) only. The dig's 10-point wear is recorded,
not derived here. Unequipping was done by re-equipping the Knife rather than
right-clicking the hand slot. Panel dragging, resizing, close-button holding and
the 32-row limit were not exercised. The Load-clears-buyback boundary is a
native lifecycle adapter; this session observes the adapter, not the original.

*Tooling.* The read-only inspector's schema 4 adds `town_wallets` (stable
index, vendor, balance, added flag). Buyback rows are transient and not
reported.
