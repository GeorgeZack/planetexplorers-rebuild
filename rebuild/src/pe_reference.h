#ifndef PE_REFERENCE_H
#define PE_REFERENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <raylib.h>

/* Optional visual asset import, independent of simulation and save data.
 * Exported OBJ positions already include Unity hierarchy and room placement.
 * Initialize to {0}; load/unload only while a raylib graphics context exists. */
typedef struct {
    Model model;
    BoundingBox bounds;
    size_t triangles;
    bool loaded;
} PeReferenceModel;

/* Headless validation of the exporter-only OBJ/MTL subset. */
bool pe_reference_validate(const char *obj_path, size_t *triangles,
                           char *error, size_t error_capacity);
bool pe_reference_load(PeReferenceModel *reference, const char *obj_path,
                       char *error, size_t error_capacity);
void pe_reference_draw(const PeReferenceModel *reference);
void pe_reference_unload(PeReferenceModel *reference);
Camera3D pe_reference_camera(const PeReferenceModel *reference);

#endif
