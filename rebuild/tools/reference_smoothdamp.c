/* Optional offline numerical oracle: invokes only the six-argument managed
 * Mathf.SmoothDamp in the installed UnityEngine.dll. No game executable,
 * Assembly-CSharp, scene, Time getter, graphics or desktop input is loaded.
 * Build: cc reference_smoothdamp.c -ldl -lm -o reference_smoothdamp
 * Usage: reference_smoothdamp /absolute/libmono.so /absolute/Managed */
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LOAD(name,ret,args) ret (*name) args; do {void *p=dlsym(lib,#name);if(!p){fprintf(stderr,"Missing %s\n",#name);return 2;}memcpy(&name,&p,sizeof p);}while(0)
int main(int argc,char **argv) {
    if(argc!=3||argv[1][0]!='/'||argv[2][0]!='/'){fprintf(stderr,"Usage: oracle /absolute/libmono.so /absolute/Managed\n");return 2;}
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_GLOBAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 2;}
    LOAD(mono_set_assemblies_path,void,(const char *));
    LOAD(mono_jit_init_version,void *,(const char *,const char *));
    LOAD(mono_domain_assembly_open,void *,(void *,const char *));
    LOAD(mono_assembly_get_image,void *,(void *));
    LOAD(mono_class_from_name,void *,(void *,const char *,const char *));
    LOAD(mono_class_get_method_from_name,void *,(void *,const char *,int));
    LOAD(mono_runtime_invoke,void *,(void *,void *,void **,void **));
    LOAD(mono_object_unbox,void *,(void *));
    mono_set_assemblies_path(argv[2]);
    void *domain=mono_jit_init_version("offline-smoothdamp-oracle","v2.0.50727");if(!domain)return 2;
    char path[4096];int n=snprintf(path,sizeof path,"%s/UnityEngine.dll",argv[2]);if(n<0||(size_t)n>=sizeof path)return 2;
    void *assembly=mono_domain_assembly_open(domain,path);if(!assembly)return 2;
    void *klass=mono_class_from_name(mono_assembly_get_image(assembly),"UnityEngine","Mathf");
    void *method=klass?mono_class_get_method_from_name(klass,"SmoothDamp",6):NULL;if(!method)return 2;
    const struct {float current,target,velocity,dt;int steps;} cases[]={
        {6,5.2f,0,1.f/60,1},{6,5.2f,0,1.f/60,30},{6,5.2f,0,1.f/60,60},
        {6,10,0,.1f,1},{6,1.8f,0,.001f,1},{6,20,0,1,1},
        {6,5.2f,-100,.1f,1},{6,6,12,.1f,1},{6,10,-3,.033f,1},
        {6,10,0,.1f,10},{6,10,0,1.f/30,30},{6,10,0,1.f/120,120}};
    for(size_t c=0;c<sizeof cases/sizeof cases[0];c++) {
        float value=cases[c].current,target=cases[c].target,velocity=cases[c].velocity,dt=cases[c].dt,smooth=.5f,max_speed=INFINITY;
        for(int step=0;step<cases[c].steps;step++) {
            void *args[]={&value,&target,&velocity,&smooth,&max_speed,&dt},*exception=NULL;
            void *boxed=mono_runtime_invoke(method,NULL,args,&exception);
            if(exception||!boxed){fprintf(stderr,"Managed SmoothDamp failed\n");return 1;}
            memcpy(&value,mono_object_unbox(boxed),sizeof value);
        }
        printf("{%#.9g, %#.9g, %#.9g, %#.9g, %d, %#.9g, %#.9g},\n",(double)cases[c].current,(double)target,(double)cases[c].velocity,(double)dt,cases[c].steps,(double)value,(double)velocity);
    }
    return 0;
}
