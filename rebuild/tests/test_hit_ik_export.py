#!/usr/bin/env python3
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_hit_ik as e
class Source(unittest.TestCase):
 def test_sources(self): self.assertEqual(len(e.validate_sources(e.ROOT)),len(e.SOURCES))
 def test_mutations(self):
  for path,old,new in [(e.HIT,'weight * weightScale','weight'),(e.EFFECT,'m_PinWordPos?deltaDir:Vector3.zero','Vector3.zero'),(e.BEAT,'forcePower * (1f + UnityEngine.Random.Range','forcePower * (2f + UnityEngine.Random.Range'),(e.CONTROL,'m_HitReaction.OnModifyOffset','m_MoveEffect.OnModifyOffset')]:
   with self.subTest(path=path),tempfile.TemporaryDirectory() as d:
    root=Path(d)
    for p in e.SOURCES:
     target=root/p;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(e.ROOT/p,target)
    target=root/path;s=e.read_text(target);self.assertIn(old,s);target.write_text(s.replace(old,new,1))
    with self.assertRaises(ValueError): e.validate_sources(root)
@unittest.skipUnless(importlib.util.find_spec('UnityPy'),'optional UnityPy unavailable')
class Prefab(unittest.TestCase):
 @classmethod
 def setUpClass(cls): cls.model=e.read_objects(e.ROOT/e.MODEL);cls.guid=e.guid(e.ROOT,e.PARAM)
 def test_bindings(self):
  data=e.qualify(self.model,self.guid);parts=data['parts'];self.assertEqual(len(parts),16)
  self.assertEqual([t['id'] for t in parts[-1]['transforms']],[400150,400236])
  self.assertEqual(parts[4]['links'][-1]['effector'],parts[4]['links'][-2]['effector'])
  self.assertEqual(sum(l['pin'] for l in parts[0]['links']),2)
 def test_mutations(self):
  for field in ('weightScale','m_Name','m_PartTrans','m_PinWordPos','m_PreInfinity','value'):
   with self.subTest(field=field):
    m=copy.deepcopy(self.model);h=m[e.COMPONENT]['data']['m_HitReaction'];p=h['m_HitParts'][0];l=p['m_Effect']['m_Effects'][0]
    if field=='weightScale':h[field]=.5
    elif field=='m_Name':p[field]='wrong'
    elif field=='m_PartTrans':p[field][0]['m_PathID']=400150
    elif field=='m_PinWordPos':l[field]=0
    elif field=='m_PreInfinity':l['m_ForceDir'][field]=1
    else:l['m_ForceDir']['m_Curve'][0][field]=.123
    with self.assertRaises(ValueError):e.qualify(m,self.guid)
 def test_profile_mutation(self):
  m=copy.deepcopy(self.model);p=next(v['data'] for v in m.values() if v.get('script_guid')==self.guid);p['m_RandomScale']=0
  with self.assertRaises(ValueError):e.qualify(m,self.guid)
 def test_regeneration(self):
  outputs=e.build_outputs();self.assertEqual(outputs,e.build_outputs())
  for name,data in outputs.items():self.assertEqual((e.ROOT/'rebuild/generated'/name).read_bytes(),data)
  data=json.loads(outputs['pe_hit_ik_source.json']);self.assertTrue(all(not Path(s['path']).is_absolute() for s in data['source_files']))
if __name__=='__main__':unittest.main()
