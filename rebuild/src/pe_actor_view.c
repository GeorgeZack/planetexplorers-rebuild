#include "pe_actor_view.h"
#include "pe_hitbox.h"
#include "pe_hare_attack.h"
#include <rlgl.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool png_bounded(const char *path) {
    FILE *f=fopen(path,"rb"); if(!f)return false;
    unsigned char h[24]; bool ok=fread(h,1,24,f)==24;
    if(fseek(f,0,SEEK_END)!=0)ok=false;
    long size=ftell(f); fclose(f);
    static const unsigned char signature[8]={137,80,78,71,13,10,26,10};
    if(!ok||size<24||size>64*1024*1024||memcmp(h,signature,8)||memcmp(h+12,"IHDR",4))return false;
    uint32_t w=(uint32_t)h[16]<<24|(uint32_t)h[17]<<16|(uint32_t)h[18]<<8|h[19];
    uint32_t height=(uint32_t)h[20]<<24|(uint32_t)h[21]<<16|(uint32_t)h[22]<<8|h[23];
    return w&&height&&w<=8192&&height<=8192&&(uint64_t)w*height<=16777216;
}
static bool draw_material(PeActorView *v,Vector3 pos,float yaw,float scale,Color tint){
    bool restore_culling=false;
    if(v->material.alpha_mode!=PE_ACTOR_ALPHA_LEGACY){
        float cutoff=v->material.alpha_mode==PE_ACTOR_ALPHA_CUTOUT?v->material.cutoff:0;
        float alpha=v->material.alpha_scale;
        SetShaderValue(v->model.materials[0].shader,v->cutoff_location,&cutoff,SHADER_UNIFORM_FLOAT);
        SetShaderValue(v->model.materials[0].shader,v->alpha_location,&alpha,SHADER_UNIFORM_FLOAT);
        if(v->material.two_sided){
            /* Restore the caller's actual enable state, including private
             * previews and the hosts' reflected-projection culling policy. */
            typedef unsigned char (*IsEnabled)(unsigned int);
            static IsEnabled query;
            if(!query){void *address=rlGetProcAddress("glIsEnabled");
                _Static_assert(sizeof query==sizeof address,"OpenGL function pointer width");
                memcpy(&query,&address,sizeof query);}
            if(!query)return false;
            rlDrawRenderBatchActive();restore_culling=query(0x0B44)!=0; /* GL_CULL_FACE */
            rlDisableBackfaceCulling();
        }
    }
    DrawModelEx(v->model,pos,(Vector3){0,1,0},yaw,(Vector3){scale,scale,scale},tint);
    if(v->material.two_sided&&v->material.alpha_mode!=PE_ACTOR_ALPHA_LEGACY){
        rlDrawRenderBatchActive();if(restore_culling)rlEnableBackfaceCulling();
    }
    return true;
}
void pe_actor_view_unload(PeActorView *v) {
    if(!v)return;
    if(v->loaded)UnloadModel(v->model);
    if(v->texture.id)UnloadTexture(v->texture);
    free(v->pose); pe_actor_free(v->actor); memset(v,0,sizeof(*v));
}
bool pe_actor_view_load(PeActorView *v,const char *path,const char *png,char *error,size_t cap) {
    if(!v||v->loaded||v->actor||v->pose||!IsWindowReady()) { if(error&&cap)snprintf(error,cap,"actor view requires empty state and graphics context");return false; }
    PeActor *actor=pe_actor_load(path,error,cap);if(!actor)return false;
    Image image={0};
    if(png){
        if(!png_bounded(png)||(image=LoadImage(png)).data==NULL){
            pe_actor_free(actor);if(error&&cap)snprintf(error,cap,"actor diffuse PNG load failed");return false;
        }
    }
    bool ok=pe_actor_view_adopt(v,actor,png?&image:NULL,error,cap);
    if(image.data)UnloadImage(image);
    return ok;
}
bool pe_actor_view_adopt(PeActorView *v,PeActor *actor,const Image *image,char *error,size_t cap) {
    if(error&&cap)*error=0;
    if(!v||v->loaded||v->actor||v->pose||!actor||!IsWindowReady()||
       (image&&(!image->data||image->width<=0||image->height<=0||image->width>8192||image->height>8192||(uint64_t)image->width*image->height>16777216))){
        pe_actor_free(actor);if(error&&cap)snprintf(error,cap,"actor adoption requires empty view, graphics context and bounded image");return false;
    }
    PeActorView next={0};next.actor=actor;PeActorStats s=pe_actor_stats(actor);
    next.pose=malloc(s.vertices*sizeof(*next.pose));if(!next.pose)goto fail;
    if(image){
        next.texture=LoadTextureFromImage(*image);if(!next.texture.id)goto fail;
        SetTextureFilter(next.texture,TEXTURE_FILTER_BILINEAR);
    }
    Mesh m={0}; m.vertexCount=(int)s.indices; m.triangleCount=(int)(s.indices/3);
    m.vertices=MemAlloc((unsigned int)(s.indices*3*sizeof(float)));
    m.normals=MemAlloc((unsigned int)(s.indices*3*sizeof(float)));
    m.texcoords=MemAlloc((unsigned int)(s.indices*2*sizeof(float)));
    if(!m.vertices||!m.normals||!m.texcoords) { MemFree(m.vertices);MemFree(m.normals);MemFree(m.texcoords);goto fail; }
    pe_actor_pose(next.actor,PE_ACTOR_BIND_POSE,0,next.pose,s.vertices);
    const uint32_t *indices=pe_actor_indices(next.actor);
    for(size_t k=0;k<s.indices;k++) {
        PeActorVertex *p=next.pose+indices[k];
        memcpy(m.vertices+k*3,p->position,3*sizeof(float));memcpy(m.normals+k*3,p->normal,3*sizeof(float));
        m.texcoords[k*2]=p->uv[0];m.texcoords[k*2+1]=1-p->uv[1];
    }
    UploadMesh(&m,true);
    if(!m.vboId) { UnloadMesh(m);goto fail; }
    next.model=LoadModelFromMesh(m); next.loaded=true;
    if(!next.model.meshes||!next.model.materials)goto fail;
    if(next.texture.id)next.model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture=next.texture;
    *v=next; return true;
fail:
    pe_actor_view_unload(&next);if(error&&cap)snprintf(error,cap,"actor view allocation or diffuse PNG load failed");return false;
}
bool pe_actor_view_draw(PeActorView *v,size_t clip,double seconds,Vector3 pos,float yaw,float scale,Color tint) {
    if(!v||!v->loaded||!isfinite(pos.x)||!isfinite(pos.y)||!isfinite(pos.z)||!isfinite(yaw)||!isfinite(scale)||scale<=0||scale>1000)return false;
    PeActorStats s=pe_actor_stats(v->actor);
    if(!pe_actor_pose(v->actor,clip,seconds,v->pose,s.vertices))return false;
    Mesh *m=v->model.meshes;const uint32_t *indices=pe_actor_indices(v->actor);
    for(size_t k=0;k<s.indices;k++) {
        PeActorVertex *p=v->pose+indices[k];memcpy(m->vertices+k*3,p->position,3*sizeof(float));memcpy(m->normals+k*3,p->normal,3*sizeof(float));
    }
    UpdateMeshBuffer(*m,0,m->vertices,(int)(s.indices*3*sizeof(float)),0);
    UpdateMeshBuffer(*m,2,m->normals,(int)(s.indices*3*sizeof(float)),0);
    return draw_material(v,pos,yaw,scale,tint);
}
bool pe_actor_view_draw_blend(PeActorView *v,const PeActorBlendSample *samples,size_t count,Vector3 pos,float yaw,float scale,Color tint){
    if(!v||!v->loaded||!v->actor||!v->pose||v->model.meshCount<1||!v->model.meshes||
       !v->model.meshes[0].vertices||!v->model.meshes[0].normals||
       !isfinite(pos.x)||!isfinite(pos.y)||!isfinite(pos.z)||!isfinite(yaw)||!isfinite(scale)||scale<=0||scale>1000)return false;
    PeActorStats s=pe_actor_stats(v->actor);
    if(!pe_actor_pose_blend(v->actor,samples,count,v->pose,s.vertices))return false;
    Mesh *m=v->model.meshes;const uint32_t *indices=pe_actor_indices(v->actor);
    for(size_t k=0;k<s.indices;k++){
        PeActorVertex *p=v->pose+indices[k];memcpy(m->vertices+k*3,p->position,3*sizeof(float));memcpy(m->normals+k*3,p->normal,3*sizeof(float));
    }
    UpdateMeshBuffer(*m,0,m->vertices,(int)(s.indices*3*sizeof(float)),0);
    UpdateMeshBuffer(*m,2,m->normals,(int)(s.indices*3*sizeof(float)),0);
    return draw_material(v,pos,yaw,scale,tint);
}

