#ifndef PE_TRAINING_UI_REPLICATOR_H
#define PE_TRAINING_UI_REPLICATOR_H
#include "pe_training.h"
#include "pe_training_ui_layout.h"
#include "pe_ui_assets.h"
#include "pe_training_ui_shader.h"
/* Source CompoundWnd chrome and the one implemented formula. Draw-only;
   does not start/cancel crafting, alter quantities, or consume input. */
void pe_training_ui_replicator_draw(const PeUiAssets *assets,
    const PeTrainingUILayout *layout,const PeTraining *training,PeTrainingUiShader *effects,float seconds);
#endif
