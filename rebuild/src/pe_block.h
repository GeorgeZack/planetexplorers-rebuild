#ifndef PE_BLOCK_H
#define PE_BLOCK_H

#include "pe_world.h"

#define PE_B45_SCALE 0.5f
#define PE_B45_MAX_VERTICES 36
#define PE_B45_CUBE 1
#define PE_B45_RAMP 2

/* Original two-byte shape/material encoding; xyz are half-meter cell indices.
 * This module supports primitive 1 (cube) and 2 (ramp), not extended blocks. */
typedef struct {
    int x, y, z;
    uint8_t type, material;
} PeB45Cell;

typedef struct {
    bool hit;
    float distance;
    PeVec3 position, normal;
} PeB45Hit;

bool pe_b45_valid(const PeB45Cell *cell);
int pe_b45_material_item(int material);
int pe_b45_item_material(int item_id);
/* Group cells by resource before calling: per-operation ceil(n/4), floor(n/4). */
uint32_t pe_b45_build_cost(uint32_t cell_count);
uint32_t pe_b45_remove_refund(uint32_t cell_count);
/* Unculled, outward-wound triangles. Returns 0 on invalid cell/capacity. */
size_t pe_b45_mesh(const PeB45Cell *cell, PeVertex *vertices, size_t capacity);
/* Point containment includes the shape boundary; AABB overlap excludes contact. */
bool pe_b45_contains(const PeB45Cell *cell, PeVec3 point);
bool pe_b45_overlaps_aabb(const PeB45Cell *cell, PeVec3 minimum, PeVec3 maximum);
/* Direction need not be normalized. Distance/range use world meters.
 * An origin inside the shape returns the exit surface, not a synthetic hit. */
bool pe_b45_raycast(const PeB45Cell *cell, PeVec3 origin, PeVec3 direction,
                    float range, PeB45Hit *hit);

#endif
