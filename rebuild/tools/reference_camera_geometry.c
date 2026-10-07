/* CPU-only camera query/projection probe. No window or graphics initialization.
 * See docs/audit/CAMERA_GEOMETRY_GATE.md for source qualification and limits. */
#include "pe_scene_collision.h"
#include "raylib.h"
#include "rlgl.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
int main(void){
    char error[256];PeVertex vertices[6]={{.position={0,-10,-10}},{.position={0,10,-10}},{.position={0,10,10}},{.position={0,-10,-10}},{.position={0,10,10}},{.position={0,-10,10}}};
    PeMeshData mesh={0};mesh.vertices=vertices;mesh.count=6;
    PeSceneCollision *s=pe_scene_from_mesh(&mesh,(PeVec3){0},error,sizeof error);assert(s);
    PeVec3 feet={.05f,-.1f,0};assert(pe_scene_capsule_overlap(s,feet,.1f,.2f));
    PeSceneHit toward=pe_scene_capsule_sweep(s,feet,.1f,.2f,(PeVec3){-1,0,0});
    PeSceneHit away=pe_scene_capsule_sweep(s,feet,.1f,.2f,(PeVec3){1,0,0});
    assert(toward.hit&&toward.fraction==0&&!away.hit);
    printf("Native initial plane overlap: inward sweep hit=%d fraction=%g; separating sweep hit=%d\n",toward.hit,toward.fraction,away.hit);
    pe_scene_free(s);
    Camera c={{0,0,6},{0,0,0},{0,1,0},60,CAMERA_PERSPECTIVE};
    double saved_near=rlGetCullDistanceNear(),saved_far=rlGetCullDistanceFar();
    printf("Installed raylib default near=%g far=%g\n",saved_near,saved_far);
    for(int i=0;i<2;i++){
        rlSetClipPlanes(i?.8:.05,1000);
        Ray r=GetScreenToWorldRayEx((Vector2){800,300},c,1280,720);
        assert(r.position.x==c.position.x&&r.position.y==c.position.y&&r.position.z==c.position.z);
        printf("near=%g ray origin=(%g,%g,%g),direction=(%.9g,%.9g,%.9g)\n",rlGetCullDistanceNear(),r.position.x,r.position.y,r.position.z,r.direction.x,r.direction.y,r.direction.z);
    }
    rlSetClipPlanes(saved_near,saved_far);
    return 0;
}
