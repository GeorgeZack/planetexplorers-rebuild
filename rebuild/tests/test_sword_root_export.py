import copy
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import export_sword_root_motion as tool


def curves():
    return {ident: dict(name=tool.evidence.CLIPS[ident], count=n, duration=duration,
                       begin=begin, rate=30., scale=tool.SCALE,
                       roots={7: [float(i) for i in range(n)], 9: [-float(i) for i in range(n)]})
            for ident, n, duration, begin in zip(tool.IDS, tool.COUNTS, tool.DURATIONS, tool.BEGINS)}


class ExportTests(unittest.TestCase):
    def test_source_knots_and_negative_begin_are_retained(self):
        c = curves(); before = copy.deepcopy(c); data = tool.encode(c)
        self.assertEqual(c, before)
        self.assertEqual(struct.unpack_from('<4sIIId', data), (b'PESR', 1, 777, 4, tool.SCALE))
        offset = 24
        for index, ident in enumerate(tool.IDS):
            n = tool.COUNTS[index]
            self.assertEqual(struct.unpack_from('<IIddd', data, offset),
                             (ident, n, tool.DURATIONS[index], tool.BEGINS[index], 30.))
            offset += 32
            for sample in range(n):
                self.assertEqual(struct.unpack_from('<dd', data, offset), (sample, -sample))
                offset += 16
        self.assertEqual(offset, len(data))

    def test_complete_order_and_exact_metadata(self):
        for keys in [tool.IDS[::-1], tool.IDS[:-1]]:
            c = curves()
            with self.assertRaises(ValueError): tool.encode({i: c[i] for i in keys})
        for key, value in [('count', 35), ('duration', 1.), ('begin', 0.), ('rate', 60.),
                           ('scale', 1.), ('name', 'Attack7')]:
            c = curves(); c[231][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): tool.encode(c)

    def test_no_fabricated_or_invalid_knots(self):
        for value in [float('nan'), float('inf'), -10001., True]:
            c = curves(); c[231]['roots'][7][3] = value
            with self.assertRaises(ValueError): tool.encode(c)
        c = curves(); c[231]['roots'][9].pop()
        with self.assertRaises(ValueError): tool.encode(c)

    def test_incomplete_proof_fails_before_source_asset_access(self):
        for report in [{}, {'ordinary_source_extraction_qualified': False},
                       {'ordinary_source_extraction_qualified': 1}]:
            with patch.object(tool.evidence, 'verify', return_value=report):
                with self.assertRaises(ValueError): tool.build_outputs(Path('/missing-game'), Path('/missing-capture'))


if __name__ == '__main__': unittest.main()
