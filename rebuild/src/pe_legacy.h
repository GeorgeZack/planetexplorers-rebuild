#ifndef PE_LEGACY_H
#define PE_LEGACY_H
#include "pe_world.h"
#define PE_LEGACY_CHUNK_AXIS 35
#define PE_LEGACY_CHUNK_BYTES (35u*35u*35u*2u)
typedef struct PeLegacyFile PeLegacyFile;
typedef struct { int lod,sample_step,extent_x,extent_y,extent_z; } PeLegacyInfo;
/* Read-only reader. Error output is optional; outputs remain unchanged on failure.
 * The handle caches a single piece and must not be shared by simultaneous calls. */
PeLegacyFile *pe_legacy_open(const char *path,int lod,char *error,size_t capacity);
void pe_legacy_close(PeLegacyFile *file);
PeLegacyInfo pe_legacy_info(const PeLegacyFile *file);
/* Base-resolution file-local coordinates, aligned to info.sample_step. */
bool pe_legacy_sample(PeLegacyFile *file,int x,int y,int z,PeVoxel *out,char *error,size_t capacity);
/* Chunk coordinates in selected LOD grid; expands uniform data, includes halos.
 * Dense byte order is x-fast {volume,material}, logical coordinate0 at halo1. */
bool pe_legacy_chunk(PeLegacyFile *file,int cx,int cy,int cz,uint8_t out[PE_LEGACY_CHUNK_BYTES],char *error,size_t capacity);
/* LOD0 only; crops adjacent raw samples and preserves all material IDs. */
bool pe_legacy_crop(PeLegacyFile *file,int x,int y,int z,PeWorld *out,char *error,size_t capacity);
/* Locate a terrain surface anchor with local support; returns base coordinates
 * of a solid sample below the surface, useful for crop selection and spawning. */
bool pe_legacy_find_surface(PeLegacyFile *file,int *x,int *y,int *z,char *error,size_t capacity);
#endif
