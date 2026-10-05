#ifndef PE_TRAINING_UI_SHADER_H
#define PE_TRAINING_UI_SHADER_H
#include <raylib.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct PeTrainingUiShader PeTrainingUiShader;
typedef enum {
    PE_UI_EFFECT_DIALOGUE, PE_UI_EFFECT_FRAME, PE_UI_EFFECT_HEALTH,
    PE_UI_EFFECT_MAP_LEFT, PE_UI_EFFECT_MAP_RIGHT,
    PE_UI_EFFECT_HUD_SCAN, PE_UI_EFFECT_MAP_SCAN,
    PE_UI_EFFECT_INVENTORY, PE_UI_EFFECT_STORAGE, PE_UI_EFFECT_CHARACTER,
    PE_UI_EFFECT_REPLICATOR, PE_UI_EFFECT_MISSION, PE_UI_EFFECT_STORAGE_SCAN,
    PE_UI_EFFECT_COUNT
} PeUiEffect;
/* Requires an active GL context. ui_directory/effects contains exported originals. */
PeTrainingUiShader *pe_training_ui_shader_load(const char *ui_directory,char *error,size_t capacity);
void pe_training_ui_shader_free(PeTrainingUiShader *effects);
/* Destination is the effect footprint, not its parent panel. Source logical size
 * remains fixed under UI resolution scaling. Call between base sprites and labels.
 * Restores standard alpha blend, default shader, depth mask and backface culling. */
void pe_training_ui_shader_draw(PeTrainingUiShader *effects,PeUiEffect role,
                               Rectangle destination,float seconds,float alpha,bool refresh);
#endif
