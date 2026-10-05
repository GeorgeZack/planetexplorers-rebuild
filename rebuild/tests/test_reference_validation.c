#define _POSIX_C_SOURCE 200809L
#include "pe_reference.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *header = "# Baked Unity XYZ meters; original vertex/normal coordinate convention\n";
static const char *geometry = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\nvn 0 0 1\nvn 0 0 1\nusemtl mAM\nf 1/1/1 2/2/2 3/3/3\n";
static void write_obj(const char *path, const char *prefix, const char *suffix) {
    FILE *file = fopen(path, "wb"); assert(file);
    fputs(header, file); fputs(prefix, file); fputs(geometry, file); fputs(suffix, file);
    assert(!fclose(file));
}
static void write_material(const char *path, bool collision) {
    FILE *file = fopen(path, "wb"); assert(file);
    fputs("# Source material slots; diffuse-only preview conversion\nnewmtl mAM\nKd 1 1 1\nd 1\nillum 1\n", file);
    if (collision) fputs("newmtl mAW\nKd 1 1 1\nd 1\nillum 1\n", file);
    assert(!fclose(file));
}
int main(int argc, char **argv) {
    char directory[] = "/tmp/pe-reference-validation-XXXXXX";
    assert(mkdtemp(directory));
    char obj[256], mtl[256], error[256]; size_t triangles = 0;
    snprintf(obj, sizeof(obj), "%s/model.obj", directory);
    snprintf(mtl, sizeof(mtl), "%s/model.mtl", directory);
    write_material(mtl, false);
    write_obj(obj, "mtllib model.mtl\n", "");
    assert(pe_reference_validate(obj, &triangles, error, sizeof(error)) && triangles == 1);
    write_obj(obj, "mtllib model.mtl\n", "f 999/1/1 2/2/2 3/3/3\n");
    assert(!pe_reference_validate(obj, &triangles, error, sizeof(error)));
    write_obj(obj, "mtllib model.mtl\nv 0e100000000 0 0\n", "");
    assert(!pe_reference_validate(obj, &triangles, error, sizeof(error)));
    write_obj(obj, "mtllib ../model.mtl\n", "");
    assert(!pe_reference_validate(obj, &triangles, error, sizeof(error)));
    write_material(mtl, true);
    write_obj(obj, "mtllib model.mtl\n", "usemtl mAW\n");
    assert(!pe_reference_validate(obj, &triangles, error, sizeof(error)));
    assert(strstr(error, "materials"));
    unlink(obj); unlink(mtl); rmdir(directory);
    if (argc == 2) {
        assert(pe_reference_validate(argv[1], &triangles, error, sizeof(error)));
        assert(triangles == 55618);
    }
    puts("Reference validation tests passed (no graphics context)");
    return 0;
}
