#include "pe_reference.h"
#include <raymath.h>
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static bool fail(char *error, size_t capacity, const char *message) {
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return false;
}

/* Accept only our bounded OBJ subset before entering raylib's general parser. */
static bool safe_basename(const char *name) {
    if (!name || !*name || name[0] == '.') return false;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.') return false;
    return true;
}

static bool finite_token(const char *token) {
    if (!token || strlen(token) > 32) return false;
    for (const char *p = token; *p; p++)
        if (!isdigit((unsigned char)*p) && *p != '.' && *p != '+' && *p != '-' && *p != 'e' && *p != 'E') return false;
    const char *exponent = strchr(token, 'e');
    if (!exponent) exponent = strchr(token, 'E');
    if (exponent) {
        char *end = NULL;
        long value = strtol(exponent + 1, &end, 10);
        if (end == exponent + 1 || *end || value < -50 || value > 50) return false;
    }
    char *end = NULL;
    float value = strtof(token, &end);
    return end != token && !*end && isfinite(value);
}

static bool numeric_fields(char **tokens, size_t count) {
    for (size_t i = 0; i < count; i++) if (!finite_token(tokens[i])) return false;
    return true;
}

static bool face_token(const char *token, const size_t limits[3]) {
    for (int i = 0; i < 3; i++) {
        if (!isdigit((unsigned char)*token)) return false;
        char *end = NULL;
        unsigned long index = strtoul(token, &end, 10);
        if (!index || index > limits[i]) return false;
        if (i == 2) return !*end;
        if (*end != '/') return false;
        token = end + 1;
    }
    return false;
}

static size_t split_line(char *line, char **tokens, size_t capacity) {
    size_t count = 0;
    for (char *token = strtok(line, " \t\r\n"); token; token = strtok(NULL, " \t\r\n")) {
        if (count == capacity) return capacity + 1;
        tokens[count++] = token;
    }
    return count;
}

bool pe_reference_validate(const char *obj_path, size_t *triangles,
                           char *error, size_t error_capacity) {
    if (!obj_path || !triangles) return fail(error, error_capacity, "A generated OBJ path is required");
    FILE *source = fopen(obj_path, "rb");
    if (!source) return fail(error, error_capacity, "Unable to read reference OBJ");
    char line[512], material_file[256] = {0};
    char symbols[256][16] = {{0}};
    size_t used_symbols = 0, limits[3] = {0}, faces = 0;
    bool valid = fgets(line, sizeof(line), source) &&
        !strcmp(line, "# Baked Unity XYZ meters; original vertex/normal coordinate convention\n");
    while (valid && fgets(line, sizeof(line), source)) {
        if (!strchr(line, '\n')) { valid = false; break; }
        char *tokens[8];
        size_t n = split_line(line, tokens, 8);
        if (!n) continue;
        if (n > 8) { valid = false; break; }
        if (!strcmp(tokens[0], "v") || !strcmp(tokens[0], "vn") || !strcmp(tokens[0], "vt")) {
            int index = !strcmp(tokens[0], "v") ? 0 : (!strcmp(tokens[0], "vt") ? 1 : 2);
            valid = n == (index == 1 ? 3u : 4u) && numeric_fields(tokens + 1, n - 1);
            if (valid) limits[index]++;
        } else if (!strcmp(tokens[0], "f")) {
            valid = n == 4;
            for (size_t i = 1; valid && i < n; i++) valid = face_token(tokens[i], limits);
            if (valid) faces++;
        } else if (!strcmp(tokens[0], "mtllib")) {
            valid = n == 2 && !*material_file && safe_basename(tokens[1]) && strlen(tokens[1]) < sizeof(material_file);
            if (valid) strcpy(material_file, tokens[1]);
        } else if (!strcmp(tokens[0], "usemtl")) {
            valid = n == 2 && strlen(tokens[1]) < sizeof(symbols[0]) && safe_basename(tokens[1]);
            if (valid) {
                bool seen = false;
                for (size_t i = 0; i < used_symbols; i++) if (!strcmp(symbols[i], tokens[1])) seen = true;
                if (!seen) {
                    valid = used_symbols < 256;
                    if (valid) strcpy(symbols[used_symbols++], tokens[1]);
                }
            }
        } else if (!strcmp(tokens[0], "o") || !strcmp(tokens[0], "g")) {
            valid = n == 2 && safe_basename(tokens[1]);
        } else valid = false;
    }
    valid = valid && !ferror(source) && faces && limits[0] && *material_file && used_symbols;
    fclose(source);
    if (!valid) return fail(error, error_capacity, "Unsupported or malformed generated reference OBJ");

    char path[4096];
    const char *slash = strrchr(obj_path, '/');
    size_t prefix = slash ? (size_t)(slash - obj_path) + 1 : 0;
    if (prefix + strlen(material_file) >= sizeof(path))
        return fail(error, error_capacity, "Reference material path too long");
    memcpy(path, obj_path, prefix);
    strcpy(path + prefix, material_file);
    source = fopen(path, "rb");
    if (!source) return fail(error, error_capacity, "Reference material file missing");
    valid = fgets(line, sizeof(line), source) &&
        !strcmp(line, "# Source material slots; diffuse-only preview conversion\n");
    size_t count = 0;
    unsigned long hashes[256] = {0};
    bool matched[256] = {0};
    while (valid && fgets(line, sizeof(line), source)) {
        if (!strchr(line, '\n')) { valid = false; break; }
        char *tokens[8];
        size_t n = split_line(line, tokens, 8);
        if (!n) continue;
        if (n > 8) { valid = false; break; }
        if (!strcmp(tokens[0], "newmtl")) {
            valid = n == 2 && count < 256;
            unsigned long hash = 5381;
            if (valid) for (const unsigned char *p = (const unsigned char *)tokens[1]; *p; p++) hash = hash*33 + *p;
            size_t capacity = 10;
            while (capacity <= count) capacity *= 2;
            /* Our symbols require no quadratic probing at any growth size. */
            for (size_t i = 0; valid && i < count; i++)
                if (hashes[i] % capacity == hash % capacity) valid = false;
            bool found = false;
            for (size_t i = 0; valid && i < used_symbols; i++) {
                if (!strcmp(symbols[i], tokens[1])) { found = true; matched[i] = true; }
            }
            valid = valid && found;
            if (valid) hashes[count++] = hash;
        } else if (!strcmp(tokens[0], "Kd")) valid = count && n == 4 && numeric_fields(tokens + 1, 3);
        else if (!strcmp(tokens[0], "d")) valid = count && n == 2 && numeric_fields(tokens + 1, 1);
        else if (!strcmp(tokens[0], "illum")) valid = count && n == 2 && !strcmp(tokens[1], "1");
        else if (!strcmp(tokens[0], "map_Kd")) valid = count && n == 2 && safe_basename(tokens[1]);
        else valid = false;
    }
    valid = valid && !ferror(source) && count == used_symbols;
    for (size_t i = 0; valid && i < used_symbols; i++) valid = matched[i];
    fclose(source);
    if (!valid) return fail(error, error_capacity, "Unsafe or unsupported reference materials; re-run export_reference_assets.py");
    *triangles = faces;
    if (error && error_capacity) error[0] = '\0';
    return true;
}