int pe_actor_preview(const char *peac,int smoke,const char *screenshot) {
    if(!peac||!*peac||smoke<0||(screenshot&&!*screenshot)) {
        fprintf(stderr,"Invalid actor preview arguments\n");return 1;
    }
    size_t length=strlen(peac);
    char diffuse[4096];
    if(length<=5||length+8>=sizeof(diffuse)||strcmp(peac+length-5,".peac")) {
        fprintf(stderr,"Actor preview requires a bounded .peac path\n");return 1;
    }
    memcpy(diffuse,peac,length-5);memcpy(diffuse+length-5,"-diffuse.png",13);
    /* Validate actor before opening a window. The view validates it again on
     * loading, so a file change between these reads cannot bypass validation. */
    char error[256];PeActor *checked=pe_actor_load(peac,error,sizeof(error));
    if(!checked){fprintf(stderr,"Actor preview: %s\n",error);return 1;}
    PeActorStats stats=pe_actor_stats(checked);
    size_t idle=pe_actor_find_clip(checked,"idle"),run=pe_actor_find_clip(checked,"run");
    if(idle==PE_ACTOR_BIND_POSE||run==PE_ACTOR_BIND_POSE) {
        fprintf(stderr,"Actor preview requires idle and run clips\n");pe_actor_free(checked);return 1;
    }
    printf("Actor: %zu vertices, %zu indices, %zu joints, %zu clips\n",stats.vertices,stats.indices,stats.joints,stats.clips);
    for(size_t c=0;c<stats.clips;c++) {
        const PeActorClip *clip=pe_actor_clip(checked,c);
        printf("Actor clip %zu: %s %u frames %.6g fps %.9g seconds loop=%d\n",c,clip->name,clip->frames,(double)clip->fps,(double)clip->duration,clip->loop);
    }
    pe_actor_free(checked);
    const char *texture=NULL;
    struct stat texture_stat;
    if(stat(diffuse,&texture_stat)==0) {
        if(!S_ISREG(texture_stat.st_mode)||!png_bounded(diffuse)){fprintf(stderr,"Invalid actor diffuse PNG: %s\n",diffuse);return 1;}
        texture=diffuse;
    } else if(errno!=ENOENT) {
        fprintf(stderr,"Cannot inspect actor diffuse PNG: %s\n",diffuse);return 1;
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1100,720,"Planet Explorers - source actor preview");
    if(!IsWindowReady()){fprintf(stderr,"Cannot open actor preview window\n");return 1;}
    SetTargetFPS(60);
    PeActorView view={0};
    if(!pe_actor_view_load(&view,peac,texture,error,sizeof(error))) {
        fprintf(stderr,"Actor preview: %s\n",error);CloseWindow();return 1;
    }
    char hitbox_path[4096];
    snprintf(hitbox_path,sizeof hitbox_path,"%.*s.pehb",(int)(length-5),peac);
    PeHitboxes *hitboxes=NULL;
    if(FileExists(hitbox_path)) {
        hitboxes=pe_hitboxes_load(hitbox_path,view.actor,error,sizeof error);
        if(!hitboxes){fprintf(stderr,"Actor hitboxes: %s\n",error);pe_actor_view_unload(&view);CloseWindow();return 1;}
    }
    idle=pe_actor_find_clip(view.actor,"idle");run=pe_actor_find_clip(view.actor,"run");
    stats=pe_actor_stats(view.actor);
    if(idle==PE_ACTOR_BIND_POSE||run==PE_ACTOR_BIND_POSE||!pe_actor_pose(view.actor,idle,0,view.pose,stats.vertices)) {
        fprintf(stderr,"Actor preview clips changed during loading\n");pe_hitboxes_free(hitboxes);pe_actor_view_unload(&view);CloseWindow();return 1;
    }
    Vector3 lo={view.pose[0].position[0],view.pose[0].position[1],view.pose[0].position[2]},hi=lo;
    for(size_t k=1;k<stats.vertices;k++) {
        const float *p=view.pose[k].position;
        lo.x=fminf(lo.x,p[0]);lo.y=fminf(lo.y,p[1]);lo.z=fminf(lo.z,p[2]);
        hi.x=fmaxf(hi.x,p[0]);hi.y=fmaxf(hi.y,p[1]);hi.z=fmaxf(hi.z,p[2]);
    }
    Vector3 target={(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f};
    float dx=hi.x-lo.x,dy=hi.y-lo.y,dz=hi.z-lo.z;
    float radius=fmaxf(.1f,.5f*sqrtf(dx*dx+dy*dy+dz*dz));
    float distance=radius*3.1f,yaw=.78539816f,pitch=.26f;
    Camera3D camera={.target=target,.up={0,1,0},.fovy=45,.projection=CAMERA_PERSPECTIVE};
    size_t current=idle;double seconds=0;bool paused=false,failed=false,captured=false,show_hitboxes=smoke>=180&&hitboxes;int frames=0;
    while(!WindowShouldClose()&&(!smoke||frames<smoke)) {
        if(smoke) {
            size_t next=smoke>=180?(size_t)frames*stats.clips/(size_t)smoke:(frames<smoke/2?idle:run);
            if(next>=stats.clips)next=stats.clips-1;
            if(next!=current){current=next;seconds=0;}
            seconds+=1.0/60.0;
        } else if(IsWindowFocused()) {
            size_t next=IsKeyPressed(KEY_ONE)?idle:(IsKeyPressed(KEY_TWO)?run:current);
            if(IsKeyPressed(KEY_RIGHT))next=(current+1)%stats.clips;
            if(IsKeyPressed(KEY_LEFT))next=(current+stats.clips-1)%stats.clips;
            if(IsKeyPressed(KEY_R))seconds=0;
            if(next!=current){current=next;seconds=0;}
            if(IsKeyPressed(KEY_SPACE))paused=!paused;
            if(IsKeyPressed(KEY_H))show_hitboxes=!show_hitboxes;
            if(!paused)seconds+=fminf(GetFrameTime(),.1f);
            if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                Vector2 delta=GetMouseDelta();yaw-=delta.x*.006f;pitch+=delta.y*.006f;
                pitch=fminf(1.45f,fmaxf(-1.2f,pitch));
            }
            distance*=powf(.9f,GetMouseWheelMove());distance=fmaxf(radius*.75f,fminf(radius*12,distance));
        }
        camera.position=(Vector3){target.x+distance*cosf(pitch)*sinf(yaw),target.y+distance*sinf(pitch),target.z+distance*cosf(pitch)*cosf(yaw)};
        BeginDrawing();ClearBackground((Color){26,33,40,255});BeginMode3D(camera);
        DrawGrid(20,fmaxf(radius*.4f,.1f));
        bool rendered=pe_actor_view_draw(&view,current,seconds,(Vector3){0,0,0},0,1,(Color){232,232,232,255});
        if(show_hitboxes&&hitboxes) {
            const PeActorClip *shown=pe_actor_clip(view.actor,current);
            unsigned active_bones=0;
            if(strlen(shown->name)==7&&!strncmp(shown->name,"attack",6)&&shown->name[6]>='0'&&shown->name[6]<='4') {
                const PeHareAttackDef *attack=pe_hare_attack_def((unsigned)(shown->name[6]-'0'));
                double normalized=seconds/shown->duration;
                for(unsigned w=0;w<attack->window_count;w++) {
                    PeHareAttackWindow window=attack->windows[w];
                    if(normalized>=window.start&&(window.end_inclusive?normalized<=window.end:normalized<window.end))active_bones|=window.bones;
                }
            }
            for(size_t i=0;i<pe_hitboxes_count(hitboxes);i++) {
                const PeHitboxDefinition *box=pe_hitboxes_get(hitboxes,i);
                if(box->joint==PE_HITBOX_DISABLED)continue;
                PeHitboxCapsule capsule;
                if(!pe_hitbox_pose(hitboxes,view.actor,i,current,seconds,(PeVec3){0},0,1,&capsule)){rendered=false;break;}
                Color color=box->kind==PE_HITBOX_DEFENCE?(Color){90,220,160,150}:
                    ((box->group&active_bones)?(Color){255,90,45,255}:(Color){235,185,65,95});
                DrawCapsuleWires((Vector3){capsule.a.x,capsule.a.y,capsule.a.z},(Vector3){capsule.b.x,capsule.b.y,capsule.b.z},capsule.radius,6,8,color);
            }
        }
        EndMode3D();
        DrawRectangle(0,0,GetScreenWidth(),82,(Color){13,22,29,230});
        DrawText("Recovered animated actor",22,14,25,RAYWHITE);
        const PeActorClip *clip=pe_actor_clip(view.actor,current);
        DrawText(TextFormat("%s | %.2f s | %zu vertices, %zu bones%s",clip->name,seconds,stats.vertices,stats.joints,paused?" | paused":""),23,49,18,(Color){191,208,215,255});
        DrawText("1/2: idle/run  Arrows: clips  R: restart  Space: pause  H: hitboxes  Right drag: orbit  Wheel: zoom",22,GetScreenHeight()-34,18,RAYWHITE);
        EndDrawing();frames++;
        if(!rendered){failed=true;break;}
        if(screenshot&&smoke&&frames==smoke) {
            Image capture=LoadImageFromScreen();captured=capture.data&&ExportImage(capture,screenshot);
            if(capture.data)UnloadImage(capture);
            if(!captured){fprintf(stderr,"Actor screenshot export failed: %s\n",screenshot);failed=true;}
        }
    }
    if(smoke&&frames<smoke){fprintf(stderr,"Actor smoke closed early (%d/%d frames)\n",frames,smoke);failed=true;}
    if(screenshot&&!smoke&&!failed&&frames) {
        Image capture=LoadImageFromScreen();captured=capture.data&&ExportImage(capture,screenshot);
        if(capture.data)UnloadImage(capture);
    }
    if(screenshot&&!captured)failed=true;
    printf("Actor preview: frames=%d clip=%s result=%s\n",frames,pe_actor_clip(view.actor,current)->name,failed?"FAIL":"PASS");
    pe_hitboxes_free(hitboxes);pe_actor_view_unload(&view);CloseWindow();return failed?1:0;
}
