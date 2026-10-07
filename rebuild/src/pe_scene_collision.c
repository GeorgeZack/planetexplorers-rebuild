#include "pe_scene_collision.h"
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCENE_MAX_VERTICES 1000000u
#define SCENE_MAX_TRIANGLES 1000000u
#define SCENE_MAX_BYTES (256u*1024u*1024u)
#define SCENE_COORD_LIMIT 1000000.0f
typedef struct { PeVec3 lo,hi; } Box;
typedef struct { PeVec3 a,b,c; Box box; float key; uint32_t id,group; bool degenerate; } Triangle;
typedef struct { Box box; uint32_t start,count,left,right; } Node;
struct PeSceneCollision { Triangle *triangles; Node *nodes; size_t count,node_count; PeSceneStats stats; PeSceneGroup *groups; size_t group_count,group_capacity; };
static PeVec3 plus(PeVec3 a,PeVec3 b){return(PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 minus(PeVec3 a,PeVec3 b){return(PeVec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static PeVec3 times(PeVec3 a,double f){return(PeVec3){(float)(a.x*f),(float)(a.y*f),(float)(a.z*f)};}
static double dot(PeVec3 a,PeVec3 b){return(double)a.x*b.x+(double)a.y*b.y+(double)a.z*b.z;}
static PeVec3 cross(PeVec3 a,PeVec3 b){return(PeVec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static PeVec3 normalized(PeVec3 a){double n=sqrt(dot(a,a));return n>1e-14?times(a,1/n):(PeVec3){0,1,0};}
static bool finite_vec(PeVec3 a){return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z);}
static float component(PeVec3 p,int a){return a==0?p.x:a==1?p.y:p.z;}
static Box empty_box(void){return(Box){{FLT_MAX,FLT_MAX,FLT_MAX},{-FLT_MAX,-FLT_MAX,-FLT_MAX}};}
static Box include_point(Box b,PeVec3 p){b.lo=(PeVec3){fminf(b.lo.x,p.x),fminf(b.lo.y,p.y),fminf(b.lo.z,p.z)};b.hi=(PeVec3){fmaxf(b.hi.x,p.x),fmaxf(b.hi.y,p.y),fmaxf(b.hi.z,p.z)};return b;}
static Box include_box(Box a,Box b){return include_point(include_point(a,b.lo),b.hi);}
static bool boxes_overlap(Box a,Box b){return a.lo.x<=b.hi.x&&a.hi.x>=b.lo.x&&a.lo.y<=b.hi.y&&a.hi.y>=b.lo.y&&a.lo.z<=b.hi.z&&a.hi.z>=b.lo.z;}
static void fail(char *error,size_t cap,const char *message,size_t line){if(error&&cap)snprintf(error,cap,"%s (line %zu)",message,line);}
static bool grow(void **buffer,size_t *capacity,size_t count,size_t item,size_t limit){
    if(count<=*capacity)return true;
    size_t next=*capacity?*capacity*2:1024;if(next<count)next=count;if(next>limit)next=limit;
    if(next<count)return false;
    void *p=realloc(*buffer,next*item);if(!p)return false;*buffer=p;*capacity=next;return true;
}
static bool end_line(char *s);
static bool source_group(const char *name,PeSceneGroup *group){
    if(strncmp(name,"entity_",7))return false;
    const char *p=name+7;char *end;errno=0;
    unsigned long entity=strtoul(p,&end,10);
    if(errno||p==end||!isdigit((unsigned char)*p)||!entity||entity>UINT32_MAX||strncmp(end,"_component_",11))return false;
    p=end+11;errno=0;long long component=strtoll(p,&end,10);
    if(errno||p==end||!component||strncmp(end,"_sub_",5))return false;
    p=end+5;errno=0;unsigned long sub=strtoul(p,&end,10);
    if(errno||p==end||!isdigit((unsigned char)*p)||sub>UINT32_MAX||!end_line(end))return false;
    *group=(PeSceneGroup){.source_identity=true,.entity_id=(uint32_t)entity,.component_id=(int64_t)component};return true;
}
static bool new_group(PeSceneCollision *s,const char *name,uint32_t *index){
    PeSceneGroup g={0};bool source=name&&source_group(name,&g);
    if(source)for(size_t i=0;i<s->group_count;i++)if(s->groups[i].source_identity&&s->groups[i].entity_id==g.entity_id&&s->groups[i].component_id==g.component_id){*index=(uint32_t)i;return true;}
    /* Most terrain scenes own exactly one group; the vertex allocator's
     * initial1024 capacity would waste tens of MB across a route snapshot. */
    if(s->group_count==s->group_capacity){
        size_t capacity=s->group_capacity?s->group_capacity*2:4;
        if(capacity>SCENE_MAX_TRIANGLES)capacity=SCENE_MAX_TRIANGLES;
        if(capacity<=s->group_count)return false;
        PeSceneGroup *groups=realloc(s->groups,capacity*sizeof *groups);
        if(!groups)return false;
        s->groups=groups;s->group_capacity=capacity;
    }
    g.minimum=(PeVec3){FLT_MAX,FLT_MAX,FLT_MAX};g.maximum=(PeVec3){-FLT_MAX,-FLT_MAX,-FLT_MAX};
    *index=(uint32_t)s->group_count;s->groups[s->group_count++]=g;return true;
}
static void group_triangle(PeSceneCollision *s,const Triangle *t){
    PeSceneGroup *g=&s->groups[t->group];Box b=include_box((Box){g->minimum,g->maximum},t->box);g->minimum=b.lo;g->maximum=b.hi;
}
static char *space(char *s){while(isspace((unsigned char)*s))s++;return s;}
static bool end_line(char *s){s=space(s);return !*s||*s=='#';}
static int read_line(FILE *file,char *text,size_t capacity,size_t *bytes){
    size_t length=0;int ch;
    while((ch=fgetc(file))!=EOF){
        if(++*bytes>SCENE_MAX_BYTES||ch==0||length+1>=capacity)return -1;
        text[length++]=(char)ch;if(ch=='\n')break;
    }
    text[length]=0;return length?1:0;
}
static bool numbers(char *s,int minimum,int maximum,float values[4]){
    int count=0;
    while(!end_line(s)){
        if(count==maximum)return false;
        char *end;errno=0;float value=strtof(s,&end);
        if(end==s||errno==ERANGE||!isfinite(value)||fabsf(value)>SCENE_COORD_LIMIT||(*end&&!isspace((unsigned char)*end)&&*end!='#'))return false;
        values[count++]=value;s=space(end);
    }
    return count>=minimum;
}
static bool positive_index(char **s,size_t limit,uint32_t *value){
    if(!isdigit((unsigned char)**s))return false;
    char *end;errno=0;unsigned long n=strtoul(*s,&end,10);
    if(errno||n==0||n>limit)return false;
    *value=(uint32_t)(n-1);*s=end;return true;
}
static bool face_index(char **s,size_t nv,size_t nt,size_t nn,uint32_t *vertex){
    *s=space(*s);if(!positive_index(s,nv,vertex))return false;
    uint32_t ignored;
    if(**s=='/'){
        (*s)++;
        if(**s!='/'){if(!positive_index(s,nt,&ignored))return false;}
        if(**s=='/'){(*s)++;if(!positive_index(s,nn,&ignored))return false;}
    }
    return !**s||isspace((unsigned char)**s)||**s=='#';
}
static int key_compare(const void *a,const void *b){float x=((const Triangle *)a)->key,y=((const Triangle *)b)->key;return(x>y)-(x<y);}
static uint32_t build_node(PeSceneCollision *scene,uint32_t start,uint32_t count){
    uint32_t id=(uint32_t)scene->node_count++;Node *node=&scene->nodes[id];
    node->box=empty_box();node->start=start;node->count=count;
    for(uint32_t i=start;i<start+count;i++)node->box=include_box(node->box,scene->triangles[i].box);
    if(count<=8)return id;
    PeVec3 extent=minus(node->box.hi,node->box.lo);int axis=extent.y>extent.x?1:0;if(extent.z>component(extent,axis))axis=2;
    for(uint32_t i=start;i<start+count;i++){Triangle *t=&scene->triangles[i];t->key=(component(t->a,axis)+component(t->b,axis)+component(t->c,axis))/3;}
    qsort(scene->triangles+start,count,sizeof(Triangle),key_compare);
    node->count=0;node->left=build_node(scene,start,count/2);node->right=build_node(scene,start+count/2,count-count/2);return id;
}
PeSceneCollision *pe_scene_load_obj(const char *path,char *error,size_t cap){
    if(error&&cap)*error=0;
    if(!path){fail(error,cap,"Missing collision path",0);return NULL;}
    FILE *file=fopen(path,"rb");if(!file){fail(error,cap,"Cannot open collision OBJ",0);return NULL;}
    PeSceneCollision *scene=calloc(1,sizeof *scene);PeVec3 *vertices=NULL;size_t nv=0,nt=0,nn=0,vc=0,tc=0,line=0,bytes=0;
    const char *problem=scene?NULL:"Out of memory";char text[4096];uint32_t group=UINT32_MAX;
    while(!problem){
        int status=read_line(file,text,sizeof text,&bytes);if(!status)break;line++;
        if(status<0){problem="Collision OBJ byte/line limit or embedded NUL";break;}
        char *s=space(text);if(end_line(s))continue;
        char *arguments=s;while(*arguments&&!isspace((unsigned char)*arguments))arguments++;
        if(*arguments)*arguments++=0;
        arguments=space(arguments);
        float values[4]={0};
        if(!strcmp(s,"v")){
            if(!numbers(arguments,3,3,values)){problem="Invalid vertex";break;}
            if(!grow((void **)&vertices,&vc,nv+1,sizeof *vertices,SCENE_MAX_VERTICES)){problem="Vertex limit or allocation failure";break;}
            vertices[nv++]=(PeVec3){values[0],values[1],values[2]};
        }else if(!strcmp(s,"vt")||!strcmp(s,"vn")){
            bool uv=s[1]=='t';if(!numbers(arguments,uv?2:3,3,values)){problem="Invalid auxiliary vertex";break;}
            size_t *count=uv?&nt:&nn;if(++*count>SCENE_MAX_VERTICES){problem="Auxiliary vertex limit exceeded";break;}
        }else if(!strcmp(s,"f")){
            uint32_t indices[3];bool valid=true;
            for(int i=0;i<3;i++)if(!face_index(&arguments,nv,nt,nn,&indices[i])){valid=false;break;}
            if(!valid||!end_line(arguments)){problem="Invalid triangle or index";break;}
            if(!grow((void **)&scene->triangles,&tc,scene->count+1,sizeof *scene->triangles,SCENE_MAX_TRIANGLES)){problem="Triangle limit or allocation failure";break;}
            if(group==UINT32_MAX&&!new_group(scene,NULL,&group)){problem="Collision group allocation failed";break;}
            Triangle *t=&scene->triangles[scene->count];memset(t,0,sizeof *t);t->group=group;
            t->a=vertices[indices[0]];t->b=vertices[indices[1]];t->c=vertices[indices[2]];t->id=(uint32_t)scene->count++;
            t->box=include_point(include_point(include_point(empty_box(),t->a),t->b),t->c);
            group_triangle(scene,t);
            PeVec3 face=cross(minus(t->b,t->a),minus(t->c,t->a));t->degenerate=dot(face,face)<1e-16;
            scene->stats.degenerate_triangles+=t->degenerate;
        }else if(!strcmp(s,"o")){scene->stats.objects++;if(!new_group(scene,arguments,&group)){problem="Collision group allocation failed";break;}}
        else if(strcmp(s,"g")&&strcmp(s,"s")&&strcmp(s,"usemtl")&&strcmp(s,"mtllib")){problem="Unsupported OBJ directive";break;}
    }
    if(ferror(file))problem="Error reading collision OBJ";
    fclose(file);free(vertices);
    if(!problem&&(!scene->count||scene->count==scene->stats.degenerate_triangles))problem="Collision OBJ has no usable triangles";
    if(!problem){scene->nodes=calloc(scene->count*2,sizeof *scene->nodes);if(!scene->nodes)problem="Out of memory building collision BVH";}
    if(problem){fail(error,cap,problem,line);pe_scene_free(scene);return NULL;}
    build_node(scene,0,(uint32_t)scene->count);
    scene->stats.vertices=nv;scene->stats.triangles=scene->count;scene->stats.nodes=scene->node_count;
    scene->stats.minimum=scene->nodes[0].box.lo;scene->stats.maximum=scene->nodes[0].box.hi;return scene;
}
PeSceneCollision *pe_scene_from_grouped_mesh(const PeMeshData *mesh,PeVec3 origin,const uint32_t *groups,size_t group_count,char *error,size_t cap){
    if(error&&cap)*error=0;
    if(!mesh||mesh->count>SCENE_MAX_VERTICES||mesh->count%3||(mesh->count&&!mesh->vertices)||!finite_vec(origin)||
       group_count>SCENE_MAX_TRIANGLES||(mesh->count&&(!group_count||(group_count>1&&!groups)))){
        fail(error,cap,"Invalid collision triangle mesh",0);return NULL;
    }
    for(int axis=0;axis<3;axis++)if(fabsf(component(origin,axis))>SCENE_COORD_LIMIT){fail(error,cap,"Invalid collision origin",0);return NULL;}
    for(size_t i=0;i<mesh->count;i++){
        PeVec3 p=mesh->vertices[i].position;
        if(!finite_vec(p)){fail(error,cap,"Nonfinite collision position",i+1);return NULL;}
        for(int axis=0;axis<3;axis++)if(fabsf(component(p,axis))>SCENE_COORD_LIMIT||
            fabs((double)component(p,axis)+component(origin,axis))>SCENE_COORD_LIMIT){fail(error,cap,"Collision position exceeds bounds",i+1);return NULL;}
    }
    if(groups)for(size_t i=0;i<mesh->count/3;i++)if(groups[i]>=group_count){fail(error,cap,"Invalid collision group index",i);return NULL;}
    PeSceneCollision *scene=calloc(1,sizeof *scene);
    if(!scene){fail(error,cap,"Collision allocation failure",0);return NULL;}
    scene->count=mesh->count/3;scene->stats.vertices=mesh->count;scene->stats.triangles=scene->count;scene->stats.objects=scene->count?1:0;
    if(!scene->count)return scene;
    for(size_t i=0;i<group_count;i++){uint32_t index;if(!new_group(scene,NULL,&index)){fail(error,cap,"Collision group allocation failed",0);pe_scene_free(scene);return NULL;}}
    scene->triangles=calloc(scene->count,sizeof *scene->triangles);scene->nodes=calloc(scene->count*2,sizeof *scene->nodes);
    if(!scene->triangles||!scene->nodes){fail(error,cap,"Collision allocation failure",0);pe_scene_free(scene);return NULL;}
    for(size_t i=0;i<scene->count;i++){
        Triangle *t=&scene->triangles[i];t->a=plus(mesh->vertices[3*i].position,origin);t->b=plus(mesh->vertices[3*i+1].position,origin);t->c=plus(mesh->vertices[3*i+2].position,origin);t->id=(uint32_t)i;t->group=groups?groups[i]:0;
        t->box=include_point(include_point(include_point(empty_box(),t->a),t->b),t->c);
        group_triangle(scene,t);
        PeVec3 face=cross(minus(t->b,t->a),minus(t->c,t->a));t->degenerate=dot(face,face)<1e-16;scene->stats.degenerate_triangles+=t->degenerate;
    }
    if(scene->stats.degenerate_triangles==scene->count){fail(error,cap,"Collision mesh has no usable triangles",0);pe_scene_free(scene);return NULL;}
    build_node(scene,0,(uint32_t)scene->count);scene->stats.nodes=scene->node_count;
    scene->stats.minimum=scene->nodes[0].box.lo;scene->stats.maximum=scene->nodes[0].box.hi;return scene;
}
PeSceneCollision *pe_scene_from_mesh(const PeMeshData *mesh,PeVec3 origin,char *error,size_t cap){return pe_scene_from_grouped_mesh(mesh,origin,NULL,1,error,cap);}
void pe_scene_free(PeSceneCollision *scene){if(scene){free(scene->triangles);free(scene->nodes);free(scene->groups);free(scene);}}
size_t pe_scene_group_count(const PeSceneCollision *s){return s?s->group_count:0;}
bool pe_scene_group(const PeSceneCollision *s,size_t i,PeSceneGroup *out){if(!s||!out||i>=s->group_count)return false;*out=s->groups[i];return true;}
PeSceneStats pe_scene_stats(const PeSceneCollision *scene){return scene?scene->stats:(PeSceneStats){0};}
/* Separating-axis test: three box axes, triangle normal, and nine edge/box
 * cross products. Strict separation includes mere boundary contact. */
static bool triangle_box_axis(PeVec3 a,PeVec3 b,PeVec3 c,PeVec3 half,PeVec3 axis){
    if(dot(axis,axis)<1e-20)return true;
    double x=dot(a,axis),y=dot(b,axis),z=dot(c,axis);
    double radius=fabs(axis.x)*half.x+fabs(axis.y)*half.y+fabs(axis.z)*half.z;
    return fmin(x,fmin(y,z))<radius&&fmax(x,fmax(y,z))> -radius;
}
static bool triangle_box(const Triangle *t,Box box){
    if(t->degenerate||!boxes_overlap(t->box,box))return false;
    PeVec3 center=times(plus(box.lo,box.hi),.5),half=times(minus(box.hi,box.lo),.5);
    PeVec3 a=minus(t->a,center),b=minus(t->b,center),c=minus(t->c,center);
    PeVec3 axes[]={{1,0,0},{0,1,0},{0,0,1}},edges[]={minus(b,a),minus(c,b),minus(a,c)};
    for(int i=0;i<3;i++)if(!triangle_box_axis(a,b,c,half,axes[i]))return false;
    if(!triangle_box_axis(a,b,c,half,cross(edges[0],edges[1])))return false;
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)if(!triangle_box_axis(a,b,c,half,cross(edges[i],axes[j])))return false;
    return true;
}
bool pe_scene_overlaps_aabb(const PeSceneCollision *scene,PeVec3 lo,PeVec3 hi){
    if(!finite_vec(lo)||!finite_vec(hi)||lo.x>=hi.x||lo.y>=hi.y||lo.z>=hi.z)return true;
    if(!scene||!scene->count)return false;
    Box box={lo,hi};uint32_t stack[64];size_t count=0;stack[count++]=0;
    while(count){const Node *node=&scene->nodes[stack[--count]];
        if(!boxes_overlap(node->box,box))continue;
        if(!node->count){stack[count++]=node->left;stack[count++]=node->right;continue;}
        for(uint32_t i=node->start;i<node->start+node->count;i++)if(triangle_box(&scene->triangles[i],box))return true;
    }
    return false;
}
static bool box_ray(Box box,PeVec3 o,PeVec3 d,double maximum){
    double low=0,high=maximum;
    for(int a=0;a<3;a++){
        double origin=component(o,a),direction=component(d,a),minimum=component(box.lo,a),max=component(box.hi,a);
        if(fabs(direction)<1e-15){if(origin<minimum||origin>max)return false;continue;}
        double x=(minimum-origin)/direction,y=(max-origin)/direction;if(x>y){double t=x;x=y;y=t;}
        low=fmax(low,x);high=fmin(high,y);if(low>high)return false;
    }
    return true;
}
static bool triangle_ray(const Triangle *t,PeVec3 o,PeVec3 d,double maximum,double *distance){
    if(t->degenerate)return false;
    PeVec3 e=minus(t->b,t->a),f=minus(t->c,t->a),p=cross(d,f);double determinant=dot(e,p);
    if(fabs(determinant)<1e-12)return false;
    PeVec3 offset=minus(o,t->a);double u=dot(offset,p)/determinant;if(u< -1e-7||u>1+1e-7)return false;
    PeVec3 q=cross(offset,e);double v=dot(d,q)/determinant;if(v< -1e-7||u+v>1+1e-7)return false;
    double hit=dot(f,q)/determinant;if(hit<0||hit>maximum)return false;*distance=hit;return true;
}
PeSceneHit pe_scene_raycast(const PeSceneCollision *scene,PeVec3 origin,PeVec3 direction,float range){
    PeSceneHit hit={0};if(!scene||!scene->count||!finite_vec(origin)||!finite_vec(direction)||!isfinite(range)||range<0)return hit;
    double length=sqrt(dot(direction,direction));if(length<1e-14)return hit;direction=times(direction,1/length);
    uint32_t stack[64];size_t count=0;stack[count++]=0;double nearest=range;
    while(count){const Node *n=&scene->nodes[stack[--count]];if(!box_ray(n->box,origin,direction,nearest))continue;
        if(!n->count){stack[count++]=n->left;stack[count++]=n->right;continue;}
        for(uint32_t i=n->start;i<n->start+n->count;i++){
            const Triangle *t=&scene->triangles[i];double distance;
            if(!triangle_ray(t,origin,direction,nearest,&distance))continue;
            nearest=distance;hit.hit=true;hit.distance=(float)distance;hit.fraction=range>0?(float)(distance/range):0;hit.triangle=t->id;
            hit.position=plus(origin,times(direction,distance));hit.normal=normalized(cross(minus(t->b,t->a),minus(t->c,t->a)));
            if(dot(hit.normal,direction)>0)hit.normal=times(hit.normal,-1);
        }
    }
    return hit;
}
/* Closest point on a nondegenerate triangle (Voronoi regions). */
static PeVec3 point_triangle(PeVec3 p,const Triangle *t){
    PeVec3 ab=minus(t->b,t->a),ac=minus(t->c,t->a),ap=minus(p,t->a);double d1=dot(ab,ap),d2=dot(ac,ap);
    if(d1<=0&&d2<=0)return t->a;
    PeVec3 bp=minus(p,t->b);double d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return t->b;
    double vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return plus(t->a,times(ab,d1/(d1-d3)));
    PeVec3 cp=minus(p,t->c);double d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return t->c;
    double vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return plus(t->a,times(ac,d2/(d2-d6)));
    double va=d3*d6-d5*d4;if(va<=0&&d4-d3>=0&&d5-d6>=0)return plus(t->b,times(minus(t->c,t->b),(d4-d3)/((d4-d3)+(d5-d6))));
    double inv=1/(va+vb+vc);return plus(t->a,plus(times(ab,vb*inv),times(ac,vc*inv)));
}
static double clamp01(double x){return fmax(0,fmin(1,x));}
static void segments(PeVec3 p,PeVec3 q,PeVec3 a,PeVec3 b,PeVec3 *first,PeVec3 *second){
    PeVec3 d=minus(q,p),e=minus(b,a),r=minus(p,a);double aa=dot(d,d),ee=dot(e,e),f=dot(e,r),s=0,t=0;
    if(aa<=1e-20){t=ee>1e-20?clamp01(f/ee):0;}
    else{double c=dot(d,r);if(ee<=1e-20)s=clamp01(-c/aa);
        else{double be=dot(d,e),denominator=aa*ee-be*be;s=denominator>1e-20?clamp01((be*f-c*ee)/denominator):0;t=(be*s+f)/ee;
            if(t<0){t=0;s=clamp01(-c/aa);}else if(t>1){t=1;s=clamp01((be-c)/aa);}}}
    *first=plus(p,times(d,s));*second=plus(a,times(e,t));
}
static double segment_triangle(PeVec3 a,PeVec3 b,const Triangle *t,PeVec3 *axis,PeVec3 *surface){
    PeVec3 direction=minus(b,a);double fraction;
    if(triangle_ray(t,a,direction,1,&fraction)){*axis=*surface=plus(a,times(direction,fraction));return 0;}
    *axis=a;*surface=point_triangle(a,t);PeVec3 delta=minus(*axis,*surface);double best=dot(delta,delta);
    PeVec3 p=point_triangle(b,t);delta=minus(b,p);double squared=dot(delta,delta);if(squared<best){best=squared;*axis=b;*surface=p;}
    PeVec3 vertices[3]={t->a,t->b,t->c};
    for(int i=0;i<3;i++){PeVec3 u,v;segments(a,b,vertices[i],vertices[(i+1)%3],&u,&v);delta=minus(u,v);squared=dot(delta,delta);if(squared<best){best=squared;*axis=u;*surface=v;}}
    return sqrt(best);
}
static bool capsule_valid(PeVec3 feet,float radius,float height){return finite_vec(feet)&&isfinite(radius)&&isfinite(height)&&radius>0&&radius<=10000&&height>=2*radius&&height<=10000;}
static Box capsule_box(PeVec3 feet,float radius,float height){return(Box){{feet.x-radius,feet.y,feet.z-radius},{feet.x+radius,feet.y+height,feet.z+radius}};}
bool pe_scene_capsule_overlap(const PeSceneCollision *scene,PeVec3 feet,float radius,float height){
    if(!scene||!capsule_valid(feet,radius,height))return true;
    if(!scene->count)return false;
    Box bounds=capsule_box(feet,radius,height);PeVec3 a=plus(feet,(PeVec3){0,radius,0}),b=plus(feet,(PeVec3){0,height-radius,0});
    uint32_t stack[64];size_t count=0;stack[count++]=0;
    while(count){const Node *n=&scene->nodes[stack[--count]];if(!boxes_overlap(bounds,n->box))continue;
        if(!n->count){stack[count++]=n->left;stack[count++]=n->right;continue;}
        for(uint32_t i=n->start;i<n->start+n->count;i++){const Triangle *t=&scene->triangles[i];if(t->degenerate||!boxes_overlap(bounds,t->box))continue;PeVec3 axis,surface;
            if(segment_triangle(a,b,t,&axis,&surface)<radius-1e-5)return true;}}
    return false;
}
PeSceneHit pe_scene_capsule_sweep(const PeSceneCollision *scene,PeVec3 feet,float radius,float height,PeVec3 delta){
    PeSceneHit hit={0};if(!scene||!scene->count||!capsule_valid(feet,radius,height)||!finite_vec(delta))return hit;
    double length=sqrt(dot(delta,delta));if(length<1e-12)return hit;
    Box bounds=include_box(capsule_box(feet,radius,height),capsule_box(plus(feet,delta),radius,height));
    PeVec3 a=plus(feet,(PeVec3){0,radius,0}),b=plus(feet,(PeVec3){0,height-radius,0});double nearest=1;
    uint32_t stack[64];size_t count=0;stack[count++]=0;
    while(count){const Node *n=&scene->nodes[stack[--count]];if(!boxes_overlap(bounds,n->box))continue;
        if(!n->count){stack[count++]=n->left;stack[count++]=n->right;continue;}
        for(uint32_t i=n->start;i<n->start+n->count;i++){
            const Triangle *t=&scene->triangles[i];if(t->degenerate||!boxes_overlap(bounds,t->box))continue;
            double fraction=0;
            for(int iteration=0;iteration<48;iteration++){
                PeVec3 shift=times(delta,fraction),axis,surface;double distance=segment_triangle(plus(a,shift),plus(b,shift),t,&axis,&surface);
                PeVec3 normal=distance>1e-8?times(minus(axis,surface),1/distance):normalized(cross(minus(t->b,t->a),minus(t->c,t->a)));
                if(distance<=1e-8&&dot(normal,delta)>0)normal=times(normal,-1);
                double closing=-dot(delta,normal);if(closing<=length*1e-6)break;
                if(distance<=radius+1e-5){nearest=fraction;hit=(PeSceneHit){true,surface,normal,(float)(length*fraction),(float)fraction,t->id};break;}
                double step=(distance-radius)/closing;if(step<1e-9)step=1e-9;fraction+=step;
                if(fraction>nearest+1e-8)break;
                fraction=fmin(fraction,nearest);
            }
        }
    }
    return hit;
}

/* Dedicated camera query path. Do not use its touching/initial-contact rules
 * for the player motor. Group convexity is asserted by the snapshot adapter. */
static bool camera_inside_convex(const PeSceneCollision *s,uint32_t group,PeVec3 point){
    bool negative=false,positive=false,found=false;
    for(size_t i=0;i<s->count;i++){
        const Triangle *t=&s->triangles[i];if(t->group!=group||t->degenerate)continue;
        found=true;PeVec3 n=cross(minus(t->b,t->a),minus(t->c,t->a));
        double d=dot(n,minus(point,t->a)),epsilon=1e-6*sqrt(dot(n,n));
        positive|=d>epsilon;negative|=d< -epsilon;if(positive&&negative)return false;
    }
    return found;
}
static Box sphere_bounds(PeVec3 p,float r){return (Box){minus(p,(PeVec3){r,r,r}),plus(p,(PeVec3){r,r,r})};}
static void camera_initial_contacts(const PeSceneCollision *s,PeVec3 center,float radius,unsigned char *flags){
    Box bounds=sphere_bounds(center,radius);uint32_t stack[64];size_t count=0;stack[count++]=0;
    while(count){const Node *n=&s->nodes[stack[--count]];if(!boxes_overlap(bounds,n->box))continue;
        if(!n->count){stack[count++]=n->left;stack[count++]=n->right;continue;}
        for(uint32_t i=n->start;i<n->start+n->count;i++){
            const Triangle *t=&s->triangles[i];if(t->degenerate||!(flags[t->group]&PE_SCENE_CAMERA_ENABLED)||!boxes_overlap(bounds,t->box))continue;
            PeVec3 delta=minus(center,point_triangle(center,t));
            if(dot(delta,delta)<=(double)radius*radius+1e-10)flags[t->group]|=4;
        }
    }
}
bool pe_scene_camera_query(const PeSceneCollision *s,const PeCameraGeometryQuery *q,
    const unsigned char *policy,size_t groups,PeCameraGeometryHit *out){
    if(!s||!q||!out||groups!=s->group_count||(groups&&!policy)||
       q->kind<PE_CAMERA_GEOMETRY_RAY||q->kind>PE_CAMERA_GEOMETRY_SPHERE_CAST||
       !finite_vec(q->origin)||!finite_vec(q->direction)||!finite_vec(q->end)||
       !isfinite(q->distance)||q->distance<0||q->distance>SCENE_COORD_LIMIT||!isfinite(q->radius)||q->radius<0||q->radius>SCENE_COORD_LIMIT||
       ((q->kind==PE_CAMERA_GEOMETRY_OVERLAP||q->kind==PE_CAMERA_GEOMETRY_SPHERE_CAST)&&q->radius<=0))return false;
    for(int a=0;a<3;a++)if(fabsf(component(q->origin,a))>SCENE_COORD_LIMIT||fabsf(component(q->end,a))>SCENE_COORD_LIMIT||fabsf(component(q->direction,a))>SCENE_COORD_LIMIT)return false;
    for(size_t i=0;i<groups;i++)if(policy[i]&~3u)return false;
    PeCameraGeometryHit result={0};
    /* Most route regions miss a camera query entirely. Cull before allocating
     * any per-group scratch; the existing immutable root bounds are exact. */
    PeVec3 direction=q->kind==PE_CAMERA_GEOMETRY_LINE?minus(q->end,q->origin):q->direction;
    double length=sqrt(dot(direction,direction));
    double range=q->kind==PE_CAMERA_GEOMETRY_LINE?length:q->distance;
    if(!isfinite(length)||!isfinite(range))return false;
    if(q->kind==PE_CAMERA_GEOMETRY_LINE&&fabs(range-q->distance)>1e-5*fmax(1,range))return false;
    if(!s->count){*out=result;return true;}
    if(q->kind!=PE_CAMERA_GEOMETRY_OVERLAP&&(length<1e-14||range==0)){*out=result;return true;}
    if(length>=1e-14)direction=times(direction,1/length);
    bool sphere=q->kind==PE_CAMERA_GEOMETRY_SPHERE_CAST;
    PeVec3 delta=times(direction,range);Box bounds=include_box(sphere_bounds(q->origin,q->radius),sphere_bounds(plus(q->origin,delta),q->radius));
    if(!finite_vec(delta)||!finite_vec(bounds.lo)||!finite_vec(bounds.hi))return false;
    bool candidate=q->kind==PE_CAMERA_GEOMETRY_OVERLAP?boxes_overlap(sphere_bounds(q->origin,q->radius),s->nodes[0].box):
        sphere?boxes_overlap(bounds,s->nodes[0].box):box_ray(s->nodes[0].box,q->origin,direction,range);
    if(!candidate){*out=result;return true;}
    unsigned char local_flags[64],*heap=groups>sizeof local_flags?malloc(groups):NULL;
    if(groups>sizeof local_flags&&!heap)return false;
    unsigned char *flags=heap?heap:local_flags;memcpy(flags,policy,groups);
    /* Box/ramp/qualified convex interiors are solids; merged concave buildings
     * remain surface colliders, so an ordinary room is never one giant solid. */
    for(size_t i=0;i<groups;i++)if((flags[i]&3)==3){
        PeSceneGroup g=s->groups[i];Box b={g.minimum,g.maximum};
        if(boxes_overlap(b,(Box){q->origin,q->origin})&&camera_inside_convex(s,(uint32_t)i,q->origin))flags[i]|=4;
    }
    if(q->kind==PE_CAMERA_GEOMETRY_OVERLAP||q->kind==PE_CAMERA_GEOMETRY_SPHERE_CAST)
        camera_initial_contacts(s,q->origin,q->radius,flags);
    if(q->kind==PE_CAMERA_GEOMETRY_OVERLAP){
        for(size_t i=0;i<groups;i++)if((flags[i]&5)==5){result.hit=true;break;}
        free(heap);*out=result;return true;
    }
    double nearest=range;
    uint32_t stack[64];size_t count=0;stack[count++]=0;
    while(count){const Node *n=&s->nodes[stack[--count]];
        if(sphere?!boxes_overlap(bounds,n->box):!box_ray(n->box,q->origin,direction,nearest))continue;
        if(!n->count){stack[count++]=n->left;stack[count++]=n->right;continue;}
        for(uint32_t i=n->start;i<n->start+n->count;i++){
            const Triangle *t=&s->triangles[i];if(t->degenerate||!(flags[t->group]&1)||(flags[t->group]&4))continue;
            if(!sphere){double d;if(triangle_ray(t,q->origin,direction,nearest,&d)){result.hit=true;nearest=d;}continue;}
            if(!boxes_overlap(bounds,t->box))continue;
            double travel=0;
            for(int iteration=0;iteration<64;iteration++){
                PeVec3 point=plus(q->origin,times(direction,travel)),surface=point_triangle(point,t),offset=minus(point,surface);
                double d=sqrt(dot(offset,offset));
                if(d<=q->radius+1e-5){nearest=travel;result.hit=true;break;}
                PeVec3 normal=times(offset,1/d);double closing=-dot(direction,normal);
                if(closing<=1e-8)break;
                double advance=(d-q->radius)/closing;if(advance<1e-9)advance=1e-9;
                travel+=advance;if(travel>nearest+1e-8)break;travel=fmin(travel,nearest);
            }
        }
    }
    result.distance=result.hit?fminf((float)nearest,q->distance):0;free(heap);
    if(!isfinite(result.distance)||result.distance<0||result.distance>q->distance)return false;
    *out=result;return true;
}
