/* Launch-only Linux x86_64 shim. No process attachment, code patching, or game
 * file edits. Interposes Unity's Mono symbol lookup and installs once at its
 * verified PeSingletonRunner.Update main-thread entry. Runtime-unverified until
 * the root operator explicitly runs the prepared isolated launch. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdatomic.h>
typedef void *(*Lookup)(void *,const char *);
typedef void *(*Invoke)(void *,void *,void **,void **);
static _Atomic(Invoke) original_invoke;
static _Atomic(void *) mono_handle;
static atomic_bool attempted;
static _Thread_local int installing;
static Lookup lookup(void){void *p=dlvsym(RTLD_NEXT,"dlsym","GLIBC_2.2.5");Lookup f;memcpy(&f,&p,sizeof f);return f;}
#define RESOLVE(name,ret,args) ret (*name) args; do{void *p=real(handle,#name);memcpy(&name,&p,sizeof name);if(!name){fprintf(stderr,"[pose] Missing Mono symbol: %s\n",#name);installing=0;return;}}while(0)
static void install_at_callback(void *method){
    if(installing||atomic_load(&attempted)||syscall(SYS_gettid)!=getpid())return;
    Lookup real=lookup();if(!real)return;
    void *handle=atomic_load(&mono_handle);
    RESOLVE(mono_method_get_name,const char *,(void *));
    RESOLVE(mono_method_get_class,void *,(void *));
    RESOLVE(mono_class_get_name,const char *,(void *));
    RESOLVE(mono_class_get_namespace,const char *,(void *));
    if(strcmp(mono_method_get_name(method),"Update"))return;
    void *klass=mono_method_get_class(method);
    if(strcmp(mono_class_get_name(klass),"PeSingletonRunner")||strcmp(mono_class_get_namespace(klass),"Pathea"))return;
    if(atomic_exchange(&attempted,1))return;
    installing=1;
    RESOLVE(mono_domain_get,void *,(void));
    RESOLVE(mono_domain_assembly_open,void *,(void *,const char *));
    RESOLVE(mono_assembly_get_image,void *,(void *));
    RESOLVE(mono_class_from_name,void *,(void *,const char *,const char *));
    RESOLVE(mono_class_get_method_from_name,void *,(void *,const char *,int));
    RESOLVE(mono_print_unhandled_exception,void,(void *));
    const char *path=getenv("PE_POSE_ASSEMBLY");void *domain=mono_domain_get();
    void *assembly=path&&domain?mono_domain_assembly_open(domain,path):NULL;
    void *probe=assembly?mono_class_from_name(mono_assembly_get_image(assembly),"PeReference","PoseProbe"):NULL;
    void *entry=probe?mono_class_get_method_from_name(probe,"Install",0):NULL;
    if(entry){void *error=NULL;atomic_load(&original_invoke)(entry,NULL,NULL,&error);
        if(error){fprintf(stderr,"[pose] Managed installation failed\n");mono_print_unhandled_exception(error);}
        else fprintf(stderr,"[pose] Probe installed on Unity main thread\n");
    }else fprintf(stderr,"[pose] Probe assembly/entry not found; disabled\n");
    installing=0;
}
static void *invoke_hook(void *method,void *object,void **args,void **exception){
    Invoke original=atomic_load(&original_invoke);
    if(!original)return NULL;
    install_at_callback(method);
    return original(method,object,args,exception);
}
void *dlsym(void *handle,const char *name){
    Lookup real=lookup();if(!real)return NULL;
    void *symbol=real(handle,name);
    if(strcmp(name,"mono_runtime_invoke")||!symbol)return symbol;
    const char *expected=getenv("PE_POSE_EXECUTABLE"),*expected_mono=getenv("PE_POSE_MONO");
    char executable[PATH_MAX],library[PATH_MAX];Dl_info info;
    if(!expected||!expected_mono||!realpath("/proc/self/exe",executable)||strcmp(executable,expected)||
       !dladdr(symbol,&info)||!realpath(info.dli_fname,library)||strcmp(library,expected_mono))return symbol;
    Invoke original;memcpy(&original,&symbol,sizeof original);atomic_store(&mono_handle,handle);atomic_store(&original_invoke,original);
    Invoke hook=invoke_hook;void *result;memcpy(&result,&hook,sizeof result);return result;
}
