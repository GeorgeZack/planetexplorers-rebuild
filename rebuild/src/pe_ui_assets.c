#include "pe_ui_assets.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SPRITES 4096
#define MAX_ATLASES 8
typedef struct {char name[256];Rectangle source;int atlas,left,top,right,bottom;} Sprite;
struct PeUiAssets {
    Sprite sprites[MAX_SPRITES];size_t count;
    Texture2D textures[MAX_ATLASES];char files[MAX_ATLASES][256];size_t atlas_count;
    Font body,bold,heading,small,small_bold,name;
};
static void error_text(char *error,size_t cap,const char *message){if(error&&cap)snprintf(error,cap,"%s",message);}
static bool basename_ok(const char *s){
    if(!s||!*s||strlen(s)>255||strstr(s,"..")||s[0]=='.')return false;
    for(;*s;s++)if(!isalnum((unsigned char)*s)&&*s!='.'&&*s!='_'&&*s!='-')return false;
    return true;
}
static bool asset_path(char *path,size_t cap,const char *directory,const char *file){int n=snprintf(path,cap,"%s/%s",directory,file);return n>0&&(size_t)n<cap;}
void pe_ui_assets_free(PeUiAssets *a){
    if(!a)return;
    for(size_t i=0;i<a->atlas_count;i++)if(a->textures[i].id)UnloadTexture(a->textures[i]);
    if(a->body.texture.id)UnloadFont(a->body);
    if(a->bold.texture.id)UnloadFont(a->bold);
    if(a->heading.texture.id)UnloadFont(a->heading);
    if(a->small.texture.id)UnloadFont(a->small);
    if(a->small_bold.texture.id)UnloadFont(a->small_bold);
    if(a->name.texture.id)UnloadFont(a->name);
    free(a);
}
static const Sprite *find(const PeUiAssets *a,const char *name){
    if(a&&name)for(size_t i=0;i<a->count;i++)if(!strcmp(a->sprites[i].name,name))return a->sprites+i;
    return NULL;
}
bool pe_ui_sprite_exists(const PeUiAssets *a,const char *name){return find(a,name)!=NULL;}
PeUiAssets *pe_ui_assets_load(const char *directory,char *error,size_t capacity){
    char path[4096],line[1024];FILE *file=NULL;PeUiAssets *a=NULL;
    error_text(error,capacity,"");
    if(!directory||!asset_path(path,sizeof path,directory,"atlas.peui")||!(file=fopen(path,"rb"))) {
        error_text(error,capacity,"Cannot open original UI atlas manifest");return NULL;
    }
    a=calloc(1,sizeof *a);if(!a)goto fail;
    if(!fgets(line,sizeof line,file)||strcmp(line,"PEUI1\n"))goto fail;
    while(fgets(line,sizeof line,file)) {
        size_t len=strlen(line);if(!len||line[len-1]!='\n')goto fail;
        line[--len]=0;if(len&&line[len-1]=='\r')line[--len]=0;
        if(!len||line[0]=='#')continue;
        if(a->count>=MAX_SPRITES)goto fail;
        char filename[256];int x,y,w,h,l,t,r,b,offset=0;
        if(sscanf(line,"sprite %255s %d %d %d %d %d %d %d %d %n",filename,&x,&y,&w,&h,&l,&t,&r,&b,&offset)!=9||
           offset<=0||offset>=(int)len||strlen(line+offset)>=256||!basename_ok(filename)||
           x<0||y<0||x>16384||y>16384||w<=0||h<=0||w>16384||h>16384||l<0||t<0||r<0||b<0||l>w||r>w-l||t>h||b>h-t||find(a,line+offset))goto fail;
        size_t atlas=0;while(atlas<a->atlas_count&&strcmp(a->files[atlas],filename))atlas++;
        if(atlas==a->atlas_count){
            if(atlas>=MAX_ATLASES||!asset_path(path,sizeof path,directory,filename))goto fail;
            int bytes=0;unsigned char *data=LoadFileData(path,&bytes);
            if(!data||bytes<24||bytes>64*1024*1024){UnloadFileData(data);goto fail;}
            const unsigned char png[]={137,80,78,71,13,10,26,10};
            if(memcmp(data,png,8)){UnloadFileData(data);goto fail;}
            unsigned width=((unsigned)data[16]<<24)|((unsigned)data[17]<<16)|((unsigned)data[18]<<8)|data[19];
            unsigned height=((unsigned)data[20]<<24)|((unsigned)data[21]<<16)|((unsigned)data[22]<<8)|data[23];
            if(!width||!height||width>8192||height>8192||(unsigned long long)width*height>33554432){UnloadFileData(data);goto fail;}
            Image image=LoadImageFromMemory(".png",data,bytes);UnloadFileData(data);
            if(!image.data)goto fail;
            a->textures[atlas]=LoadTextureFromImage(image);UnloadImage(image);
            if(!a->textures[atlas].id)goto fail;
            strcpy(a->files[atlas],filename);a->atlas_count++;
            SetTextureFilter(a->textures[atlas],TEXTURE_FILTER_BILINEAR);
        }
        Texture2D texture=a->textures[atlas];if(x>texture.width-w||y>texture.height-h)goto fail;
        Sprite *s=&a->sprites[a->count++];strcpy(s->name,line+offset);
        s->source=(Rectangle){(float)x,(float)y,(float)w,(float)h};s->atlas=(int)atlas;s->left=l;s->top=t;s->right=r;s->bottom=b;
    }
    if(ferror(file)||!a->count)goto fail;
    fclose(file);file=NULL;
    const char *fonts[]={"ui_Font_18.fnt","ui_Font_18_bold.fnt","ui_Font_32.fnt","ui_Font_14.fnt","ui_Font_14_bold.fnt","ui_Font_26.fnt"};
    for(unsigned i=0;i<6;i++) {
        if(!asset_path(path,sizeof path,directory,fonts[i])||!FileExists(path))goto fail;
        Font font=LoadFont(path);
        if(!font.texture.id||font.texture.id==GetFontDefault().texture.id)goto fail;
        SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);
        if(i==5)a->name=font;else if(i==4)a->small_bold=font;else if(i==3)a->small=font;
        else if(i==2)a->heading=font;else if(i==1)a->bold=font;else a->body=font;
    }
    return a;
