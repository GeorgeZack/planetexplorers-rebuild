#define _POSIX_C_SOURCE 200809L
#include "pe_test_paths.h"
#include "pe_actor_scene_view.h"
#include <assert.h>
#include <math.h>
#include <rlgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void u32(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static void f32(unsigned char *p,float n){uint32_t b;memcpy(&b,&n,4);u32(p,b);}
static void matrix(unsigned char *p){for(unsigned i=0;i<12;i++)f32(p+4*i,(i==0||i==5||i==10)?1:0);}
static void write_file(const char *path,const void *bytes,size_t size){FILE *f=fopen(path,"wb");assert(f&&fwrite(bytes,1,size,f)==size&&fclose(f)==0);}
static bool same(Color a,Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b;}
static Image shot(const char *root,const char *name){char path[4096];snprintf(path,sizeof path,"%s/%s.png",root,name);Image im=LoadImageFromScreen();assert(im.data&&ExportImage(im,path));return im;}
static unsigned char culling(void){typedef unsigned char(*Query)(unsigned int);void *p=rlGetProcAddress("glIsEnabled");Query fn;memcpy(&fn,&p,sizeof fn);assert(fn);return fn(0x0b44);}
static void synthetic(const char *root){
    unsigned char data[464]={0};memcpy(data,"PEAC",4);u32(data+4,1);u32(data+8,4);u32(data+12,6);u32(data+16,1);u32(data+20,1);
    const float xy[][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
    for(unsigned i=0;i<4;i++){unsigned char *v=data+24+56*i;f32(v,xy[i][0]);f32(v+4,xy[i][1]);f32(v+20,1);f32(v+24,(xy[i][0]+1)*.5f);f32(v+28,(xy[i][1]+1)*.5f);f32(v+32,1);}
    const unsigned indices[]={0,1,2,0,2,3};for(unsigned i=0;i<6;i++)u32(data+248+4*i,indices[i]);matrix(data+272);
    memcpy(data+320,"idle",5);f32(data+352,1);u32(data+356,2);f32(data+360,1);matrix(data+368);matrix(data+416);
    char actor[4096],png[4096],scene[4096],material[4096],text[1024],error[256];
    snprintf(actor,sizeof actor,"%s/fixture.peac",root);snprintf(png,sizeof png,"%s/fixture.png",root);snprintf(scene,sizeof scene,"%s/fixture.peas",root);snprintf(material,sizeof material,"%s/fixture.peas.materials",root);
    write_file(actor,data,sizeof data);Image image=GenImageColor(12,4,(Color){255,255,255,0});ImageDrawRectangle(&image,4,0,4,4,(Color){255,255,255,127});ImageDrawRectangle(&image,8,0,4,4,WHITE);assert(ExportImage(image,png));UnloadImage(image);
    int length=0;unsigned char *bytes=LoadFileData(png,&length);assert(bytes);uint64_t hash=UINT64_C(14695981039346656037);for(int i=0;i<length;i++){hash^=bytes[i];hash*=UINT64_C(1099511628211);}UnloadFileData(bytes);
    const char *rows="PEAS1\nfixture.peac fixture.png\n";write_file(scene,rows,strlen(rows));
    snprintf(text,sizeof text,"PEAM1\nfixture.peac fixture.png cutout 0.6 1 1 %016llx\n",(unsigned long long)hash);write_file(material,text,strlen(text));
    PeActorSceneView view={0};assert(pe_actor_scene_view_load(&view,scene,error,sizeof error));SetTextureFilter(view.parts[0].texture,TEXTURE_FILTER_POINT);
    Camera3D camera={.position={0,0,5},.target={0,0,0},.up={0,1,0},.fovy=2,.projection=CAMERA_ORTHOGRAPHIC};
    const Color blue={40,100,160,255},red={230,20,30,255};
    for(unsigned mode=0;mode<4;mode++){
        view.parts[0].material=(PeActorMaterial){mode==0?PE_ACTOR_ALPHA_LEGACY:mode==1?PE_ACTOR_ALPHA_OPAQUE:PE_ACTOR_ALPHA_CUTOUT,mode==3?.4f:.6f,1,true};
        view.parts[0].model.materials[0].shader=mode==0?(Shader){rlGetShaderIdDefault(),rlGetShaderLocsDefault()}:view.material_shader;
        for(unsigned pass=0;pass<2;pass++){
            BeginDrawing();ClearBackground(blue);BeginMode3D(camera);
            if(pass)rlEnableBackfaceCulling();else rlDisableBackfaceCulling();
            assert(pe_actor_scene_view_draw(&view,0,(Vector3){0},0,1,WHITE));assert((culling()!=0)==(pass!=0));
            DrawCube((Vector3){0,0,-.25f},2,2,.02f,red);EndMode3D();EndDrawing();
        }
        char name[64];snprintf(name,sizeof name,"alpha-policy-%u",mode);Image im=shot(root,name);Color a=GetImageColor(im,160,240),b=GetImageColor(im,320,240),c=GetImageColor(im,480,240);
        if(mode==0){assert(same(a,blue)&&!same(b,red)&&same(c,WHITE));}
        if(mode==1){assert(same(a,WHITE)&&same(b,WHITE)&&same(c,WHITE));}
        if(mode==2){assert(same(a,red)&&same(b,red)&&same(c,WHITE));}
        if(mode==3){assert(same(a,red)&&same(b,WHITE)&&same(c,WHITE));}
        UnloadImage(im);
    }
    pe_actor_scene_view_unload(&view);rlEnableBackfaceCulling();
    puts("Actor material GPU checks: legacy depth holes reproduced; opaque gloss, source cutoffs and cull-state restoration pass");
}
static void portrait(const char *root,const char *name,const char *relative){
    PeActorSceneView view={0};char path[4096],error[256];pe_test_format(path,sizeof path,"%s/rebuild/build/reference-assets/%s/recording.peas",pe_test_root(),relative);
    if(!pe_actor_scene_view_load(&view,path,error,sizeof error)){fprintf(stderr,"%s: %s\n",path,error);abort();}
    unsigned qualified=0;for(size_t i=0;i<view.count;i++)qualified+=pe_actor_scene_part(view.manifest,i)->material.alpha_mode!=PE_ACTOR_ALPHA_LEGACY;assert(qualified);
    Vector3 lo,hi;assert(pe_actor_scene_view_bounds(&view,&lo,&hi));
    Camera3D camera={.target={0,hi.y-.24f,0},.up={0,1,0},.fovy=35,.projection=CAMERA_PERSPECTIVE};
    for(unsigned angle=0;angle<3;angle++)for(unsigned corrected=0;corrected<2;corrected++){
        float yaw=(float)angle*.6f-.6f;camera.position=(Vector3){1.05f*sinf(yaw),camera.target.y+.02f,1.05f*cosf(yaw)};
        for(size_t i=0;i<view.count;i++){
            view.parts[i].material=corrected?pe_actor_scene_part(view.manifest,i)->material:(PeActorMaterial){0};
            view.parts[i].model.materials[0].shader=corrected&&view.parts[i].material.alpha_mode!=PE_ACTOR_ALPHA_LEGACY?view.material_shader:(Shader){rlGetShaderIdDefault(),rlGetShaderLocsDefault()};
        }
        for(unsigned frame=0;frame<3;frame++){
            BeginDrawing();ClearBackground((Color){75,135,160,255});BeginMode3D(camera);
            Matrix projection=rlGetMatrixProjection();projection.m0=-projection.m0;rlSetMatrixProjection(projection);rlSetCullFace(RL_CULL_FACE_FRONT);
            PeActorBlendSample sample={0,0,1};assert(pe_actor_scene_view_draw_blend(&view,&sample,1,(Vector3){0},0,1,WHITE));
            EndMode3D();rlSetCullFace(RL_CULL_FACE_BACK);EndDrawing();
        }
        char filename[128];snprintf(filename,sizeof filename,"%s-%u-%s",name,angle,corrected?"corrected":"legacy");Image im=shot(root,filename);UnloadImage(im);
    }
    pe_actor_scene_view_unload(&view);
}
int main(int argc,char **argv){
    const char *display=getenv("DISPLAY"),*owned=getenv("PE_UI_PRIVATE_DISPLAY");
    if(argc!=2||!display||!owned||strcmp(display,owned)||getenv("WAYLAND_DISPLAY")||display[0]!=':'||atoi(display+1)<100||atoi(display+1)>999)return 2;
    InitWindow(640,480,"Source actor material regression");assert(IsWindowReady());
    synthetic(argv[1]);portrait(argv[1],"npc-20009","adventure-population/20009");portrait(argv[1],"player","player");portrait(argv[1],"tutorial-9004","npcs/9004");
    CloseWindow();puts("Actor material shared NPC/player renderer: PASS");return 0;
}
