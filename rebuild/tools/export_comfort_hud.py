#!/usr/bin/env python3
"""Export the original Adventure HUD comfort presentation (UIMainMidCtrl) from the
GameMain prefab and its C# without launching Unity: the comfort slider, the
drop-flash alpha curve and the face thresholds."""
import argparse
import hashlib
import json
import math
import re
import sys
from pathlib import Path

from export_content import ROOT, describe_source
from export_starter_outfit import body, uncomment
from export_hit_reaction import f

UI = 'Assets/Scripts/Chenzhi/NewUI/'
PREFAB = UI + 'Resources/Prefab/GameUI/GameMain.prefab'
MAINMID = UI + 'Scripts/PE_GameUI/GameMain/UIMainMidCtrl.cs'
STATIC_WND = UI + 'Scripts/UIComponent/BaseClass/UIStaticWnd.cs'
UI_MODE = UI + 'Scripts/GameUISystem/GameUIMode.cs'
SOURCE_FILES = (PREFAB, MAINMID, MAINMID + '.meta', STATIC_WND, UI_MODE)
COMPONENT = 11400156
WIDGETS = {'slider': 11400300, 'foreground': 11400042, 'background': 11400046, 'face': 11400062}
KEY_FIELDS = ('time', 'value', 'inSlope', 'outSlope')
CURVE_DIGEST = '2a0ccaf14f2f11a4c06cb03a5409d84096a8865b3608421f0dbd9716e9d648a7'
# Reference canvas; ButtomAnchor (side 7) is the bottom centre.
WIDTH, HEIGHT = 1280, 720
FRAGMENTS = {
    'update': 'if(GameUI.Instance.mMainPlayer!=null)UpdatePlayerAtrbute();',
    'slider': 'curValue=GameUI.Instance.mMainPlayer.GetAttribute(AttribType.Comfort);'
              'maxValue=GameUI.Instance.mMainPlayer.GetAttribute(AttribType.ComfortMax);'
              'mComfort_4.sliderValue=maxValue<=0?0:Convert.ToSingle(curValue/maxValue);',
    'fall': 'm_CurComfort=GameUI.Instance.mMainPlayer.GetAttribute(AttribType.Comfort);'
            'if(m_CurComfort>m_lastComfort){m_lastComfort=m_CurComfort;}'
            'elseif(m_lastComfort-m_CurComfort>m_ComfortFallValue){m_CurvePlay=true;m_lastComfort=m_CurComfort;m_CurveTotalTime=0f;}'
            'else{m_lastComfort=m_CurComfort;}',
    'curve': 'if(m_CurvePlay){m_CurveTotalTime+=Time.deltaTime;'
             'mComfortForeground.alpha=mComfortAlphaCurve.Evaluate(m_CurveTotalTime);'
             'if(mComfortForeground.alpha>=1f){m_CurvePlay=false;}}',
    'face': 'stringsmiliesSpr="";if(mComfort_4.sliderValue<0.2f)smiliesSpr="face3";'
            'elseif(mComfort_4.sliderValue<0.5f)smiliesSpr="face2";elsesmiliesSpr="face1";'
            'if(smiliesSpr!=""&&smiliesSpr!=mComfortSprite.spriteName){mComfortSprite.spriteName=smiliesSpr;mComfortSprite.MakePixelPerfect();}',
}
FIELDS = '[SerializeField]floatm_ComfortFallValue=10f;[SerializeField]floatm_lastComfort;floatm_CurComfort;boolm_CurvePlay;floatm_CurveTotalTime;'
HIDE = 'if(mWndCenter!=null)mWndCenter.SetActive(false);elsegameObject.SetActive(false);'
HIDE_MODE = 'if(curUIMode==UIMode.um_base){GameUI.Instance.mUIMainMidCtrl.Hide();}'
SHOW_MODE = 'if(curUIMode==UIMode.um_base){GameUI.Instance.mUIMainMidCtrl.Show();}'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def compact(text):
    return re.sub(r'\s+', '', uncomment(text))


