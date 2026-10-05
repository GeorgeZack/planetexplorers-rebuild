#include "pe_training_ui_shader.h"
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GameMain.prefab UISpecularHandler / UIHolographicHandler source values.
 * Texture filenames and exact source records: ui/effects/manifest.json. */
typedef struct {const char *mask;float width,height,intensity,threshold;Vector4 border;int holo;} Effect;
static const Effect effects[PE_UI_EFFECT_COUNT]={
    {"effect-talkbg_mask.png",850,150,.9f,.45f,{193,18,16,18},0},
    {"effect-shortbar1_mask.png",844,110,.9f,.7f,{0,0,0,0},0},
    {"effect-bloodlan_6_mask.png",164,30,.5f,.7f,{0,0,0,0},0},
    {"effect-map_01_mask.png",20,208,1.1f,.7f,{0,58,0,58},0},
    {"effect-map_03_mask.png",34,208,2.2f,.7f,{0,58,0,58},0},
    {"effect-shortbar1bg_Mask.png",830,50,.2f,0,{0,0,0,0},1},
    {"effect-map_02_mask.png",148,208,.2f,0,{0,0,0,0},1},
    {"effect-midbg_mask.png",385,578,.8f,.7f,{50,30,50,30},0},
    {"effect-midbg_mask.png",385,460,.8f,.7f,{50,30,50,30},0},
    {"effect-midbg_mask.png",620,602,.9f,.7f,{50,30,50,30},0},
    {"effect-bigbg_mask.png",903,492,1.8f,.45f,{432,80,86,310},0},
    {"effect-bigbg_mask.png",1062,552,1.8f,.45f,{432,80,86,310},0},
    {"effect-cneterBg_Mask.png",378,326,.2f,0,{0,0,0,0},1}
};
struct PeTrainingUiShader {Shader specular,hologram;Texture2D mask[PE_UI_EFFECT_COUNT],noise;};
static const char *specular_fs=
"#version 330\n"
"in vec2 fragTexCoord; out vec4 finalColor;\n"
"uniform sampler2D texture0; uniform sampler2D noiseTexture;\n"
"uniform vec4 sizeSettings,border,color0,color1,color2,color3;\n"
"uniform float seconds,intensity,threshold,alpha;\n"
"float trans(float x,float left,float right,float w,float sw){float px=x*w;"
"if(px<left)return px/sw;if(w-px<right)return 1.0-(w-px)/sw;"
"return (left+(px-left)/(w-left-right)*(sw-left-right))/sw;}\n"
"void main(){vec2 uv=vec2(fragTexCoord.x,1.0-fragTexCoord.y);"
"vec2 b=vec2(trans(uv.x,border.x,border.z,sizeSettings.x,sizeSettings.z),"
"trans(uv.y,border.w,border.y,sizeSettings.y,sizeSettings.w));"
"vec4 mask=texture(texture0,vec2(b.x,1.0-b.y));"
"vec2 n=(uv+seconds)*sizeSettings.xy/64.0;"
"vec2 coord=uv+texture(noiseTexture,vec2(n.x,1.0-n.y)).rg*.05;"
"coord+=vec2(-seconds*.05,seconds*.08);coord=abs(fract(coord*.5)*2.0-1.0);"
"vec4 c=mix(mix(color0,color1,coord.x),mix(color2,color3,coord.x),coord.y);"
"float wave=sin((uv.x+uv.y+seconds/20.0*4.0)*20.0)+2.0;"
"wave=mix(1.0,wave,clamp(mask.r-threshold,0.0,1.0)*4.0);"
"finalColor=vec4(c.rgb*mask.r*intensity*wave*alpha,0.0);}\n";
/* Null source RandomTex and AlphabetTex use shader defaults (bump red=.5,
 * white). Consequently random1*c is .25. RandomTex2 is the original noise. */
