#define _POSIX_C_SOURCE 200809L
#include "pe_legacy.h"
#include <lz4.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* VFDataReader.cs: file pieces ordered X,Z,Y; chunk bodies X,Y,Z;
 * sample bodies X-fast. LZ4 is raw-block framing, not an LZ4 frame stream. */
#define PIECE_MAX (64u*PE_LEGACY_CHUNK_BYTES+256u)
struct PeLegacyFile {
    FILE *stream;
    PeLegacyInfo info;
    uint32_t *offsets;
    size_t pieces;
    int nx,ny;
    unsigned char *compressed,*decoded;
    size_t compressed_capacity;
    int64_t cached;
    bool uniform;
    size_t decoded_size;
};
static void message(char *out,size_t cap,const char *s) {if(out&&cap)snprintf(out,cap,"%s",s);}
static uint32_t u32(const unsigned char *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
PeLegacyInfo pe_legacy_info(const PeLegacyFile *f) {return f?f->info:(PeLegacyInfo){0};}
void pe_legacy_close(PeLegacyFile *f) {if(f){if(f->stream)fclose(f->stream);free(f->offsets);free(f->compressed);free(f->decoded);free(f);}}
PeLegacyFile *pe_legacy_open(const char *path,int lod,char *error,size_t cap) {
    if(!path||!path[0]||lod<0||lod>4){message(error,cap,"Expected a terrain path and LOD 0 through 4.");return NULL;}
    PeLegacyFile *f=calloc(1,sizeof *f);
    if(!f){message(error,cap,"Out of memory opening terrain.");return NULL;}
    f->cached=-1;f->nx=48>>lod;f->ny=23>>lod;f->pieces=(size_t)f->nx*f->nx*f->ny;
    f->info=(PeLegacyInfo){lod,1<<lod,f->nx*128*(1<<lod),f->ny*128*(1<<lod),f->nx*128*(1<<lod)};
    f->stream=fopen(path,"rb");
    struct stat st;
    if(!f->stream||fstat(fileno(f->stream),&st)!=0||!S_ISREG(st.st_mode)||st.st_size<0||
       (uint64_t)st.st_size>UINT32_MAX||(uint64_t)st.st_size<f->pieces*6u){message(error,cap,"Missing, oversized, or truncated terrain file.");pe_legacy_close(f);return NULL;}
    f->offsets=malloc((f->pieces+1)*sizeof *f->offsets);
    unsigned char *header=malloc(f->pieces*4);
    if(!f->offsets||!header){free(header);message(error,cap,"Out of memory reading terrain offsets.");pe_legacy_close(f);return NULL;}
    if(fread(header,4,f->pieces,f->stream)!=f->pieces){free(header);message(error,cap,"Could not read terrain offsets.");pe_legacy_close(f);return NULL;}
    for(size_t i=0;i<f->pieces;i++)f->offsets[i]=u32(header+4*i);
    free(header);f->offsets[f->pieces]=(uint32_t)st.st_size;
    bool ok=f->offsets[0]==f->pieces*4;
    for(size_t i=0;ok&&i<f->pieces;i++) {
        uint32_t a=f->offsets[i],b=f->offsets[i+1];
        ok=a<b&&b<=(uint64_t)st.st_size&&(b-a==2||(b-a>2&&b-a<=(uint32_t)LZ4_compressBound(PIECE_MAX)));
    }
    if(!ok){message(error,cap,"Terrain piece offsets are invalid.");pe_legacy_close(f);return NULL;}
    message(error,cap,"");return f;
}
static bool load_piece(PeLegacyFile *f,size_t piece,char *error,size_t cap) {
    if(f->cached==(int64_t)piece)return true;
    f->cached=-1;
    size_t bytes=f->offsets[piece+1]-f->offsets[piece];
    if(bytes>f->compressed_capacity){unsigned char *p=realloc(f->compressed,bytes);if(!p){message(error,cap,"Out of memory reading compressed terrain.");return false;}f->compressed=p;f->compressed_capacity=bytes;}
    if(fseeko(f->stream,(off_t)f->offsets[piece],SEEK_SET)!=0||fread(f->compressed,1,bytes,f->stream)!=bytes){message(error,cap,"Terrain piece could not be read.");return false;}
    if(bytes==2){f->uniform=true;f->cached=(int64_t)piece;return true;}
    if(!f->decoded){f->decoded=malloc(PIECE_MAX);if(!f->decoded){message(error,cap,"Out of memory decompressing terrain.");return false;}}
    int size=LZ4_decompress_safe((const char *)f->compressed,(char *)f->decoded,(int)bytes,PIECE_MAX);
    if(size<384||u32(f->decoded)!=256){message(error,cap,"Invalid compressed terrain piece.");return false;}
    for(int i=0;i<64;i++) {
        uint32_t a=u32(f->decoded+4*i),b=i==63?(uint32_t)size:u32(f->decoded+4*(i+1));
        if(a<256||a>=b||b>(uint32_t)size||(b-a!=2&&b-a!=PE_LEGACY_CHUNK_BYTES)){message(error,cap,"Invalid decompressed chunk offsets or size.");return false;}
    }
    f->uniform=false;f->decoded_size=(size_t)size;f->cached=(int64_t)piece;return true;
}
static bool chunk_body(PeLegacyFile *f,int cx,int cy,int cz,const unsigned char **body,bool *uniform,char *error,size_t cap) {
    if(!f||cx<0||cy<0||cz<0||cx>=f->nx*4||cy>=f->ny*4||cz>=f->nx*4){message(error,cap,"Chunk coordinates are outside the terrain file.");return false;}
    size_t piece=((size_t)(cx/4)*f->nx+(size_t)(cz/4))*f->ny+(size_t)(cy/4);
    if(!load_piece(f,piece,error,cap))return false;
    if(f->uniform){*body=f->compressed;*uniform=true;return true;}
    int i=16*(cx%4)+4*(cy%4)+(cz%4);
    uint32_t a=u32(f->decoded+4*i),b=i==63?(uint32_t)f->decoded_size:u32(f->decoded+4*(i+1));
    *body=f->decoded+a;*uniform=b-a==2;return true;
}
bool pe_legacy_chunk(PeLegacyFile *f,int cx,int cy,int cz,uint8_t out[PE_LEGACY_CHUNK_BYTES],char *error,size_t cap) {
    if(!out){message(error,cap,"Missing chunk output.");return false;}
    const unsigned char *body;bool uniform;
    if(!chunk_body(f,cx,cy,cz,&body,&uniform,error,cap))return false;
    if(uniform)for(size_t i=0;i<PE_LEGACY_CHUNK_BYTES;i+=2){out[i]=body[0];out[i+1]=body[1];}
    else memcpy(out,body,PE_LEGACY_CHUNK_BYTES);
    message(error,cap,"");return true;
}
bool pe_legacy_sample(PeLegacyFile *f,int x,int y,int z,PeVoxel *out,char *error,size_t cap) {
    if(!f||!out||x<0||y<0||z<0||x>=f->info.extent_x||y>=f->info.extent_y||z>=f->info.extent_z||
       x%f->info.sample_step||y%f->info.sample_step||z%f->info.sample_step){message(error,cap,"Sample is outside the file or unaligned for its LOD.");return false;}
    x/=f->info.sample_step;y/=f->info.sample_step;z/=f->info.sample_step;
    const unsigned char *body;bool uniform;
    if(!chunk_body(f,x/32,y/32,z/32,&body,&uniform,error,cap))return false;
    size_t i=uniform?0:2u*((size_t)(z%32+1)*35*35+(size_t)(y%32+1)*35+(size_t)(x%32+1));
    *out=(PeVoxel){body[i],body[i+1]};message(error,cap,"");return true;
}
bool pe_legacy_crop(PeLegacyFile *f,int ox,int oy,int oz,PeWorld *out,char *error,size_t cap) {
    if(!f||!out||f->info.lod!=0||ox<0||oy<0||oz<0||ox>f->info.extent_x-PE_WORLD_X||oy>f->info.extent_y-PE_WORLD_Y||oz>f->info.extent_z-PE_WORLD_Z){message(error,cap,"Crop requires LOD0 and a complete in-bounds 64x32x64 region.");return false;}
    PeWorld *next=calloc(1,sizeof *next);
    if(!next){message(error,cap,"Out of memory importing terrain crop.");return false;}
    next->seed=0;next->revision=1;next->source=1;next->origin_x=ox;next->origin_y=oy;next->origin_z=oz;
    for(int cz=oz/32;cz<=(oz+PE_WORLD_Z-1)/32;cz++)for(int cy=oy/32;cy<=(oy+PE_WORLD_Y-1)/32;cy++)for(int cx=ox/32;cx<=(ox+PE_WORLD_X-1)/32;cx++) {
        const unsigned char *body;bool uniform;
        if(!chunk_body(f,cx,cy,cz,&body,&uniform,error,cap)){free(next);return false;}
        for(int z=0;z<32;z++)for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
            int dx=cx*32+x-ox,dy=cy*32+y-oy,dz=cz*32+z-oz;
            if(dx<0||dy<0||dz<0||dx>=PE_WORLD_X||dy>=PE_WORLD_Y||dz>=PE_WORLD_Z)continue;
            size_t s=uniform?0:2u*((size_t)(z+1)*35*35+(size_t)(y+1)*35+(size_t)(x+1));
            next->voxels[((size_t)dz*PE_WORLD_Y+(size_t)dy)*PE_WORLD_X+(size_t)dx]=(PeVoxel){body[s],body[s+1]};
        }
    }
    *out=*next;free(next);message(error,cap,"");return true;
}
bool pe_legacy_find_surface(PeLegacyFile *f,int *out_x,int *out_y,int *out_z,char *error,size_t cap) {
    if(!f||!out_x||!out_y||!out_z){message(error,cap,"Missing surface search arguments.");return false;}
    /* Dense chunks contain authored surfaces. Uniform pieces are skipped without
     * decompression. Candidate scan starts centrally to avoid thin edge slivers. */
    static const int scan[7]={16,12,20,8,24,4,28};
    for(size_t piece=0;piece<f->pieces;piece++) {
        if(f->offsets[piece+1]-f->offsets[piece]==2)continue;
        if(!load_piece(f,piece,error,cap))return false;
        int px=(int)(piece/((size_t)f->nx*f->ny)),pz=(int)(piece/f->ny)%f->nx,py=(int)(piece%f->ny);
        for(int ci=0;ci<64;ci++) {
            uint32_t a=u32(f->decoded+4*ci),b=ci==63?(uint32_t)f->decoded_size:u32(f->decoded+4*(ci+1));
            if(b-a==2)continue;
            const unsigned char *d=f->decoded+a;
            for(int zi=0;zi<7;zi++)for(int xi=0;xi<7;xi++)for(int y=29;y>=0;y--) {
                int x=scan[xi],z=scan[zi];size_t i=2u*((size_t)(z+1)*35*35+(size_t)(y+1)*35+(size_t)(x+1));
                if(d[i]<128||d[i+70]>=128||d[i+140]>=128)continue;
                if(d[i-2]<128||d[i+2]<128||d[i-2450]<128||d[i+2450]<128)continue;
                *out_x=(px*128+(ci/16)*32+x)*f->info.sample_step;
                *out_y=(py*128+((ci/4)%4)*32+y)*f->info.sample_step;
                *out_z=(pz*128+(ci%4)*32+z)*f->info.sample_step;
                message(error,cap,"");return true;
            }
        }
    }
    message(error,cap,"No supported exposed terrain surface was found.");return false;
}
