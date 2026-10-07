#define _POSIX_C_SOURCE 200809L
#include "pe_preferences.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "preferences:%d %s\n", __LINE__, #c); failures++; \
} } while (0)

static void write_bytes(const char *path, const char *bytes, size_t count) {
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    if (!file) return;
    CHECK(fwrite(bytes, 1, count, file) == count);
    CHECK(fclose(file) == 0);
}

int main(void) {
    char temporary[] = "/tmp/pe-preferences-XXXXXX";
    CHECK(mkdtemp(temporary) != NULL);
    if (failures) return 1;
    char path[512], directory[512], error[128], resolved[512];
    snprintf(directory, sizeof directory, "%s/nested", temporary);
    snprintf(path, sizeof path, "%s/nested/settings.conf", temporary);
    PePreferences preferences = {.camera_facing_attacks = true};
    CHECK(pe_preferences_load(&preferences, path, error, sizeof error));
    CHECK(!preferences.camera_facing_attacks && !*error);
    CHECK(pe_preferences_save(&preferences, path, error, sizeof error));
    preferences.camera_facing_attacks = true;
    CHECK(pe_preferences_load(&preferences, path, error, sizeof error));
    CHECK(!preferences.camera_facing_attacks);
    preferences.camera_facing_attacks = true;
    CHECK(pe_preferences_save(&preferences, path, error, sizeof error));
    pe_preferences_defaults(&preferences);
    CHECK(!preferences.camera_facing_attacks);
    CHECK(pe_preferences_load(&preferences, path, error, sizeof error));
    CHECK(preferences.camera_facing_attacks);
    struct stat status;
    CHECK(stat(path, &status) == 0 && (status.st_mode & 0777) == 0600);

    const char *invalid[] = {
        "", "PE_SETTINGS 2\ncamera_facing_attacks=1\n",
        "PE_SETTINGS 1\ncamera_facing_attacks=2\n",
        "PE_SETTINGS 1\ncamera_facing_attacks=true\n",
        "PE_SETTINGS 1\ncamera_facing_attacks=1\ncamera_facing_attacks=0\n",
        "PE_SETTINGS 1\ncamera_facing_attacks=0\nunknown=1\n",
        "PE_SETTINGS 1\n", "PE_SETTINGS 1\ncamera_facing_attacks=0"
    };
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        write_bytes(path, invalid[i], strlen(invalid[i]));
        for (int value = 0; value < 2; ++value) {
            preferences.camera_facing_attacks = value != 0;
            CHECK(!pe_preferences_load(&preferences, path, error, sizeof error));
            CHECK(preferences.camera_facing_attacks == (value != 0) && *error);
        }
    }
    const char hidden[] = "PE_SETTINGS 1\ncamera_facing_attacks=0\n\0garbage";
    write_bytes(path, hidden, sizeof hidden);
    CHECK(!pe_preferences_load(&preferences, path, error, sizeof error));
    CHECK(preferences.camera_facing_attacks);
    char oversized[512]; memset(oversized, 'x', sizeof oversized);
    write_bytes(path, oversized, sizeof oversized);
    CHECK(!pe_preferences_load(&preferences, path, error, sizeof error));
    CHECK(preferences.camera_facing_attacks);
    CHECK(!pe_preferences_load(&preferences, directory, error, sizeof error));
    CHECK(preferences.camera_facing_attacks);
    CHECK(!pe_preferences_load(&preferences, "", NULL, 0));
    CHECK(!pe_preferences_load(NULL, path, NULL, 0));
    CHECK(!pe_preferences_save(NULL, path, NULL, 0));
    CHECK(!pe_preferences_save(&preferences, "", NULL, 0));
    CHECK(!pe_preferences_save(&preferences, "/", NULL, 0));

    /* A file in the parent path fails; the existing file remains loadable. */
    CHECK(pe_preferences_save(&preferences, path, error, sizeof error));
    char blocked[600]; snprintf(blocked, sizeof blocked, "%s/child", path);
    CHECK(!pe_preferences_save(&preferences, blocked, error, sizeof error));
    CHECK(*error);
    pe_preferences_defaults(&preferences);
    CHECK(pe_preferences_load(&preferences, path, error, sizeof error));
    CHECK(preferences.camera_facing_attacks);
    /* Replacing a directory fails after writing the temporary; it stays intact. */
    CHECK(!pe_preferences_save(&preferences, directory, error, sizeof error));
    CHECK(stat(directory, &status) == 0 && S_ISDIR(status.st_mode));

    CHECK(setenv("PE_SETTINGS_PATH", path, 1) == 0);
    CHECK(setenv("XDG_CONFIG_HOME", "/ignored", 1) == 0);
    CHECK(pe_preferences_default_path(resolved, sizeof resolved, error, sizeof error));
    CHECK(!strcmp(resolved, path));
    CHECK(unsetenv("PE_SETTINGS_PATH") == 0);
    CHECK(pe_preferences_default_path(resolved, sizeof resolved, error, sizeof error));
    CHECK(!strcmp(resolved, "/ignored/planet-explorers-rebuild/settings.conf"));
    CHECK(setenv("HOME", temporary, 1) == 0);
    CHECK(unsetenv("XDG_CONFIG_HOME") == 0);
    CHECK(pe_preferences_default_path(resolved, sizeof resolved, error, sizeof error));
    snprintf(blocked, sizeof blocked, "%s/.config/planet-explorers-rebuild/settings.conf", temporary);
    CHECK(!strcmp(resolved, blocked));
    CHECK(setenv("XDG_CONFIG_HOME", "relative-is-not-xdg", 1) == 0);
    CHECK(pe_preferences_default_path(resolved, sizeof resolved, error, sizeof error));
    CHECK(!strcmp(resolved, blocked));
    strcpy(resolved, "unchanged");
    CHECK(!pe_preferences_default_path(resolved, 4, error, sizeof error));
    CHECK(!strcmp(resolved, "unchanged"));
    CHECK(unsetenv("HOME") == 0);
    CHECK(!pe_preferences_default_path(resolved, sizeof resolved, error, sizeof error));
    CHECK(!strcmp(resolved, "unchanged"));

    CHECK(unlink(path) == 0);
    CHECK(rmdir(directory) == 0); /* Also proves failed save left no temp file. */
    CHECK(rmdir(temporary) == 0);
    printf("preferences checks: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
