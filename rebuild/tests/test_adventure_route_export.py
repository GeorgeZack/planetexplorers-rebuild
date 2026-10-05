#!/usr/bin/env python3
"""Route coverage and byte provenance reject fabricated or incomplete terrain."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0, str(TOOLS))
import export_adventure_route as export
import export_adventure_route_walk as walk_export


class RouteTests(unittest.TestCase):
    def fixture(self, root):
        origin = (-32, 0, 32)
        size = (64, 32, 32)
        capture = root/'route.pevx'
        directory = Path(str(capture)+'.chunks')
        directory.mkdir()
        header = struct.Struct('<4sIiiiIII')
        data = bytearray(2*64*32*32)
        keys = export.chunk_keys(origin, size)
        for cx, cy, cz in keys:
            chunk = bytearray(2*32**3)
            for z in range(32):
                for x in range(32):
                    for y in range(5):
                        at = 2*((z*32+y)*32+x)
                        chunk[at:at+2] = bytes((255, 17))
                        dst = 2*((z*32+y)*64+x+(cx+1)*32)
                        data[dst:dst+2] = chunk[at:at+2]
            (directory/f'{cx},{cy},{cz}.pevx').write_bytes(header.pack(b'PEVX', 1, cx*32, cy*32, cz*32, 32, 32, 32)+chunk)
        capture.write_bytes(header.pack(b'PEVX', 1, *origin, *size)+data)
        metadata = dict(schema=1, kind='observed-original-loaded-terrain-route', origin=list(origin), size=list(size),
                        unity='5.2.4f1', stableConsecutiveFramesPerChunk=2, simultaneousSnapshot=False,
                        complete=True, requiredChunks=2, capturedChunks=2, layout='x-fastest,y,z;volume,type',
                        frames={','.join(map(str, p)): 120+i for i, p in enumerate(keys)})
        Path(str(capture)+'.json').write_text(json.dumps(metadata))
        (directory/'manifest.json').write_text(json.dumps(metadata))
        return capture, origin, size, metadata

    def test_complete_negative_coordinate_capture(self):
        with tempfile.TemporaryDirectory(dir=TOOLS.parent/'build') as name:
            capture, origin, size, _ = self.fixture(Path(name))
            metadata, hashes, surface = export.validate_capture(capture, origin, size)
            self.assertEqual(len(hashes), 2)
            self.assertFalse(metadata['simultaneousSnapshot'])
            self.assertEqual(surface['highest_solid_y_max'], 4)
            self.assertEqual(surface['top_solid_columns'], 0)
            self.assertEqual(export.coverage(capture)['missing'], [])
            with self.assertRaisesRegex(ValueError, 'bounds'):
                export.validate_capture(capture)

    def test_missing_unstable_and_changed_chunks_rejected(self):
        with tempfile.TemporaryDirectory(dir=TOOLS.parent/'build') as name:
            capture, origin, size, metadata = self.fixture(Path(name))
            path = Path(str(capture)+'.json')
            metadata['complete'] = False
            path.write_text(json.dumps(metadata))
            with self.assertRaisesRegex(ValueError, 'coverage'):
                export.validate_capture(capture, origin, size)
            metadata['complete'] = True
            metadata['frames']['-1,0,1'] = 0
            path.write_text(json.dumps(metadata))
            with self.assertRaisesRegex(ValueError, 'frame'):
                export.validate_capture(capture, origin, size)
            metadata['frames']['-1,0,1'] = 120
            path.write_text(json.dumps(metadata))
            data = bytearray(capture.read_bytes())
            data[32] ^= 1
            capture.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'differs'):
                export.validate_capture(capture, origin, size)

    def test_surface_clipping_is_reported(self):
        data = bytes((255, 17))*8
        result = export.surface_summary(data, (-10, 16, 20), (2, 2, 2))
        self.assertEqual(result['highest_solid_y_max'], 17)
        self.assertEqual(result['top_solid_columns'], 4)
        self.assertEqual(result['top_solid_examples'][0], (-10, 20))

    def test_walk_evidence_does_not_infer_arrival_or_out_of_crop_positions(self):
        with tempfile.TemporaryDirectory(dir=TOOLS.parent/'build') as name:
            root=Path(name);capture=root/'route.pevx';capture.write_bytes(b'fixture already validated by terrain exporter')
            Path(str(capture)+'.json').write_text(json.dumps(dict(complete=True,kind='observed-original-loaded-terrain-route',
                origin=[-640,0,64],size=[416,64,416],unity='5.2.4f1')))
            walk=root/'walk.jsonl'
            records=[dict(time=1,player=[-600,25,350]),dict(time=2,player=[-640.256,23,394]),dict(time=3,player=[-366,16,124])]
            walk.write_text(''.join(json.dumps(row)+'\n' for row in records))
            evidence,waypoints=walk_export.derive(walk,capture)
            self.assertFalse(evidence['reached_Annot_radius'])
            self.assertIsNone(evidence['arrival_utc'])
            self.assertEqual(len(waypoints),2)
            self.assertEqual(evidence['excluded_samples'][0]['source_record'],1)
            records.append(dict(time=4,player=[-287,22,145]))
            walk.write_text(''.join(json.dumps(row)+'\n' for row in records))
            self.assertTrue(walk_export.derive(walk,capture)[0]['reached_Annot_radius'])
            records[-1]['time']=2
            walk.write_text(''.join(json.dumps(row)+'\n' for row in records))
            with self.assertRaisesRegex(ValueError,'nonmonotonic'):
                walk_export.derive(walk,capture)


if __name__ == '__main__':
    unittest.main()
