#include "pe_block.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static void close_float(float actual, float expected)
{
    assert(fabsf(actual-expected) < 0.00002f);
}

static double signed_volume(const PeVertex *vertices, size_t count)
{
    double total = 0;
    for (size_t i = 0; i < count; i += 3) {
        PeVec3 a = vertices[i].position;
        PeVec3 b = vertices[i+1].position;
        PeVec3 c = vertices[i+2].position;
        total += (double)a.x*((double)b.y*c.z-(double)b.z*c.y)
               + (double)a.y*((double)b.z*c.x-(double)b.x*c.z)
               + (double)a.z*((double)b.x*c.y-(double)b.y*c.x);
    }
    return total/6.0;
}

static void test_meshes(void)
{
    for (int shape = PE_B45_CUBE; shape <= PE_B45_RAMP; ++shape) {
        for (int rotation = 0; rotation < 4; ++rotation) {
            PeB45Cell cell = {0,0,0,(uint8_t)((shape<<2)|rotation),11};
            PeVertex vertices[PE_B45_MAX_VERTICES];
            size_t count = pe_b45_mesh(&cell, vertices, PE_B45_MAX_VERTICES);
            assert(count == (shape == PE_B45_CUBE ? 36u : 24u));
            close_float((float)signed_volume(vertices,count), shape == PE_B45_CUBE ? .125f : .0625f);
            for (size_t i = 0; i < count; i += 3) {
                PeVec3 normal = vertices[i].normal;
                close_float(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z,1.0f);
                PeVec3 center = {
                    (vertices[i].position.x+vertices[i+1].position.x+vertices[i+2].position.x)/3,
                    (vertices[i].position.y+vertices[i+1].position.y+vertices[i+2].position.y)/3,
                    (vertices[i].position.z+vertices[i+1].position.z+vertices[i+2].position.z)/3
                };
                PeVec3 inside = {center.x-normal.x*.0001f,center.y-normal.y*.0001f,center.z-normal.z*.0001f};
                PeVec3 outside = {center.x+normal.x*.0001f,center.y+normal.y*.0001f,center.z+normal.z*.0001f};
                assert(pe_b45_contains(&cell,inside));
                assert(!pe_b45_contains(&cell,outside));
                assert(vertices[i].material == 11);
            }
        }
    }
    /* Exact first source triangle 5,0,1, not an invented unit-cube proxy. */
    PeB45Cell cube = {-2,4,-6,4,12};
    PeVertex vertices[36];
    assert(pe_b45_mesh(&cube,vertices,36) == 36);
    assert(vertices[0].position.x == -1 && vertices[0].position.y == 2.5f && vertices[0].position.z == -2.5f);
    assert(vertices[1].position.x == -1 && vertices[1].position.y == 2 && vertices[1].position.z == -3);
    assert(vertices[2].position.x == -1 && vertices[2].position.y == 2 && vertices[2].position.z == -2.5f);
    assert(pe_b45_contains(&cube,(PeVec3){-.75f,2.25f,-2.75f}));
    assert(!pe_b45_contains(&cube,(PeVec3){-.49f,2.25f,-2.75f}));
    vertices[0].material = 213;
    assert(pe_b45_mesh(&cube,vertices,35) == 0);
    assert(vertices[0].material == 213);
}

