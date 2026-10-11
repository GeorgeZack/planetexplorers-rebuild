#define _POSIX_C_SOURCE 200809L
#include "pe_test_paths.h"
#include "pe_legacy.h"
#include <lz4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(expr) do {if(!(expr)){fprintf(stderr,"legacy: %s:%d: %s\n",__FILE__,__LINE__,#expr);failures++;}}while(0)
static void le32(unsigned char *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static bool write_file(const char *path,const unsigned char *data,size_t size){FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(data,1,size,f)==size;if(fclose(f)!=0)ok=false;return ok;}
static unsigned char *fixture(int lod,const unsigned char *first,size_t first_size,size_t *size) {
    size_t n=(size_t)(48>>lod)*(48>>lod)*(23>>lod);
    *size=n*4+first_size+(n-1)*2;
    unsigned char *data=malloc(*size);if(!data)return NULL;
    size_t pos=n*4;
    for(size_t i=0;i<n;i++){le32(data+4*i,(uint32_t)pos);if(i==0){memcpy(data+pos,first,first_size);pos+=first_size;}else{data[pos++]=255;data[pos++]=201;}}
    return data;
}
int pe_test_legacy(void) {
    int failures=0;char error[200],path[1024];PeVoxel sample={99,99};
    uint8_t *chunk=malloc(PE_LEGACY_CHUNK_BYTES);PeWorld *world=calloc(1,sizeof *world),*before=malloc(sizeof *before);
    if(!chunk||!world||!before){free(chunk);free(world);free(before);return 1;}
    for(int lod=0;lod<=4;lod++)for(int water=0;water<2;water++) {
        if(lod)pe_test_format(path,sizeof path,"%s/TutorialMode/Mainland/%s_x0_y0_%d.voxelform",pe_test_root(),water?"water":"map",lod);
        else pe_test_format(path,sizeof path,"%s/TutorialMode/Mainland/%s_x0_y0.voxelform",pe_test_root(),water?"water":"map");
        PeLegacyFile *f=pe_legacy_open(path,lod,error,sizeof error);CHECK(f!=NULL);if(!f)continue;
        PeLegacyInfo info=pe_legacy_info(f);CHECK(info.lod==lod&&info.sample_step==(1<<lod)&&info.extent_x==6144);
        CHECK(pe_legacy_chunk(f,0,0,0,chunk,error,sizeof error));
        CHECK(pe_legacy_sample(f,12*(1<<lod),2*(1<<lod),12*(1<<lod),&sample,error,sizeof error));
        size_t at=2*((12+1)*35*35+(2+1)*35+(12+1));CHECK(sample.volume==chunk[at]&&sample.material==chunk[at+1]);
        if(lod==0&&!water) {
            CHECK(sample.volume==177&&sample.material==1);
            int x=-1,y=-1,z=-1;CHECK(pe_legacy_find_surface(f,&x,&y,&z,error,sizeof error));CHECK(x>=6&&x<=25&&y>=0&&y<=3&&z>=2&&z<=21);
            CHECK(pe_legacy_crop(f,0,0,0,world,error,sizeof error));CHECK(world->source==1&&world->seed==0&&world->origin_x==0);
            CHECK(pe_world_get(world,12,2,12).volume==177);
            CHECK(pe_world_height(world,12,12)>2.9f&&pe_world_height(world,12,12)<3);
            PeMeshData mesh={0};CHECK(pe_world_mesh(world,&mesh));CHECK(mesh.count>0);pe_mesh_free(&mesh);
            *before=*world;CHECK(!pe_legacy_crop(f,6140,0,0,world,error,sizeof error));CHECK(memcmp(world,before,sizeof *world)==0);
        }
        sample=(PeVoxel){99,99};CHECK(!pe_legacy_sample(f,-1,0,0,&sample,error,sizeof error));CHECK(sample.volume==99&&sample.material==99);
        if(lod){CHECK(!pe_legacy_sample(f,1,0,0,&sample,error,sizeof error));CHECK(!pe_legacy_crop(f,0,0,0,world,error,sizeof error));}
        pe_legacy_close(f);
    }
    pe_test_format(path,sizeof path,"%s/CustomGames/Dragon Desert/Worlds/Dragon Desert/map_x0_y0.voxelform",pe_test_root());
    PeLegacyFile *desert=pe_legacy_open(path,0,error,sizeof error);CHECK(desert!=NULL);
    if(desert) {
        int x,y,z;CHECK(pe_legacy_find_surface(desert,&x,&y,&z,error,sizeof error));
        CHECK(x==16&&y==116&&z==16);
        CHECK(pe_legacy_crop(desert,0,108,0,world,error,sizeof error));
        CHECK(pe_world_height(world,16,16)>=8&&pe_world_height(world,16,16)<10);
        PeMeshData mesh={0};CHECK(pe_world_mesh(world,&mesh));CHECK(mesh.count>0);pe_mesh_free(&mesh);
        pe_legacy_close(desert);
    }
    char tmp[]="/tmp/pe-legacy-test-XXXXXX";int fd=mkstemp(tmp);CHECK(fd>=0);if(fd<0){free(chunk);free(world);free(before);return failures;}close(fd);
    size_t size;unsigned char uniform[2]={255,201};unsigned char *data=fixture(0,uniform,2,&size);
    CHECK(data!=NULL);
    if(data) {
        CHECK(write_file(tmp,data,size));PeLegacyFile *f=pe_legacy_open(tmp,0,error,sizeof error);CHECK(f!=NULL);
        if(f){CHECK(pe_legacy_crop(f,117,17,117,world,error,sizeof error));CHECK(world->origin_x==117&&world->origin_y==17&&world->origin_z==117);CHECK(pe_world_get(world,63,31,63).material==201);CHECK(pe_world_get(world,0,0,0).volume==255);pe_legacy_close(f);}
        le32(data,0);CHECK(write_file(tmp,data,size));f=pe_legacy_open(tmp,0,error,sizeof error);CHECK(f==NULL);pe_legacy_close(f);free(data);
    }
    /* A raw LZ4 piece with one dense chunk, others uniform; exercise all axes,
     * original halo indexing and materials outside the prototype enum. */
    size_t raw_size=256+PE_LEGACY_CHUNK_BYTES+63*2;
    unsigned char *raw=malloc(raw_size);char *zipped=malloc((size_t)LZ4_compressBound((int)raw_size));
    CHECK(raw&&zipped);
    if(raw&&zipped) {
        size_t pos=256;
        for(int i=0;i<64;i++){le32(raw+4*i,(uint32_t)pos);if(i==0){for(size_t k=0;k<PE_LEGACY_CHUNK_BYTES;k+=2){raw[pos+k]=0;raw[pos+k+1]=233;}raw[pos+2*(13*1225+3*35+13)]=177;pos+=PE_LEGACY_CHUNK_BYTES;}else{raw[pos++]=(unsigned char)i;raw[pos++]=202;}}
        int zipped_size=LZ4_compress_default((char *)raw,zipped,(int)raw_size,LZ4_compressBound((int)raw_size));CHECK(zipped_size>0);
        data=fixture(4,(unsigned char *)zipped,(size_t)zipped_size,&size);CHECK(data!=NULL);
        if(data){CHECK(write_file(tmp,data,size));PeLegacyFile *f=pe_legacy_open(tmp,4,error,sizeof error);CHECK(f!=NULL);
            if(f){CHECK(pe_legacy_sample(f,12*16,2*16,12*16,&sample,error,sizeof error));CHECK(sample.volume==177&&sample.material==233);CHECK(pe_legacy_sample(f,32*16,0,0,&sample,error,sizeof error));CHECK(sample.volume==16&&sample.material==202);CHECK(pe_legacy_sample(f,0,32*16,0,&sample,error,sizeof error));CHECK(sample.volume==4);CHECK(pe_legacy_sample(f,0,0,32*16,&sample,error,sizeof error));CHECK(sample.volume==1);pe_legacy_close(f);}
            /* A repeated piece offset must be rejected before sampling. */
            memcpy(data+4,data,4);CHECK(write_file(tmp,data,size));f=pe_legacy_open(tmp,4,error,sizeof error);CHECK(f==NULL);pe_legacy_close(f);free(data);}
        /* Chunk offset points into the chunk offset table: valid LZ4, bad structure. */
        le32(raw,255);zipped_size=LZ4_compress_default((char *)raw,zipped,(int)raw_size,LZ4_compressBound((int)raw_size));
        data=fixture(0,(unsigned char *)zipped,(size_t)zipped_size,&size);
        if(data){CHECK(write_file(tmp,data,size));PeLegacyFile *f=pe_legacy_open(tmp,0,error,sizeof error);CHECK(f!=NULL);
            if(f){pe_world_destroy(world);world->seed=0x5a5a5a5a;*before=*world;CHECK(!pe_legacy_crop(f,0,0,0,world,error,sizeof error));CHECK(memcmp(world,before,sizeof *world)==0);memset(chunk,0x7b,PE_LEGACY_CHUNK_BYTES);CHECK(!pe_legacy_chunk(f,0,0,0,chunk,error,sizeof error));CHECK(chunk[0]==0x7b&&chunk[PE_LEGACY_CHUNK_BYTES-1]==0x7b);pe_legacy_close(f);}free(data);}
    }
    free(raw);free(zipped);
    const unsigned char bad_lz4[3]={255,255,255};data=fixture(4,bad_lz4,sizeof bad_lz4,&size);
    if(data){CHECK(write_file(tmp,data,size));PeLegacyFile *f=pe_legacy_open(tmp,4,error,sizeof error);CHECK(f!=NULL);if(f){sample=(PeVoxel){45,46};CHECK(!pe_legacy_sample(f,0,0,0,&sample,error,sizeof error));CHECK(sample.volume==45&&sample.material==46);pe_legacy_close(f);}CHECK(write_file(tmp,data,12));f=pe_legacy_open(tmp,4,error,sizeof error);CHECK(f==NULL);pe_legacy_close(f);free(data);}
    CHECK(pe_legacy_open(tmp,-1,error,sizeof error)==NULL);CHECK(pe_legacy_open(tmp,5,error,sizeof error)==NULL);
    unlink(tmp);free(chunk);free(world);free(before);return failures;
}
