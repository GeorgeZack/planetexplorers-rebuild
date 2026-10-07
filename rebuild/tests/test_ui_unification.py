#!/usr/bin/env python3
"""Source ownership guards for shared tutorial/sandbox presentation.

These enforce the chosen module boundary, not rendered correctness. Pair them
with the C draw-call/input fixtures and the two-host pixel render fixture.
No graphics, input, original-game launch, or generated assets are needed.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*')


def c_code(text, strings=True):
    """Ignore comments; optionally blank quoted tokens while keeping offsets."""
    def replace(match):
        token = match.group()
        return token if strings and token[0] in "\"'" else re.sub(r"[^\n]", " ", token)
    return TOKEN.sub(replace, text)


def functions(text):
    code = c_code(text, strings=False)
    pattern = re.compile(r"^[ \t]*(?:static\s+)?[A-Za-z_]\w*[ \t*]+([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{", re.M)
    result = {}
    for match in pattern.finditer(code):
        opening = match.end() - 1
        depth = 1
        end = opening + 1
        while end < len(code) and depth:
            depth += (code[end] == "{") - (code[end] == "}")
            end += 1
        if depth:
            raise AssertionError(f"Unclosed function {match[1]}")
        result[match[1]] = code[opening + 1:end - 1]
    return result


def reachable(text, entry):
    bodies = functions(text)
    if entry not in bodies:
        raise AssertionError(f"Missing production entry point {entry}")
    pending, visited = [entry], set()
    while pending:
        name = pending.pop()
        if name in visited:
            continue
        visited.add(name)
        pending.extend(re.findall(r"\b([A-Za-z_]\w*)\s*\(", bodies.get(name, "")))
    return visited


class UIUnification(unittest.TestCase):
    HOSTS = (("pe_training_ui.c", "pe_training_ui_draw"),
             ("pe_sandbox_ui.c", "pe_sandbox_ui_draw"))

    def test_real_draw_entries_reach_shared_owners(self):
        shared = {"pe_ui_hud_draw", "pe_ui_minimap_chrome_draw", "pe_ui_tracker_draw",
                  "pe_ui_draw_inventory_frame", "pe_ui_draw_pagination",
                  "pe_ui_draw_dialogue_frame", "pe_ui_draw_wrap"}
        for filename, entry in self.HOSTS:
            with self.subTest(host=filename):
                reached = reachable((SRC / filename).read_text(), entry)
                self.assertFalse(shared - reached, f"{filename} bypasses shared owners: {shared - reached}")

    def test_common_hud_and_minimap_sprite_literals_have_one_owner(self):
        shared = c_code((SRC / "pe_ui_hud.c").read_text())
        names = ("shortbar1bg", "shortbar1", "bloodlan_6bg", "bloodlan_6",
                 "bloodlan_7bg", "bloodlan_7_1", "map_01", "map_02", "map_03",
                 "mapeye", "maplight1", "NpcTalkHistoryBtn")
        for name in names:
            literal = '"' + name + '"'
            self.assertIn(literal, shared)
            for filename, _ in self.HOSTS:
                with self.subTest(host=filename, sprite=name):
                    self.assertNotIn(literal, c_code((SRC / filename).read_text()),
                                     "Change the shared presentation owner instead of copying chrome into a host")

    def test_host_wrap_helpers_only_delegate(self):
        for filename, _ in self.HOSTS:
            text = (SRC / filename).read_text()
            code = c_code(text, strings=False)
            for name, body in functions(text).items():
                if name in ("wrap", "wrapped", "wrapped_spaced"):
                    with self.subTest(host=filename, function=name):
                        self.assertIn("pe_ui_draw_wrap", reachable(text, name))
                        self.assertNotRegex(body, r"\b(?:for|while|do)\b")
            self.assertNotRegex(code, r"\b(?:strcspn|GetCodepointNext|GetCodepoint|DrawTextCodepoint)\s*\(",
                                "Word/codepoint layout belongs to pe_ui_draw.c")

    def test_mission_button_drawing_and_hit_testing_use_named_geometry(self):
        hud = c_code((SRC / "pe_ui_hud.c").read_text())
        self.assertRegex(hud, r'pe_ui_draw_sprite\([^;]*"Icon_mission"\s*,\s*(?:c->layout|l)->mission_button\s*,')
        body = functions((SRC / "pe_ui_input.c").read_text())["pe_ui_chrome_click"]
        self.assertRegex(body, r"pe_training_ui_contains\(\s*l->mission_button\s*,")
        layout = c_code((SRC / "pe_training_ui_layout.c").read_text())
        self.assertRegex(layout, r"R\(mission_button,right,1254,173,20,26\)")

    def test_both_host_input_paths_use_common_chrome_and_pointer_owners(self):
        tutorial = (SRC / "pe_training_view.c").read_text()
        sandbox = (SRC / "pe_sandbox_ui_input.c").read_text()
        self.assertIn("pe_ui_chrome_click", reachable(tutorial, "pe_training_view"))
        self.assertIn("pe_ui_pointer_route", reachable(tutorial, "pe_training_view"))
        self.assertIn("pe_ui_chrome_click", reachable(sandbox, "pe_sandbox_ui_route"))
        self.assertIn("pe_ui_pointer_route", reachable(sandbox, "pe_sandbox_pointer_route"))
        self.assertIn("pe_ui_pointer_route", reachable(sandbox, "pe_sandbox_pointer_release_gate"))
        host = (SRC / "pe_sandbox_view.c").read_text()
        self.assertIn("pe_sandbox_pointer_route", reachable(host, "pe_sandbox_view"))
        header = c_code((SRC / "pe_sandbox_ui.h").read_text())
        self.assertRegex(header, r"typedef\s+PeUiPointerGate\s+PeSandboxPointerGate\s*;")

    def test_shared_renderer_snapshots_do_not_import_gameplay_owners(self):
        for filename in ("pe_ui_draw.h", "pe_ui_hud.h", "pe_ui_draw.c", "pe_ui_hud.c"):
            code = c_code((SRC / filename).read_text())
            with self.subTest(file=filename):
                self.assertNotRegex(code, r"\b(?:PeGame|PeTraining)\b")
                self.assertNotRegex(code, r'#include\s+"pe_(?:game|training)\.h"')
                self.assertNotRegex(code, r"\bpe_(?:game|training)_(?:step|use|grant|accept|save|load)\w*\s*\(")

    def test_reachability_guard_does_not_accept_an_unused_adapter(self):
        source = 'void unused(void){shared();}\nvoid actual(void){helper();}\nstatic void helper(void){draw();}'
        self.assertNotIn("shared", reachable(source, "actual"))
        self.assertIn("draw", reachable(source, "actual"))


if __name__ == "__main__":
    unittest.main()
