#include "pe_terrain_material.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(pe_terrain_source_material(0,PE_MAT_DIRT)==1);
    assert(pe_terrain_source_material(0,PE_MAT_STONE)==17);
    assert(pe_terrain_source_material(0,PE_MAT_GRASS)==3);
    assert(pe_terrain_source_material(1,2)==2); /* Imported2 is not synthetic stone. */
    for(int i=1;i<=84;i++)assert(pe_terrain_source_material(1,(uint8_t)i)==i);
    assert(!pe_terrain_source_material(1,0)&&!pe_terrain_source_material(1,85));
    assert(!pe_terrain_source_material(0,99)&&!pe_terrain_source_material(2,1));
    bool seen[6][16]={0};
    for(int id=1;id<=84;id++) {
        PeTerrainTile t;assert(pe_terrain_material_tile(id,&t));
        int grid=t.tile_size==.5f?2:4;
        int x=(int)(t.tile_x*grid),y=(int)(t.tile_y*grid);
        assert(t.atlas>=0&&t.atlas<6&&x>=0&&x<grid&&y>=0&&y<grid);
        assert(!seen[t.atlas][y*grid+x]);seen[t.atlas][y*grid+x]=true;
        assert(t.repetition==(grid==2?10.0f:3.0f));
        assert(t.inset==(grid==2?.015625f:.03125f));
    }
    PeTerrainTile t,old;memset(&t,0x55,sizeof t);old=t;
    assert(!pe_terrain_material_tile(0,&t)&&!memcmp(&t,&old,sizeof t));
    assert(!pe_terrain_material_tile(85,&t)&&!memcmp(&t,&old,sizeof t));
    assert(!pe_terrain_material_tile(1,NULL));
    assert(pe_terrain_material_tile(64,&t)&&t.atlas==3&&t.tile_x==.75f&&t.tile_y==.75f);
    assert(pe_terrain_material_tile(65,&t)&&t.atlas==4&&t.tile_x==0&&t.tile_y==0);
    assert(pe_terrain_material_tile(68,&t)&&t.atlas==4&&t.tile_x==.5f&&t.tile_y==.5f);
    assert(pe_terrain_material_tile(69,&t)&&t.atlas==5&&t.tile_x==0&&t.tile_y==0);
    assert(pe_terrain_material_tile(84,&t)&&t.atlas==5&&t.tile_x==.75f&&t.tile_y==.75f);
    puts("Terrain palette: source/synthetic separation and all84 atlas tiles PASS");return 0;
}
