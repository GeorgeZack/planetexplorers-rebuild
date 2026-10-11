#ifndef PE_MATERIAL_VIEW_H
#define PE_MATERIAL_VIEW_H
/* Shared source-qualified alpha and scoped culling for actors and static OBJ
 * models. Policies are recovered offline; no texture/name heuristics here. */
#include "pe_actor_scene.h"
#include <raylib.h>
#include <rlgl.h>
#include <string.h>

static inline bool pe_material_shader_load(Shader *shader,int *cutoff,int *alpha){
    *shader=LoadShaderFromMemory(NULL,
        "#version 330\n"
        "in vec2 fragTexCoord; in vec4 fragColor; uniform sampler2D texture0; uniform vec4 colDiffuse;\n"
        "uniform float cutoff; uniform float alphaScale; out vec4 finalColor;\n"
        "void main(){vec4 texel=texture(texture0,fragTexCoord);"
        "if(texel.a*alphaScale<cutoff)discard;"
        "finalColor=vec4(texel.rgb*colDiffuse.rgb*fragColor.rgb,colDiffuse.a*fragColor.a);}\n");
    *cutoff=GetShaderLocation(*shader,"cutoff");*alpha=GetShaderLocation(*shader,"alphaScale");
    return shader->id&&shader->id!=rlGetShaderIdDefault()&&*cutoff>=0&&*alpha>=0;
}
static inline void pe_material_uniforms(Shader shader,PeActorMaterial policy,int cutoff,int alpha){
    float value=policy.alpha_mode==PE_ACTOR_ALPHA_CUTOUT?policy.cutoff:0;
    SetShaderValue(shader,cutoff,&value,SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader,alpha,&policy.alpha_scale,SHADER_UNIFORM_FLOAT);
}
static inline bool pe_material_culling_begin(PeActorMaterial policy,bool *restore){
    *restore=false;
    if(policy.alpha_mode==PE_ACTOR_ALPHA_LEGACY||!policy.two_sided)return true;
    typedef unsigned char (*IsEnabled)(unsigned int);
    void *address=rlGetProcAddress("glIsEnabled");IsEnabled query;
    _Static_assert(sizeof query==sizeof address,"OpenGL function pointer width");
    memcpy(&query,&address,sizeof query);if(!query)return false;
    rlDrawRenderBatchActive();*restore=query(0x0B44)!=0;
    rlDisableBackfaceCulling();return true;
}
static inline void pe_material_culling_end(PeActorMaterial policy,bool restore){
    if(policy.alpha_mode!=PE_ACTOR_ALPHA_LEGACY&&policy.two_sided){
        rlDrawRenderBatchActive();if(restore)rlEnableBackfaceCulling();
    }
}
#endif