fail:
    if(file)fclose(file);
    pe_ui_assets_free(a);
    error_text(error,capacity,"Invalid or incomplete original UI assets");return NULL;
}
bool pe_ui_sprite_flip(const PeUiAssets *a,const char *name,Rectangle dest,Color tint,bool horizontal,bool vertical){
    const Sprite *s=find(a,name);if(!s||!isfinite(dest.x)||!isfinite(dest.y)||!isfinite(dest.width)||!isfinite(dest.height)||dest.width<=0||dest.height<=0)return false;
    Rectangle src=s->source;
    if(horizontal)src.width=-src.width;
    if(vertical)src.height=-src.height;
    DrawTexturePro(a->textures[s->atlas],src,dest,(Vector2){0},0,tint);return true;
}
bool pe_ui_sprite(const PeUiAssets *a,const char *name,Rectangle dest,Color tint,float scale){
    const Sprite *s=find(a,name);if(!s||!isfinite(scale)||scale<=0||!isfinite(dest.x)||!isfinite(dest.y)||!isfinite(dest.width)||!isfinite(dest.height)||dest.width<=0||dest.height<=0)return false;
    if(s->left||s->top||s->right||s->bottom) {
        float dl=s->left*scale,dr=s->right*scale,dt=s->top*scale,db=s->bottom*scale;
        if(dl+dr>dest.width){float k=dest.width/(dl+dr);dl*=k;dr*=k;}
        if(dt+db>dest.height){float k=dest.height/(dt+db);dt*=k;db*=k;}
        float sx[]={s->source.x,s->source.x+s->left,s->source.x+s->source.width-s->right,s->source.x+s->source.width};
        float sy[]={s->source.y,s->source.y+s->top,s->source.y+s->source.height-s->bottom,s->source.y+s->source.height};
        float dx[]={dest.x,dest.x+dl,dest.x+dest.width-dr,dest.x+dest.width};
        float dy[]={dest.y,dest.y+dt,dest.y+dest.height-db,dest.y+dest.height};
        for(unsigned y=0;y<3;y++)for(unsigned x=0;x<3;x++)
            if(sx[x+1]>sx[x]&&sy[y+1]>sy[y]&&dx[x+1]>dx[x]&&dy[y+1]>dy[y])
                DrawTexturePro(a->textures[s->atlas],(Rectangle){sx[x],sy[y],sx[x+1]-sx[x],sy[y+1]-sy[y]},
                    (Rectangle){dx[x],dy[y],dx[x+1]-dx[x],dy[y+1]-dy[y]},(Vector2){0},0,tint);
    }else DrawTexturePro(a->textures[s->atlas],s->source,dest,(Vector2){0},0,tint);
    return true;
}
Font pe_ui_font(const PeUiAssets *a,bool bold){return a?(bold?a->bold:a->body):GetFontDefault();}
Font pe_ui_font_heading(const PeUiAssets *a){return a?a->heading:GetFontDefault();}
Font pe_ui_font_small(const PeUiAssets *a,bool bold){return a?(bold?a->small_bold:a->small):GetFontDefault();}
Font pe_ui_font_name(const PeUiAssets *a){return a?a->name:GetFontDefault();}