def validate_sources(root=ROOT):
    """The UIMainMidCtrl call chain the C owner reproduces, in source order."""
    read = lambda p: (root / p).read_text(encoding='utf-8-sig', errors='replace')
    mid = read(MAINMID)
    update = body(uncomment(mid), r'void\s+Update\s*\(\s*\)')
    require(FRAGMENTS['update'] in update, 'UIMainMidCtrl.Update no longer calls UpdatePlayerAtrbute for a main player')
    attributes = body(uncomment(mid), r'void\s+UpdatePlayerAtrbute\s*\(\s*\)')
    positions = []
    for name in ('slider', 'fall', 'curve', 'face'):
        require(attributes.count(FRAGMENTS[name]) == 1, 'UIMainMidCtrl comfort source changed: ' + name)
        positions.append(attributes.index(FRAGMENTS[name]))
    require(positions == sorted(positions), 'UIMainMidCtrl comfort order changed')
    require(attributes.count('mComfortForeground') == 2 and attributes.count('m_lastComfort') == 5,
            'UIMainMidCtrl gained another comfort writer')
    require(FIELDS in compact(mid), 'UIMainMidCtrl comfort fields or serialization changed')
    require(HIDE in body(uncomment(read(STATIC_WND)), r'protected\s+override\s+void\s+OnHide\s*\(\s*\)'),
            'UIStaticWnd.OnHide changed')
    mode = uncomment(read(UI_MODE))
    require(HIDE_MODE in body(mode, r'void\s+HideMode\s*\(\s*\)') and SHOW_MODE in body(mode, r'void\s+ShowMode\s*\(\s*\)'),
            'GameUIMode base hide/show changed')
    guid = re.search(r'^guid: ([0-9a-f]{32})$', read(MAINMID + '.meta'), re.M)
    require(guid, 'UIMainMidCtrl meta changed')
    return guid.group(1)


def read_prefab(root=ROOT):
    from export_human_phy import read_objects
    return read_objects(root / PREFAB)


def chain(objects, widget):
    """Local positions from the widget up to ButtomAnchor; every parent unrotated and unscaled."""
    transforms = {v['data']['m_GameObject']['m_PathID']: v['data'] for v in objects.values() if v['kind'] in ('Transform', 'RectTransform')}
    transform = transforms[objects[widget]['data']['m_GameObject']['m_PathID']]
    size = transform['m_LocalScale']; x = y = 0.0; names = []
    while True:
        name = objects[transform['m_GameObject']['m_PathID']]['data']['m_Name']
        if name == 'ButtomAnchor':
            break
        rotation = transform['m_LocalRotation']
        require((rotation['x'], rotation['y'], rotation['z'], rotation['w']) == (0, 0, 0, 1), 'Comfort widget chain is rotated: ' + name)
        if names:
            scale = transform['m_LocalScale']
            require((scale['x'], scale['y']) == (1, 1), 'Comfort widget chain is scaled: ' + name)
        x += transform['m_LocalPosition']['x']; y += transform['m_LocalPosition']['y']; names.append(name)
        parent = transform['m_Father']['m_PathID']
        require(parent, 'Comfort widget is not under ButtomAnchor')
        transform = objects[parent]['data']
    return dict(names=names[::-1], x=x, y=y, width=size['x'], height=size['y'])


def screen_rect(position, pivot):
    """NGUI pivot 3 (left) or 4 (centre) at the bottom-centre anchor of the 1280x720 canvas."""
    require(pivot in (3, 4), 'Unsupported comfort widget pivot')
    left = WIDTH / 2 + position['x'] - (0 if pivot == 3 else position['width'] / 2)
    top = HEIGHT - position['y'] - position['height'] / 2
    return [left, top, position['width'], position['height']]


