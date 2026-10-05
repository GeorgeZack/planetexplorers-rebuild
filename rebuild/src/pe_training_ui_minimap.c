#include "pe_training_ui_minimap.h"
#include "pe_training_npc.h"
#include <rlgl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Original UI/UICircle: signed (not radial) alpha falloff, RGB bright2.5.
   PNG/raylib v increases downward; Unity's shader coordinates increase up. */
static const char fragment[]=
    "#version 330\n"
    "in vec2 fragTexCoord;in vec4 fragColor;out vec4 finalColor;\n"
    "uniform sampler2D texture0;uniform vec4 colDiffuse;uniform vec2 mapCenter;\n"
    "void main(){vec4 c=texture(texture0,fragTexCoord)*colDiffuse*fragColor;"
    "vec2 uv=vec2(fragTexCoord.x,1.0-fragTexCoord.y);"
    "vec2 u=(uv-mapCenter)/0.24;"
    "c.a=clamp((1.0-u.x)*(1.0-u.y)*30.0,0.0,1.0)*0.5;"
    "c.rgb*=2.5;finalColor=c;}\n";
static Rectangle rectangle(PeTrainingUIRect r) { return (Rectangle){r.x,r.y,r.w,r.h}; }
static bool drawable(PeTrainingUIRect r) {
    const float limit=INT_MAX/4;
    return isfinite(r.x)&&isfinite(r.y)&&isfinite(r.w)&&isfinite(r.h)&&
        fabsf(r.x)<limit&&fabsf(r.y)<limit&&r.w>0&&r.h>0&&r.w<limit&&r.h<limit;
}
bool pe_training_ui_minimap_load(PeTrainingUIMinimap *map,char *error,size_t capacity) {
    if(error&&capacity)error[0]=0;
    if(!map){if(error&&capacity)snprintf(error,capacity,"Missing minimap state");return false;}
    PeTrainingUIMinimap next={0};next.shader=LoadShaderFromMemory(NULL,fragment);
    if(!next.shader.id || next.shader.id==rlGetShaderIdDefault()) {
        if(error&&capacity)snprintf(error,capacity,"Could not compile original minimap shader");
        return false;
    }
    next.center_location=GetShaderLocation(next.shader,"mapCenter");
    if(next.center_location<0){UnloadShader(next.shader);if(error&&capacity)snprintf(error,capacity,"Missing minimap shader uniform");return false;}
    *map=next;return true;
}
void pe_training_ui_minimap_unload(PeTrainingUIMinimap *map) {
    if(!map)return;
    if(map->shader.id && map->shader.id!=rlGetShaderIdDefault())UnloadShader(map->shader);
    memset(map,0,sizeof *map);
}
bool pe_training_ui_minimap_draw(PeTrainingUIMinimap *map,const PeUiAssets *assets,
    Texture2D texture,const PeTrainingUILayout *l,PeVec3 player,float heading,double time) {
    if(!map || !map->shader.id || !assets || !texture.id || texture.width<=0 || texture.height<=0 ||
       !l || !isfinite(l->scale) || l->scale<=0 || !isfinite(player.x) || !isfinite(player.z) ||
       !isfinite(heading) || !isfinite(time) || !drawable(l->minimap_clip) ||
       !drawable(l->minimap_texture) || !drawable(l->minimap_player))return false;
    double dx=(double)player.x-map->center_x,dz=(double)player.z-map->center_z;
    if(!map->initialized || dx*dx+dz*dz>62.*62. || time-map->refresh_time>600. || time<map->refresh_time) {
        map->center_x=player.x;map->center_z=player.z;map->refresh_time=time;map->initialized=true;
    }
    PeTrainingUIRect uv;
    if(!pe_training_ui_minimap_uv(player.x,player.z,map->center_x,map->center_z,&uv))return false;
    float center[2]={uv.x+.25f,uv.y+.25f};
    SetShaderValue(map->shader,map->center_location,center,SHADER_UNIFORM_VEC2);
    PeTrainingUIRect clip=l->minimap_clip;
    BeginScissorMode((int)floorf(clip.x),(int)floorf(clip.y),(int)ceilf(clip.w),(int)ceilf(clip.h));
    BeginShaderMode(map->shader);
    DrawTexturePro(texture,(Rectangle){uv.x*texture.width,(1-uv.y-uv.h)*texture.height,uv.w*texture.width,uv.h*texture.height},
                   rectangle(l->minimap_texture),(Vector2){0},0,WHITE);
    EndShaderMode();
    float px=l->minimap_player.x+l->minimap_player.w*.5f;
    float py=l->minimap_player.y+l->minimap_player.h*.5f;
    for(size_t i=0;i<PE_TRAINING_NPC_COUNT;i++) {
        const PeTrainingNpc *npc=pe_training_npc_at(i);
        float x=px+(npc->position.x-player.x)*(190.f/120.f)*l->scale;
        float y=py-(npc->position.z-player.z)*(200.f/120.f)*l->scale;
        if(x+9*l->scale<clip.x || y+9*l->scale<clip.y || x-9*l->scale>clip.x+clip.w || y-9*l->scale>clip.y+clip.h)continue;
        pe_ui_sprite(assets,"sign_npc",(Rectangle){x-8.5f*l->scale,y-9*l->scale,17*l->scale,18*l->scale},WHITE,l->scale);
    }
    rlPushMatrix();rlTranslatef(px,py,0);rlRotatef(fmodf(heading,360.f),0,0,1);
    pe_ui_sprite(assets,"map_Pointer",(Rectangle){-12.5f*l->scale,-15.5f*l->scale,25*l->scale,31*l->scale},WHITE,l->scale);
    rlPopMatrix();EndScissorMode();return true;
}
