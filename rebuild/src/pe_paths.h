#ifndef PE_PATHS_H
#define PE_PATHS_H
/* Runtime paths for the supported native Linux build. No build-machine root. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static inline bool pe_paths_executable_directory(char *out,size_t cap) {
    if(!out||cap<2)return false;
    ssize_t count=readlink("/proc/self/exe",out,cap-1);
    if(count<=0||(size_t)count>=cap-1)return false;
    out[count]=0;
    char *slash=strrchr(out,'/');
    if(!slash)return false;
    slash[1]=0;
    return true;
}

static inline bool pe_paths_default_assets(char *out,size_t cap) {
    char directory[4096];
    if(!out||!pe_paths_executable_directory(directory,sizeof directory))return false;
    int n=snprintf(out,cap,"%sreference-assets",directory);
    return n>0&&(size_t)n<cap;
}

/* Reference fixtures and the optional source-logo fallback need the checkout.
 * Prefer the executable's ancestors, then cwd for temporary test binaries. */
static inline bool pe_paths_project_root(char *out,size_t cap) {
    if(!out||cap<2)return false;
    for(int attempt=0;attempt<2;attempt++) {
        char directory[4096];
        if(attempt==0) {
            if(!pe_paths_executable_directory(directory,sizeof directory))continue;
        } else if(!getcwd(directory,sizeof directory))continue;
        size_t length=strlen(directory);
        while(length>1&&directory[length-1]=='/')directory[--length]=0;
        for(;;) {
            char marker[4096];struct stat st;
            int n=snprintf(marker,sizeof marker,"%s/rebuild/Makefile",directory);
            if(n>0&&(size_t)n<sizeof marker&&stat(marker,&st)==0&&S_ISREG(st.st_mode)) {
                n=snprintf(out,cap,"%s",directory);
                return n>0&&(size_t)n<cap;
            }
            char *slash=strrchr(directory,'/');
            if(!slash||slash==directory)break;
            *slash=0;
        }
    }
    return false;
}
#endif