static void test_collisions_and_rays(void)
{
    const float heights[4] = {.375f,.125f,.125f,.375f};
    const PeVec3 expected_normals[4] = {{1,1,0},{0,1,-1},{-1,1,0},{0,1,1}};
    for (int rotation = 0; rotation < 4; ++rotation) {
        PeB45Cell ramp = {0,0,0,(uint8_t)(8|rotation),11};
        PeB45Hit hit;
        assert(pe_b45_raycast(&ramp,(PeVec3){.125f,1,.125f},(PeVec3){0,-5,0},2,&hit));
        close_float(hit.position.y,heights[rotation]);
        close_float(hit.distance,1-heights[rotation]);
        close_float(hit.normal.x,expected_normals[rotation].x/sqrtf(2));
        close_float(hit.normal.y,1/sqrtf(2));
        close_float(hit.normal.z,expected_normals[rotation].z/sqrtf(2));
        assert(!pe_b45_raycast(&ramp,(PeVec3){.125f,1,.125f},(PeVec3){0,-1,0},.1f,&hit));
        assert(!hit.hit);
    }
    PeB45Cell ramp = {0,0,0,8,11};
    assert(pe_b45_contains(&ramp,(PeVec3){.1f,.3f,.25f}));
    assert(!pe_b45_contains(&ramp,(PeVec3){.4f,.3f,.25f}));
    assert(pe_b45_overlaps_aabb(&ramp,(PeVec3){.1f,.2f,.1f},(PeVec3){.2f,.3f,.2f}));
    assert(!pe_b45_overlaps_aabb(&ramp,(PeVec3){.3f,.3f,.1f},(PeVec3){.4f,.4f,.2f}));
    /* Touching only a slope, an axis face, or a zero-size box is not overlap. */
    assert(!pe_b45_overlaps_aabb(&ramp,(PeVec3){.25f,.25f,.1f},(PeVec3){.3f,.3f,.2f}));
    assert(!pe_b45_overlaps_aabb(&ramp,(PeVec3){-.2f,.1f,.1f},(PeVec3){0,.2f,.2f}));
    assert(!pe_b45_overlaps_aabb(&ramp,(PeVec3){.1f,.1f,.1f},(PeVec3){.1f,.2f,.2f}));
    PeB45Cell cube = {0,0,0,4,0};
    PeB45Hit hit;
    assert(pe_b45_raycast(&cube,(PeVec3){.25f,.25f,.25f},(PeVec3){1,0,0},1,&hit));
    close_float(hit.distance,.25f);
    close_float(hit.normal.x,1);
    assert(!pe_b45_raycast(&cube,(PeVec3){1,1,1},(PeVec3){0,0,0},1,&hit));
    assert(!pe_b45_raycast(&cube,(PeVec3){NAN,0,0},(PeVec3){1,0,0},1,&hit));
    assert(!pe_b45_contains(&cube,(PeVec3){NAN,0,0}));
    assert(!pe_b45_raycast(&cube,(PeVec3){1,1,1},(PeVec3){1,0,0},-1,&hit));
}

void test_block(void)
{
    test_meshes();
    test_collisions_and_rays();
    const uint32_t counts[] = {0,1,3,4,5,8,UINT32_MAX};
    const uint32_t costs[] = {0,1,1,1,2,2,1073741824};
    const uint32_t refunds[] = {0,0,0,1,1,2,1073741823};
    for (size_t i = 0; i < sizeof(counts)/sizeof(counts[0]); ++i) {
        assert(pe_b45_build_cost(counts[i]) == costs[i]);
        assert(pe_b45_remove_refund(counts[i]) == refunds[i]);
    }
    assert(pe_b45_material_item(0) == 268);
    assert(pe_b45_material_item(11) == 279);
    assert(pe_b45_material_item(12) == 280);
    assert(pe_b45_material_item(9) == -1);
    assert(pe_b45_material_item(-1) == -1);
    assert(pe_b45_material_item(256) == -1);
    assert(pe_b45_item_material(279) == 11);
    assert(pe_b45_item_material(280) == 12);
    assert(pe_b45_item_material(308) == -1);
    assert(pe_b45_item_material(-1) == -1);
    assert(!pe_b45_valid(NULL));
    PeB45Cell invalid = {0,0,0,0,11};
    assert(!pe_b45_valid(&invalid));
    invalid.type = 132;
    assert(!pe_b45_valid(&invalid));
    invalid.type = 4;
    invalid.material = 9;
    assert(!pe_b45_valid(&invalid));
    puts("Block45 source geometry, collisions, rotations and costs: OK");
}
