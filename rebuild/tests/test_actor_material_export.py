#!/usr/bin/env python3
"""Source material selection and sidecar binding; no Unity/runtime required."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('actor_material_export',ROOT/'rebuild/tools/export_actor_materials.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

def saved(**values):return dict(m_Floats=[[{'name':k},v] for k,v in values.items()],m_Colors=[[{'name':'_Color'},{'r':1,'g':1,'b':1,'a':1}]])

class Materials(unittest.TestCase):
    def test_replacement_requires_renderer_descendant(self):
        def ptr(identity,file=0):return dict(m_FileID=file,m_PathID=identity)
        records={1:('Transform',dict(m_GameObject=ptr(101),m_Father=ptr(0))),2:('Transform',dict(m_GameObject=ptr(102),m_Father=ptr(1))),3:('Transform',dict(m_GameObject=ptr(103),m_Father=ptr(0))),4:('SkinnedMeshRenderer',dict(m_GameObject=ptr(102),m_Materials=[ptr(40)])),5:('MeshRenderer',dict(m_GameObject=ptr(103),m_Materials=[ptr(50)]))}
        self.assertEqual(m.replacement_materials(records,dict(m_GameObject=ptr(101))),[(4,40,[2,1])])
        self.assertEqual(m.replacement_materials(records,dict(m_GameObject=ptr(101,1))),[])
        records[2][1]['m_Father']=ptr(3)
        self.assertEqual(m.replacement_materials(records,dict(m_GameObject=ptr(101))),[])
        resolver=m.Resolver.__new__(m.Resolver)
        resolver.cache={('bundle',40):dict(shader='DoubleSideStdShell')}
        resolver.replacements={('bundle',40,4):dict(component_path_id=10)}
        resolver.shader_pairs={'DoubleSideStdShell':'DoubleSideStdEntity'}
        material=dict(asset='bundle',path_id=40)
        self.assertEqual(resolver.material(material,4)['effective_shader'],'DoubleSideStdEntity')
        self.assertNotIn('effective_shader',resolver.material(material,5))
        self.assertNotIn('effective_shader',resolver.material(material))
    def test_active_source_cutoff(self):
        rule=m.policy('AvatarStdEntity','_ALPHATEST_ON _NORMALMAP',saved(_Mode=1,_Cutoff=.5,_AlphaCutOff=.51923078),{})
        self.assertEqual(rule,dict(mode='cutout',cutoff=.5,alpha=1.,two_sided=False))
        hair=m.policy('DoubleSideStdEntity','_ALPHATEST_ON',saved(_Mode=1,_Cutoff=.6),{'baked_color':[1,1,1,.75]})
        self.assertEqual(hair,dict(mode='cutout',cutoff=.6,alpha=.75,two_sided=True))
    def test_opaque_gloss_is_not_opacity(self):
        rule=m.policy('AvatarStdEntity','',saved(_Mode=0,_Cutoff=.5,_AlphaCutOff=.9),{})
        self.assertEqual(rule,dict(mode='opaque',cutoff=0.,alpha=1.,two_sided=False))
    def test_unsupported_is_explicit_legacy(self):
        self.assertEqual(m.policy('unknown','_ALPHATEST_ON',saved(_Mode=1,_Cutoff=.5),{})['mode'],'legacy')
        self.assertEqual(m.policy('Standard','_ALPHABLEND_ON',saved(_Mode=2),{})['mode'],'legacy')
    def test_changed_contract_rejects(self):
        for values,keywords in [(dict(_Mode=1,_Cutoff=.5),''),(dict(_Mode=0),'_ALPHATEST_ON'),(dict(_Mode=1,_Cutoff=float('nan')),'_ALPHATEST_ON'),(dict(_Mode=1,_Cutoff=1.1),'_ALPHATEST_ON')]:
            with self.assertRaises(ValueError):m.policy('AvatarStdEntity',keywords,saved(**values),{})
        self.assertEqual(len(m.check_source()),7)
        for mode in (float('nan'),float('inf'),-1,4):
            with self.assertRaises(ValueError):m.policy('AvatarStdEntity','',saved(_Mode=mode),{})
    def test_exact_row_and_texture_binding(self):
        class Resolver:
            def material(self,material,renderer_id=None):return dict(shader='AvatarStdEntity',keywords='_ALPHATEST_ON',saved=saved(_Mode=1,_Cutoff=.5),material_sha256='source',shader_sha256='shader')
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);scene=root/'recording.peas';scene.write_text('PEAS1\na.peac texture.png\n')
            image=b'exact retained PNG bytes';(root/'texture.png').write_bytes(image)
            part=dict(file='a.peac',texture='texture.png',texture_sha256=hashlib.sha256(image).hexdigest(),material=dict(asset='resources.assets',path_id=42,name='actual source material'))
            manifest=root/'recording-manifest.json';manifest.write_text(json.dumps(dict(parts=[part])))
            text,evidence=m.export_scene(scene,Resolver())
            self.assertEqual(text,f'PEAM1\na.peac texture.png cutout 0.5 1 0 {m.fnv(image):016x}\n')
            self.assertEqual(evidence['parts'][0]['material_path_id'],42)
            (root/'texture.png').write_bytes(image+b' changed')
            with self.assertRaisesRegex(ValueError,'texture hash'):m.export_scene(scene,Resolver())

if __name__=='__main__':unittest.main()
