"""Population identity, lazy male avatar and read-only metadata regressions."""
from contextlib import closing
import copy
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_adventure_population as tool
from read_reference_adventure import Reader,records,decode,DEFAULT_SAVE
from test_adventure_supplier import fixture,integer,string,blob,archive
from export_adventure_settlement import decode_entities


def replace_creature(directory, change):
    rows,_,_=records(directory)
    outer=Reader(rows['ArchiveKeyCreature']);outer.version(0);r=outer.blob();r.version(0);count=r.count()
    entities=[]
    for _ in range(count):
        identity,prefab,data=r.integer(),r.string(),r.blob();data.version(2)
        kind,prototype=data.unpack('ii');components={}
        for _ in range(data.count(128)):
            key,value=data.string(),data.blob().data;components[key]=value
        change(components)
        payload=integer(2)+integer(kind)+integer(prototype)+integer(len(components))+b''.join(string(k)+blob(v) for k,v in components.items())
        entities.append(integer(identity)+string(prefab)+blob(payload))
    rows['ArchiveKeyCreature']=integer(0)+blob(integer(0)+integer(count)+b''.join(entities))
    index=integer(len(rows));world=b''
    for key,value in rows.items():
        import struct
        index+=string(key)+string('world')+b'\0'+struct.pack('<qq',28+len(world),len(value));world+=value
    (directory/'index.arc').write_bytes(archive(index));(directory/'world.arc').write_bytes(archive(world))


def male_components(c):
    a=Reader(c['AvatarCmpt']);a.string();a.version(0);appear=a.blob().data;n=a.blob();n.version(150911);count=n.count();n.string()
    nude=integer(150911)+integer(count)+string('Prefab/PlayerPrefab/MaleModel')+n.take(len(n.data)-n.at)
    c['AvatarCmpt']=string('')+integer(0)+blob(appear)+blob(nude)
    c['CommonCmpt']=b'\0'+integer(2)+integer(1)*4


class PopulationTests(unittest.TestCase):
    def test_source_case_insensitive_mouth_binding_keeps_unknown_for_strict_mapper(self):
        from export_adventure_supplier import source_bone_names
        avatar={'m_TOS':[(1,'Bip01/Head/Bip01 Mouth')]}
        self.assertEqual(source_bone_names(['Bip01 mouth','missing'],avatar),['Bip01 Mouth','missing'])
        avatar['m_TOS'].append((2,'Bip01/Head/Bip01 mouth'))
        with self.assertRaisesRegex(ValueError,'Ambiguous'):source_bone_names(['Bip01 mouth'],avatar)

    def test_lazy_male_nude_model_supplies_gender_without_name_guess(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp);db=fixture(directory,entity_id=20018,prototype_id=24,role=3,given='Naoko',family='Ueda')
            replace_creature(directory,male_components)
            with closing(sqlite3.connect(db)) as c:c.execute('UPDATE PrototypeItem SET sex=2');c.commit()
            before={p.name:p.read_bytes() for p in directory.iterdir()}
            a=decode(directory,20018,db,expected=dict(prototype_id=24,adventure_role=3))
            self.assertEqual((a['name'],a['sex'],a['base_model']),('Naoko Ueda','male','Prefab/PlayerPrefab/MaleModel'))
            self.assertEqual(a['prefab_path'],a['base_model'])
            self.assertEqual(before,{p.name:p.read_bytes() for p in directory.iterdir()})
            with closing(sqlite3.connect(db)) as c:c.execute('UPDATE PrototypeItem SET sex=1');c.commit()
            with self.assertRaisesRegex(ValueError,'Equipment sex'):decode(directory,20018,db,expected=dict(prototype_id=24,adventure_role=3))

    def test_common_sex_mismatch_fails_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp);db=fixture(directory)
            def mismatch(c):c['CommonCmpt']=b'\0'+integer(2)+integer(1)*4
            replace_creature(directory,mismatch)
            with self.assertRaisesRegex(ValueError,'Common sex'):decode(directory,database=db)

    @unittest.skipUnless((Path.home()/'.local/share/planet-explorers-route-reference/PlanetExplorers/GameSave/save2').is_dir() and tool.DEFAULT_OBSERVATION.is_file(), 'observed Annot archive/capture required; skip is not validation')
    def test_annot_extension_preserves_baseline_and_excludes_remote_actor(self):
        save=Path.home()/'.local/share/planet-explorers-route-reference/PlanetExplorers/GameSave/save2'
        base=tool.build_manifest();extended=tool.extended_manifest(save)
        self.assertEqual(extended['npcs'][:13],base['npcs'])
        self.assertEqual({n['entity_id']:n['prototype_id'] for n in extended['npcs'][13:]},tool.ANNOT_IDENTITIES)
        self.assertEqual(len(extended['npcs']),25)
        self.assertTrue(all(n['town_id']==4 and n['hp']>0 for n in extended['npcs'][13:]))
        self.assertEqual([n['entity_id'] for n in extended['extension']['excluded_npcs']],[20020])
        data=json.loads(tool.DEFAULT_OBSERVATION.read_text());data['complete']=False
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'observation.json';path.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError,'complete observed'):tool.extended_manifest(save,observation=path)
        rows,_,_=records(save);entities,_=decode_entities(rows)
        bad=copy.deepcopy(entities[20027]);bad['components']['NpcCmpt']=bad['components']['NpcCmpt'][:-1]
        with self.assertRaises(ValueError):tool.fixed_point(bad)

    @unittest.skipUnless(DEFAULT_SAVE.is_dir(),'preserved original save0 is required; skip is not archive validation')
    def test_actual_population_deterministic_identity_liveness_and_source_poses(self):
        a=tool.build_manifest();b=tool.build_manifest();self.assertEqual(a,b)
        npcs={n['entity_id']:n for n in a['npcs']};self.assertEqual(set(npcs),set(tool.IDENTITIES))
        self.assertEqual(sum(n['sex']=='male' for n in npcs.values()),5)
        self.assertEqual(npcs[20018]['position'],[-277.114990234375,26.5,126.74765014648438])
        self.assertEqual(npcs[20019]['adventure_role'],4)
        self.assertFalse(npcs[20015]['in_starting_crop'])
        self.assertTrue(all(n['hp']>0 for n in npcs.values()))
        header=tool.header(a);self.assertIn('PE_ADVENTURE_POPULATION_COUNT 13',header)
        rows,_,_=records(DEFAULT_SAVE);entities,player=decode_entities(rows)
        bad=copy.deepcopy(entities);bad[20010]['prototype_id']=17
        with mock.patch.object(tool,'decode_entities',return_value=(bad,player)):
            with self.assertRaisesRegex(ValueError,'population differs'):tool.build_manifest()
        bad=copy.deepcopy(entities);bad[20018]['components']['SkAliveEntity']=integer(3)+b'\0'*8
        with mock.patch.object(tool,'decode_entities',return_value=(bad,player)):
            with self.assertRaisesRegex(ValueError,'living'):tool.build_manifest()

if __name__=='__main__':unittest.main()
