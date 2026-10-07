#ifndef PE_CAMERA_QUERY_H
#define PE_CAMERA_QUERY_H
#include "pe_camera_geometry.h"
#include "pe_sandbox_collision.h"
/* Borrowed current snapshot; no saved ownership or clock. Native policy:
 * exact triangle surfaces, inclusive touching, convex-solid interiors,
 * per-collider initial-contact exclusion, no synthetic crop walls or trees.
 * Terrain grouping is one existing native mesh region; construction grouping
 * is one cell/proxy box. These identities are not Unity chunk parity. */
typedef struct { const PeSandboxCollision *collision; } PeCameraQueryContext;
/* Suitable for pe_camera_geometry_evaluate/step; false is backend failure.
 * Unknown static source identities, masks and trigger policies fail closed.
 * Output and all snapshots remain unchanged on failure. */
bool pe_camera_query(void *context,const PeCameraGeometryQuery *request,PeCameraGeometryHit *out);
#endif
