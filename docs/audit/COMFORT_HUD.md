# Adventure HUD comfort: fill, face and drop flash

October 14, 2026. **Source-derived and implemented in C; rendered in staged
private-display fixtures only.** No original-game session, desktop input or
ordinary play is claimed. Comfort simulation and its PECSv23 value already
existed ([stamina and comfort recovery](../FEATURE_CONTRACTS.md#health-stamina-and-consumables));
this increment changes presentation only: no gameplay state and no save format.

Before it, every host drew an always-empty `bloodlan_11bg` track and the
prefab's sad `face3` as fixed decorations.

## Source call chain

`UIMainMidCtrl` (`Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/GameMain/UIMainMidCtrl.cs`)
is component 11400156 on `GameMain/ButtomAnchor/MainMid` in `GameMain.prefab`.

- `Update` calls `UpdatePlayerAtrbute()` every frame while
  `GameUI.Instance.mMainPlayer != null`.
- **Slider.** `mComfort_4.sliderValue = maxValue <= 0 ? 0 : Convert.ToSingle(curValue / maxValue)`
  from `Comfort` and `ComfortMax`, with no lerp (HP and stamina are the lerped bars).
- **Fall check**, after the other sliders: `m_CurComfort = Comfort`; if
  `m_CurComfort > m_lastComfort`, store it; else if
  `m_lastComfort - m_CurComfort > m_ComfortFallValue`, set `m_CurvePlay`,
  store it and set `m_CurveTotalTime = 0`; else store it. The comparison is
  with the previous frame only, so small drops never accumulate.
- **Flash.** While `m_CurvePlay`: `m_CurveTotalTime += Time.deltaTime`;
  `mComfortForeground.alpha = mComfortAlphaCurve.Evaluate(m_CurveTotalTime)`;
  stop once that alpha `>= 1`. Nothing else writes the alpha, so it keeps its
  last value.
- **Face.** `sliderValue < 0.2` is `face3`, `< 0.5` is `face2`, otherwise
  `face1`; the sprite and `MakePixelPerfect` change only when the name changes.
- **Hidden HUD.** `GameUIMode.HideMode` calls `mUIMainMidCtrl.Hide()` when
  entering building mode, and `ShowMode` shows it again on return.
  `UIStaticWnd.OnHide` deactivates `mWndCenter` when set, otherwise the
  component's own object. `mWndCenter` is null here, so `MainMid` itself is
  deactivated and `Update` stops. The state freezes while building; a drop of
  more than 10 across that gap flashes on the first frame back.

## Serialized values (`export_comfort_hud.py`, UnityPy)

| Field | Value |
|---|---|
| `m_ComfortFallValue` | 10 |
| `m_lastComfort` | 0 (serialized: the `[SerializeField]` before a blank line applies to it) |
| `m_CurvePlay`, `m_CurveTotalTime` | not serialized: false, 0 |
| `mWndCenter` | null |
| `mComfort_4` (11400300) | `direction` 0, `numberOfSteps` 0, `rawValue` 1; `foreground` is the Transform of 11400042 |
| `mComfortForeground` (11400042) | filled sprite `bloodlan_11`, horizontal, not inverted, fill 1, pivot left, depth 7, white, alpha 1 |
| Background (11400046) | `bloodlan_11bg`, pivot left, depth 5 |
| `mComfortSprite` (11400062) | `face3`, pivot centre, depth 5 |

The comfort Progress Bar is not rotated (unlike hunger's), so the track fills
left to right. From the transform chain the widgets sit at (551,626,64,5) for
the track and (535,620,12,13) for the face on the 1280×720 canvas, which are
the same as the earlier decorations. All three faces are 12×13 with zero padding in
the installed atlas, so `MakePixelPerfect` keeps that size.

`mComfortAlphaCurve` has nine keys, all tangent mode 10, with serialized end
mode 2 on both sides (the internal clamp, per
[the Unity 5 curve audit](UNITY5_CURVE_EVALUATOR.md)):

| Time | 0 | 0.15 | 0.2 | 0.3 | 0.5 | 0.7 | 0.8 | 0.85 | 1.0 |
|---|---|---|---|---|---|---|---|---|---|
| Value | 0.98 | 0.7 | 0.3 | 0.6 | 0 | 0.6 | 0.3 | 0.7 | 1.0 |

The flash dims, pulses twice and recovers over one second. The last segment
rises monotonically to exactly 1.0 at 1.0 s, so play stops on the first frame
whose total time reaches 1.0.

The exporter pins every fragment above in source order, the key digest, the
end modes, the tangent mode, the fall and initial values, the hidden-window
path, the slider-to-foreground binding, the filled-sprite fields and the
widget sprites, pivots and colours. Outputs:
`rebuild/generated/pe_comfort_hud_source.{h,json}`.

## Not traceable in source

- What recreates the HUD on a load or a menu return. A Save/Load menu load
  calls `PeFlowMgr.LoadScene(GameScene, …)`; `GameUI.Awake` instantiates
  `GameMain`, and `GameUI` is not `DontDestroyOnLoad`. But `PeFlowMgr` and
  whatever creates `GameUISystem` are compiled; no C# script or `PeGame.unity`
  references them.
- Whether Pause stops `Time.deltaTime`: `PeGameMgr.gamePause` is compiled.
- The frame on which the hide/show tween deactivates or reactivates `MainMid`,
  and script order relative to the attribute update.
- `UISlider` and `UIFilledSprite` internals (NGUI firstpass assembly).

## Implementation

- **`pe_comfort_hud`** (core, no raylib): `PeComfortHud` holds the last
  comfort, curve time, alpha, slider, flash flag and face. It is presentation
  state owned by the host UI, never by `PeGame`, and never saved.
  `pe_comfort_hud_reset` loads the prefab values (last 0, alpha 1, slider 1,
  `face3`). `pe_comfort_hud_frame(hud, comfort, max, dt)` follows the source
  order exactly, including the restart from time 0 within the same frame and
  keeping the last alpha. Invalid input or state returns false and changes
  nothing.
- **Sandbox host.** `PeSandboxUI.comfort_hud`. `pe_sandbox_ui_comfort_frame`
  is the single update, called once per display frame after the attribute
  frame and revival and never from drawing. It runs only in Adventure, and only
  while the normal HUD is shown, unpaused and focused. `pe_sandbox_ui_comfort_reset`
  runs at sandbox start and after a load. `pe_sandbox_ui_comfort_view` fills
  the read-only snapshot.
- **Shared drawing.** `pe_ui_hud` draws the track, the left-anchored scissored
  `bloodlan_11` fill at the snapshot's alpha, and the snapshot's face, from
  `pe_training_ui_layout`'s new `comfort` and `comfort_face` rectangles.
  Without a snapshot (tutorial, Legacy, or an invalid face or alpha) it draws
  the empty track and `face3`, unchanged from before.
- **Absent where unsupported.** The tutorial and Legacy have no comfort data;
  their snapshots never set `comfort_available`, and the host never updates the
  state for them.

### Labelled adapters

- **Curve arithmetic.** C evaluates these nine keys with the same explicit
  float Hermite polynomial and clamped ends as `pe_attack_playback_curve`. It is
  a bounded arithmetic adapter for this curve, not separately qualified engine
  output. The attack playback owner is unchanged.
- **Slider clamp.** The slider value is clamped to 0..1, because `UISlider` was
  not decompiled. In the rebuild comfort is already within 0..500.
- **Fill clipping.** The fill uses the shared scissor adapter, as health,
  stamina and hunger do.
- **Alpha conversion.** Alpha reaches the vertex colour as `roundf(alpha*255)`.
- **Lifecycle.** The reset on load, new session and menu return (a fresh
  component, last 0, then one zero-time update so the first drawn frame shows
  the game's value) is a native lifecycle adapter. A load therefore never
  flashes. Freezing while paused or unfocused is also a native adapter.
  Freezing while building is source-derived.

## Verification

- `core:comfort_hud`:
  - the curve passes through every key and clamps both ends;
  - the last segment is monotonic below 1;
  - the first frame never flashes;
  - a drop of exactly 10 does not flash and 10.5 does, from time 0 plus that
    frame;
  - a rise while playing continues, and a second large drop restarts;
  - play stops on the frame whose alpha reaches 1, and alpha keeps 1;
  - a long frame jumps to the clamped end;
  - forty drops of 9.9 never flash;
  - face boundaries are exact at 0.2 and 0.5;
  - a maximum of 0 or less gives slider 0, while the fall check still reads raw
    comfort;
  - invalid input, state or curve time changes nothing.
- `test_comfort_hud_export.py` has 6 tests: 3 source tests run everywhere, and 3
  prefab tests need UnityPy and are skipped by the ordinary interpreter (a skip,
  not a pass). The source tests reject:
  - each comparison operator, the clock source, the restart time, the
    thresholds, face names and the maximum guard;
  - removal of the main-player guard or of `m_lastComfort`'s serialization;
  - the hide path and the base-mode hide;
  - a reordered update.

  The prefab tests regenerate deterministically and reject 19 serialized
  changes, including the slider's foreground binding, a non-filled foreground
  and a changed foreground script. All 6 pass under `check-sources`.
- `test_ui_hud` covers:
  - the layout rectangles;
  - empty track and `face3` without data;
  - fill width from the left, alpha, and each face;
  - slider 0 with no fill;
  - over-range clamping;
  - invalid face or alpha falling back to absent.
- `test_sandbox_ui` covers:
  - reset and the zero-time prime;
  - a 25-point (revival) drop flashing, with the snapshot carrying the curve's
    alpha;
  - an inactive host holding the state exactly, and a flash on return;
  - invalid clocks;
  - reset after a load not flashing;
  - Legacy never updated and absent.
- `ui-shared-render-capture` on a private Xvfb display, at 1280×720, 640×480
  and 320×240:
  - the tutorial and Legacy sandbox HUDs still match exactly (93,170 / 23,320 /
    5,936 pixels);
  - the Adventure hunger capture, with comfort staged at 0 through the host
    reset, matches the tutorial outside the hunger track (92,850 / 23,224 /
    5,902 pixels);
  - the low, mid and high captures (comfort 50/175/400) match the hunger capture
    outside the comfort face and track (92,130 / 23,033 / 5,852 pixels), with
    their fills starting at the track's left end (7, 23 and 52 changed columns
    at 1280×720 for 6.4, 22.4 and 51.2 expected);
  - a flash staged as a 25-point drop 0.2 s earlier (alpha 0.300) differs from
    the steady capture only inside the track: 0 differences elsewhere in 92,850
    / 23,221 / 5,902 pixels, and the face is unchanged.

  Captures: `rebuild/build/ui-shared-captures/sandbox-hud-comfort-*.png`.
  Inspected visually at 1280×720 and 640×480. At 320×240 the face is 3×3
  pixels and `face2` is pixel-identical to `face3` (`face1` differs); fills
  remain visible.

**Not checked:** ordinary play (for example a revival flash in the running
game), an original-game screenshot, the original's behaviour across a load or
a pause, and engine-qualified curve values.
