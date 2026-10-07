#include "pe_vegetation_view.h"
#include "pe_vegetation.h"
#include "pe_reference.h"
#include "../generated/pe_vegetation_visual_source.h"
#include <rlgl.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
struct PeVegetationView {
    PeReferenceModel models[PE_VEGETATION_VISUAL_COUNT];
    Shader shader;
    int cutoff;
    bool available;
};
static bool fail(char *error,size_t capacity,const char *message){
    if(error&&capacity)snprintf(error,capacity,"%s",message);
    return false;
}
static bool path(char out[4096],const char *root,const char *name,const char *extension){
    int n=snprintf(out,4096,"%s/sandbox-vegetation/%s%s",root,name,extension);
    return n>=0&&n<4096;
}
static bool regular_bounded(const char *file,size_t maximum){
    struct stat info;return !stat(file,&info)&&S_ISREG(info.st_mode)&&info.st_size>0&&(uint64_t)info.st_size<=maximum;
}
static uint32_t big_u32(const unsigned char *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static bool image_envelope(const char *file){
    FILE *f=fopen(file,"rb");if(!f)return false;
    unsigned char header[24];bool ok=fread(header,1,sizeof header,f)==sizeof header;fclose(f);
    if(!ok||memcmp(header,"\x89PNG\r\n\x1a\n",8)||big_u32(header+8)!=13||memcmp(header+12,"IHDR",4))return false;
    uint32_t width=big_u32(header+16),height=big_u32(header+20);
    return width>0&&height>0&&(uint64_t)width*height<=16777216;
}
static bool validate_model(const char *root,const PeVegetationVisualSource *source,char *error,size_t cap){
    char file[4096];size_t triangles=0;
    if(!path(file,root,source->model,".obj")||!regular_bounded(file,16u*1024u*1024u))
        return fail(error,cap,"Vegetation source mesh missing or oversized");
    if(!pe_reference_validate(file,&triangles,error,cap))return false;
    if(triangles!=source->triangles)return fail(error,cap,"Vegetation source triangle count changed");
    if(!isfinite(source->cutoff)||source->cutoff<0||source->cutoff>1)return fail(error,cap,"Invalid vegetation cutoff metadata");
    for(int axis=0;axis<3;axis++)if(!isfinite(source->minimum[axis])||!isfinite(source->maximum[axis])||source->minimum[axis]>source->maximum[axis])
        return fail(error,cap,"Invalid vegetation source bounds");
    FILE *f=fopen(file,"rb");if(!f)return fail(error,cap,"Cannot read vegetation mesh");
    char line[512];bool valid=true;size_t vertices=0;
    while(fgets(line,sizeof line,f))if(!strncmp(line,"v ",2)){
        float p[3];if(sscanf(line+2,"%f%f%f",p,p+1,p+2)!=3){valid=false;break;}
        for(int axis=0;axis<3;axis++)if(!isfinite(p[axis])||p[axis]<source->minimum[axis]-.0001f||p[axis]>source->maximum[axis]+.0001f)valid=false;
        vertices++;
    }
    valid=valid&&!ferror(f)&&vertices>0;fclose(f);
    if(!valid)return fail(error,cap,"Vegetation vertices exceed recovered local mesh bounds");
    if(!path(file,root,source->model,".mtl")||!regular_bounded(file,64u*1024u))return fail(error,cap,"Vegetation material missing");
    f=fopen(file,"rb");if(!f)return fail(error,cap,"Cannot read vegetation material");
    size_t textures=0;valid=true;
    while(valid&&fgets(line,sizeof line,f))if(!strncmp(line,"map_Kd ",7)){
        char name[256],extra;
        if(sscanf(line+7,"%255s %c",name,&extra)!=1||strchr(name,'/')||strchr(name,'\\')||name[0]=='.'||
           !path(file,root,name,"")||!regular_bounded(file,64u*1024u*1024u)||!image_envelope(file)){valid=false;break;}
        Image image=LoadImage(file);
        valid=image.data&&image.width>0&&image.height>0&&(uint64_t)image.width*(uint64_t)image.height<=16777216;
        if(image.data)UnloadImage(image);
        textures++;
    }
    valid=valid&&!ferror(f)&&textures>0;fclose(f);
    return valid?true:fail(error,cap,"Vegetation diffuse image missing, malformed or oversized");
}
bool pe_vegetation_view_validate(const char *root,bool *available,char *error,size_t cap){
    if(!root||!*root||!available)return fail(error,cap,"Invalid vegetation asset root");
    char file[4096];int n=snprintf(file,sizeof file,"%s/sandbox-vegetation",root);struct stat info;
    if(n<0||(size_t)n>=sizeof file)return fail(error,cap,"Vegetation asset path too long");
    if(stat(file,&info)){
        if(errno==ENOENT){*available=false;if(error&&cap)*error=0;return true;}
        return fail(error,cap,"Cannot inspect vegetation asset group");
    }
    if(!S_ISDIR(info.st_mode))return fail(error,cap,"Vegetation asset group is not a directory");
    for(size_t i=0;i<PE_VEGETATION_VISUAL_COUNT;i++)if(!validate_model(root,&pe_vegetation_visual_sources[i],error,cap))return false;
    *available=true;if(error&&cap)*error=0;return true;
}
void pe_vegetation_view_unload(PeVegetationView *view){
    if(!view)return;
    for(size_t i=0;i<PE_VEGETATION_VISUAL_COUNT;i++)pe_reference_unload(&view->models[i]);
    if(view->shader.id&&view->shader.id!=rlGetShaderIdDefault())UnloadShader(view->shader);
    free(view);
}
PeVegetationView *pe_vegetation_view_load(const char *root,char *error,size_t cap){
    bool available=false;
    if(!IsWindowReady()){fail(error,cap,"Vegetation upload requires a graphics context");return NULL;}
    if(!pe_vegetation_view_validate(root,&available,error,cap))return NULL;
    PeVegetationView *view=calloc(1,sizeof *view);if(!view){fail(error,cap,"Vegetation allocation failed");return NULL;}
    if(!available)return view;
    view->shader=LoadShaderFromMemory(NULL,
        "#version 330\nin vec2 fragTexCoord;in vec4 fragColor;\n"
        "uniform sampler2D texture0;uniform vec4 colDiffuse;uniform float cutoff;\n"
        "out vec4 finalColor;void main(){vec4 c=texture(texture0,fragTexCoord)*colDiffuse*fragColor;"
        /* Source ForwardBase is opaque cutout, ColorMask RGB/no Blend. */
        "if(c.a<cutoff)discard;finalColor=vec4(c.rgb,1.0);}\n");
    view->cutoff=GetShaderLocation(view->shader,"cutoff");
    if(!view->shader.id||view->shader.id==rlGetShaderIdDefault()||view->cutoff<0){fail(error,cap,"Vegetation cutoff shader unavailable");goto invalid;}
    for(size_t i=0;i<PE_VEGETATION_VISUAL_COUNT;i++){
        char file[4096];if(!path(file,root,pe_vegetation_visual_sources[i].model,".obj")||
            !pe_reference_load(&view->models[i],file,error,cap))goto invalid;
        Model *model=&view->models[i].model;
        if(view->models[i].triangles!=pe_vegetation_visual_sources[i].triangles||!model->meshes||!model->materials||!model->meshMaterial){fail(error,cap,"Vegetation model upload incomplete");goto invalid;}
        for(int j=0;j<model->meshCount;j++){
            Mesh *mesh=&model->meshes[j];int material=model->meshMaterial[j];
            if(!mesh->vertices||!mesh->normals||!mesh->texcoords||!mesh->vboId||!mesh->vboId[0]||!mesh->vboId[1]||!mesh->vboId[2]||
               (mesh->indices&&!mesh->vboId[6])||material<0||material>=model->materialCount||!model->materials[material].maps){fail(error,cap,"Vegetation mesh channels failed upload");goto invalid;}
            Texture2D image=model->materials[material].maps[MATERIAL_MAP_DIFFUSE].texture;
            if(!image.id||image.id==rlGetTextureIdDefault()||image.width<1||image.height<1){fail(error,cap,"Vegetation diffuse upload failed");goto invalid;}
            model->materials[material].shader=view->shader;
        }
    }
    view->available=true;return view;
invalid:pe_vegetation_view_unload(view);return NULL;
}
bool pe_vegetation_view_available(const PeVegetationView *view){return view&&view->available;}
bool pe_vegetation_view_draw(const PeVegetationView *view,const PeGame *game){
    if(!view||!pe_vegetation_state_valid(game)||(!view->available&&game->vegetation.installed))return false;
    /* The recovered shader specifies Cull Off in every pass. Restore the
     * sandbox's reflected-projection front culling after these two-sided leaves. */
    rlDisableBackfaceCulling();
    for(unsigned i=0;i<game->vegetation.count;i++){
        const PePlant *plant=&game->vegetation.plants[i];if(plant->depleted)continue;
        size_t model=0;while(model<PE_VEGETATION_VISUAL_COUNT&&pe_vegetation_visual_sources[model].prototype_id!=plant->prototype_id)model++;
        if(model==PE_VEGETATION_VISUAL_COUNT){rlEnableBackfaceCulling();rlSetCullFace(RL_CULL_FACE_FRONT);return false;}
        float cutoff=pe_vegetation_visual_sources[model].cutoff;SetShaderValue(view->shader,view->cutoff,&cutoff,SHADER_UNIFORM_FLOAT);
        DrawModel(view->models[model].model,(Vector3){plant->position.x,plant->position.y,plant->position.z},1,WHITE);
    }
    rlEnableBackfaceCulling();rlSetCullFace(RL_CULL_FACE_FRONT);
    return true;
}
