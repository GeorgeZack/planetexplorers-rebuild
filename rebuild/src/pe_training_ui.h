#ifndef PE_TRAINING_UI_H
#define PE_TRAINING_UI_H
#include "pe_training.h"
#include "pe_training_ui_layout.h"
#include "pe_ui_assets.h"
#include "pe_training_ui_minimap.h"
#include "pe_training_ui_shader.h"
#include "pe_ui_history.h"
typedef struct {
    PeUiAssets *assets;
    PeTrainingUiShader *effects;
    RenderTexture2D preview;
    Texture2D minimap;
    PeTrainingUIMinimap map;
    double seconds,notice_until;
    int previous_mission,completed_mission;
    PeTrainingUILayout layout;
    PeUIWindows windows;
    PeUiHistoryState history;
    int recorded_talk;
    PeConversationHistory conversation_history;
} PeTrainingUi;
typedef struct {
    bool paused,free_cursor,autorun,walk,log,dragging,melee_ready,knife_drawn,menu,cursor_visible;
    int inventory_tab;
    float hare_hp,heading;
    unsigned receivers_blocked;
} PeTrainingUiState;
bool pe_training_ui_load(PeTrainingUi *ui,const char *asset_dir,char *error,size_t capacity);
void pe_training_ui_unload(PeTrainingUi *ui);
void pe_training_ui_resize(PeTrainingUi *ui);
void pe_training_ui_record_dialogue(PeTrainingUi *ui,const PeTraining *training);
void pe_training_ui_draw(PeTrainingUi *ui,const PeTraining *training,PeTrainingUiState state);
/* UI surfaces consume world clicks; modal panels are handled by their caller. */
bool pe_training_ui_pointer_over(const PeTrainingUi *ui,Vector2 mouse);
#endif