static const char *hologram_fs=
"#version 330\n"
"in vec2 fragTexCoord;out vec4 finalColor;uniform sampler2D texture0,noiseTexture;"
"uniform float seconds,height,intensity,alpha,twinkle;\n"
"float random2(vec2 coord){vec3 c=texture(noiseTexture,vec2(48.0*coord.x/64.0,1.0)).rgb;"
"coord.y+=seconds*(c.g+.3)*.3;coord.x+=float(int(seconds*4.0*64.0))/64.0;"
"vec2 p=coord*vec2(48.0,7.0)/64.0;"
"return float(int(texture(noiseTexture,vec2(p.x,1.0-p.y)).r*10.0))*.1;}\n"
"void main(){vec2 uv=vec2(fragTexCoord.x,1.0-fragTexCoord.y);"
"vec3 mask=texture(texture0,fragTexCoord).rgb;float scr=sin(uv.y*height*3.14159);"
"vec3 c=mask*(.25+scr*.2)*intensity*vec3(.3308823705,.8061866164,1.0);"
"c*=(scr*.3+1.0)*(random2(vec2(seconds/20.0,seconds)*10.0)*twinkle+1.0);"
"finalColor=vec4(c*alpha,0.0);}\n";
static void setf(Shader s,const char *name,float v){SetShaderValue(s,GetShaderLocation(s,name),&v,SHADER_UNIFORM_FLOAT);}
static void set4(Shader s,const char *name,const float *v){SetShaderValue(s,GetShaderLocation(s,name),v,SHADER_UNIFORM_VEC4);}
static bool texture_load(Texture2D *texture,const char *directory,const char *filename,bool noise){
    char path[4096];int n=snprintf(path,sizeof path,"%s/effects/%s",directory,filename);
    if(n<=0||(size_t)n>=sizeof path||!FileExists(path))return false;
    Image image=LoadImage(path);if(!image.data||image.width<=0||image.height<=0||image.width>4096||image.height>4096){if(image.data)UnloadImage(image);return false;}
    *texture=LoadTextureFromImage(image);UnloadImage(image);if(!texture->id)return false;
    SetTextureFilter(*texture,noise?TEXTURE_FILTER_POINT:TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(*texture,noise?TEXTURE_WRAP_REPEAT:TEXTURE_WRAP_CLAMP);return true;
}
void pe_training_ui_shader_free(PeTrainingUiShader *e){
    if(!e)return;
    for(int i=0;i<PE_UI_EFFECT_COUNT;i++)if(e->mask[i].id)UnloadTexture(e->mask[i]);
    if(e->noise.id)UnloadTexture(e->noise);
    if(e->specular.id&&e->specular.id!=rlGetShaderIdDefault())UnloadShader(e->specular);
    if(e->hologram.id&&e->hologram.id!=rlGetShaderIdDefault())UnloadShader(e->hologram);
    free(e);
}
PeTrainingUiShader *pe_training_ui_shader_load(const char *directory,char *error,size_t cap){
    if(error&&cap)*error=0;
    PeTrainingUiShader *e=calloc(1,sizeof *e);if(!e||!directory)goto fail;
    e->specular=LoadShaderFromMemory(NULL,specular_fs);e->hologram=LoadShaderFromMemory(NULL,hologram_fs);
    if(!e->specular.id||!e->hologram.id||e->specular.id==rlGetShaderIdDefault()||e->hologram.id==rlGetShaderIdDefault())goto fail;
    if(!texture_load(&e->noise,directory,"effect-RandomVectors.png",true))goto fail;
    for(int i=0;i<PE_UI_EFFECT_COUNT;i++)if(!texture_load(e->mask+i,directory,effects[i].mask,false))goto fail;
    return e;
fail:
    pe_training_ui_shader_free(e);if(error&&cap)snprintf(error,cap,"Cannot load original UI effect shaders/textures");return NULL;
}
void pe_training_ui_shader_draw(PeTrainingUiShader *e,PeUiEffect role,Rectangle dst,float seconds,float alpha,bool refresh){
    if(!e||role<0||role>=PE_UI_EFFECT_COUNT||!isfinite(seconds)||seconds<0||!isfinite(alpha)||alpha<=0||
       !isfinite(dst.x)||!isfinite(dst.y)||!isfinite(dst.width)||!isfinite(dst.height)||dst.width<=0||dst.height<=0)return;
    if(alpha>1)alpha=1;
    const Effect *p=effects+role;Shader shader=p->holo?e->hologram:e->specular;Texture2D mask=e->mask[role];
    BeginShaderMode(shader);
    SetShaderValueTexture(shader,GetShaderLocation(shader,"noiseTexture"),e->noise);
    setf(shader,"seconds",seconds);setf(shader,"intensity",p->intensity);setf(shader,"alpha",alpha);
    if(p->holo){setf(shader,"height",p->height);setf(shader,"twinkle",refresh?.13f:.01f);}
    else {
        const float blue[4][4]={{0,.6689653397f,1,1},{.241379261f,0,1,1},{.632352948f,.756592333f,1,1},{.115051918f,.823529422f,.794212997f,1}};
        const float health[4][4]={{.558823526f,.135596886f,.135596886f,1},{.199999809f,0,1,1},{.91911763f,.250054091f,.540750563f,1},{.904411793f,.7438007f,.219452858f,1}};
        const float (*colors)[4]=role==PE_UI_EFFECT_HEALTH?health:blue;
        const float size[]={p->width,p->height,(float)mask.width,(float)mask.height};
        set4(shader,"sizeSettings",size);set4(shader,"border",&p->border.x);setf(shader,"threshold",p->threshold);
        set4(shader,"color0",colors[0]);set4(shader,"color1",colors[1]);set4(shader,"color2",colors[2]);set4(shader,"color3",colors[3]);
    }
    /* Source Blend One One. Alpha output zero keeps framebuffer alpha unchanged. */
    BeginBlendMode(BLEND_ADDITIVE);rlSetBlendFactors(RL_ONE,RL_ONE,RL_FUNC_ADD);rlSetBlendMode(BLEND_CUSTOM);
    rlDisableBackfaceCulling();rlDisableDepthMask();
    DrawTexturePro(mask,(Rectangle){0,0,(float)mask.width,(float)mask.height},dst,(Vector2){0},0,WHITE);
    rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableBackfaceCulling();EndBlendMode();EndShaderMode();
}
