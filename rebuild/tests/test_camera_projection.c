#include "pe_camera_projection.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void){
    const float sizes[][2]={{1280,720},{640,480},{320,240},{954,1048}};
    for(unsigned s=0;s<4;s++)for(int pitch=0;pitch<2;pitch++){
        PeCameraProjection p={{3,4,5},{0,pitch?.6f:0,pitch?-.8f:-1},{0,1,0},sizes[s][0],sizes[s][1],60,PE_CAMERA_RENDER_NEAR};
        for(int y=0;y<=4;y++)for(int x=0;x<=4;x++){
            float px=p.width*x/4,py=p.height*y/4;PeCameraRay ray;
            assert(pe_camera_projection_ray(&p,px,py,&ray));
            PeVec3 delta={ray.origin.x-p.position.x,ray.origin.y-p.position.y,ray.origin.z-p.position.z};
            assert(fabsf(delta.x*p.forward.x+delta.y*p.forward.y+delta.z*p.forward.z-p.near_clip)<1e-5f);
            PeVec3 target={ray.origin.x+ray.direction.x*8,ray.origin.y+ray.direction.y*8,ray.origin.z+ray.direction.z*8};float sx,sy;
            assert(pe_camera_projection_screen(&p,target,&sx,&sy)&&fabsf(sx-px)<.002f&&fabsf(sy-py)<.002f);
        }
        PeCameraRay right;assert(pe_camera_projection_ray(&p,p.width,p.height*.5f,&right)&&right.direction.x<0);
    }
    PeCameraProjection bad={{0},{0,0,-1},{0,1,0},0,720,60,.03};PeCameraRay out={{1,2,3},{4,5,6}},before=out;
    assert(!pe_camera_projection_ray(&bad,100,100,&out)&&!memcmp(&before,&out,sizeof out));
    puts("camera projection: reflected near-plane rays and screen round trips at four sizes pass");return 0;
}
