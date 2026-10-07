/* CPU matrix agreement with the installed renderer; no window/display/GPU. */
#include "pe_camera_projection.h"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"projection renderer:%d: %s\n",__LINE__,#c);return 1;}}while(0)
int main(void){
    double old_near=rlGetCullDistanceNear(),old_far=rlGetCullDistanceFar();
    rlSetClipPlanes(PE_CAMERA_RENDER_NEAR,PE_CAMERA_RENDER_FAR);
    const int sizes[][2]={{1280,720},{640,480},{320,240},{954,1048}};
    Camera3D camera={{4,12,30},{7,13,26},{0,1,0},60,CAMERA_PERSPECTIVE};
    for(unsigned s=0;s<4;s++){
        int w=sizes[s][0],h=sizes[s][1];
        PeCameraProjection p={{camera.position.x,camera.position.y,camera.position.z},{3,1,-4},{0,1,0},w,h,60,PE_CAMERA_RENDER_NEAR};
        for(int y=0;y<=4;y++)for(int x=0;x<=4;x++){
            Vector2 pixel={w*x/4.f,h*y/4.f};PeCameraRay ray;CHECK(pe_camera_projection_ray(&p,pixel.x,pixel.y,&ray));
            Ray renderer=GetScreenToWorldRayEx((Vector2){w-pixel.x,pixel.y},camera,w,h);
            Vector3 direction={ray.direction.x,ray.direction.y,ray.direction.z};CHECK(Vector3Distance(direction,renderer.direction)<.0003f);
            Vector3 point={ray.origin.x+ray.direction.x*5,ray.origin.y+ray.direction.y*5,ray.origin.z+ray.direction.z*5};
            Vector2 projected=GetWorldToScreenEx(point,camera,w,h);
            CHECK(fabsf(w-projected.x-pixel.x)<.004f&&fabsf(projected.y-pixel.y)<.004f);
        }
    }
    rlSetClipPlanes(old_near,old_far);
    puts("camera renderer: reflected pixel/ray matrix agreement at four viewports passed without graphics");return 0;
}
