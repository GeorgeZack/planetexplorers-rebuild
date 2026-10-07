#define _POSIX_C_SOURCE 200809L
#include "pe_vegetation_view.h"
#include "pe_vegetation.h"
#include "../generated/pe_vegetation_visual_source.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void copy_file(const char *from,const char *to){
    FILE *in=fopen(from,"rb"),*out=fopen(to,"wb");assert(in&&out);
    unsigned char bytes[4096];size_t count;
    while((count=fread(bytes,1,sizeof bytes,in)))assert(fwrite(bytes,1,count,out)==count);
    assert(!ferror(in)&&!fclose(in)&&!fclose(out));
}
int main(void){
    char error[200]={0};bool available=true;
    assert(!pe_vegetation_view_validate(NULL,&available,error,sizeof error)&&available);
    assert(!pe_vegetation_view_validate("",&available,error,sizeof error)&&available);
    assert(!pe_vegetation_view_validate(".",NULL,error,sizeof error));
    char directory[]="build/vegetation-view-fixture-XXXXXX";assert(mkdtemp(directory));
    assert(pe_vegetation_view_validate(directory,&available,error,sizeof error)&&!available);
    char group[512];assert(snprintf(group,sizeof group,"%s/sandbox-vegetation",directory)>0);
    assert(!mkdir(group,0700));available=true;
    assert(!pe_vegetation_view_validate(directory,&available,error,sizeof error)&&available); /* partial group, atomic */
    char bad[600];assert(snprintf(bad,sizeof bad,"%s/plant-15.obj",group)>0);FILE *f=fopen(bad,"wb");assert(f);
    assert(fputs("# malformed source model\nv nan 0 0\n",f)>=0&&!fclose(f));
    assert(!pe_vegetation_view_validate(directory,&available,error,sizeof error)&&available);
    assert(!unlink(bad)&&!rmdir(group));f=fopen(group,"wb");assert(f);assert(fputs("not a directory",f)>=0&&!fclose(f));
    assert(!pe_vegetation_view_validate(directory,&available,error,sizeof error)&&available);
    assert(!unlink(group)&&!rmdir(directory));
    /* Render and pointer bounds must name the same recovered prototype space. */
    assert(PE_VEGETATION_VISUAL_COUNT==4);
    for(unsigned i=0;i<PE_VEGETATION_VISUAL_COUNT;i++){
        const PeVegetationVisualSource *visual=&pe_vegetation_visual_sources[i];
        const PePlantSource *source=pe_vegetation_source(visual->prototype_id);assert(source);
        assert(source->minimum.x==visual->minimum[0]&&source->minimum.y==visual->minimum[1]&&source->minimum.z==visual->minimum[2]);
        assert(source->maximum.x==visual->maximum[0]&&source->maximum.y==visual->maximum[1]&&source->maximum.z==visual->maximum[2]);
        assert(visual->cutoff==0.649999976f&&visual->triangles>0);
    }
    const char *root=getenv("PE_VEGETATION_ASSETS");if(!root||!*root)root="build/reference-assets";
    available=false;
    if(!pe_vegetation_view_validate(root,&available,error,sizeof error)){fprintf(stderr,"Vegetation asset validation failed: %s\n",error);return 1;}
    if(!available){puts("SKIP actual vegetation models: optional sandbox-vegetation asset group absent (synthetic checks passed)");return 77;}
    /* Copy only the first model into a private fixture. A forged gigantic PNG
     * must be rejected from IHDR before allocation; valid tiny dimensions in
     * a truncated file must still fail the real decoder. No installed edits. */
    char corrupt[]="build/vegetation-image-fixture-XXXXXX";assert(mkdtemp(corrupt));
    assert(snprintf(group,sizeof group,"%s/sandbox-vegetation",corrupt)>0&&!mkdir(group,0700));
    char from[4096],to[600];
    for(int type=0;type<2;type++){
        const char *extension=type?"mtl":"obj";
        int n=snprintf(from,sizeof from,"%s/sandbox-vegetation/plant-15.%s",root,extension);assert(n>0&&(size_t)n<sizeof from);
        assert(snprintf(to,sizeof to,"%s/plant-15.%s",group,extension)>0);copy_file(from,to);
    }
    unsigned char header[24]="\x89PNG\r\n\x1a\n\0\0\0\x0dIHDR";
    header[17]=header[21]=1; /* 65536 x65536 */
    assert(snprintf(bad,sizeof bad,"%s/grassland-tree-a.png",group)>0);
    for(int attempt=0;attempt<2;attempt++){
        if(attempt){header[17]=header[21]=0;header[19]=header[23]=1;}
        f=fopen(bad,"wb");assert(f&&fwrite(header,1,sizeof header,f)==sizeof header&&!fclose(f));available=true;
        assert(!pe_vegetation_view_validate(corrupt,&available,error,sizeof error)&&available);
    }
    assert(!unlink(bad));
    for(int type=0;type<2;type++){assert(snprintf(to,sizeof to,"%s/plant-15.%s",group,type?"mtl":"obj")>0);assert(!unlink(to));}
    assert(!rmdir(group)&&!rmdir(corrupt));
    puts("Vegetation view: PASS (four actual OBJ/material/PNG groups, source bounds/cutoffs; no graphics)");return 0;
}
