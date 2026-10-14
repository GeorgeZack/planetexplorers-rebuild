#!/usr/bin/env python3
"""Source/prefab drift tests for the HUD comfort presentation; only disposable copies change."""
from contextlib import redirect_stderr
import copy
import importlib.util
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import export_comfort_hud as e

HAVE_UNITYPY = importlib.util.find_spec('UnityPy') is not None
GENERATED = e.ROOT / 'rebuild/generated'


class SourceTests(unittest.TestCase):
    def mutate(self, relative, old, new):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in e.SOURCE_FILES[1:]:
                path = root / name; path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(e.ROOT / name, path)
            path = root / relative; data = path.read_bytes(); self.assertIn(old, data)
            path.write_bytes(data.replace(old, new, 1))
            with self.assertRaises(ValueError):
                e.validate_sources(root)

    def test_sources_validate(self):
        self.assertRegex(e.validate_sources(), '^[0-9a-f]{32}$')

    def test_call_chain_changes_rejected(self):
        m = e.MAINMID
        for old, new in [
            (b'm_lastComfort - m_CurComfort > m_ComfortFallValue', b'm_lastComfort - m_CurComfort >= m_ComfortFallValue'),
            (b'if (m_CurComfort > m_lastComfort)', b'if (m_CurComfort >= m_lastComfort)'),
            (b'if (mComfortForeground.alpha >= 1f)', b'if (mComfortForeground.alpha > 1f)'),
            (b'm_CurveTotalTime += Time.deltaTime;', b'm_CurveTotalTime += Time.unscaledDeltaTime;'),
            (b'm_CurveTotalTime = 0f;', b'm_CurveTotalTime = 0.1f;'),
            (b'mComfort_4.sliderValue < 0.2f', b'mComfort_4.sliderValue < 0.25f'),
            (b'mComfort_4.sliderValue < 0.5f', b'mComfort_4.sliderValue <= 0.5f'),
            (b'smiliesSpr = "face2";', b'smiliesSpr = "face1";'),
            (b'mComfort_4.sliderValue = maxValue <= 0 ? 0', b'mComfort_4.sliderValue = maxValue < 0 ? 0'),
            (b'if (GameUI.Instance.mMainPlayer != null)\n            UpdatePlayerAtrbute();', b'if (false)\n            UpdatePlayerAtrbute();'),
            (b'[SerializeField]\n\n    float m_lastComfort;', b'\n    float m_lastComfort;'),
        ]:
            with self.subTest(old=old):
                self.mutate(m, old, new)
        self.mutate(e.STATIC_WND, b'gameObject.SetActive(false);\n\t\tif (e_OnHide', b'gameObject.SetActive(true);\n\t\tif (e_OnHide')
        self.mutate(e.UI_MODE, b'GameUI.Instance.mUIMainMidCtrl.Hide();\n\t\t}\n\t\telse', b'GameUI.Instance.mUIMainMidCtrl.Show();\n\t\t}\n\t\telse')

    def test_reordered_update_rejected(self):
        # Move the face block ahead of the fall check: same text, different order.
        source = (e.ROOT / e.MAINMID).read_bytes()
        fall = source.index(b'        m_CurComfort = GameUI')
        start = source.index(b'        string smiliesSpr', fall)
        tail = b'            mComfortSprite.MakePixelPerfect();\n        }\n'
        end = source.index(tail, start) + len(tail)
        self.mutate(e.MAINMID, source[fall:end], source[start:end] + source[fall:start])


@unittest.skipUnless(HAVE_UNITYPY, 'UnityPy unavailable (a skip, not a pass)')
class PrefabTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.objects = e.read_prefab(); cls.guid = e.validate_sources()

    def test_generated_outputs_current_and_deterministic(self):
        first = e.build_outputs()
        self.assertEqual(first, e.build_outputs())
        for name, data in first.items():
            self.assertEqual((GENERATED / name).read_bytes(), data)
        recovered = json.loads(first['pe_comfort_hud_source.json'])['comfort_hud']
        self.assertEqual((recovered['fall_value'], recovered['initial_last_comfort'], recovered['initial_alpha']), (10, 0, 1))
        self.assertEqual(recovered['widgets']['background']['rect'], [551, 626, 64, 5])
        self.assertEqual(recovered['widgets']['face']['rect'], [535, 620, 12, 13])
        self.assertEqual([k[1] for k in recovered['curve']['keys']][0::4], [0.9800000190734863, 0.0, 1.0])

    def rejects(self, change):
        objects = copy.deepcopy(self.objects); change(objects)
        with self.assertRaises(ValueError):
            e.recover(objects, self.guid)

    def test_serialized_changes_rejected(self):
        c = lambda o: o[e.COMPONENT]['data']
        def setkey(index, field, value):
            return lambda o: c(o)['mComfortAlphaCurve']['m_Curve'][index].__setitem__(field, value)
        changes = {
            'fall value': lambda o: c(o).__setitem__('m_ComfortFallValue', 12.0),
            'initial last comfort': lambda o: c(o).__setitem__('m_lastComfort', 500.0),
            'curve value': setkey(4, 'value', .1),
            'curve slope': setkey(8, 'inSlope', 0.0),
            'curve tangent mode': setkey(0, 'tangentMode', 0),
            'end mode': lambda o: c(o)['mComfortAlphaCurve'].__setitem__('m_PostInfinity', 1),
            'window centre': lambda o: c(o)['mWndCenter'].__setitem__('m_PathID', 123),
            'flash target': lambda o: c(o)['mComfortForeground'].__setitem__('m_PathID', e.WIDGETS['background']),
            'slider target': lambda o: c(o)['mComfort_4'].__setitem__('m_PathID', 11489644),
            'face target': lambda o: c(o)['mComfortSprite'].__setitem__('m_PathID', 11400042),
            'slider foreground binding': lambda o: o[e.WIDGETS['slider']]['data']['foreground'].__setitem__('m_PathID', 400045),
            'slider steps': lambda o: o[e.WIDGETS['slider']]['data'].__setitem__('numberOfSteps', 5),
            'foreground not filled': lambda o: o[e.WIDGETS['foreground']]['data'].pop('mFillDirection'),
            'foreground inverted': lambda o: o[e.WIDGETS['foreground']]['data'].__setitem__('mInvert', 1),
            'foreground sprite': lambda o: o[e.WIDGETS['foreground']]['data'].__setitem__('mSpriteName', 'bloodlan_12'),
            'foreground script': lambda o: o[e.WIDGETS['foreground']].__setitem__('script_guid', o[e.WIDGETS['background']]['script_guid']),
            'face sprite': lambda o: o[e.WIDGETS['face']]['data'].__setitem__('mSpriteName', 'face1'),
            'colour': lambda o: o[e.WIDGETS['foreground']]['data']['mColor'].__setitem__('a', .5),
            'disabled': lambda o: c(o).__setitem__('m_Enabled', 0),
        }
        for name, change in changes.items():
            with self.subTest(name=name):
                self.rejects(change)

    def test_check_does_not_rewrite_stale_output(self):
        with tempfile.TemporaryDirectory() as directory:
            for name, data in e.build_outputs().items():
                (Path(directory) / name).write_bytes(data)
            self.assertEqual(e.main(['--output', directory, '--check']), 0)
            header = Path(directory) / 'pe_comfort_hud_source.h'; header.write_bytes(b'stale')
            with redirect_stderr(io.StringIO()):
                self.assertEqual(e.main(['--output', directory, '--check']), 1)
            self.assertEqual(header.read_bytes(), b'stale')


if __name__ == '__main__':
    unittest.main()
