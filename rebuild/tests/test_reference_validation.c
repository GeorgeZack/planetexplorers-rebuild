#define _POSIX_C_SOURCE 200809L
#include "pe_reference.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

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
static uint64_t hash_file(const char *path){
    FILE *f=fopen(path,"rb");assert(f);uint64_t hash=UINT64_C(14695981039346656037);int c;
    while((c=fgetc(f))!=EOF){hash^=(unsigned char)c;hash*=UINT64_C(1099511628211);}assert(!ferror(f));fclose(f);return hash;
}
static void sidecar(const char *path,const char *obj,const char *mtl,const char *texture,const char *mode,const char *cutoff,const char *extra){
    FILE *f=fopen(path,"wb");assert(f);
    fprintf(f,"PERM1 %016llx %016llx\nmAM texture.png %s %s 1 1 %016llx\n%s",
        (unsigned long long)hash_file(obj),(unsigned long long)hash_file(mtl),mode,cutoff,(unsigned long long)hash_file(texture),extra);
    assert(!fclose(f));
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
    write_material(mtl,false);write_obj(obj,"mtllib model.mtl\n","");
    char metadata[256],texture[256];snprintf(metadata,sizeof metadata,"%s/model.obj.materials",directory);snprintf(texture,sizeof texture,"%s/texture.png",directory);
    FILE *f=fopen(mtl,"ab");assert(f);fputs("map_Kd texture.png\n",f);assert(!fclose(f));
    f=fopen(texture,"wb");assert(f);fputs("exact PNG snapshot",f);assert(!fclose(f));
    PeReferenceMaterials *materials=NULL;
    assert(pe_reference_materials_load(obj,&materials,error,sizeof error)&&!materials);
    sidecar(metadata,obj,mtl,texture,"cutout","0.7","");
    assert(pe_reference_validate(obj,&triangles,error,sizeof error));
    assert(pe_reference_materials_load(obj,&materials,error,sizeof error)&&materials->count==1);
    assert(materials->entries[0].policy.alpha_mode==PE_ACTOR_ALPHA_CUTOUT&&materials->entries[0].policy.two_sided);
    free(materials);materials=NULL;
    sidecar(metadata,obj,mtl,texture,"cutout","nan","");assert(!pe_reference_validate(obj,&triangles,error,sizeof error));
    sidecar(metadata,obj,mtl,texture,"opaque","0.7","");assert(!pe_reference_validate(obj,&triangles,error,sizeof error));
    sidecar(metadata,obj,mtl,texture,"cutout","0.7","extra\n");assert(!pe_reference_validate(obj,&triangles,error,sizeof error));
    sidecar(metadata,obj,mtl,texture,"cutout","0.7","");
    f=fopen(texture,"ab");assert(f);fputs("changed",f);assert(!fclose(f));assert(!pe_reference_validate(obj,&triangles,error,sizeof error));
    sidecar(metadata,obj,mtl,texture,"cutout","0.7","");
    f=fopen(obj,"ab");assert(f);fputs("v 2 2 2\n",f);assert(!fclose(f));assert(!pe_reference_validate(obj,&triangles,error,sizeof error));
    unlink(metadata);unlink(texture);unlink(obj); unlink(mtl); rmdir(directory);
    if (argc == 2) {
        assert(pe_reference_validate(argv[1], &triangles, error, sizeof(error)));
        assert(triangles == 55618);
    }
    puts("Reference validation tests passed (no graphics context)");
    return 0;
}
