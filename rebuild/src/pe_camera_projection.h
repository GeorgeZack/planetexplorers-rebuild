#ifndef PE_CAMERA_PROJECTION_H
#define PE_CAMERA_PROJECTION_H
#include "pe_world.h"
/* Native combined-depth adapter: original auxiliary near, one rendered pass.
 * Logical GeometryClamp near remains separate from visible/pick near. */
#define PE_CAMERA_RENDER_NEAR .03f
#define PE_CAMERA_RENDER_FAR 4000.f
typedef struct {
    PeVec3 position,forward,up;
    float width,height,fov_degrees,near_clip;
} PeCameraProjection;
typedef struct { PeVec3 origin,direction; } PeCameraRay;
/* Source/display right points opposite conventional OpenGL camera right.
 * Orthonormal result satisfies right cross up = forward. */
bool pe_camera_projection_basis(PeVec3 forward,PeVec3 up,PeVec3 *right,PeVec3 *camera_up);
/* Pixel coordinates and reflected perspective share one basis. Rays start on
 * the visible near plane, including off-axis rays; reach remains caller-owned. */
bool pe_camera_projection_ray(const PeCameraProjection *projection,float x,float y,PeCameraRay *ray);
bool pe_camera_projection_screen(const PeCameraProjection *projection,PeVec3 point,float *x,float *y);
#endif
