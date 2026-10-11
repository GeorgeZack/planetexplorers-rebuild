#ifndef PE_TEST_PATHS_H
#define PE_TEST_PATHS_H
#include "../src/pe_paths.h"
#include <stdlib.h>
#include <stdarg.h>

static inline int pe_test_format(char *out,size_t cap,const char *format,...) {
    va_list args;va_start(args,format);
    int n=vsnprintf(out,cap,format,args);va_end(args);
    if(n<0||(size_t)n>=cap) {fputs("Reference fixture path too long\n",stderr);abort();}
    return n;
}

static inline const char *pe_test_root(void) {
    static char root[4096];
    if(!root[0]&&!pe_paths_project_root(root,sizeof root)) {
        fputs("Cannot locate checkout for reference fixtures\n",stderr);abort();
    }
    return root;
}

/* Fixture callers sometimes use several paths in one expression. Each return
 * survives the next 15 calls in this translation unit; never gameplay state. */
static inline const char *pe_test_path(const char *relative) {
    static char paths[16][4096];static unsigned int next;
    char *path=paths[next++%16];
    int n=snprintf(path,sizeof paths[0],"%s/%s",pe_test_root(),relative);
    if(n<0||(size_t)n>=sizeof paths[0]) {fputs("Reference fixture path too long\n",stderr);abort();}
    return path;
}
#endif
