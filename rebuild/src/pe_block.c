/* Source: Block45Constants.cs, Block45Kernel.cs corner/triangle/rotation tables,
 * PEBuildingMan.cs cost rounding, localData.bytes blocktype table.
 * See docs/audit/BLOCKS.md for the implemented subset and original formulas. */
#include "pe_block.h"
#include <math.h>
#include <string.h>

static const float epsilon = 0.000001f;
static const PeVec3 corners[8] = {
    {0,0,0}, {0,0,1}, {1,0,1}, {1,0,0},
    {0,1,0}, {0,1,1}, {1,1,1}, {1,1,0}
};
static const uint8_t cube_triangles[36] = {
    5,0,1, 5,4,0, 4,3,0, 4,7,3, 7,2,3, 7,6,2,
    6,1,2, 6,5,1, 3,1,0, 3,2,1, 4,6,7, 4,5,6
};
static const uint8_t ramp_triangles[24] = {
    5,0,1, 5,4,0, 4,3,0, 5,1,2, 3,1,0, 3,2,1,
    4,2,3, 4,5,2
};

static bool finite_vector(PeVec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static PeVec3 rotate(PeVec3 point, unsigned rotation)
{
    switch (rotation & 3) {
    case 1: return (PeVec3){point.z, point.y, 1.0f-point.x};
    case 2: return (PeVec3){1.0f-point.x, point.y, 1.0f-point.z};
    case 3: return (PeVec3){1.0f-point.z, point.y, point.x};
    default: return point;
    }
}

static PeVec3 rotate_vector(PeVec3 value, unsigned rotation)
{
    switch (rotation & 3) {
    case 1: return (PeVec3){value.z, value.y, -value.x};
    case 2: return (PeVec3){-value.x, value.y, -value.z};
    case 3: return (PeVec3){-value.z, value.y, value.x};
    default: return value;
    }
}

static PeVec3 local_point(const PeB45Cell *cell, PeVec3 point)
{
    PeVec3 relative = {point.x/PE_B45_SCALE-(float)cell->x,
                       point.y/PE_B45_SCALE-(float)cell->y,
                       point.z/PE_B45_SCALE-(float)cell->z};
    return rotate(relative, (4-(cell->type & 3)) & 3);
}

int pe_b45_material_item(int material)
{
    /* blocktype is separate from terrain buildblock and mining resource. */
    static const int items[16] = {
        268,269,270,271,272,273,274,275,276,-1,278,279,280,281,282,1401
    };
    return material >= 0 && material < 16 ? items[material] : -1;
}

int pe_b45_item_material(int item_id)
{
    if (item_id <= 0) return -1;
    for (int material = 0; material < 16; ++material)
        if (pe_b45_material_item(material) == item_id) return material;
    return -1;
}

bool pe_b45_valid(const PeB45Cell *cell)
{
    return cell && ((cell->type >> 2) == PE_B45_CUBE ||
                    (cell->type >> 2) == PE_B45_RAMP) &&
           pe_b45_material_item(cell->material) > 0;
}

uint32_t pe_b45_build_cost(uint32_t count)
{
    return count/4 + (count%4 != 0);
}

uint32_t pe_b45_remove_refund(uint32_t count)
{
    return count/4;
}

size_t pe_b45_mesh(const PeB45Cell *cell, PeVertex *vertices, size_t capacity)
{
    if (!pe_b45_valid(cell) || !vertices) return 0;
    bool cube = (cell->type >> 2) == PE_B45_CUBE;
    size_t count = cube ? 36 : 24;
    if (capacity < count) return 0;
    const uint8_t *indices = cube ? cube_triangles : ramp_triangles;
    for (size_t triangle = 0; triangle < count; triangle += 3) {
        PeVec3 points[3];
        for (size_t j = 0; j < 3; ++j) {
            points[j] = rotate(corners[indices[triangle+j]], cell->type & 3);
        }
        PeVec3 a = {points[1].x-points[0].x, points[1].y-points[0].y, points[1].z-points[0].z};
        PeVec3 b = {points[2].x-points[0].x, points[2].y-points[0].y, points[2].z-points[0].z};
        PeVec3 normal = {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
        float length = sqrtf(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
        for (size_t j = 0; j < 3; ++j) {
            PeVec3 position = {((float)cell->x+points[j].x)*PE_B45_SCALE,
                               ((float)cell->y+points[j].y)*PE_B45_SCALE,
                               ((float)cell->z+points[j].z)*PE_B45_SCALE};
            vertices[triangle+j] = (PeVertex){position, normal, cell->material};
        }
    }
    return count;
}

bool pe_b45_contains(const PeB45Cell *cell, PeVec3 point)
{
    if (!pe_b45_valid(cell) || !finite_vector(point)) return false;
    PeVec3 p = local_point(cell, point);
    if (p.x < -epsilon || p.y < -epsilon || p.z < -epsilon ||
        p.x > 1+epsilon || p.y > 1+epsilon || p.z > 1+epsilon) return false;
    return (cell->type >> 2) == PE_B45_CUBE || p.x+p.y <= 1+epsilon;
}

bool pe_b45_overlaps_aabb(const PeB45Cell *cell, PeVec3 minimum, PeVec3 maximum)
{
    if (!pe_b45_valid(cell) || !finite_vector(minimum) || !finite_vector(maximum) ||
        minimum.x >= maximum.x || minimum.y >= maximum.y || minimum.z >= maximum.z) return false;
    /* A quarter-turn maps an axis-aligned box to another axis-aligned box. */
    PeVec3 a = local_point(cell, minimum);
    PeVec3 b = local_point(cell, maximum);
    PeVec3 low = {fmaxf(0, fminf(a.x,b.x)), fmaxf(0, fminf(a.y,b.y)), fmaxf(0, fminf(a.z,b.z))};
    PeVec3 high = {fminf(1, fmaxf(a.x,b.x)), fminf(1, fmaxf(a.y,b.y)), fminf(1, fmaxf(a.z,b.z))};
    if (high.x-low.x <= epsilon || high.y-low.y <= epsilon || high.z-low.z <= epsilon) return false;
    return (cell->type >> 2) == PE_B45_CUBE || low.x+low.y < 1-epsilon;
}

bool pe_b45_raycast(const PeB45Cell *cell, PeVec3 origin, PeVec3 direction,
                    float range, PeB45Hit *hit)
{
    if (!hit) return false;
    memset(hit, 0, sizeof(*hit));
    if (!pe_b45_valid(cell) || !finite_vector(origin) || !finite_vector(direction) ||
        !isfinite(range) || range < 0) return false;
    float length = sqrtf(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
    if (!isfinite(length) || length <= epsilon) return false;
    direction.x /= length;
    direction.y /= length;
    direction.z /= length;
    PeVec3 p = local_point(cell, origin);
    PeVec3 d = rotate_vector(direction, (4-(cell->type & 3)) & 3);
    d.x /= PE_B45_SCALE;
    d.y /= PE_B45_SCALE;
    d.z /= PE_B45_SCALE;
    const PeVec3 planes[7] = {{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1},{1,1,0}};
    const float bounds[7] = {0,1,0,1,0,1,1};
    int plane_count = (cell->type >> 2) == PE_B45_CUBE ? 6 : 7;
    float entry = -INFINITY, exit = INFINITY;
    PeVec3 entry_normal = {0}, exit_normal = {0};
    for (int i = 0; i < plane_count; ++i) {
        PeVec3 n = planes[i];
        float remaining = bounds[i]-(n.x*p.x+n.y*p.y+n.z*p.z);
        float rate = n.x*d.x+n.y*d.y+n.z*d.z;
        if (fabsf(rate) <= epsilon) {
            if (remaining < -epsilon) return false;
            continue;
        }
        float time = remaining/rate;
        if (rate < 0 && time > entry) {entry = time; entry_normal = n;}
        if (rate > 0 && time < exit) {exit = time; exit_normal = n;}
        if (entry > exit+epsilon) return false;
    }
    float distance = entry >= 0 ? entry : exit;
    if (!isfinite(distance) || distance < 0 || distance > range) return false;
    PeVec3 normal = entry >= 0 ? entry_normal : exit_normal;
    length = sqrtf(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
    normal.x /= length;
    normal.y /= length;
    normal.z /= length;
    hit->hit = true;
    hit->distance = distance;
    hit->normal = rotate_vector(normal, cell->type & 3);
    hit->position = (PeVec3){origin.x+direction.x*distance,
                            origin.y+direction.y*distance,
                            origin.z+direction.z*distance};
    return true;
}
