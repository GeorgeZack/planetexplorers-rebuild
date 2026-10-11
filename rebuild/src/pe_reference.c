#define _POSIX_C_SOURCE 200809L
#include "pe_reference.h"
#include "pe_material_view.h"
#include <raymath.h>
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

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

static bool sibling(char *out,size_t cap,const char *obj,const char *name){
    const char *slash=strrchr(obj,'/');size_t prefix=slash?(size_t)(slash-obj)+1:0;
    if(prefix+strlen(name)>=cap)return false;
    memcpy(out,obj,prefix);strcpy(out+prefix,name);return true;
}
static bool file_hash(const char *path,uint64_t limit,uint64_t *hash){
    struct stat st;
    if(lstat(path,&st)||!S_ISREG(st.st_mode)||st.st_size<0||(uint64_t)st.st_size>limit)return false;
    FILE *f=fopen(path,"rb");if(!f)return false;
    unsigned char bytes[8192];size_t n;uint64_t total=0,result=UINT64_C(14695981039346656037);
    while((n=fread(bytes,1,sizeof bytes,f))){total+=n;if(total>limit)break;for(size_t i=0;i<n;i++){result^=bytes[i];result*=UINT64_C(1099511628211);}}
    bool ok=!ferror(f)&&total==(uint64_t)st.st_size;fclose(f);if(ok)*hash=result;return ok;
}
static bool hex_hash(const char *text,uint64_t *out){
    if(strlen(text)!=16)return false;
    for(size_t i=0;i<16;i++)if(!isxdigit((unsigned char)text[i]))return false;
    errno=0;char *end;unsigned long long value=strtoull(text,&end,16);
    if(errno||*end)return false;
    *out=(uint64_t)value;return true;
}
bool pe_reference_materials_load(const char *obj,PeReferenceMaterials **out,char *error,size_t cap){
    if(!obj||!out||*out)return fail(error,cap,"Material load requires a path and empty output");
    char path[4096],mtl[256]={0},line[512];
    int written=snprintf(path,sizeof path,"%s.materials",obj);
    if(written<0||(size_t)written>=sizeof path)return fail(error,cap,"Material path too long");
    struct stat st;
    if(lstat(path,&st))return errno==ENOENT?true:fail(error,cap,"Unable to stat reference materials");
    if(!S_ISREG(st.st_mode)||st.st_size<6||st.st_size>65536)return fail(error,cap,"Invalid reference material size/type");
    FILE *side=fopen(path,"rb");if(!side)return fail(error,cap,"Unable to read reference materials");
    char *data=malloc((size_t)st.st_size+1);PeReferenceMaterials *next=calloc(1,sizeof *next);
    bool ok=data&&next;
    if(ok){ok=fread(data,1,(size_t)st.st_size,side)==(size_t)st.st_size&&fgetc(side)==EOF&&!ferror(side);data[st.st_size]=0;}
    fclose(side);
    if(ok)for(size_t i=0;i<(size_t)st.st_size;i++)if((unsigned char)data[i]>126||((unsigned char)data[i]<32&&data[i]!='\n')){ok=false;break;}
    /* Recover the exact MTL row order used by raylib's generated OBJ loader. */
    FILE *f=ok?fopen(obj,"rb"):NULL;if(!f)ok=false;
    while(ok&&fgets(line,sizeof line,f))if(!strncmp(line,"mtllib ",7)){
        char extra;if(sscanf(line,"mtllib %255s %c",mtl,&extra)!=1||!safe_basename(mtl)){ok=false;break;}
    }
    if(f){ok=ok&&!ferror(f);fclose(f);}
    if(ok)ok=*mtl&&sibling(path,sizeof path,obj,mtl);
    uint64_t obj_hash=0,mtl_hash=0;
    if(ok)ok=file_hash(obj,128u*1024u*1024u,&obj_hash)&&file_hash(path,65536,&mtl_hash);
    f=ok?fopen(path,"rb"):NULL;if(!f)ok=false;
    while(ok&&fgets(line,sizeof line,f)){
        char *tokens[8];size_t n=split_line(line,tokens,8);
        if(n&&n<=8&&!strcmp(tokens[0],"newmtl")){
            ok=n==2&&next->count<PE_REFERENCE_MAX_MATERIALS&&strlen(tokens[1])<sizeof next->entries[0].symbol;
            if(ok)strcpy(next->entries[next->count++].symbol,tokens[1]);
        }else if(n&&n<=8&&!strcmp(tokens[0],"map_Kd")){
            ok=n==2&&next->count&&safe_basename(tokens[1])&&strlen(tokens[1])<sizeof next->entries[0].diffuse;
            if(ok){PeReferenceMaterial *entry=&next->entries[next->count-1];ok=!entry->diffuse[0];if(ok)strcpy(entry->diffuse,tokens[1]);}
        }
    }
    if(f){ok=ok&&!ferror(f);fclose(f);}
    char *row=data;size_t number=0;
    while(ok&&*row){
        char *end=strchr(row,'\n');if(!end){ok=false;break;}*end++=0;
        char *tokens[8];size_t n=split_line(row,tokens,8);uint64_t a,b;
        if(number==0)ok=n==3&&!strcmp(tokens[0],"PERM1")&&hex_hash(tokens[1],&a)&&hex_hash(tokens[2],&b)&&a==obj_hash&&b==mtl_hash;
        else if(number>next->count)ok=false;
        else{
            PeReferenceMaterial *entry=&next->entries[number-1];
            ok=n==7&&!strcmp(tokens[0],entry->symbol)&&!strcmp(tokens[1],entry->diffuse[0]?entry->diffuse:"-")&&
                numeric_fields(tokens+3,2)&&(!strcmp(tokens[5],"0")||!strcmp(tokens[5],"1"))&&hex_hash(tokens[6],&a);
            if(ok){
                float cutoff=strtof(tokens[3],NULL),alpha=strtof(tokens[4],NULL);
                ok=cutoff>=0&&cutoff<=1&&alpha>=0&&alpha<=1&&!signbit(cutoff)&&!signbit(alpha);
                PeActorAlphaMode mode=PE_ACTOR_ALPHA_LEGACY;bool two=tokens[5][0]=='1';
                if(!strcmp(tokens[2],"legacy"))ok=ok&&!cutoff&&!alpha&&!two;
                else if(!strcmp(tokens[2],"opaque")){mode=PE_ACTOR_ALPHA_OPAQUE;ok=ok&&!cutoff&&alpha==1;}
                else if(!strcmp(tokens[2],"cutout"))mode=PE_ACTOR_ALPHA_CUTOUT;
                else ok=false;
                if(ok&&entry->diffuse[0])ok=sibling(path,sizeof path,obj,entry->diffuse)&&file_hash(path,64u*1024u*1024u,&b)&&a==b;
                else if(ok)ok=a==0;
                entry->policy=(PeActorMaterial){mode,cutoff,alpha,two};
            }
        }
        number++;row=end;
    }
    ok=ok&&next->count&&number==next->count+1;
    free(data);if(!ok){free(next);return fail(error,cap,"Malformed or stale reference material metadata");}
    *out=next;if(error&&cap)error[0]=0;return true;
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
    PeReferenceMaterials *materials=NULL;
    if(!pe_reference_materials_load(obj_path,&materials,error,error_capacity))return false;
    free(materials);
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
    if(reference->material_shader.id&&reference->material_shader.id!=rlGetShaderIdDefault())UnloadShader(reference->material_shader);
    free(reference->source_materials);
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
    if(!pe_reference_materials_load(obj_path,&candidate.source_materials,error,error_capacity)){
        pe_reference_unload(&candidate);return false;
    }
    if(candidate.source_materials){
        PeReferenceMaterials *materials=candidate.source_materials;
        /* Supported raylib OBJ loaders retain MTL declaration order; some
         * prepend a default material. Require one of those bounded layouts. */
        candidate.material_offset=candidate.model.materialCount-(int)materials->count;
        bool valid=candidate.material_offset==0||candidate.material_offset==1,qualified=false;
        for(int i=0;valid&&i<candidate.model.meshCount;i++)valid=candidate.model.meshMaterial[i]>=candidate.material_offset&&candidate.model.meshMaterial[i]<candidate.model.materialCount;
        for(size_t i=0;i<materials->count;i++)qualified|=materials->entries[i].policy.alpha_mode!=PE_ACTOR_ALPHA_LEGACY;
        if(!valid||(qualified&&!pe_material_shader_load(&candidate.material_shader,&candidate.cutoff_location,&candidate.alpha_location))){
            pe_reference_unload(&candidate);return fail(error,error_capacity,"Reference material layout/shader unavailable");
        }
        for(size_t i=0;i<materials->count;i++){
            PeActorMaterial policy=materials->entries[i].policy;bool restore;
            if(!pe_material_culling_begin(policy,&restore)){pe_reference_unload(&candidate);return fail(error,error_capacity,"Reference culling query unavailable");}
            pe_material_culling_end(policy,restore);
            if(policy.alpha_mode!=PE_ACTOR_ALPHA_LEGACY){
                Material *material=&candidate.model.materials[i+candidate.material_offset];
                material->shader=candidate.material_shader;
                /* Source alpha drives the cutout, not blended output opacity.
                 * The static host draws with WHITE; it has no native fade. */
                material->maps[MATERIAL_MAP_DIFFUSE].color.a=255;
            }
        }
    }
    pe_reference_unload(reference);
    *reference = candidate;
    if (error && error_capacity) error[0] = '\0';
    return true;
}

void pe_reference_draw(const PeReferenceModel *reference) {
    if(!reference||!reference->loaded)return;
    if(!reference->source_materials){DrawModel(reference->model,(Vector3){0},1,WHITE);return;}
    for(int i=0;i<reference->model.meshCount;i++){
        int index=reference->model.meshMaterial[i];
        PeActorMaterial policy=reference->source_materials->entries[index-reference->material_offset].policy;bool restore;
        if(policy.alpha_mode!=PE_ACTOR_ALPHA_LEGACY)pe_material_uniforms(reference->material_shader,policy,reference->cutoff_location,reference->alpha_location);
        if(!pe_material_culling_begin(policy,&restore))return;
        DrawMesh(reference->model.meshes[i],reference->model.materials[index],reference->model.transform);
        pe_material_culling_end(policy,restore);
    }
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
