#include "pe_camera_query.h"
#include "../generated/pe_camera_query_source.h"
#include "../generated/pe_camera_geometry_source.h"
#include <math.h>
#include <stdlib.h>

static bool group_policy(const PeSceneCollision *scene,size_t index,PeSandboxSceneKind kind,uint32_t mask,unsigned char *out){
    unsigned layer;bool convex=false;
    if(kind==PE_SANDBOX_SCENE_TERRAIN)layer=12;
    else if(kind==PE_SANDBOX_SCENE_CONSTRUCTION){layer=13;convex=true;}
    else{
        PeSceneGroup group;if(!pe_scene_group(scene,index,&group)||!group.source_identity)return false;
        const PeCameraQuerySourceGroup *source=NULL;
        for(size_t i=0;i<sizeof pe_camera_query_source_groups/sizeof *pe_camera_query_source_groups;i++)
            if(pe_camera_query_source_groups[i].entity_id==group.entity_id&&pe_camera_query_source_groups[i].component_id==group.component_id){source=&pe_camera_query_source_groups[i];break;}
        if(!source)return false;
        layer=source->layer;convex=source->convex_solid;
    }
    *out=(mask&(UINT32_C(1)<<layer))?(unsigned char)(PE_SCENE_CAMERA_ENABLED|(convex?PE_SCENE_CAMERA_CONVEX_SOLID:0)):0;return true;
}
bool pe_camera_query(void *opaque,const PeCameraGeometryQuery *request,PeCameraGeometryHit *out){
    const PeCameraQueryContext *context=opaque;
    if(!context||!request||!out||request->mask!=PE_CAMERA_GEOMETRY_MASK||!request->ignore_triggers)return false;
    size_t count=pe_sandbox_collision_scene_count(context->collision);if(!count)return false;
    PeCameraGeometryHit result={0};
    for(size_t i=0;i<count;i++){
        const PeSceneCollision *scene;PeSandboxSceneKind kind;
        if(!pe_sandbox_collision_scene(context->collision,i,&scene,&kind))return false;
        if(!scene){if(kind!=PE_SANDBOX_SCENE_STATIC)return false;continue;}
        size_t groups=pe_scene_group_count(scene);unsigned char local_flags[64],*heap=groups>sizeof local_flags?malloc(groups):NULL;
        if(groups>sizeof local_flags&&!heap)return false;
        unsigned char *flags=heap?heap:local_flags;
        for(size_t j=0;j<groups;j++)if(!group_policy(scene,j,kind,request->mask,&flags[j])){free(heap);return false;}
        PeCameraGeometryHit hit;bool ok=pe_scene_camera_query(scene,request,flags,groups,&hit);free(heap);
        if(!ok)return false;
        if(hit.hit&&(!result.hit||hit.distance<result.distance))result=hit;
        /* Continue even after overlap/zero-distance hit: a later unknown scene
         * is failure, never silently hidden by an earlier successful query. */
    }
    *out=result;return true;
}
