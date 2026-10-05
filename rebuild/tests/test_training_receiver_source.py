#!/usr/bin/env python3
"""Compare compiled native receiver geometry with the optional recovered source fixture."""
import ctypes
import itertools
import json
import math
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'build/reference-assets/building/manifest.json'

class Vec3(ctypes.Structure):
    _fields_ = [('x', ctypes.c_float), ('y', ctypes.c_float), ('z', ctypes.c_float)]

class Ray(ctypes.Structure):
    _fields_ = [('origin', Vec3), ('direction', Vec3), ('range', ctypes.c_float)]

class Vertex(ctypes.Structure):
    _fields_ = [('position', Vec3), ('normal', Vec3), ('material', ctypes.c_uint8)]

class Mesh(ctypes.Structure):
    _fields_ = [('vertices', ctypes.POINTER(Vertex)), ('count', ctypes.c_size_t)]

def vector(v):
    return [v.x, v.y, v.z]

def bounds(points):
    return ([min(p[i] for p in points) for i in range(3)],
            [max(p[i] for p in points) for i in range(3)])

class ReceiverSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not MANIFEST.is_file():
            raise unittest.SkipTest('Optional source/installed receiver manifest has not been exported')
        compiler = shutil.which('cc')
        if not compiler:
            raise unittest.SkipTest('Native C compiler unavailable for cross-fixture test')
        cls.fixture = json.loads(MANIFEST.read_text())
        cls.directory = tempfile.TemporaryDirectory(prefix='pe-receiver-source-')
        cls.addClassCleanup(cls.directory.cleanup)
        library = Path(cls.directory.name) / 'receiver.so'
        # The wrapper frees native allocation through the same C runtime.
        wrapper = Path(cls.directory.name) / 'wrapper.c'
        wrapper.write_text('#include "pe_training_receiver.h"\n#include <stdlib.h>\n'
                           'void receiver_test_free(PeMeshData *m){free(m->vertices);m->vertices=0;m->count=0;}\n')
        result = subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                                 '-shared', '-fPIC', '-I'+str(ROOT/'src'),
                                 str(ROOT/'src/pe_training_receiver.c'), str(wrapper),
                                 '-lm', '-o', str(library)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError('Native receiver cross-fixture build failed:\n'+result.stderr)
        cls.native = ctypes.CDLL(str(library))
        cls.native.pe_training_receiver_ray.argtypes = [ctypes.c_uint, ctypes.POINTER(Ray)]
        cls.native.pe_training_receiver_ray.restype = ctypes.c_bool
        cls.native.pe_training_receiver_mesh.argtypes = [ctypes.POINTER(Mesh)]
        cls.native.pe_training_receiver_mesh.restype = ctypes.c_bool
        cls.native.receiver_test_free.argtypes = [ctypes.POINTER(Mesh)]
        cls.native.receiver_test_free.restype = None

    def test_all_native_rays_match_unique_recovered_queries(self):
        self.assertTrue(self.fixture['source_installed_transforms_match'])
        remaining = list(self.fixture['beams'])
        self.assertEqual(len(remaining), 50)
        for index in range(50):
            ray = Ray()
            self.assertTrue(self.native.pe_training_receiver_ray(index, ctypes.byref(ray)))
            candidates = [i for i, beam in enumerate(remaining)
                          if math.dist(beam['query']['origin'], vector(ray.origin)) < 0.00002
                          and math.dist(beam['query']['direction'], vector(ray.direction)) < 0.000002]
            self.assertEqual(len(candidates), 1, f'Native ray{index} lost its unique source match')
            query = remaining.pop(candidates[0])['query']
            self.assertEqual(query['distance'], ray.range)
            self.assertEqual((query['layer'], query['mask']), (12, 4096))
        self.assertFalse(remaining)

    def test_native_collider_triangles_match_recovered_transforms(self):
        colliders = self.fixture['receiver_colliders']
        self.assertEqual([c['name'] for c in colliders], ['receiver-upright', 'receiver-flat'])
        mesh = Mesh()
        self.assertTrue(self.native.pe_training_receiver_mesh(ctypes.byref(mesh)))
        try:
            self.assertEqual(mesh.count, 72)
            for index, collider in enumerate(colliders):
                self.assertEqual(collider['layer'], 12)
                fields = collider['fields']
                self.assertTrue(fields['m_Enabled'])
                self.assertFalse(fields['m_IsTrigger'])
                center = [fields['m_Center'][a] for a in 'xyz']
                size = [fields['m_Size'][a] for a in 'xyz']
                matrix = collider['world_matrix']
                source = []
                for signs in itertools.product((-1, 1), repeat=3):
                    local = [center[n] + signs[n]*size[n]/2 for n in range(3)]
                    source.append([sum(matrix[k][n]*local[n] for n in range(3))+matrix[k][3]
                                   for k in range(3)])
                native = [vector(mesh.vertices[index*36+j].position) for j in range(36)]
                # Match every actual vertex, not merely counts or a shared constant.
                for point in native:
                    self.assertLess(min(math.dist(point, q) for q in source), 0.000005)
                for point in source:
                    self.assertLess(min(math.dist(point, q) for q in native), 0.000005)
                for a, b in zip(bounds(source), bounds(native)):
                    for x, y in zip(a, b):
                        self.assertAlmostEqual(x, y, delta=0.000005)
        finally:
            self.native.receiver_test_free(ctypes.byref(mesh))

if __name__ == '__main__':
    unittest.main()
