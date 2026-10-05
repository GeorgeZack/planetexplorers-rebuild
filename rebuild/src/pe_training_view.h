#ifndef PE_TRAINING_VIEW_H
#define PE_TRAINING_VIEW_H
#include "pe_training_ui_layout.h"

/* Shared pause geometry: the renderer and input use the same source-styled
 * rows. Commands are intentionally host requests, never process termination. */
static inline PeTrainingUIRect pe_training_pause_button(const PeTrainingUILayout *layout,int index) {
    if(index<0||index>2)return (PeTrainingUIRect){0};
    return pe_training_ui_map(layout,(PeTrainingUIRect){485,302+48*(float)index,310,38},PE_UI_CENTER);
}
static inline int pe_training_pause_command(const PeTrainingUILayout *layout,float x,float y,
                                            bool click,bool main_key,bool quit_key) {
    if(main_key)return 2;
    if(quit_key)return 3;
    if(click)for(int i=0;i<3;i++)if(pe_training_ui_contains(pe_training_pause_button(layout,i),x,y))return i+1;
    return 0;
}
/* Recovered training session, with its own window/resource lifetime.
 * asset_dir contains room.obj, textures, room-collision.obj and optional groups.
 * smoke_frames=0 is interactive. Returns0 for quit/window close/smoke success,
 * 1 for failure (including screenshot error),2 for a main-menu request.
 * Every exit releases graphics resources and closes this session's window. */
int pe_training_view(const char *asset_dir, int smoke_frames, const char *screenshot);

#endif
