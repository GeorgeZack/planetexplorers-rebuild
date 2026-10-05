#include "pe_actor_scene.h"
#include "pe_actor_scene_view.h"
#include <math.h>
#include <stdio.h>

static bool screenshot_write(const char *path){
    Image image=LoadImageFromScreen();bool ok=image.data&&ExportImage(image,path);
    if(image.data)UnloadImage(image);
    return ok;
}
int pe_actor_scene_preview(const char *path,int smoke,const char *screenshot){
    if(!path||!*path||smoke<0||smoke>36000||(screenshot&&!*screenshot)||IsWindowReady()){
        fprintf(stderr,"Invalid actor scene preview arguments/context\n");return 1;
    }
    char error[256];PeActorSceneView scene={0};
    if(!pe_actor_scene_view_prepare(&scene,path,error,sizeof error)){fprintf(stderr,"Actor scene: %s\n",error);return 1;}
    size_t count=pe_actor_scene_view_part_count(&scene);bool failed=false,opened=false,captured=false;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);InitWindow(1100,720,"Planet Explorers - captured multipart actor");
    if(!IsWindowReady()){fprintf(stderr,"Cannot open actor scene preview\n");failed=true;goto done;}opened=true;SetTargetFPS(60);
    if(!pe_actor_scene_view_upload(&scene,error,sizeof error)){fprintf(stderr,"Actor scene upload: %s\n",error);failed=true;goto done;}
    Vector3 lo,hi;pe_actor_scene_view_bounds(&scene,&lo,&hi);
    float min[3]={lo.x,lo.y,lo.z},max[3]={hi.x,hi.y,hi.z};
    Vector3 target={(min[0]+max[0])*.5f,(min[1]+max[1])*.5f,(min[2]+max[2])*.5f};
    float dx=max[0]-min[0],dy=max[1]-min[1],dz=max[2]-min[2];
    float radius=fmaxf(.1f,.5f*sqrtf(dx*dx+dy*dy+dz*dz));
    float distance=3.1f*radius,yaw=.78539816f,pitch=.26f;
    Camera3D camera={.target=target,.up={0,1,0},.fovy=45,.projection=CAMERA_PERSPECTIVE};
    size_t clip_index=0,clip_count=pe_actor_scene_view_clip_count(&scene);
    const PeActorClip *clip=pe_actor_scene_view_clip(&scene);double seconds=0;bool paused=false;int frames=0;
    while(!WindowShouldClose()&&(!smoke||frames<smoke)){
        bool step=false;
        if(!smoke&&IsWindowFocused()){
            if(IsKeyPressed(KEY_SPACE))paused=!paused;
            if(IsKeyPressed(KEY_R))seconds=0;
            int selection=IsKeyPressed(KEY_DOWN)?1:IsKeyPressed(KEY_UP)?-1:0;
            if(selection){
                clip_index=selection>0?(clip_index+1)%clip_count:(clip_index+clip_count-1)%clip_count;
                clip=pe_actor_scene_view_clip_at(&scene,clip_index);seconds=0;step=true;
            }
            if(IsKeyPressed(KEY_RIGHT)){seconds=fmin(clip->duration,(floor(seconds*clip->fps+1e-6)+1)/clip->fps);paused=true;step=true;}
            if(IsKeyPressed(KEY_LEFT)){seconds=fmax(0,(ceil(seconds*clip->fps-1e-6)-1)/clip->fps);paused=true;step=true;}
            if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)){Vector2 d=GetMouseDelta();yaw-=d.x*.006f;pitch=fmaxf(-1.2f,fminf(1.45f,pitch+d.y*.006f));}
            distance*=powf(.9f,GetMouseWheelMove());distance=fmaxf(radius*.75f,fminf(radius*12,distance));
        }
        camera.position=(Vector3){target.x+distance*cosf(pitch)*sinf(yaw),target.y+distance*sinf(pitch),target.z+distance*cosf(pitch)*cosf(yaw)};
        BeginDrawing();ClearBackground((Color){26,33,40,255});BeginMode3D(camera);DrawGrid(20,fmaxf(radius*.4f,.1f));
        if(!pe_actor_scene_view_draw_clip(&scene,clip_index,seconds,(Vector3){0},0,1,WHITE))failed=true;
        EndMode3D();DrawRectangle(0,0,GetScreenWidth(),84,(Color){13,22,29,230});
        DrawText("Captured player pose reference",22,14,25,RAYWHITE);
        DrawText(TextFormat("%s (%zu/%zu) | %zu parts | %.3f / %.3f s | frame %u / %u%s",clip->name,clip_index+1,clip_count,count,seconds,(double)clip->duration,
            (unsigned)fmin((double)clip->frames-1,floor(seconds*clip->fps)),clip->frames,paused?" | paused":""),22,49,18,(Color){191,208,215,255});
        DrawText("Space: pause  R: restart  Left/right: frame  Up/down: clip  Right drag: orbit  Wheel: zoom",22,GetScreenHeight()-34,18,RAYWHITE);
        EndDrawing();frames++;
        if(screenshot&&smoke&&frames==smoke){captured=screenshot_write(screenshot);if(!captured)failed=true;}
        if(failed)break;
        if(smoke||(!paused&&!step&&IsWindowFocused())){
            double delta=smoke?1.0/60.0:fmin(GetFrameTime(),.1f);
            if(isfinite(delta)&&delta>=0)seconds+=delta;
            if(seconds>=clip->duration){if(clip->loop)seconds=fmod(seconds,clip->duration);else{seconds=clip->duration;paused=true;}}
        }
    }
    if(smoke&&frames<smoke){fprintf(stderr,"Actor scene smoke closed early (%d/%d)\n",frames,smoke);failed=true;}
    if(screenshot&&!smoke&&!failed&&frames)captured=screenshot_write(screenshot);
    if(screenshot&&!captured)failed=true;
    printf("Actor scene preview: parts=%zu frames=%d clip=%s result=%s\n",count,frames,clip->name,failed?"FAIL":"PASS");
done:
    pe_actor_scene_view_unload(&scene);
    if(opened)CloseWindow();
    return failed?1:0;
}
