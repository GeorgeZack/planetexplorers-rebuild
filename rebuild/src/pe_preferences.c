#define _POSIX_C_SOURCE 200809L
#include "pe_preferences.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool failure(char *error, size_t capacity, const char *message) {
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return false;
}

static bool success(char *error, size_t capacity) {
    if (error && capacity) error[0] = '\0';
    return true;
}

void pe_preferences_defaults(PePreferences *preferences) {
    if (preferences) *preferences = (PePreferences){0};
}

bool pe_preferences_default_path(char *out, size_t capacity,
                                char *error, size_t error_capacity) {
    const char *base = getenv("PE_SETTINGS_PATH"), *suffix = "";
    if (!base || !*base) {
        base = getenv("XDG_CONFIG_HOME");
        suffix = "/planet-explorers-rebuild/settings.conf";
        if (!base || base[0] != '/') {
            base = getenv("HOME");
            suffix = "/.config/planet-explorers-rebuild/settings.conf";
        }
    }
    if (!out || !base || !*base)
        return failure(error, error_capacity, "No user settings path is available.");
    size_t base_length = strlen(base), suffix_length = strlen(suffix);
    if (base_length >= capacity || suffix_length >= capacity - base_length)
        return failure(error, error_capacity, "User settings path is too long.");
    memcpy(out, base, base_length);
    memcpy(out + base_length, suffix, suffix_length + 1);
    return success(error, error_capacity);
}

bool pe_preferences_load(PePreferences *preferences, const char *path,
                         char *error, size_t error_capacity) {
    if (!preferences || !path || !*path)
        return failure(error, error_capacity, "Invalid user settings load request.");
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (errno != ENOENT)
            return failure(error, error_capacity, "Could not open user settings.");
        pe_preferences_defaults(preferences);
        return success(error, error_capacity);
    }
    /* This deliberately small, versioned format accepts only its two canonical
     * records. Compare byte lengths too, so embedded NULs cannot hide garbage. */
    char bytes[128];
    size_t count = fread(bytes, 1, sizeof bytes, file);
    bool read_failed = ferror(file) != 0;
    int close_result = fclose(file);
    if (read_failed || close_result)
        return failure(error, error_capacity, "Could not read user settings.");
    const char *off = "PE_SETTINGS 1\ncamera_facing_attacks=0\n";
    const char *on = "PE_SETTINGS 1\ncamera_facing_attacks=1\n";
    PePreferences next = {0};
    if (count == strlen(on) && !memcmp(bytes, on, count))
        next.camera_facing_attacks = true;
    else if (count != strlen(off) || memcmp(bytes, off, count))
        return failure(error, error_capacity, "Invalid or unsupported user settings format.");
    *preferences = next;
    return success(error, error_capacity);
}

static bool ensure_parents(char *path) {
    for (char *cursor = path + 1; *cursor; ++cursor) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        bool okay = mkdir(path, 0700) == 0;
        if (!okay && errno == EEXIST) {
            struct stat status;
            okay = stat(path, &status) == 0 && S_ISDIR(status.st_mode);
        }
        *cursor = '/';
        if (!okay) return false;
    }
    return true;
}

bool pe_preferences_save(const PePreferences *preferences, const char *path,
                         char *error, size_t error_capacity) {
    if (!preferences || !path || !*path || path[strlen(path) - 1] == '/')
        return failure(error, error_capacity, "Invalid user settings save request.");
    size_t length = strlen(path);
    if (length > (size_t)-1 - sizeof ".tmp.XXXXXX")
        return failure(error, error_capacity, "User settings path is too long.");
    char *temporary = malloc(length + sizeof ".tmp.XXXXXX");
    if (!temporary)
        return failure(error, error_capacity, "Could not allocate user settings path.");
    memcpy(temporary, path, length + 1);
    if (!ensure_parents(temporary)) {
        free(temporary);
        return failure(error, error_capacity, "Could not create user settings directory.");
    }
    memcpy(temporary + length, ".tmp.XXXXXX", sizeof ".tmp.XXXXXX");
    int descriptor = mkstemp(temporary);
    if (descriptor < 0) {
        free(temporary);
        return failure(error, error_capacity, "Could not create temporary user settings.");
    }
    const char *bytes = preferences->camera_facing_attacks
        ? "PE_SETTINGS 1\ncamera_facing_attacks=1\n"
        : "PE_SETTINGS 1\ncamera_facing_attacks=0\n";
    size_t remaining = strlen(bytes);
    bool okay = true;
    while (remaining) {
        ssize_t written = write(descriptor, bytes, remaining);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { okay = false; break; }
        bytes += written;
        remaining -= (size_t)written;
    }
    if (okay && fsync(descriptor) != 0) okay = false;
    if (close(descriptor) != 0) okay = false;
    if (okay && rename(temporary, path) != 0) okay = false;
    if (!okay) unlink(temporary);
    free(temporary);
    return okay ? success(error, error_capacity)
        : failure(error, error_capacity, "Could not replace user settings; previous file retained.");
}
