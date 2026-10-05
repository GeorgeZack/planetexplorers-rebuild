#include "pe_training_terrain_view.h"
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

bool pe_training_terrain_view_update(PeTrainingTerrainView *view) {
    if(!view||!view->initialized||!pe_training_terrain_valid(&view->world))return false;
    if(view->collision&&view->rendered_revision==view->world.revision)return true;
    PeMeshData data={0};
    if(!pe_world_mesh(&view->world,&data))return false;
    if(data.count>INT_MAX/12){pe_mesh_free(&data);return false;}
    PeVec3 offset={(float)view->world.origin_x,(float)view->world.origin_y,(float)view->world.origin_z};
    char error[256];PeSceneCollision *collision=pe_scene_from_mesh(&data,offset,error,sizeof error);
    if(!collision){fprintf(stderr,"Training terrain collision: %s\n",error);pe_mesh_free(&data);return false;}
    Model candidate={0};
    if(data.count) {
        Mesh mesh={0};mesh.vertexCount=(int)data.count;mesh.triangleCount=(int)(data.count/3);
        mesh.vertices=MemAlloc((unsigned int)(data.count*3*sizeof(float)));
        mesh.normals=MemAlloc((unsigned int)(data.count*3*sizeof(float)));
        mesh.colors=MemAlloc((unsigned int)(data.count*4));
        if(!mesh.vertices||!mesh.normals||!mesh.colors) {
            MemFree(mesh.vertices);MemFree(mesh.normals);MemFree(mesh.colors);
            pe_scene_free(collision);pe_mesh_free(&data);return false;
        }
        for(size_t i=0;i<data.count;i++) {
            PeVertex v=data.vertices[i];
            mesh.vertices[i*3]=v.position.x;mesh.vertices[i*3+1]=v.position.y;mesh.vertices[i*3+2]=v.position.z;
            mesh.normals[i*3]=v.normal.x;mesh.normals[i*3+1]=v.normal.y;mesh.normals[i*3+2]=v.normal.z;
            /* Temporary diffuse dirt color; original terrain shader is not ported. */
            float light=.59f+.41f*fmaxf(0,v.normal.x*-.35f+v.normal.y*.85f+v.normal.z*.4f);
            mesh.colors[i*4]=(unsigned char)(145*light);mesh.colors[i*4+1]=(unsigned char)(111*light);
            mesh.colors[i*4+2]=(unsigned char)(81*light);mesh.colors[i*4+3]=255;
        }
        UploadMesh(&mesh,false);candidate=LoadModelFromMesh(mesh);
        if(!candidate.meshCount||!candidate.materialCount) {
            UnloadModel(candidate);pe_scene_free(collision);pe_mesh_free(&data);return false;
        }
    }
    if(view->loaded)UnloadModel(view->model);
    pe_scene_free(view->collision);view->collision=collision;view->model=candidate;
    view->loaded=data.count>0;view->triangles=data.count/3;view->rendered_revision=view->world.revision;
    pe_mesh_free(&data);return true;
}
void pe_training_terrain_view_draw(const PeTrainingTerrainView *view) {
    if(view&&view->loaded)DrawModel(view->model,(Vector3){view->world.origin_x,view->world.origin_y,view->world.origin_z},1,WHITE);
}
void pe_training_terrain_view_unload(PeTrainingTerrainView *view) {
    if(!view)return;
    if(view->loaded)UnloadModel(view->model);
    pe_scene_free(view->collision);memset(view,0,sizeof *view);
}
