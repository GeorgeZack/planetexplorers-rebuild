#!/usr/bin/env python3
import copy
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_shield_controller as tool
from test_shield_layer_inventory import fixture


def controller_fixture():
    c=fixture();c['m_ControllerSize']=1234
    return c


class SafetyTests(unittest.TestCase):
    def test_strip_all_callback_tables_only(self):
        source=controller_fixture();before=copy.deepcopy(source);out=tool.sanitize_controller(source)
        self.assertEqual(source,before)
        self.assertEqual(out['m_Controller'],source['m_Controller'])
        self.assertEqual(out['m_AnimationClips'],source['m_AnimationClips'])
        self.assertEqual(out['m_StateMachineBehaviours'],[])
        self.assertEqual(out['m_StateMachineBehaviourVectorDescription'],{'m_StateMachineBehaviourRanges':[],'m_StateMachineBehaviourIndices':[]})
        self.assertEqual(out['m_Name'],tool.CONTROLLER_NAME)
    def test_explicit_ik_adapter_changes_only_two_flags(self):
        c=controller_fixture();layers=c['m_Controller']['m_LayerArray']
        for layer in layers[:3]:layer['data']={'m_IKPass':False}
        layers[0]['data']['m_IKPass']=True
        c['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'].append(
            {'data':{'m_FullPathID':2858398145,'m_IKOnFeet':True}})
        expected=copy.deepcopy(c);expected['m_Controller']['m_LayerArray'][0]['data']['m_IKPass']=False
        expected['m_Controller']['m_StateMachineArray'][0]['data']['m_StateConstantArray'][-1]['data']['m_IKOnFeet']=False
        self.assertEqual(tool.disable_clone_ik(c),expected)
        self.assertTrue(c['m_Controller']['m_LayerArray'][0]['data']['m_IKPass'])
        with self.assertRaisesRegex(ValueError,'IK changed'):tool.disable_clone_ik(expected)
    def test_unknown_controller_field_fails_closed(self):
        c=controller_fixture();c['futureCallback']=[]
        with self.assertRaisesRegex(ValueError,'Unknown controller'):tool.sanitize_controller(c)
    def test_unknown_callback_table_fails_closed(self):
        c=controller_fixture();c['m_StateMachineBehaviourVectorDescription']['futureCallbacks']=[]
        with self.assertRaisesRegex(ValueError,'Unknown behaviour'):tool.sanitize_controller(c)
    def test_external_clip_reference_rejected(self):
        c=controller_fixture();c['m_AnimationClips'].append({'m_FileID':1,'m_PathID':42})
        with self.assertRaisesRegex(ValueError,'reference'):tool.sanitize_controller(c)
    def test_hidden_controller_reference_rejected(self):
        c=controller_fixture();c['m_Controller']['hidden']={'m_FileID':0,'m_PathID':42}
        with self.assertRaisesRegex(ValueError,'reference'):tool.sanitize_controller(c)
    def test_clip_events_removed_without_motion_changes(self):
        c={'m_Name':'source','m_Events':[{'objectReference':{'m_FileID':1,'m_PathID':44}}],
           'm_MuscleClip':{'data':[1,2,3]},'m_PPtrCurves':[]}
        out=tool.sanitize_clip(c)
        self.assertEqual(out['m_Events'],[]);self.assertEqual(out['m_MuscleClip'],c['m_MuscleClip'])
        self.assertEqual(len(c['m_Events']),1)
    def test_other_clip_reference_rejected(self):
        c={'m_Events':[],'m_PPtrCurves':[{'m_FileID':0,'m_PathID':15}]}
        with self.assertRaisesRegex(ValueError,'retains object'):tool.sanitize_clip(c)
    def test_nan_motion_bits_compare_without_forgiving_changes(self):
        a=struct.unpack('<d',bytes.fromhex('010000000000f87f'))[0]
        b=struct.unpack('<d',bytes.fromhex('020000000000f87f'))[0]
        self.assertTrue(tool.exact_tree({'v':[a]},{'v':[a]}))
        self.assertFalse(tool.exact_tree({'v':[a]},{'v':[b]}))
        self.assertFalse(tool.exact_tree([1.],[1]))


class InstalledTests(unittest.TestCase):
    def test_fresh_safe_bundle_and_readonly_check(self):
        game=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'
        if not importlib.util.find_spec('UnityPy') or not (game/'PE_Client_Data/sharedassets1.assets').is_file():
            self.skipTest('optional UnityPy and installed source required')
        temp_root=Path(__file__).resolve().parents[1]/'build/compiler-tmp';temp_root.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='shield-controller-test-',dir=temp_root) as directory:
            output=Path(directory)/'fresh';manifest=tool.export(game,output)
            self.assertEqual(manifest['validated']['callback_objects'],0)
            self.assertEqual(manifest['validated']['objects'],454)
            self.assertEqual(manifest['validated']['animation_clips'],452)
            self.assertEqual(manifest['source_unique_behaviours'],140)
            self.assertTrue(manifest['clip_prefix_bytes_preserved'])
            self.assertTrue(manifest['controller_graph_masks_states_equal'])
            self.assertFalse(manifest['runtime_load_validated'])
            self.assertEqual(tool.check(game,output),manifest)
            with self.assertRaisesRegex(ValueError,'fresh'):tool.export(game,output)
            noik=Path(directory)/'fresh-noik';m=tool.export(game,noik,disable_ik=True)
            self.assertTrue(m['ik_disabled']);self.assertEqual(len(m['ik_disabled_graph_byte_offsets']),2)
            self.assertFalse(m['controller_graph_masks_states_equal'])
            self.assertEqual(tool.check(game,noik),m)
            p=output/manifest['bundle'];p.write_bytes(p.read_bytes()+b'bad')
            with self.assertRaisesRegex(ValueError,'hash/size'):tool.check(game,output)


if __name__=='__main__':unittest.main()
