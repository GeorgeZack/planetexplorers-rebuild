#!/usr/bin/env python3
"""Guard the original history rules supporting the bounded native port.

These source checks do not substitute for model/save/UI behavioral tests.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
UI = ROOT / 'Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI'


def method(text, name):
    match = re.search(r'\b' + name + r'\s*\([^)]*\)\s*\{', text)
    if not match:
        raise AssertionError(f'Missing source method {name}')
    start = end = match.end()
    depth = 1
    while depth and end < len(text):
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    if depth:
        raise AssertionError(f'Unclosed source method {name}')
    return text[start:end - 1]


class HistorySource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model = (UI / 'NGUIScripts/NPCTalkHistroy.cs').read_text(encoding='utf-8-sig')
        cls.view = (UI / 'NpcTalkHistoryWnd/NpcTalkHistoryWnd.cs').read_text(encoding='utf-8-sig')

    def test_retention_and_content_only_identity(self):
        count = int(re.search(r'const int SaveCount\s*=\s*(\d+)', self.model)[1])
        native = (ROOT / 'rebuild/src/pe_conversation_history.h').read_text()
        self.assertEqual(count, int(re.search(r'PE_CONVERSATION_HISTORY_CAPACITY\s+(\d+)', native)[1]))
        body = method(self.model, 'AddHistroy')
        self.assertRegex(body, r'if\(countent == m_Histroy\[i\]\.countent\)\s*return;')
        self.assertLess(body.index('return;'), body.index('m_Histroy.Add(histroy)'))
        self.assertIn('if(m_Histroy.Count > SaveCount)', body)
        self.assertIn('m_Histroy.RemoveAt(0)', body)
        self.assertNotRegex(body, r'if\([^\n]*npcName')

    def test_global_archive_stores_resolved_raw_strings(self):
        self.assertIn('return false;', method(self.model, 'GetYird'))
        body = method(self.model, 'WriteData')
        for token in ('bw.Write(CURRENT_VERSION)', 'bw.Write(m_Histroy.Count)',
                      'bw.Write(m_Histroy[i].npcName)', 'bw.Write(m_Histroy[i].countent)'):
            self.assertIn(token, body)
        self.assertNotIn('Replace', body)
        self.assertIn('m_Histroy.Clear()', method(self.model, 'Clear'))

    def test_capture_on_presentation_and_selected_response(self):
        text = (UI / 'NGUIScripts/UINPCTalk.cs').read_text(encoding='utf-8-sig')
        self.assertEqual(text.count('NPCTalkHistroy.Instance.AddHistroy(mName.text, mContent.text)'), 3)
        choice = method(text, 'OnMutexBtnClick')
        record = 'NPCTalkHistroy.Instance.AddHistroy(MainPlayer.Instance.entity.ExtGetName(), content)'
        self.assertIn(record, choice)
        self.assertLess(choice.index('SetMission('), choice.index(record))

    def test_view_normalization_clear_and_minimap_open(self):
        self.assertIn('countent.Replace("\\n", string.Empty)', method(self.view, 'AddHistroy'))
        self.assertIn('[45D0FF]{0} : [-]\\n[ffffff]{1}[-]', self.view)
        self.assertIn('Invoke("RepostionScroll", 0.2f)', method(self.view, 'Reposition'))
        self.assertIn('m_ScrollBar.scrollValue = 1', method(self.view, 'RepostionScroll'))
        self.assertIn('NPCTalkHistroy.Instance.Clear()', method(self.view, 'OnClearBtn'))
        minimap = (UI / 'GameMain/UIMinMapCtrl.cs').read_text(encoding='latin-1')
        self.assertIn('mNpcTalkHistoryWnd.ChangeWindowShowState()', method(minimap, 'OnNpcTalkHistoryClick'))


if __name__ == '__main__':
    unittest.main()
