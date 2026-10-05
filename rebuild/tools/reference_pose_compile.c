/* Offline compiler host for the installed game's Mono.CSharp library.
 * Loads no game executable or Assembly-CSharp; writes only compiler output. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LOAD(name,ret,args) ret (*name) args; do { void *p=dlsym(lib,#name); if(!p){fprintf(stderr,"Missing %s\n",#name);return 2;} memcpy(&name,&p,sizeof p); }while(0)
int main(int argc,char **argv){
    if(argc<4){fprintf(stderr,"usage: compiler libmono.so Managed-dir [--inspect | compiler args...]\n");return 2;}
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_GLOBAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 2;}
    LOAD(mono_set_assemblies_path,void,(const char *));
    LOAD(mono_jit_init_version,void *,(const char *,const char *));
    LOAD(mono_domain_assembly_open,void *,(void *,const char *));
    LOAD(mono_assembly_get_image,void *,(void *));
    LOAD(mono_class_from_name,void *,(void *,const char *,const char *));
    LOAD(mono_class_get_methods,void *,(void *,void **));
    LOAD(mono_method_get_name,const char *,(void *));
    LOAD(mono_method_signature,void *,(void *));
    LOAD(mono_signature_get_param_count,unsigned,(void *));
    LOAD(mono_class_get_method_from_name,void *,(void *,const char *,int));
    LOAD(mono_runtime_invoke,void *,(void *,void *,void **,void **));
    LOAD(mono_array_new,void *,(void *,void *,size_t));
    LOAD(mono_get_string_class,void *,(void));
    LOAD(mono_string_new,void *,(void *,const char *));
    LOAD(mono_array_addr_with_size,char *,(void *,int,size_t));
    LOAD(mono_gc_wbarrier_set_arrayref,void,(void *,void *,void *));
    LOAD(mono_print_unhandled_exception,void,(void *));
    LOAD(mono_object_unbox,void *,(void *));
    LOAD(mono_get_corlib,void *,(void));
    mono_set_assemblies_path(argv[2]);void *domain=mono_jit_init_version("reference-pose-compiler","v2.0.50727");
    char path[4096];if(snprintf(path,sizeof path,"%s/Mono.CSharp.dll",argv[2])>=(int)sizeof path)return 2;
    void *assembly=mono_domain_assembly_open(domain,path);if(!assembly){fprintf(stderr,"Cannot load compiler\n");return 2;}
    void *image=mono_assembly_get_image(assembly);
    if(!strcmp(argv[3],"--inspect")){
        const char *names[]={"Driver","CompilerCallableEntryPoint"};
        for(unsigned i=0;i<2;i++){void *c=mono_class_from_name(image,"Mono.CSharp",names[i]),*it=NULL,*m;
            while(c&&(m=mono_class_get_methods(c,&it)))printf("%s.%s/%u\n",names[i],mono_method_get_name(m),mono_signature_get_param_count(mono_method_signature(m)));}
        return 0;
    }
    void *klass=mono_class_from_name(image,"Mono.CSharp","CompilerCallableEntryPoint");
    void *method=klass?mono_class_get_method_from_name(klass,"InvokeCompiler",2):NULL;
    if(!method){fprintf(stderr,"Compiler entry point missing\n");return 2;}
    void *array=mono_array_new(domain,mono_get_string_class(),(size_t)(argc-3));
    for(int i=3;i<argc;i++){void *s=mono_string_new(domain,argv[i]);void *slot=mono_array_addr_with_size(array,sizeof(void *),(size_t)(i-3));mono_gc_wbarrier_set_arrayref(array,slot,s);}
    void *exception=NULL;
    void *console=mono_class_from_name(mono_get_corlib(),"System","Console");
    void *error_getter=mono_class_get_method_from_name(console,"get_Error",0);
    void *writer=mono_runtime_invoke(error_getter,NULL,NULL,&exception);
    if(exception){mono_print_unhandled_exception(exception);return 1;}
    void *args[]={array,writer};
    void *result=mono_runtime_invoke(method,NULL,args,&exception);
    if(exception){mono_print_unhandled_exception(exception);return 1;}
    return result&&*(unsigned char *)mono_object_unbox(result)?0:1;
}
