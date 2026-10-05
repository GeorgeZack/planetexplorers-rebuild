#ifndef PE_TRAINING_UI_MISSION_H
#define PE_TRAINING_UI_MISSION_H
#include "pe_training.h"
#include "pe_training_ui_layout.h"
#include "pe_ui_assets.h"
#include "pe_training_ui_shader.h"
/* Draw-only source MissionWnd, showing the authoritative current lesson.
   Other categories/history/abandon actions are not invented. Close hitbox:
   layout->mission_close (caller handles input). */
void pe_training_ui_mission_draw(const PeUiAssets *assets,
    const PeTrainingUILayout *layout,const PeTraining *training,PeTrainingUiShader *effects,float seconds);
#endif
