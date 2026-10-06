/* Offline metadata only: no game executable or Unity native method invocation. */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#define LOAD(name,ret,args) ret (*name) args; do{void *p=dlsym(lib,#name);if(!p){fprintf(stderr,"Missing %s\n",#name);return 2;}memcpy(&name,&p,sizeof p);}while(0)
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"usage: inspect-unity-animation-api libmono.so Managed-dir\n");return 2;}
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_GLOBAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 2;}
    LOAD(mono_set_assemblies_path,void,(const char*));
    LOAD(mono_jit_init_version,void*,(const char*,const char*));
    LOAD(mono_domain_assembly_open,void*,(void*,const char*));
    LOAD(mono_assembly_get_image,void*,(void*));
    LOAD(mono_image_get_table_info,void*,(void*,int));
    LOAD(mono_table_info_get_rows,int,(void*));
    LOAD(mono_class_get,void*,(void*,unsigned));
    LOAD(mono_class_get_name,const char*,(void*));
    LOAD(mono_class_get_namespace,const char*,(void*));
    LOAD(mono_class_get_methods,void*,(void*,void**));
    LOAD(mono_method_full_name,char*,(void*,int));
    LOAD(mono_method_get_flags,unsigned,(void*,unsigned*));
    LOAD(mono_free,void,(void*));
    mono_set_assemblies_path(argv[2]);void *domain=mono_jit_init_version("shield-animation-api-metadata","v2.0.50727");
    char path[4096];if(snprintf(path,sizeof path,"%s/UnityEngine.dll",argv[2])>=(int)sizeof path)return 2;
    void *assembly=mono_domain_assembly_open(domain,path);if(!assembly)return 2;
    void *image=mono_assembly_get_image(assembly);int rows=mono_table_info_get_rows(mono_image_get_table_info(image,2));
    for(int i=1;i<=rows;i++){
        void *c=mono_class_get(image,0x02000000u+(unsigned)i);const char *name=mono_class_get_name(c),*space=mono_class_get_namespace(c);
        if(!strstr(name,"Animation")&&!strstr(name,"Animator")&&!strstr(name,"AvatarMask")&&!strstr(name,"HumanPose")&&strcmp(name,"Playable")&&strcmp(name,"AssetBundle"))continue;
        printf("TYPE %s.%s\n",space,name);void *it=NULL,*method;
        while((method=mono_class_get_methods(c,&it))){unsigned impl=0,flags=mono_method_get_flags(method,&impl);char *signature=mono_method_full_name(method,1);printf("METHOD flags=0x%x impl=0x%x %s\n",flags,impl,signature);mono_free(signature);}
    }
    return 0;
}
