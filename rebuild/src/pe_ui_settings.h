#ifndef PE_UI_SETTINGS_H
#define PE_UI_SETTINGS_H
#include "pe_preferences.h"
#include "pe_ui_assets.h"

/* Per-host transient UI owner; preferences persist outside gameplay saves. */
typedef struct {
    PePreferences preferences;
    char path[4096], error[256];
    bool ready, open;
    int selected;
} PeUiSettings;

void pe_ui_settings_init(PeUiSettings *settings);
void pe_ui_settings_open(PeUiSettings *settings);
/* Returns whether the panel owned this frame, including its closing frame.
 * Hosts suppress underlying input and pass false on the opening frame. */
bool pe_ui_settings_update(PeUiSettings *settings, bool input_allowed);
/* Read-only presentation, including the recovered cursor when focused. */
void pe_ui_settings_draw(const PeUiSettings *settings, const PeUiAssets *assets);
#endif
