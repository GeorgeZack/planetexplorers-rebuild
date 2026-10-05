#include "pe_sandbox_world_view.h"
#include "pe_reference.h"
#include "../generated/pe_tree_collider.h"
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PeSandboxWorldView {PeReferenceModel tree;Shader foliage;int cutoff;PeVec3 center;float radius,height;};
static void error_set(char *error,size_t capacity,const char *value){if(error&&capacity)snprintf(error,capacity,"%s",value);}
static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static bool valid_trees(const PeGame *g){
    if(!g||g->tree_count<0||g->tree_count>PE_MAX_TREES)return false;
    for(int i=0;i<g->tree_count;i++)if(!finite3(g->trees[i].position)||!isfinite(g->trees[i].health)||g->trees[i].remaining<0)return false;
    return true;
}
static uint32_t little_u32(const unsigned char *p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static float little_float(const unsigned char *p){uint32_t bits=little_u32(p);float value;memcpy(&value,&bits,4);return value;}
static bool load_collider(PeSandboxWorldView *v,const char *root){
    char path[4096];int n=snprintf(path,sizeof path,"%s/sandbox-world/tree-collider.pesc",root);
    if(n<0||(size_t)n>=sizeof path)return false;
    FILE *f=fopen(path,"rb");if(!f)return false;
    unsigned char b[36];bool ok=fread(b,1,sizeof b,f)==sizeof b&&fgetc(f)==EOF&&!ferror(f);fclose(f);
    if(!ok||memcmp(b,"PESC",4)||little_u32(b+4)!=1||little_u32(b+28)!=1||little_u32(b+32)!=1)return false;
    v->center=(PeVec3){little_float(b+8),little_float(b+12),little_float(b+16)};
    v->radius=little_float(b+20);v->height=little_float(b+24);
    /* Interaction/render provenance and headless movement must agree exactly.
     * A valid but different capsule is not this recovered tree asset. */
    return v->center.x==pe_tree_collider_source[0]&&v->center.y==pe_tree_collider_source[1]&&
        v->center.z==pe_tree_collider_source[2]&&v->radius==pe_tree_collider_source[3]&&v->height==pe_tree_collider_source[4];
}
static void release_tree(PeReferenceModel *tree){
    if(!tree->loaded)return;
    Model *m=&tree->model;
    for(int i=0;m->materials&&i<m->materialCount;i++)if(m->materials[i].maps){
        Texture2D texture=m->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
        if(!texture.id||texture.id==rlGetTextureIdDefault())continue;
        bool first=true;
        for(int j=0;j<i;j++)if(m->materials[j].maps&&m->materials[j].maps[MATERIAL_MAP_DIFFUSE].texture.id==texture.id)first=false;
        if(first)UnloadTexture(texture);
    }
    if(!m->meshes)m->meshCount=0;
    if(!m->materials)m->materialCount=0;
    UnloadModel(*m);tree->loaded=false;
}
void pe_sandbox_world_view_unload(PeSandboxWorldView *v){
    if(!v)return;
    release_tree(&v->tree);
    if(v->foliage.id&&v->foliage.id!=rlGetShaderIdDefault())UnloadShader(v->foliage);
    free(v);
}
PeSandboxWorldView *pe_sandbox_world_view_load(const char *root,char *error,size_t capacity){
    if(error&&capacity)*error=0;
    if(!root||!*root||!IsWindowReady()){error_set(error,capacity,"Sandbox trees require assets and an active graphics context");return NULL;}
    char path[4096];int n=snprintf(path,sizeof path,"%s/sandbox-world/tree.obj",root);
    if(n<0||(size_t)n>=sizeof path){error_set(error,capacity,"Sandbox tree path is too long");return NULL;}
    PeSandboxWorldView *v=calloc(1,sizeof *v);
    if(!v){error_set(error,capacity,"Sandbox tree allocation failed");return NULL;}
    if(!load_collider(v,root)){error_set(error,capacity,"Sandbox source tree collider is missing or malformed");goto fail;}
    if(!pe_reference_load(&v->tree,path,error,capacity))goto fail;
    Model *m=&v->tree.model;
    if(v->tree.triangles!=4216||m->meshCount!=1||!m->meshes||m->materialCount<1||!m->materials||!m->meshMaterial){
        error_set(error,capacity,"Sandbox source tree geometry is incomplete");goto fail;
    }
    int material=m->meshMaterial[0];Mesh *mesh=&m->meshes[0];
    if(material<0||material>=m->materialCount||!m->materials[material].maps||
       !mesh->vertices||!mesh->normals||!mesh->texcoords||!mesh->vboId||
       !mesh->vboId[0]||!mesh->vboId[1]||!mesh->vboId[2]||(mesh->indices&&!mesh->vboId[6])){
        error_set(error,capacity,"Sandbox tree mesh upload or material failed");goto fail;
    }
    if(mesh->triangleCount!=4216||mesh->vertexCount<3||mesh->vertexCount>12648||
       (!mesh->indices&&mesh->vertexCount!=mesh->triangleCount*3)){
        error_set(error,capacity,"Sandbox tree triangle channels changed");goto fail;
    }
    for(int i=0;i<mesh->vertexCount;i++)for(int axis=0;axis<3;axis++){
        float value=mesh->vertices[3*i+axis];
        if(!isfinite(value)||fabsf(value)>20){error_set(error,capacity,"Sandbox tree requires source-local vertices");goto fail;}
    }
    if(mesh->indices)for(int i=0;i<mesh->triangleCount*3;i++)if(mesh->indices[i]>=mesh->vertexCount){
        error_set(error,capacity,"Sandbox tree triangle index is invalid");goto fail;
    }
    Texture2D texture=m->materials[material].maps[MATERIAL_MAP_DIFFUSE].texture;
    if(!texture.id||texture.id==rlGetTextureIdDefault()||texture.width<1||texture.height<1){
        error_set(error,capacity,"Sandbox tree requires its recovered diffuse texture");goto fail;
    }
    v->foliage=LoadShaderFromMemory(NULL,
        "#version 330\nin vec2 fragTexCoord;in vec4 fragColor;\n"
        "uniform sampler2D texture0;uniform vec4 colDiffuse;uniform float cutoff;\n"
        "out vec4 finalColor;void main(){vec4 texel=texture(texture0,fragTexCoord);"
        "if(texel.a<cutoff)discard;finalColor=texel*colDiffuse*fragColor;}\n");
    v->cutoff=GetShaderLocation(v->foliage,"cutoff");
    if(!v->foliage.id||v->foliage.id==rlGetShaderIdDefault()||v->cutoff<0){
        error_set(error,capacity,"Sandbox source foliage shader failed");goto fail;
    }
    m->materials[material].shader=v->foliage;
    return v;
fail:pe_sandbox_world_view_unload(v);return NULL;
}
bool pe_sandbox_world_view_draw_trees(const PeSandboxWorldView *v,const PeGame *g){
    if(!v||!v->tree.loaded||!valid_trees(g))return false;
    const float cutoff=.25f; /* recovered Grassland_tree_B material _Cutoff */
    SetShaderValue(v->foliage,v->cutoff,&cutoff,SHADER_UNIFORM_FLOAT);
    for(int i=0;i<g->tree_count;i++)if(g->trees[i].remaining>0){
        PeVec3 p=g->trees[i].position;
        DrawModel(v->tree.model,(Vector3){p.x,p.y,p.z},1,WHITE);
    }
    return true;
}
static bool ray_box(const double o[3],const double d[3],BoundingBox box,double limit){
    const double lo[]={box.min.x,box.min.y,box.min.z},hi[]={box.max.x,box.max.y,box.max.z};
    double enter=0,leave=limit;
    for(int i=0;i<3;i++){
        if(fabs(d[i])<1e-14){if(o[i]<lo[i]||o[i]>hi[i])return false;continue;}
        double a=(lo[i]-o[i])/d[i],b=(hi[i]-o[i])/d[i];
        if(a>b){double swap=a;a=b;b=swap;}
        if(a>enter)enter=a;
        if(b<leave)leave=b;
        if(enter>leave)return false;
    }
    return true;
}
static double dot(const double a[3],const double b[3]){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static void cross(const double a[3],const double b[3],double o[3]){o[0]=a[1]*b[2]-a[2]*b[1];o[1]=a[2]*b[0]-a[0]*b[2];o[2]=a[0]*b[1]-a[1]*b[0];}
static bool ray_triangle(const double origin[3],const double dir[3],const float *a,const float *b,const float *c,double *distance){
    double e1[3],e2[3],s[3],p[3],q[3];
    for(int i=0;i<3;i++){e1[i]=(double)b[i]-a[i];e2[i]=(double)c[i]-a[i];s[i]=origin[i]-a[i];}
    cross(dir,e2,p);double determinant=dot(e1,p);
    if(fabs(determinant)<1e-12)return false;
    double u=dot(s,p)/determinant;if(u<0||u>1)return false;
    cross(s,e1,q);double w=dot(dir,q)/determinant;if(w<0||u+w>1)return false;
    double t=dot(e2,q)/determinant;if(t<0||t>*distance)return false;
    *distance=t;return true;
}
bool pe_sandbox_world_view_ray_tree(const PeSandboxWorldView *v,const PeGame *g,
    PeVec3 origin,PeVec3 direction,float max_distance,int *index,float *distance){
    if(!v||!v->tree.loaded||!valid_trees(g)||!finite3(origin)||!finite3(direction)||
       !isfinite(max_distance)||max_distance<0||!index||!distance)return false;
    double length=hypot(hypot((double)direction.x,direction.y),direction.z);
    if(length<1e-20)return false;
    const double d[]={direction.x/length,direction.y/length,direction.z/length};
    const Mesh *mesh=&v->tree.model.meshes[0];int selected=-1;double nearest=max_distance;
    for(int i=0;i<g->tree_count;i++)if(g->trees[i].remaining>0){
        PeVec3 p=g->trees[i].position;
        double o[]={(double)origin.x-p.x,(double)origin.y-p.y,(double)origin.z-p.z};
        if(!ray_box(o,d,v->tree.bounds,nearest))continue;
        for(int t=0;t<mesh->triangleCount;t++){
            unsigned a=mesh->indices?mesh->indices[3*t]:(unsigned)(3*t);
            unsigned b=mesh->indices?mesh->indices[3*t+1]:(unsigned)(3*t+1);
            unsigned c=mesh->indices?mesh->indices[3*t+2]:(unsigned)(3*t+2);
            double candidate=nearest;
            if(ray_triangle(o,d,mesh->vertices+3*a,mesh->vertices+3*b,mesh->vertices+3*c,&candidate)&&
               (selected<0||candidate<nearest)){nearest=candidate;selected=i;}
        }
    }
    *index=selected;*distance=(float)nearest;return true;
}
static void cap_sphere(const double p[3],const double d[3],double y,double radius,double *nearest){
    double q[]={p[0],p[1]-y,p[2]},b=dot(q,d),c=dot(q,q)-radius*radius,disc=b*b-c;
    if(disc<0)return;
    double t=-b-sqrt(disc);if(t>=0&&t<*nearest)*nearest=t;
}
bool pe_sandbox_world_view_ray_trunk(const PeSandboxWorldView *v,const PeGame *g,
    PeVec3 origin,PeVec3 direction,float max_distance,int *index,float *distance){
    if(!v||!v->tree.loaded||!valid_trees(g)||!finite3(origin)||!finite3(direction)||
       !isfinite(max_distance)||max_distance<0||!index||!distance)return false;
    double length=hypot(hypot((double)direction.x,direction.y),direction.z);
    if(length<1e-20)return false;
    double d[]={direction.x/length,direction.y/length,direction.z/length};
    double nearest=max_distance,half=(double)v->height*.5-v->radius,r=v->radius;int selected=-1;
    for(int i=0;i<g->tree_count;i++)if(g->trees[i].remaining>0){
        PeVec3 pos=g->trees[i].position;
        double p[]={(double)origin.x-pos.x-v->center.x,(double)origin.y-pos.y-v->center.y,(double)origin.z-pos.z-v->center.z};
        double y=fmax(fabs(p[1])-half,0);
        if(p[0]*p[0]+y*y+p[2]*p[2]<r*r)continue;
        double candidate=INFINITY,a=d[0]*d[0]+d[2]*d[2],b=p[0]*d[0]+p[2]*d[2];
        if(a>0){
            double disc=b*b-a*(p[0]*p[0]+p[2]*p[2]-r*r);
            if(disc>=0){double t=(-b-sqrt(disc))/a,h=p[1]+t*d[1];
                if(t>=0&&h>=-half&&h<=half)candidate=t;}
        }
        cap_sphere(p,d,-half,r,&candidate);cap_sphere(p,d,half,r,&candidate);
        if(candidate<=nearest&&(selected<0||candidate<nearest)){selected=i;nearest=candidate;}
    }
    *index=selected;*distance=(float)nearest;return true;
}
