# Numbered conversation responses

Implemented October 6, 2026 as a requested native usability addition, not a claim about original Unity input behavior.

Both sandbox and tutorial show numbered responses and accept a fresh top-row or keypad digit1–9. A single dialogue page shows `1. Continue`; existing Enter, Space and mouse acknowledgment remain available. Sandbox choices follow visible order: an available NPC offer is1 and closing is2; without an offer, closing becomes1. Vendor Shop/Close and Torch Get/Turn/Close use the same numbering. A shop's item purchase list is not a conversation response list. Digits for absent responses do nothing while retaining modal ownership.

`pe_sandbox_ui_dialogue_choice_index` supplies the compact source-choice ordering to both rendering and input. `PeSandboxUIInput.response` is transient and represents1–9, with0 meaning no press. The normal quickslot index comes from the same keyboard event, but conversation routing finishes before gameplay shortcuts. Tutorial input calls `pe_training_dialogue_response`, whose return value owns the whole frame even when acknowledgment closes the final page. The existing held-pointer release gate remains intact.

Selections invoke the existing authoritative dialogue operations. Disabled medicine delivery remains disabled without five Herbal Drinks; only acknowledgment of4100 completes that exchange. Refusal, supply grants, hunt choices and return pages retain their previous transaction rules. Number labels are presentation text; generated source dialogue and saves are unchanged.

The mission window also uses one hunt mission-ID mapping for both title and description:9075 during travel,9126 during combat,9127 for returning, pending4105/4106, and completed reporting. The obsolete claim that reporting/rewards were unavailable was removed. Goals still distinguish returning, rewards already received, and completed Bug Hunt with Town Defense unavailable.

## Verification

- Optimized sandbox UI suite passed, including dynamic close numbering, nine welcome acknowledgments, supply final acknowledgment, both source choices, vendor and nonvendor menus, disabled medicine delivery, final4100 acknowledgment, and input/pointer ownership. The return states3–6 all select9127 for title/description.
- Sandbox UI and isolated training suites passed ASan/UBSan with leak detection enabled. The additional return-description mapping received the subsequent optimized UI check.
- `test_training_ui_input.py` passed both tests under ASan/UBSan. Its harness executes the actual extracted frontend input block with mocked keyboard/mouse queries and real core transactions, checking top-row/keypad selection, absent response9, Enter/mouse fallback, and no quickslot use or construction toggle on the closing frame. A subsequent fresh key press can use the quickslot normally.
- Changed frontends passed strict C11 `-Wall -Wextra -Wpedantic -Werror` syntax checks without launching graphics.

## Native physical check, October 6

The main desktop operator checked both hosts at1280×720. In the tutorial, the [welcome page](../reference/numbered-tutorial-welcome.png) displayed `1. Continue`: absent response9 left it unchanged,1 advanced to Camera Tips, and another1 closed the page. The tutorial session exited0.

In the sandbox, [Naoko's menu](../reference/numbered-sandbox-naoko.png) contained only `1. That's all.` Response2 left the menu open with ten medpacks;1 closed it. [Tammana's menu](../reference/numbered-sandbox-vendor.png) showed `1. Shop` and `2. That's all.` Response1 opened the actual shop. After reopening the NPC menu,2 closed it with its then-current medpack count unchanged at nine.

Escape from the shop closes the entire interaction and returns to the world; it does not reopen the NPC response menu. A subsequent2 used the normal medpack quickslot. Another later2 after an interaction attempt that had not opened a menu also used that quickslot. Neither event is evidence of input leaking through an open or closing conversation. The valid response2 closing check above started with a visibly reopened menu.

These physical observations cover top-row keys and the listed visible responses. Keypad input, medicine final4100 acknowledgment, other choices and pointer-release ownership retain their separate headless evidence; they were not newly physically tested here. Four real hare kills and hunt completion are outside this conversation check. The sandbox was handed to the user for further manual combat testing; no sandbox exit0 claim is made. Local captures/logs remain under `rebuild/build/hunt-qol-20261006`; see the corresponding [playtest record](../PLAYTEST.md).
