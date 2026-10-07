#ifndef PE_VEGETATION_VIEW_H
#define PE_VEGETATION_VIEW_H
#include "pe_game.h"
#include <stdbool.h>
#include <stddef.h>
typedef struct PeVegetationView PeVegetationView;
/* CPU-only source OBJ/material/PNG preflight; no window required. Whole group
 * absence is optional, partial/malformed groups fail. Output unchanged on failure. */
bool pe_vegetation_view_validate(const char *asset_root,bool *available,char *error,size_t capacity);
/* Requires live graphics. Returns an empty optional view for an absent group;
 * an installed gameplay patch must require available()==true. */
PeVegetationView *pe_vegetation_view_load(const char *asset_root,char *error,size_t capacity);
bool pe_vegetation_view_available(const PeVegetationView *view);
bool pe_vegetation_view_draw(const PeVegetationView *view,const PeGame *game);
void pe_vegetation_view_unload(PeVegetationView *view);
#endif
