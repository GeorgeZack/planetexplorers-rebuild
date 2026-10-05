"""Archive and appearance reconstruction checks; no Unity or graphics required."""
from contextlib import closing
import importlib.util
import json
import math
from pathlib import Path
import sqlite3
import struct
import sys
import tempfile
import unittest

TOOLS=Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0,str(TOOLS))
from read_reference_adventure import Reader, records, decode
from export_adventure_supplier import morph_mesh, tint_pixel


def integer(value):return struct.pack('<i',value)
def string(value):
    data=value.encode();n=len(data);prefix=bytearray()
    while n>=128:prefix.append((n&127)|128);n>>=7
    return bytes(prefix)+bytes([n])+data
def blob(value):return integer(len(value))+value
def nullable(value):return b'\1' if value is None else b'\0'+string(value)
def archive(value):return integer(5)+bytes(range(16))+struct.pack('<q',28+len(value))+value

def fixture(directory,weights=None,entity_id=20016,prototype_id=22,role=2,family='Takayama',given='Eitai'):
    weights=[0.]*27 if weights is None else weights
    name=blob(nullable(None)+nullable(family)+nullable(given))
    appearance=integer(1)+integer(27)+struct.pack('<27f',*weights)+struct.pack('<16f',*([1.]*16))
    nude=integer(150911)+integer(8)+string('Prefab/PlayerPrefab/FemaleModel')
    nude+=b''.join(integer(i)+string('Model/part-'+str(i)) for i in range(8))
    components={'EntityInfoCmpt':integer(0)+name,'AvatarCmpt':string('Prefab/PlayerPrefab/FemaleModel')+integer(0)+blob(appearance)+blob(nude),
                'EquipmentCmpt':integer(1)+integer(1)+integer(66),'PeTrans':integer(1)+struct.pack('<7f',1,2,3,0,0,0,1)}
    entity=integer(2)+integer(1)+integer(prototype_id)+integer(len(components))+b''.join(string(k)+blob(v) for k,v in components.items())
    creature=integer(0)+blob(integer(0)+integer(1)+integer(entity_id)+string('EntityNpc')+blob(entity))
    items=blob(integer(66))+integer(1)+blob(integer(97)+integer(66)+blob(b''))+integer(0)
    rows={'ArchiveKeyCreature':creature,'ArchiveKeyItemAsset':items};offset=28;index=integer(len(rows));world=b''
    for key,value in rows.items():
        index+=string(key)+string('world')+b'\0'+struct.pack('<qq',offset,len(value));offset+=len(value);world+=value
    (directory/'index.arc').write_bytes(archive(index));(directory/'world.arc').write_bytes(archive(world))
    db=directory/'content.sqlite'
    with closing(sqlite3.connect(db)) as connection:
        connection.execute('CREATE TABLE PrototypeItem(id,_modelPath,sex,_replacePos)')
        connection.execute('INSERT INTO PrototypeItem VALUES(97,\'Model/armor-torso\',1,2)')
        connection.execute('CREATE TABLE AdRandNPCMission(ID,RNPC_ID)')
        connection.execute('INSERT INTO AdRandNPCMission VALUES(?,?)',(role,prototype_id));connection.commit()
    return db

class SupplierTest(unittest.TestCase):
    def test_archived_identity_clothing_and_no_mutation(self):
        with tempfile.TemporaryDirectory() as temp:
            d=Path(temp);db=fixture(d);before={p.name:p.read_bytes() for p in d.iterdir()}
            a=decode(d,database=db)
            self.assertEqual(a['name'],'Eitai Takayama');self.assertEqual(a['slots'][4],'Model/armor-torso')
            self.assertEqual(a['equipment'][0]['instance_id'],66);self.assertEqual(a['weights'],[0.]*27)
            self.assertEqual(before,{p.name:p.read_bytes() for p in d.iterdir()})
            with self.assertRaisesRegex(ValueError,'Expected archived'):decode(d,20017,db)
    def test_nonfinite_weights_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            d=Path(temp);db=fixture(d,[math.nan]+[0.]*26)
            with self.assertRaisesRegex(ValueError,'Nonfinite'):decode(d,database=db)
    def test_truncated_guid_and_ranges_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            d=Path(temp);fixture(d);path=d/'world.arc';original=path.read_bytes()
            path.write_bytes(original[:-1])
            with self.assertRaisesRegex(ValueError,'length'):records(d)
            changed=bytearray(original);changed[4]^=1;path.write_bytes(changed)
            with self.assertRaisesRegex(ValueError,'GUID'):records(d)
            path.write_bytes(original);index=d/'index.arc';changed=bytearray(index.read_bytes());struct.pack_into('<q',changed,len(changed)-16,len(original)+1);index.write_bytes(changed)
            with self.assertRaisesRegex(ValueError,'range'):records(d)
    def test_bounded_reader(self):
        for data in (b'\x80'*5,b'\xff\xff\xff\xff\x07',b'\x02x',b'\x01\xff'):
            with self.assertRaises(ValueError):Reader(data).string()
        with self.assertRaises(ValueError):Reader(b'\x02').boolean()
        with self.assertRaises(ValueError):Reader(integer(-1)).blob()
    def morph(self):
        return {'m_Shapes':{'channels':[{'frameCount':1,'frameIndex':i} for i in range(2)],'shapes':[{'firstVertex':i,'vertexCount':1,'hasNormals':False} for i in range(2)],'fullWeights':[100,100],
            'vertices':[{'index':0,'vertex':dict(x=v,y=0,z=0),'normal':dict(x=0,y=0,z=0)} for v in (2,-3)]}}
    def test_source_signed_pair_and_abs_weight(self):
        for w,expected,index in ((.5,2.,0),(-.5,-.5,1),(0,1.,1)):
            positions,normals,used=morph_mesh([[1,0,0]],[[0,1,0]],self.morph(),'w_head_08',[w]+[0.]*26)
            self.assertEqual(positions[0][0],expected);self.assertEqual(normals,[[0,1,0]])
            self.assertEqual(len(used),1);self.assertEqual(used[0]['channel'],index)
    def test_malformed_shapes_fail(self):
        raw=self.morph();raw['m_Shapes']['vertices'][0]['index']=1
        with self.assertRaisesRegex(ValueError,'morph vertex'):morph_mesh([[0,0,0]],[[0,1,0]],raw,'head',[1.]*27)
        raw=self.morph();raw['m_Shapes']['fullWeights'][0]=50
        with self.assertRaisesRegex(ValueError,'frame weight'):morph_mesh([[0,0,0]],[[0,1,0]],raw,'head',[1.]*27)
    def test_mask_tint_and_alpha(self):
        self.assertEqual(tint_pixel((200,100,50,80),False,[.5,1,1,1],[.5]*4,0),(100,100,50,80))
        self.assertEqual(tint_pixel((200,100,50,80),True,[.5,1,1,1],[.5]*4,0),(50,50,25,80))
        self.assertEqual(tint_pixel((0,0,0,80),True,[1]*4,[.5]*4,1),(128,128,128,80))

if __name__=='__main__':unittest.main()
