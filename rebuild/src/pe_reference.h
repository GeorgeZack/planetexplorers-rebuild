#ifndef PE_REFERENCE_H
#define PE_REFERENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <raylib.h>
#include "pe_actor_scene.h"

#define PE_REFERENCE_MAX_MATERIALS 256u
typedef struct {
    char symbol[16],diffuse[256];
    PeActorMaterial policy;
} PeReferenceMaterial;
typedef struct {
    size_t count;
    PeReferenceMaterial entries[PE_REFERENCE_MAX_MATERIALS];
} PeReferenceMaterials;

/* Optional visual asset import, independent of simulation and save data.
 * Exported OBJ positions already include Unity hierarchy and room placement.
 * Initialize to {0}; load/unload only while a raylib graphics context exists. */
typedef struct {
    Model model;
    BoundingBox bounds;
    size_t triangles;
    bool loaded;
    PeReferenceMaterials *source_materials;
    Shader material_shader;
    int cutoff_location,alpha_location,material_offset;
} PeReferenceModel;

/* Headless validation of the exporter-only OBJ/MTL subset. */
bool pe_reference_validate(const char *obj_path, size_t *triangles,
                           char *error, size_t error_capacity);
/* Optional PERM1 binds OBJ, ordered MTL symbols and exact PNG bytes. Missing
 * metadata returns success with NULL; malformed present data fails closed.
 * Caller owns *out; initialize it to NULL and free it after use. */
bool pe_reference_materials_load(const char *obj_path,PeReferenceMaterials **out,
                                char *error,size_t capacity);
bool pe_reference_load(PeReferenceModel *reference, const char *obj_path,
                       char *error, size_t error_capacity);
void pe_reference_draw(const PeReferenceModel *reference);
void pe_reference_unload(PeReferenceModel *reference);
Camera3D pe_reference_camera(const PeReferenceModel *reference);

#endif
