#!/usr/bin/env python3
"""Headless source-mask, parameter and GLSL syntax checks; never creates a window."""
import ast,hashlib,json,re,shutil,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CFILE=ROOT/'rebuild/src/pe_training_ui_shader.c'
class Effects(unittest.TestCase):
    def test_shaders_compile_offline(self):
        compiler=shutil.which('glslangValidator')
        if not compiler:self.skipTest('Optional glslangValidator not installed')
        source=CFILE.read_text()
        with tempfile.TemporaryDirectory() as folder:
            for name in ('specular_fs','hologram_fs'):
                match=re.search(r'static const char \*'+name+r'=\n(.*?);\n',source,re.S);self.assertIsNotNone(match)
                glsl=''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',match[1]))
                path=Path(folder)/(name+'.frag');path.write_text(glsl)
                result=subprocess.run([compiler,'-S','frag',str(path)],capture_output=True,text=True)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                self.assertIn('1.0-fragTexCoord.y',glsl)
    def test_original_masks_and_native_footprints(self):
        directory=ROOT/'rebuild/build/reference-assets/ui/effects'
        if not (directory/'manifest.json').exists():self.skipTest('Optional original effects not exported')
        manifest=json.loads((directory/'manifest.json').read_text());self.assertEqual(len(manifest['effects']),13);self.assertEqual(len(manifest['textures']),11)
        for name,texture in manifest['textures'].items():
            original=ROOT/texture['source'];self.assertEqual((directory/name).read_bytes(),original.read_bytes())
            self.assertEqual(hashlib.sha256(original.read_bytes()).hexdigest(),manifest['sources'][texture['source']])
        self.assertEqual(manifest['textures']['effect-RandomVectors.png']['filter_mode'],0)
        self.assertEqual(manifest['textures']['effect-RandomVectors.png']['wrap_mode'],0)
        rows={r['role']:r for r in manifest['effects']};source=CFILE.read_text()
        for role,w,h in [('dialogue',850,150),('frame',844,110),('health',164,30),('map-left',20,208),('map-right',34,208),('hud-scan',830,50),('map-scan',148,208),('inventory',385,578),('storage',385,460),('character',620,602),('replicator',903,492),('mission',1062,552),('storage-scan',378,326)]:
            row=rows[role];self.assertEqual((row['transform']['m_LocalScale']['x'],row['transform']['m_LocalScale']['y']),(w,h))
            key='MainTex' if role.endswith('scan') else 'SpecularMask'
            self.assertIn('"'+row['textures'][key]+'",'+str(w)+','+str(h),source)
        dialogue=rows['dialogue']['fields'];self.assertEqual(dialogue['Border'],{'m_Left':193,'m_Right':16,'m_Top':18,'m_Bottom':18})
        self.assertAlmostEqual(dialogue['Intensity'],.9);self.assertAlmostEqual(dialogue['WaveThreshold'],.45)
        self.assertIn('193,18,16,18',source)
        self.assertEqual(rows['hud-scan']['textures']['RandomTex'],None)
        self.assertEqual(rows['hud-scan']['textures']['AlphabetTex'],None)
if __name__=='__main__':unittest.main()