def recover(objects, guid):
    owners = [k for k, v in objects.items() if v['kind'] == 'MonoBehaviour' and v.get('script_guid') == guid]
    require(owners == [COMPONENT], 'Expected one UIMainMidCtrl in GameMain')
    data = objects[COMPONENT]['data']
    require(data['m_Enabled'] == 1, 'UIMainMidCtrl disabled')
    require(not data['mWndCenter']['m_PathID'], 'UIMainMidCtrl gained a window centre: Hide would no longer stop its Update')
    require(objects[data['m_GameObject']['m_PathID']]['data']['m_Name'] == 'MainMid', 'UIMainMidCtrl moved off MainMid')
    for name, field in (('slider', 'mComfort_4'), ('face', 'mComfortSprite'), ('foreground', 'mComfortForeground')):
        require(data[field]['m_PathID'] == WIDGETS[name], 'UIMainMidCtrl.%s target changed' % field)
    curve = data['mComfortAlphaCurve']
    require(curve['m_PreInfinity'] == 2 and curve['m_PostInfinity'] == 2, 'Comfort alpha curve end mode changed')
    keys = [[k[x] for x in KEY_FIELDS] for k in curve['m_Curve']]
    require(digest(keys) == CURVE_DIGEST, 'Serialized comfort alpha curve changed')
    require(len(keys) >= 2 and all(math.isfinite(v) for k in keys for v in k) and all(a[0] < b[0] for a, b in zip(keys, keys[1:])),
            'Unusable comfort alpha curve')
    require(all(k['tangentMode'] == 10 for k in curve['m_Curve']), 'Comfort alpha curve tangent mode changed')
    fall, last = data['m_ComfortFallValue'], data['m_lastComfort']
    require((fall, last) == (10.0, 0.0), 'Comfort fall value or initial last comfort changed')
    slider = objects[WIDGETS['slider']]['data']
    # UISlider.foreground is the Transform of the filled foreground widget it drives.
    foreground_go = objects[WIDGETS['foreground']]['data']['m_GameObject']['m_PathID']
    foreground_transform = [k for k, v in objects.items() if v['kind'] in ('Transform', 'RectTransform') and v['data']['m_GameObject']['m_PathID'] == foreground_go]
    require(slider['foreground']['m_PathID'] in foreground_transform and len(foreground_transform) == 1, 'Comfort slider drives another foreground')
    require(slider['direction'] == 0 and slider['numberOfSteps'] == 0, 'Comfort slider changed')
    widgets = {}
    for name in ('foreground', 'background', 'face'):
        w = objects[WIDGETS[name]]['data']
        position = chain(objects, WIDGETS[name])
        require(position['names'][:4] == ['MainMid', 'Content', 'CenterInfo', '4_Comfort'], 'Comfort widget moved')
        widgets[name] = dict(id=WIDGETS[name], sprite=w['mSpriteName'], pivot=w['mPivot'], depth=w['mDepth'],
                             color=[w['mColor'][c] for c in 'rgba'], chain=position['names'], anchor_local=[position['x'], position['y']],
                             rect=screen_rect(position, w['mPivot']))
    fg, bg, face = widgets['foreground'], widgets['background'], widgets['face']
    w = objects[WIDGETS['foreground']]['data']
    # A filled sprite (UIFilledSprite) serializes its fill fields; a plain UISprite does not.
    require(all(x in w for x in ('mFillDirection', 'mFillAmount', 'mInvert')), 'Comfort foreground is not a filled sprite')
    require(objects[WIDGETS['foreground']]['script_guid'] != objects[WIDGETS['background']]['script_guid'], 'Comfort foreground script changed')
    require((fg['sprite'], w['mFillDirection'], w['mInvert'], w['mFillAmount']) == ('bloodlan_11', 0, 0, 1.0), 'Comfort foreground changed')
    require(bg['sprite'] == 'bloodlan_11bg' and fg['rect'] == bg['rect'], 'Comfort track changed')
    require(face['sprite'] == 'face3' and face['pivot'] == 4, 'Comfort face changed')
    require(all(x['color'] == [1, 1, 1, 1] for x in widgets.values()), 'Comfort widget colour changed')
    return dict(component=COMPONENT, script_guid=guid, fall_value=fall, initial_last_comfort=last,
                initial_alpha=fg['color'][3], initial_slider=slider['rawValue'], initial_face='face3',
                curve=dict(key_fields=list(KEY_FIELDS), keys=keys, pre_infinity=2, post_infinity=2, tangent_mode=10, sha256=CURVE_DIGEST),
                widgets=widgets)


