#ifndef PE_UI_ASSETS_H
#define PE_UI_ASSETS_H
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct PeUiAssets PeUiAssets;
/* Original NGUI atlases exported as PEUI1. Owns textures and bitmap fonts. */
PeUiAssets *pe_ui_assets_load(const char *directory,char *error,size_t capacity);
void pe_ui_assets_free(PeUiAssets *assets);
bool pe_ui_sprite(const PeUiAssets *assets,const char *name,Rectangle destination,Color tint,float border_scale);
bool pe_ui_sprite_flip(const PeUiAssets *assets,const char *name,Rectangle destination,Color tint,bool horizontal,bool vertical);
bool pe_ui_sprite_exists(const PeUiAssets *assets,const char *name);
Font pe_ui_font(const PeUiAssets *assets,bool bold);
/* Original 32px bitmap title font; source title widgets may draw it at 37px. */
Font pe_ui_font_heading(const PeUiAssets *assets);
/* Original 14px regular/bold sheets for small labels and item counts. */
Font pe_ui_font_small(const PeUiAssets *assets,bool bold);
/* Original 26px overhead-name sheet; billboard projection determines draw size. */
Font pe_ui_font_name(const PeUiAssets *assets);
#endif
