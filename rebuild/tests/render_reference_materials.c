#define _POSIX_C_SOURCE 200809L
#include "pe_test_paths.h"
#include "pe_reference.h"
#include "pe_sandbox_collision.h"
#include <assert.h>
#include <raymath.h>
#include <rlgl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const Color blue={40,100,160,255},red={230,20,30,255};
static void write_file(const char *path,const char *text){FILE *f=fopen(path,"wb");assert(f);assert(fputs(text,f)>=0&&fclose(f)==0);}
static uint64_t hash_file(const char *path){int size=0;unsigned char *bytes=LoadFileData(path,&size);assert(bytes);uint64_t hash=UINT64_C(14695981039346656037);for(int i=0;i<size;i++){hash^=bytes[i];hash*=UINT64_C(1099511628211);}UnloadFileData(bytes);return hash;}
static Image shot(const char *root,const char *name){char path[4096];pe_test_format(path,sizeof path,"%s/%s.png",root,name);Image image=LoadImageFromScreen();assert(image.data&&ExportImage(image,path));return image;}
static unsigned char culling(void){typedef unsigned char(*Query)(unsigned int);void *p=rlGetProcAddress("glIsEnabled");Query fn;memcpy(&fn,&p,sizeof fn);assert(fn);return fn(0x0b44);}
static unsigned cull_face(void){typedef void(*Query)(unsigned int,int *);void *p=rlGetProcAddress("glGetIntegerv");Query fn;memcpy(&fn,&p,sizeof fn);assert(fn);int mode=0;fn(0x0b45,&mode);return (unsigned)mode;}
static bool same(Color a,Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b;}
static void synthetic(const char *root){
    char obj[4096],mtl[4096],png[4096],side[4096],text[1024],error[256];
    pe_test_format(obj,sizeof obj,"%s/fixture.obj",root);pe_test_format(mtl,sizeof mtl,"%s/fixture.mtl",root);
    pe_test_format(png,sizeof png,"%s/fixture.png",root);pe_test_format(side,sizeof side,"%s/fixture.obj.materials",root);
    write_file(obj,"# Baked Unity XYZ meters; original vertex/normal coordinate convention\nmtllib fixture.mtl\no panel\nv -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvn 0 0 1\nvn 0 0 1\nvn 0 0 1\nvn 0 0 1\nusemtl mAM\nf 1/1/1 2/2/2 3/3/3\nf 1/1/1 3/3/3 4/4/4\n");
    write_file(mtl,"# Source material slots; diffuse-only preview conversion\nnewmtl mAM\nKd 1 1 1\nd 1\nillum 1\nmap_Kd fixture.png\n");
    Image image=GenImageColor(12,4,(Color){255,255,255,0});ImageDrawRectangle(&image,4,0,4,4,(Color){255,255,255,127});ImageDrawRectangle(&image,8,0,4,4,WHITE);assert(ExportImage(image,png));UnloadImage(image);
    snprintf(text,sizeof text,"PERM1 %016llx %016llx\nmAM fixture.png cutout 0.7 1 1 %016llx\n",(unsigned long long)hash_file(obj),(unsigned long long)hash_file(mtl),(unsigned long long)hash_file(png));write_file(side,text);
    PeReferenceModel model={0};if(!pe_reference_load(&model,obj,error,sizeof error)){fprintf(stderr,"fixture load: %s\n",error);abort();}assert(model.source_materials);
    int index=model.material_offset;
    SetTextureFilter(model.model.materials[index].maps[MATERIAL_MAP_DIFFUSE].texture,TEXTURE_FILTER_POINT);
    PeActorMaterial source=model.source_materials->entries[0].policy;
    Camera3D camera={.position={0,0,5},.target={0},.up={0,1,0},.fovy=2,.projection=CAMERA_ORTHOGRAPHIC};
    for(unsigned mode=0;mode<3;mode++){
        model.source_materials->entries[0].policy=mode==0?(PeActorMaterial){0}:mode==1?(PeActorMaterial){PE_ACTOR_ALPHA_OPAQUE,0,1,true}:source;
        model.model.materials[index].shader=mode?model.material_shader:(Shader){rlGetShaderIdDefault(),rlGetShaderLocsDefault()};
        for(unsigned pass=0;pass<3;pass++){
            BeginDrawing();ClearBackground(blue);BeginMode3D(camera);
            if(pass)rlEnableBackfaceCulling();else rlDisableBackfaceCulling();
            rlSetCullFace(pass==2?RL_CULL_FACE_FRONT:RL_CULL_FACE_BACK);unsigned before=cull_face();
            pe_reference_draw(&model);assert((culling()!=0)==(pass!=0));assert(cull_face()==before);
            /* Restore ordinary culling for the deliberately later background. */
            rlSetCullFace(RL_CULL_FACE_BACK);DrawCube((Vector3){0,0,-.25f},2,2,.02f,red);EndMode3D();EndDrawing();
            if(pass==1){
                char name[64];snprintf(name,sizeof name,"building-alpha-%u",mode);Image im=shot(root,name);
                Color a=GetImageColor(im,160,240),b=GetImageColor(im,320,240),c=GetImageColor(im,480,240);
                if(mode==0)assert(same(a,blue)&&!same(b,red)&&same(c,WHITE));
                if(mode==1)assert(same(a,WHITE)&&same(b,WHITE)&&same(c,WHITE));
                if(mode==2)assert(same(a,red)&&same(b,red)&&same(c,WHITE));
                UnloadImage(im);
            }
        }
    }
    /* Reverse-side cloth remains visible, with the same cutout policy. */
    camera.position.z=-5;
    for(unsigned frame=0;frame<2;frame++){BeginDrawing();ClearBackground(blue);BeginMode3D(camera);rlEnableBackfaceCulling();pe_reference_draw(&model);EndMode3D();EndDrawing();}
    Image reverse=shot(root,"building-cloth-backface");assert(same(GetImageColor(reverse,160,240),WHITE));UnloadImage(reverse);
    pe_reference_unload(&model);rlEnableBackfaceCulling();
    puts("Static material GPU checks: legacy depth holes reproduced; opaque alpha, cutout and both faces pass; culling state/face restored");
}
static void building_views(const char *root,const char *relative,Vector3 origin){
    PeReferenceModel model={0};char error[256];assert(pe_reference_load(&model,pe_test_path(relative),error,sizeof error));
    assert(model.source_materials);PeActorMaterial policies[PE_REFERENCE_MAX_MATERIALS];
    for(size_t i=0;i<model.source_materials->count;i++)policies[i]=model.source_materials->entries[i].policy;
    const char *names[]={"dwelling","store","hotel","store-underside"};
    const Vector3 eyes[]={{-595,31,394},{-594,29,361},{-603,30,413},{-576,29,355}};
    const Vector3 targets[]={{-584,32,388},{-583,31,356},{-601,31,408},{-583,32,356}};
    for(unsigned view=0;view<4;view++)for(unsigned corrected=0;corrected<2;corrected++){
        for(size_t i=0;i<model.source_materials->count;i++){
            model.source_materials->entries[i].policy=corrected?policies[i]:(PeActorMaterial){0};
            model.model.materials[i+model.material_offset].shader=corrected&&policies[i].alpha_mode!=PE_ACTOR_ALPHA_LEGACY?model.material_shader:(Shader){rlGetShaderIdDefault(),rlGetShaderLocsDefault()};
        }
        Camera3D camera={.position=Vector3Subtract(eyes[view],origin),.target=Vector3Subtract(targets[view],origin),.up={0,1,0},.fovy=60};
        for(unsigned frame=0;frame<3;frame++){
            BeginDrawing();ClearBackground(blue);BeginMode3D(camera);Matrix p=rlGetMatrixProjection();p.m0=-p.m0;rlSetMatrixProjection(p);rlSetCullFace(RL_CULL_FACE_FRONT);
            pe_reference_draw(&model);EndMode3D();rlSetCullFace(RL_CULL_FACE_BACK);EndDrawing();
        }
        char name[128];snprintf(name,sizeof name,"%s-%s-%s",origin.x==-640?"route":"settlement",names[view],corrected?"corrected":"legacy");Image image=shot(root,name);UnloadImage(image);
    }
    pe_reference_unload(&model);
}
static void mixed_materials(const char *root){
    char obj[4096],mtl[4096],png[4096],side[4096],text[4096],error[256];
    pe_test_format(obj,sizeof obj,"%s/mixed.obj",root);pe_test_format(mtl,sizeof mtl,"%s/mixed.mtl",root);
    pe_test_format(png,sizeof png,"%s/fixture.png",root);pe_test_format(side,sizeof side,"%s/mixed.obj.materials",root);
    FILE *f=fopen(obj,"wb");assert(f);fputs("# Baked Unity XYZ meters; original vertex/normal coordinate convention\nmtllib mixed.mtl\n",f);
    const char *symbols[]={"mAM","mAN","mAO"};
    for(unsigned i=0;i<3;i++){
        float left=(float)i-1.5f;unsigned base=4*i+1;
        fprintf(f,"o panel%u\nv %g -1 0\nv %g -1 0\nv %g 1 0\nv %g 1 0\n",i,left,left+1,left+1,left);
        fputs("vt 0.5 0.5\nvt 0.5 0.5\nvt 0.5 0.5\nvt 0.5 0.5\nvn 0 0 1\nvn 0 0 1\nvn 0 0 1\nvn 0 0 1\n",f);
        fprintf(f,"usemtl %s\nf %u/%u/%u %u/%u/%u %u/%u/%u\nf %u/%u/%u %u/%u/%u %u/%u/%u\n",symbols[i],base,base,base,base+1,base+1,base+1,base+2,base+2,base+2,base,base,base,base+2,base+2,base+2,base+3,base+3,base+3);
    }
    assert(!fclose(f));f=fopen(mtl,"wb");assert(f);fputs("# Source material slots; diffuse-only preview conversion\n",f);
    for(unsigned i=0;i<3;i++)fprintf(f,"newmtl %s\nKd 1 1 1\nd 0.25\nillum 1\nmap_Kd fixture.png\n",symbols[i]);
    assert(!fclose(f));
    unsigned long long hash=(unsigned long long)hash_file(png);
    snprintf(text,sizeof text,"PERM1 %016llx %016llx\nmAM fixture.png opaque 0 1 0 %016llx\nmAN fixture.png cutout 0.4 1 0 %016llx\nmAO fixture.png cutout 0.7 1 1 %016llx\n",(unsigned long long)hash_file(obj),(unsigned long long)hash_file(mtl),hash,hash,hash);write_file(side,text);
    PeReferenceModel model={0};assert(pe_reference_load(&model,obj,error,sizeof error));
    for(int i=model.material_offset;i<model.model.materialCount;i++)assert(model.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color.a==255);
    Camera3D camera={.position={0,0,5},.target={0},.up={0,1,0},.fovy=3,.projection=CAMERA_ORTHOGRAPHIC};
    for(unsigned frame=0;frame<3;frame++){BeginDrawing();ClearBackground(blue);BeginMode3D(camera);pe_reference_draw(&model);DrawCube((Vector3){0,0,-.25f},4,3,.02f,red);EndMode3D();EndDrawing();}
    Image image=shot(root,"building-mixed-materials");assert(same(GetImageColor(image,160,240),WHITE));assert(same(GetImageColor(image,320,240),WHITE));assert(same(GetImageColor(image,480,240),red));UnloadImage(image);pe_reference_unload(&model);
    puts("Static material mapping: ordered policies with shared texture and differing cutoffs pass; source alpha never blends output");
}
static void label_views(const char *root){
    PeWorld *world=calloc(1,sizeof *world);assert(world);world->revision=1;
    PeSandboxCollision *collision=pe_sandbox_collision_create();assert(collision);
    PeSandboxBox wall={{29,0,31},{35,5,32}};char error[256];
    assert(pe_sandbox_collision_update(collision,world,NULL,0,&wall,1,error,sizeof error));
    Camera3D camera={.position={32,2.5f,28},.target={32,2,34},.up={0,1,0},.fovy=60};
    PeVec3 blocked={32,2.5f,34},clear={31,2.5f,30};
    assert(!pe_sandbox_collision_label_visible(collision,(PeVec3){32,2.5f,28},blocked));
    assert(pe_sandbox_collision_label_visible(collision,(PeVec3){32,2.5f,28},clear));
    for(unsigned corrected=0;corrected<2;corrected++){
        for(unsigned frame=0;frame<3;frame++){
            BeginDrawing();ClearBackground(blue);BeginMode3D(camera);DrawCube((Vector3){32,2.5f,31.5f},6,5,1,GRAY);EndMode3D();
            Vector2 pos=GetWorldToScreen((Vector3){32,2.5f,34},camera);
            if(!corrected||pe_sandbox_collision_label_visible(collision,(PeVec3){32,2.5f,28},blocked))DrawText("NPC behind wall",(int)pos.x-85,(int)pos.y,18,YELLOW);
            pos=GetWorldToScreen((Vector3){31,2.5f,30},camera);DrawText("Visible NPC",(int)pos.x-50,(int)pos.y,18,YELLOW);EndDrawing();
        }
        Image image=shot(root,corrected?"labels-corrected":"labels-legacy");UnloadImage(image);
    }
    pe_sandbox_collision_free(collision);pe_world_destroy(world);free(world);
    puts("Native NPC label check: wall hides blocked anchor; clear anchor stays visible");
}
int main(int argc,char **argv){
    const char *display=getenv("DISPLAY"),*owned=getenv("PE_UI_PRIVATE_DISPLAY");
    if(argc!=2||!display||!owned||strcmp(display,owned)||getenv("WAYLAND_DISPLAY")||display[0]!=':'||atoi(display+1)<100||atoi(display+1)>999)return 2;
    InitWindow(640,480,"Static source material regression");assert(IsWindowReady());
    synthetic(argv[1]);
    mixed_materials(argv[1]);
    building_views(argv[1],"rebuild/build/reference-assets/adventure-route/route.obj",(Vector3){-640,0,64});
    building_views(argv[1],"rebuild/build/reference-assets/adventure-settlement/settlement.obj",(Vector3){-627,16,323});
    label_views(argv[1]);
    CloseWindow();puts("Static material native rendering: PASS");return 0;
}