def header(r):
    keys = r['curve']['keys']; t, face = r['widgets']['foreground']['rect'], r['widgets']['face']['rect']
    text = '/* Generated by tools/export_comfort_hud.py; do not edit. */\n#ifndef PE_COMFORT_HUD_SOURCE_H\n#define PE_COMFORT_HUD_SOURCE_H\n'
    text += '/* UIMainMidCtrl on GameMain/ButtomAnchor/MainMid (component %d). */\n' % r['component']
    text += 'typedef struct { float time, value, in_slope, out_slope; } PeComfortHudKey;\n'
    text += 'typedef struct {\n    float fall_value, initial_last_comfort, initial_alpha, initial_slider, face2_below, face1_from;\n' \
            '    float track[4], face[4];\n} PeComfortHudSource;\n'
    text += 'static const PeComfortHudSource pe_comfort_hud_source = {\n    %s, %s, %s, %s, 0.2f, 0.5f,\n    {%s}, {%s}\n};\n' % (
        f(r['fall_value']), f(r['initial_last_comfort']), f(r['initial_alpha']), f(r['initial_slider']),
        ', '.join(f(v) for v in t), ', '.join(f(v) for v in face))
    text += '/* mComfortAlphaCurve: seconds since the drop to foreground alpha; both ends clamp (serialized 2). */\n'
    text += '#define PE_COMFORT_HUD_SOURCE_KEYS %du\nstatic const PeComfortHudKey pe_comfort_hud_source_keys[PE_COMFORT_HUD_SOURCE_KEYS] = {\n' % len(keys)
    return text + ''.join('    {%s},\n' % ', '.join(f(v) for v in k) for k in keys) + '};\n#endif\n'


def build_outputs(root=ROOT):
    recovered = recover(read_prefab(root), validate_sources(root))
    metadata = dict(schema=1, sources=[describe_source(root / p) for p in SOURCE_FILES], comfort_hud=recovered,
        call_chain=dict(update='UIMainMidCtrl.Update calls UpdatePlayerAtrbute every frame while GameUI.Instance.mMainPlayer is set and MainMid is active.',
            slider='mComfort_4.sliderValue = ComfortMax <= 0 ? 0 : Comfort / ComfortMax; no lerp.',
            fall='m_CurComfort = Comfort; if it is above m_lastComfort, store it; else if m_lastComfort - m_CurComfort > m_ComfortFallValue, start the curve (m_CurvePlay, m_CurveTotalTime = 0) and store it; else store it. The comparison is with the previous frame only.',
            curve='While m_CurvePlay: m_CurveTotalTime += Time.deltaTime; mComfortForeground.alpha = mComfortAlphaCurve.Evaluate(m_CurveTotalTime); stop once that alpha >= 1. The alpha is never otherwise written.',
            face='sliderValue < 0.2 face3, < 0.5 face2, else face1; reassigned with MakePixelPerfect only when the name changes.',
            hide='GameUIMode.HideMode (building) calls mUIMainMidCtrl.Hide; UIStaticWnd.OnHide deactivates MainMid itself because mWndCenter is null, so Update stops until ShowMode.'),
        scope='Source presentation of the Adventure HUD comfort bar, face and drop flash. Not a comfort simulation.',
        limitations=['UISlider and UIFilledSprite are in the compiled NGUI firstpass assembly, not decompiled: the fill is drawn with the shared scissor adapter and the slider value is clamped to 0..1 as an adapter.',
            'AnimationCurve.Evaluate is native; C evaluates these keys with the float Hermite polynomial used for attack playback, a bounded arithmetic adapter not separately qualified for this curve.',
            'Which code recreates GameUI on a load or a menu return is compiled (PeFlowMgr); the C reset on load is a native lifecycle adapter.',
            'PeGameMgr.gamePause is compiled; freezing the presentation clock while paused or unfocused is a native adapter.',
            'Widget alpha reaches the vertex colour through NGUI; C rounds alpha*255.',
            'Rects are the reference 1280x720 canvas; the hide tween\'s exact frame is not modelled.'])
    return {'pe_comfort_hud_source.h': header(recovered).encode(),
            'pe_comfort_hud_source.json': (json.dumps(metadata, indent=2, allow_nan=False) + '\n').encode()}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--check', action='store_true')
    p.add_argument('--output', type=Path, default=ROOT / 'rebuild/generated')
    a = p.parse_args(argv)
    try:
        for name, data in build_outputs().items():
            if a.check:
                require((a.output / name).is_file() and (a.output / name).read_bytes() == data, 'Stale or missing ' + name)
            else:
                a.output.mkdir(parents=True, exist_ok=True); (a.output / name).write_bytes(data)
    except (OSError, ValueError, KeyError, ImportError, IndexError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
