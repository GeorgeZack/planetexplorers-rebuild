#include "pe_deployable_view.h"
#include "../generated/pe_torch_source.h"
#include <raymath.h>
#include <rlgl.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static void message(char *error,size_t capacity,const char *text){if(error&&capacity)snprintf(error,capacity,"%s",text);}
bool pe_deployable_view_load(PeDeployableView *v,const char *root,char *error,size_t cap){
    if(!v||!root){message(error,cap,"Invalid deployable assets");return false;}
    char path[4096];int n=snprintf(path,sizeof path,"%s/torch",root);struct stat status;
    if(n<0||(size_t)n>=sizeof path){message(error,cap,"Torch asset path too long");return false;}
    if(stat(path,&status)){
        if(errno==ENOENT)return true;
        message(error,cap,"Cannot inspect Torch asset directory");return false;
    }
    if(!S_ISDIR(status.st_mode)){message(error,cap,"Torch asset path is not a directory");return false;}
    PeDeployableView candidate={0};
    n=snprintf(path,sizeof path,"%s/torch/torch.obj",root);
    if(n<0||(size_t)n>=sizeof path||!pe_reference_load(&candidate.torch,path,error,cap))return false;
    n=snprintf(path,sizeof path,"%s/torch/fx_flame01.png",root);
    if(n<0||(size_t)n>=sizeof path||!FileExists(path)){message(error,cap,"Incomplete Torch flame asset");pe_deployable_view_unload(&candidate);return false;}
    candidate.flame=LoadTexture(path);
    if(!candidate.flame.id){message(error,cap,"Cannot load original Torch flame texture");pe_deployable_view_unload(&candidate);return false;}
    SetTextureFilter(candidate.flame,TEXTURE_FILTER_BILINEAR);candidate.available=true;
    pe_deployable_view_unload(v);*v=candidate;return true;
}
void pe_deployable_view_unload(PeDeployableView *v){
    if(!v)return;
    pe_reference_unload(&v->torch);if(v->flame.id)UnloadTexture(v->flame);memset(v,0,sizeof *v);
}
static void model(const PeDeployableView *v,const PeDeployable *d,Color tint){
    Quaternion q=QuaternionMultiply(QuaternionFromAxisAngle((Vector3){1,0,0},-PI*.5f),QuaternionFromAxisAngle((Vector3){0,1,0},d->quarter_turn*PI*.5f));
    Vector3 axis;float angle;QuaternionToAxisAngle(q,&axis,&angle);
    DrawModelEx(v->torch.model,(Vector3){d->position.x,d->position.y,d->position.z},axis,angle*RAD2DEG,(Vector3){1,1,1},tint);
}
static float alpha_at(float fraction){
    size_t count=sizeof pe_torch_flame_alpha/sizeof pe_torch_flame_alpha[0];
    if(fraction<=pe_torch_flame_alpha[0][0])return pe_torch_flame_alpha[0][1];
    for(size_t i=1;i<count;i++)if(fraction<=pe_torch_flame_alpha[i][0]){
        float f=(fraction-pe_torch_flame_alpha[i-1][0])/(pe_torch_flame_alpha[i][0]-pe_torch_flame_alpha[i-1][0]);
        return pe_torch_flame_alpha[i-1][1]+f*(pe_torch_flame_alpha[i][1]-pe_torch_flame_alpha[i-1][1]);
    }
    return pe_torch_flame_alpha[count-1][1];
}
void pe_deployable_view_flame(const PeDeployableView *v,PeVec3 point,Camera3D camera,double seconds,uint32_t identity){
    if(!v||!v->available||!isfinite(seconds)||seconds<0||!isfinite(point.x)||!isfinite(point.y)||!isfinite(point.z))return;
    /* Deterministic identity-seeded phase is a native adapter; source randomSeed
     * is0. Keep source40/s,.15s lifetime,max8,.5m size and authored alpha curve. */
    double clock=seconds+(identity%997)/997.0,latest=floor(clock*PE_TORCH_FLAME_RATE);
    BeginBlendMode(BLEND_ADDITIVE);rlDisableBackfaceCulling();rlDisableDepthMask();
    for(int i=0;i<PE_TORCH_FLAME_MAX_PARTICLES;i++){
        double birth=(latest-i)/PE_TORCH_FLAME_RATE,age=clock-birth;
        if(birth<0||age<0||age>=PE_TORCH_FLAME_LIFETIME)continue;
        float f=(float)(age/PE_TORCH_FLAME_LIFETIME),t=Clamp((f-pe_torch_flame_size[0][0])/(pe_torch_flame_size[1][0]-pe_torch_flame_size[0][0]),0,1);
        t=t*t*(3-2*t);float size=PE_TORCH_FLAME_SIZE*(pe_torch_flame_size[0][1]+t*(pe_torch_flame_size[1][1]-pe_torch_flame_size[0][1]));
        float phase=(float)fmod((latest-i)*2.39996323+identity,2*PI);
        Vector3 p={point.x+cosf(phase)*PE_TORCH_FLAME_RADIUS,point.y+(float)(PE_TORCH_FLAME_SPEED*age+.5*PE_TORCH_PARTICLE_GRAVITY_Y*PE_TORCH_FLAME_GRAVITY_MODIFIER*age*age),point.z+sinf(phase)*PE_TORCH_FLAME_RADIUS};
        Color color={pe_torch_flame_color[0],pe_torch_flame_color[1],pe_torch_flame_color[2],(unsigned char)(pe_torch_flame_color[3]*alpha_at(f))};
        DrawBillboardPro(camera,v->flame,(Rectangle){0,0,(float)v->flame.width,(float)v->flame.height},p,(Vector3){0,1,0},(Vector2){size,size},(Vector2){size*.5f,size*.5f},phase*RAD2DEG,color);
    }
    rlEnableDepthMask();rlEnableBackfaceCulling();rlSetCullFace(RL_CULL_FACE_FRONT);EndBlendMode();
}
bool pe_deployable_view_draw(const PeDeployableView *v,const PeGame *g,Camera3D camera,double seconds){
    if(!v||!g||!pe_deployable_state_valid(g)||(!v->available&&g->deployed_count))return false;
    for(unsigned i=0;i<g->deployed_count;i++){
        const PeDeployable *d=&g->deployed[i];model(v,d,WHITE);
        PeVec3 tip=pe_deployable_point(d,(PeVec3){PE_TORCH_FLAME_POSITION_X,PE_TORCH_FLAME_POSITION_Y,PE_TORCH_FLAME_POSITION_Z});
        pe_deployable_view_flame(v,tip,camera,seconds,d->instance_id);
    }
    return true;
}
void pe_deployable_view_preview(const PeDeployableView *v,PeVec3 position,Camera3D camera,double seconds,bool allowed){
    (void)camera;(void)seconds;
    if(!v||!v->available)return;
    PeDeployable item={.position=position};model(v,&item,allowed?(Color){110,255,150,155}:(Color){255,110,110,155});
}
