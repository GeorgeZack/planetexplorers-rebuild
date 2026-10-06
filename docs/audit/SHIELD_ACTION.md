# Shield hold: source rules and headless owner

2026-10-06. This increment adds a **disconnected, transient C action owner**, not ordinary shield input or a rendered blocking pose. No original game, graphics process or desktop input was used. [Masked composition qualification](SHIELD_LAYER_CAPTURE.md) remains the presentation gate; [starter shield ownership](STARTER_SWORD_SHIELD.md) and saved inventory identity remain separate systems.

## Source admission and input

[`MainPlayerCmpt.UpdateOtherAction`](../../Assets/Scripts/PeEntity/Cmpt/MainPlayerCmpt.cs#L409) skips its input calls during free-look or disabled action input. Otherwise its held Block input calls [`Motion_Equip.HoldSheild`](../../Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Equip.cs#L1049): a present shield requests HoldShield while held, or immediately ends the action when released. A skipped input callback is not a release. This owner therefore distinguishes HOLD, RELEASE and NONE.

[`Action_SheildHold.CanDoAction`](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sheild.cs#L21) requires only a non-null shield. It contains no ground, stamina, durability, speed or EquipmentHold requirement. That statement does **not** erase the separate action-manager dependencies.

[`ActionRelationData.LoadActionRelation`](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs#L169) reads masks by column ordinal beginning at column5: `0` means no dependency, `1` requires true and other positive values require false. The full exported row15 in [source metadata](../../rebuild/generated/pe_starter_sword_shield_source.json) requires these26 masks to be false:

- Sprint; Repulsed; Wentfly; Knocked; Death; GetUp; Revive; Step; SwordAttack.
- Sleep; Eat; Gather; PickUpItem; OnVehicle; Glider; Climb; Build; Stuned; Sit.
- Lie; GetOnTrain; Cutscene; Whacked; Cure; Hand; Ride.

Ground, InAir, InWater, Jump, EquipmentHold, Fall and the other zero-valued columns add no admission rule. Database display names such as `HoldEquip`, `GetOnVehicle` and `Leisure` do not rename the enum positions; the runtime follows the ordinal contract. The focused test compares all56 columns, including unconstrained ones, to this existing generated JSON. No duplicate metadata export was introduced.

[`MotionMgrCmpt.DoAction`](../../Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs#L326) checks these dependencies only for a new action. An existing running action calls the empty inherited `ResetAction`, bypassing dependency checks. The headless owner preserves this distinction: setting an incompatible mask alone does not terminate an already running block. An accepted action relation or lifecycle cancellation is an explicit event.

## Relations, execution and effects

Row15 requests `PauseAction=40` (HoldFlashLight), `EndAction=12,58` (Step, Leisure), and `EndImmediately=2` (Sprint). [`RunningAction`](../../Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs#L40) issues these requests before execution, then waits until both EndAction actions stop and its own pause clears. There is no arbitrary timer. The new owner emits those requests once and remains WAITING, with no protective effect, until the caller supplies their resulting running-action state. A caller can apply synchronous relation results and call again with NONE in the same display update; this API invents no delay or scheduling cadence.

Once executing, [`Action_SheildHold.DoAction/Update`](../../Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sheild.cs#L26) sets `SheildBlock`, mask HoldShield, EnableShieldBlock attribute35 and buff30200177. The buff ID comes from the existing generated C header. It sets Sword movement style whenever a movement component exists and **action** EquipmentHold is not running; the action-running flag and EquipmentHold mask are different inputs. Ongoing updates reassert that style condition without mounting another buff.

EndImmediately clears the Animator bool and mask, resets attribute35, cancels buff30200177 and restores the base movement style only when EquipmentHold is not running. Hidden-when-inactive equipment becomes visible on execution and hidden on ending. The C output separates transition effects from the holding snapshot, so repeated key holds cannot repeatedly mount/cancel buffs. It emits style intent, not an invented base style value. Buff30200177 changes reaction attributes70–73 in the source; its ID is **not** an additional shield damage-reduction factor. No buff interpreter or reaction simulation is added here.

Source row14 SwordAttack immediately ends action15. Sprint also ends it; death, model destruction, other accepted relation actions and assigning a null shield have their own cancellation paths. The owner accepts the resulting explicit cancel event rather than treating an unaccepted attack input as an action start. Source `PEAction.EndAction` delegates to EndImmediately, so normal EndAction cancellations of shield hold also clear its effects. Whacked/Repulsed can pause HoldShield; the base PauseAction is empty and the waiting check is separate from an already executing action. This module does not manufacture a blanket stun/reaction policy from those names.

## C contract and deliberate boundaries

[`pe_shield_action.h`](../../rebuild/src/pe_shield_action.h) and [implementation](../../rebuild/src/pe_shield_action.c) own only phase and a transient shield-instance binding. Zero is IDLE; WAITING has admitted relations but no effects; HOLDING exposes the logical block snapshot. There is no gameplay clock, inventory transfer, HP/stamina debit, durability owner, saved flag or graphical dependency.

`pe_shield_action_step` validates all inputs before publishing state/output. It takes source masks and running-action IDs separately, component availability, a held/released/skipped request, and explicit host capability/cancel flags. It returns transition requests and logical holding/style/visibility effects for a future larger host transaction. The caller must commit these with the corresponding action-manager, buff and presentation changes; the helper does not execute callbacks or modify another owner.

Native capability remains false until the caller explicitly qualifies presentation and integration. Capability loss, missing components, instance replacement, load/model teardown and modal/focus policy are host lifecycle concerns, not extra source stamina/ground restrictions. The helper cancels cleanly on capability/component loss or instance replacement; replacement can start on a subsequent held request. A cancel event wins over held input for that call. These are explicit native ownership adapters. In particular, source `Update` can naturally end when its Animator disappears without directly executing EndImmediately; ordinary model destruction separately calls EndImmediately. This module deliberately emits cleanup instead of preserving effects through an incomplete native component teardown. It does not claim to reproduce that unusual missing-component sequence.

Future integration must supply accepted SwordAttack/Sprint/reaction cancellations in source order, use the correct equipment instance, apply/clear the source buff exactly once, and pair the logical state with a qualified composed pose. A holding result alone must not enable unsupported rendered blocking. Neither sandbox nor tutorial currently calls this new owner, and ordinary shield selection/activation remains gated as previously documented.

## Verification

[`test_shield_action.c`](../../rebuild/tests/test_shield_action.c) passes a strict optimized `-Wall -Wextra -Wpedantic -Werror` build and a focused ASan/UBSan run with `detect_leaks=1:halt_on_error=1`; both exit0. The suite requires the committed source metadata, with no asset-dependent skip:

- All56 mask columns match exported row15; each forbidden bit rejects new admission and each unconstrained bit admits it.
- Relations pause40/end12+58/immediate-end2 emit once; both pending end actions and initial pause are respected.
- Continuous holding mounts one buff; NONE preserves the hold; existing actions bypass changed admission masks until explicit cancellation.
- Accepted cancellation wins, clears effects once, resumes the paused action, and allows later held-key retry.
- EquipmentHold action ownership governs style restoration independently of its mask; hidden equipment gets bounded visibility transitions.
- Host capability/component/instance loss, waiting cancellation and malformed-state/input rollback are checked.

These tests establish the headless rule boundary only. They add no manual-play coverage, composed animation qualification, native damage-path integration or complete ActionRelation graph implementation.