void pe_reference_unload(PeReferenceModel *reference) {
    if (!reference || !reference->loaded) return;
    /* UnloadModel does not own textures. The exporter writes only diffuse maps.
     * Skip raylib's shared default and avoid deleting a shared texture twice. */
    for (int i = 0; i < reference->model.materialCount; i++) {
        Texture2D texture = reference->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
        if (!texture.id || texture.id == rlGetTextureIdDefault()) continue;
        bool first = true;
        for (int j = 0; j < i; j++) {
            if (reference->model.materials[j].maps[MATERIAL_MAP_DIFFUSE].texture.id == texture.id) {
                first = false;
                break;
            }
        }
        if (first) UnloadTexture(texture);
    }
    UnloadModel(reference->model);
    memset(reference, 0, sizeof(*reference));
}

bool pe_reference_load(PeReferenceModel *reference, const char *obj_path,
                       char *error, size_t error_capacity) {
    if (!reference || !obj_path || !*obj_path)
        return fail(error, error_capacity, "A reference OBJ path is required");
    if (!IsWindowReady()) return fail(error, error_capacity, "Initialize graphics before loading a model");
    if (!FileExists(obj_path)) return fail(error, error_capacity, "Reference OBJ missing; run export_reference_assets.py first");
    if (!IsFileExtension(obj_path, ".obj"))
        return fail(error, error_capacity, "Reference model must be an exported OBJ");
    size_t expected_triangles = 0;
    if (!pe_reference_validate(obj_path, &expected_triangles, error, error_capacity)) return false;
    PeReferenceModel candidate = {0};
    candidate.model = LoadModel(obj_path);
    candidate.loaded = true;
    if (candidate.model.meshCount <= 0 || candidate.model.materialCount <= 0) {
        pe_reference_unload(&candidate);
        return fail(error, error_capacity, "Unable to decode reference model");
    }
    for (int i = 0; i < candidate.model.meshCount; i++)
        candidate.triangles += (size_t)candidate.model.meshes[i].triangleCount;
    if (candidate.triangles != expected_triangles) {
        pe_reference_unload(&candidate);
        return fail(error, error_capacity, "Reference OBJ triangle count changed during loading");
    }
    candidate.bounds = GetModelBoundingBox(candidate.model);
    Vector3 span = Vector3Subtract(candidate.bounds.max, candidate.bounds.min);
    if (!candidate.triangles || !isfinite(span.x) || !isfinite(span.y) || !isfinite(span.z)) {
        pe_reference_unload(&candidate);
        return fail(error, error_capacity, "Reference model geometry is empty or invalid");
    }
    pe_reference_unload(reference);
    *reference = candidate;
    if (error && error_capacity) error[0] = '\0';
    return true;
}

void pe_reference_draw(const PeReferenceModel *reference) {
    if (reference && reference->loaded)
        DrawModel(reference->model, (Vector3){0, 0, 0}, 1.0f, WHITE);
}

Camera3D pe_reference_camera(const PeReferenceModel *reference) {
    Camera3D camera = {{30, 20, 30}, {12, 4, 12}, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
    if (!reference || !reference->loaded) return camera;
    Vector3 center = Vector3Scale(Vector3Add(reference->bounds.min, reference->bounds.max), .5f);
    Vector3 extent = Vector3Subtract(reference->bounds.max, reference->bounds.min);
    float radius = fmaxf(Vector3Length(extent) * .5f, 1.0f);
    camera.target = center;
    camera.position = Vector3Add(center, (Vector3){radius * .9f, radius * .7f, radius * 1.4f});
    return camera;
}
