#define _POSIX_C_SOURCE 200809L
/* Headless contract tests: real manifest/PNG headers, stubbed GPU ownership and draws. */
#include "pe_ui_assets.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static unsigned next_id=2,live_textures,live_fonts,draws;
static Rectangle last_source;
unsigned char *LoadFileData(const char *name,int *size){
    FILE *f=fopen(name,"rb");if(!f)return NULL;
    assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>=0&&n<64*1024*1024);rewind(f);
    unsigned char *p=malloc((size_t)n);assert(p);assert(fread(p,1,(size_t)n,f)==(size_t)n);fclose(f);*size=(int)n;return p;
}
void UnloadFileData(unsigned char *p){free(p);}
bool FileExists(const char *name){return access(name,F_OK)==0;}
static int be32(const unsigned char *p){return (int)((unsigned)p[0]<<24|(unsigned)p[1]<<16|(unsigned)p[2]<<8|p[3]);}
Image LoadImageFromMemory(const char *ext,const unsigned char *p,int n){
    assert(!strcmp(ext,".png")&&n>=24);return (Image){malloc(1),be32(p+16),be32(p+20),1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}
void UnloadImage(Image image){free(image.data);}
Texture2D LoadTextureFromImage(Image image){live_textures++;return (Texture2D){next_id++,image.width,image.height,1,image.format};}
void UnloadTexture(Texture2D texture){assert(texture.id>1&&live_textures);live_textures--;}
void SetTextureFilter(Texture2D texture,int filter){assert(texture.id>1&&filter==TEXTURE_FILTER_BILINEAR);}
Font GetFontDefault(void){Font f={0};f.texture.id=1;return f;}
Font LoadFont(const char *name){assert(FileExists(name));live_fonts++;Font f={0};f.texture.id=next_id++;f.baseSize=strstr(name,"ui_Font_26.fnt")?26:strstr(name,"ui_Font_32.fnt")?32:strstr(name,"ui_Font_14")?14:18;return f;}
void UnloadFont(Font font){assert(font.texture.id>1&&live_fonts);live_fonts--;}
void DrawTexturePro(Texture2D texture,Rectangle src,Rectangle dst,Vector2 origin,float rotation,Color tint){
    (void)origin;(void)tint;assert(texture.id>1&&dst.width>0&&dst.height>0&&rotation==0);last_source=src;draws++;
}
static void manifest(const char *directory,const char *body){char path[4608];int n=snprintf(path,sizeof path,"%s/atlas.peui",directory);assert(n>0&&(size_t)n<sizeof path);FILE *f=fopen(path,"wb");assert(f);fputs(body,f);fclose(f);}
static void rejects(const char *directory,const char *body){char error[256];manifest(directory,body);assert(!pe_ui_assets_load(directory,error,sizeof error));assert(*error);assert(!live_textures&&!live_fonts);}
int main(int argc,char **argv){
    char error[256];const char *assets=argc>1?argv[1]:"build/reference-assets/ui";
    char fixture_manifest[4096];snprintf(fixture_manifest,sizeof fixture_manifest,"%s/atlas.peui",assets);
    if(FileExists(fixture_manifest)){
        PeUiAssets *ui=pe_ui_assets_load(assets,error,sizeof error);assert(ui);
        assert(live_textures==3&&live_fonts==6);
        assert(pe_ui_font_heading(ui).baseSize==32);
        assert(pe_ui_font_name(ui).baseSize==26&&pe_ui_font_name(NULL).texture.id==GetFontDefault().texture.id);
        assert(pe_ui_font_small(ui,false).baseSize==14&&pe_ui_font_small(ui,true).baseSize==14);
        assert(pe_ui_font_small(NULL,true).texture.id==GetFontDefault().texture.id);
        assert(pe_ui_font(ui,false).baseSize==18&&pe_ui_font(ui,true).baseSize==18);
        assert(pe_ui_font_heading(NULL).texture.id==GetFontDefault().texture.id);
        assert(pe_ui_sprite_exists(ui,"talkbg")&&pe_ui_sprite_exists(ui,"npc_big_Andy")&&pe_ui_sprite_exists(ui,"icons:close"));
        assert(!pe_ui_sprite_exists(ui,"not-a-source-sprite"));
        assert(pe_ui_sprite(ui,"talkbg",(Rectangle){0,0,850,150},WHITE,1)&&draws>0);
        draws=0;assert(!pe_ui_sprite(ui,"talkbg",(Rectangle){0,0,NAN,150},WHITE,1)&&!draws);
        assert(pe_ui_sprite_flip(ui,"npc_big_Andy",(Rectangle){0,0,100,100},WHITE,false,true));assert(last_source.height<0&&last_source.width>0);
        assert(last_source.x==1644&&last_source.y==722&&last_source.width==130&&last_source.height==-130);
        assert(pe_ui_sprite(ui,"talkbg",(Rectangle){0,0,1,1},WHITE,100));
        pe_ui_assets_free(ui);assert(!live_textures&&!live_fonts);
    }else puts("Optional recovered UI fixture absent; running synthetic loader contract checks");
    const char *tmp=getenv("TMPDIR");if(!tmp||!*tmp)tmp="/tmp";
    char temporary[4096];int n=snprintf(temporary,sizeof temporary,"%s/pe-ui-test-XXXXXX",tmp);
    assert(n>0&&(size_t)n<sizeof temporary&&mkdtemp(temporary));
    rejects(temporary,"PEUI1\nsprite ../bad.png 0 0 1 1 0 0 0 0 bad\n");
    rejects(temporary,"PEUI1\nsprite a.png 0 0 1 1 2 0 0 0 bad\n");
    rejects(temporary,"PEUI1\nsprite a.png 0 0 1 1 0 0 0 0 bad");
    rejects(temporary,"PEUI1\n");
    char linkpath[4608];
    snprintf(linkpath,sizeof linkpath,"%s/icons-atlas.png",temporary);
    /* Deliberately header-only PNG: image decoder is a stub in this test. */
    const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R',0,0,8,0,0,0,16,0};
    FILE *fake=fopen(linkpath,"wb");assert(fake);assert(fwrite(png,1,sizeof png,fake)==sizeof png);fclose(fake);
    /* Reject after an atlas has been uploaded: every partial allocation must unwind. */
    rejects(temporary,"PEUI1\nsprite icons-atlas.png 0 0 1 1 0 0 0 0 same\nsprite icons-atlas.png 0 0 1 1 0 0 0 0 same\n");
    rejects(temporary,"PEUI1\nsprite icons-atlas.png 2048 0 1 1 0 0 0 0 outside\n");
    rejects(temporary,"PEUI1\nsprite icons-atlas.png 0 0 1 1 0 0 0 0 missing-font\n");
    /* Heading failure must release both successfully loaded 18px fonts. */
    const char *font_names[]={"ui_Font_18.fnt","ui_Font_18_bold.fnt"};
    for(unsigned i=0;i<2;i++){
        snprintf(linkpath,sizeof linkpath,"%s/%s",temporary,font_names[i]);fake=fopen(linkpath,"wb");assert(fake);fclose(fake);
    }
    rejects(temporary,"PEUI1\nsprite icons-atlas.png 0 0 1 1 0 0 0 0 missing-heading\n");
    snprintf(linkpath,sizeof linkpath,"%s/ui_Font_32.fnt",temporary);fake=fopen(linkpath,"wb");assert(fake);fclose(fake);
    rejects(temporary,"PEUI1\nsprite icons-atlas.png 0 0 1 1 0 0 0 0 missing-small\n");
    snprintf(linkpath,sizeof linkpath,"%s/ui_Font_14.fnt",temporary);fake=fopen(linkpath,"wb");assert(fake);fclose(fake);
    rejects(temporary,"PEUI1\nsprite icons-atlas.png 0 0 1 1 0 0 0 0 missing-small-bold\n");
    snprintf(linkpath,sizeof linkpath,"%s/ui_Font_14_bold.fnt",temporary);fake=fopen(linkpath,"wb");assert(fake);fclose(fake);
    rejects(temporary,"PEUI1\nsprite icons-atlas.png 0 0 1 1 0 0 0 0 missing-name\n");
    snprintf(linkpath,sizeof linkpath,"%s/ui_Font_26.fnt",temporary);fake=fopen(linkpath,"wb");assert(fake);fclose(fake);
    manifest(temporary,"PEUI1\nsprite icons-atlas.png 0 0 28 28 0 0 0 0 synthetic\n");
    PeUiAssets *synthetic=pe_ui_assets_load(temporary,error,sizeof error);assert(synthetic&&live_fonts==6&&live_textures==1);
    assert(pe_ui_font_heading(synthetic).baseSize==32&&pe_ui_sprite_exists(synthetic,"synthetic"));
    pe_ui_assets_free(synthetic);assert(!live_fonts&&!live_textures);unlink(linkpath);
    snprintf(linkpath,sizeof linkpath,"%s/ui_Font_14_bold.fnt",temporary);unlink(linkpath);
    snprintf(linkpath,sizeof linkpath,"%s/ui_Font_14.fnt",temporary);unlink(linkpath);
    snprintf(linkpath,sizeof linkpath,"%s/ui_Font_32.fnt",temporary);unlink(linkpath);
    for(unsigned i=0;i<2;i++){snprintf(linkpath,sizeof linkpath,"%s/%s",temporary,font_names[i]);unlink(linkpath);}
    snprintf(linkpath,sizeof linkpath,"%s/icons-atlas.png",temporary);
    unlink(linkpath);
    char path[4608];snprintf(path,sizeof path,"%s/atlas.peui",temporary);unlink(path);rmdir(temporary);
    puts("UI manifest, atlas coordinates, GPU ownership stubs and malformed-input tests passed");return 0;
}
