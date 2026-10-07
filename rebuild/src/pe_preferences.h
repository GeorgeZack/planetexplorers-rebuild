#ifndef PE_PREFERENCES_H
#define PE_PREFERENCES_H
#include <stdbool.h>
#include <stddef.h>

/* User preferences are independent of PeGame and its save version. */
typedef struct {
    bool camera_facing_attacks;
} PePreferences;

void pe_preferences_defaults(PePreferences *preferences);
/* PE_SETTINGS_PATH overrides XDG_CONFIG_HOME, then HOME/.config. Failed
 * resolution leaves out untouched. Error output is optional throughout. */
bool pe_preferences_default_path(char *out, size_t capacity,
                                char *error, size_t error_capacity);
/* A missing file successfully loads defaults. Other failures leave preferences
 * unchanged; unsupported versions, unknown keys and duplicate keys fail. */
bool pe_preferences_load(PePreferences *preferences, const char *path,
                         char *error, size_t error_capacity);
/* Creates missing parent directories and atomically replaces the destination.
 * On failure the prior file remains intact. No process-global owner exists. */
bool pe_preferences_save(const PePreferences *preferences, const char *path,
                         char *error, size_t error_capacity);
#endif
