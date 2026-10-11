"""Read-only inspector output contract, isolated from optional archived assets."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

STUBS = r'''
#include "pe_game.h"
#include "pe_settlement.h"
#include "pe_adventure_hunt.h"
#include <string.h>
bool pe_settlement_configure(const char *p,char *e,size_t n){(void)p;(void)e;(void)n;return true;}
bool pe_game_load(PeGame *g,const char *p,char *e,size_t n){
    (void)e;(void)n;g->inventory.categorized=strcmp(p,"legacy")!=0;
    g->mode=g->inventory.categorized?PE_GAME_ADVENTURE:PE_GAME_LEGACY;
    int stride=g->inventory.categorized?PE_INVENTORY_CATEGORY_SLOTS:1;
    for(int k=0;k<4;k++)g->inventory.slots[k*stride]=(PeSlot){.item_id=100+k,.count=k+1};
    g->yaw=.5f;g->pitch=-.25f;g->velocity=(PeVec3){1,2,3};g->grounded=true;
    g->quickslots[0]=(PeQuickslot){100,42};return true;
}
void pe_game_destroy(PeGame *g){(void)g;}
int pe_adventure_hunt_return_talk(const PeGame *g){(void)g;return 0;}
float pe_outfit_defense(const PeOutfit *o){(void)o;return 0;}
int pe_inventory_capacity(const PeInventory *p){return p->categorized?PE_INVENTORY_MAX_SLOTS:PE_INVENTORY_SLOTS;}
int pe_inventory_count(const PeInventory *p,int id){
    int n=0;for(int k=0;k<pe_inventory_capacity(p);k++)if(p->slots[k].item_id==id)n+=p->slots[k].count;return n;
}
const PeItemDef *pe_item_def(int id){
    static const PeItemDef d[4]={{.tab=0},{.tab=1},{.tab=2},{.tab=3}};
    return id>=100&&id<=103?&d[id-100]:NULL;
}
'''


class InspectorOutput(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        folder = Path(cls.temp.name)
        stub = folder / 'stubs.c'
        stub.write_text(STUBS)
        cls.binary = folder / 'inspect'
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(ROOT / 'src'),
                        str(ROOT / 'tools/inspect_hunt_save.c'), str(stub), '-o', str(cls.binary)],
                       check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def inspect(self, layout):
        return json.loads(subprocess.check_output([str(self.binary), layout], text=True))

    def test_all_adventure_categories_and_pose(self):
        data = self.inspect('adventure')
        self.assertEqual(data['schema'], 2)
        self.assertTrue(data['inventory_categorized'])
        self.assertEqual(data['inventory_capacity'], 1680)
        self.assertEqual([s['slot'] for s in data['inventory']], [0, 420, 840, 1260])
        self.assertEqual([s['category'] for s in data['inventory']], [0, 1, 2, 3])
        self.assertEqual([s['count'] for s in data['inventory']], [1, 2, 3, 4])
        self.assertEqual((data['yaw'], data['pitch']), (.5, -.25))
        self.assertEqual(data['velocity'], [1, 2, 3])
        self.assertTrue(data['grounded'])
        self.assertEqual(data['quickslots'][0], {'key': 1, 'item_id': 100, 'instance_id': 42})

    def test_legacy_layout_remains_flat(self):
        data = self.inspect('legacy')
        self.assertFalse(data['inventory_categorized'])
        self.assertEqual(data['inventory_capacity'], 32)
        self.assertEqual([s['slot'] for s in data['inventory']], [0, 1, 2, 3])


if __name__ == '__main__':
    unittest.main()
