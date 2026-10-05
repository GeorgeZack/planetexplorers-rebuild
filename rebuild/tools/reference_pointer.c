#define _POSIX_C_SOURCE 200809L
#include <wayland-client.h>
#include "virtual-pointer.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Private helper for reference_input.py. Coordinates are normalized to the
 * compositor layout bounding box; the Python caller verifies the game window. */
static struct zwlr_virtual_pointer_manager_v1 *manager;
static volatile sig_atomic_t interrupted;
static void stop(int signal_number) {(void)signal_number;interrupted=1;}
static void added(void *data,struct wl_registry *registry,uint32_t name,const char *interface,uint32_t version) {
    (void)data;(void)version;
    if(!manager&&!strcmp(interface,"zwlr_virtual_pointer_manager_v1"))
        manager=wl_registry_bind(registry,name,&zwlr_virtual_pointer_manager_v1_interface,1);
}
static void removed(void *data,struct wl_registry *registry,uint32_t name) {(void)data;(void)registry;(void)name;}
static bool number(const char *s,uint32_t *out) {
    if(!s||!*s||*s=='-')return false;
    errno=0;char *end=NULL;unsigned long value=strtoul(s,&end,10);
    if(errno||*end||value>65535)return false;
    *out=(uint32_t)value;return true;
}
static uint32_t milliseconds(void) {
    struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint32_t)((uint64_t)now.tv_sec*1000+(uint64_t)now.tv_nsec/1000000);
}
static void pause_ms(long ms) {
    struct timespec delay={ms/1000,(ms%1000)*1000000};
    while(!interrupted&&nanosleep(&delay,&delay)<0&&errno==EINTR){}
}
int main(int argc,char **argv) {
    uint32_t p[9]={0};
    bool drag=argc==10;
    if(argc!=8&&!drag){fprintf(stderr,"Expected x y width height button jitter_x jitter_y [end_x end_y]\n");return 2;}
    for(int i=0;i<argc-1;i++)if(!number(argv[i+1],&p[i]))return 2;
    if(p[2]<8||p[3]<8||p[0]>=p[2]||p[1]>=p[3]||p[5]>=p[2]||p[6]>=p[3]||p[4]<272||p[4]>274)return 2;
    if(drag&&(p[7]>=p[2]||p[8]>=p[3]))return 2;
    struct sigaction handler={0};handler.sa_handler=stop;sigemptyset(&handler.sa_mask);
    if(sigaction(SIGINT,&handler,NULL)<0||sigaction(SIGTERM,&handler,NULL)<0)return 1;
    /* Bound compositor round trips too; disconnecting destroys virtual input. */
    alarm(5);
    struct wl_display *display=wl_display_connect(NULL);
    if(!display){fprintf(stderr,"Cannot connect to Wayland display\n");return 1;}
    struct wl_registry *registry=wl_display_get_registry(display);
    const struct wl_registry_listener listener={added,removed};
    wl_registry_add_listener(registry,&listener,NULL);
    int result=1;
    bool held=false;
    struct zwlr_virtual_pointer_v1 *pointer=NULL;
    if(wl_display_roundtrip(display)<0||!manager){fprintf(stderr,"No Wayland virtual pointer protocol\n");goto done;}
    pointer=zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager,NULL);
    if(!pointer||wl_display_roundtrip(display)<0)goto done;
    /* A repeated click at an unchanged coordinate may not update Unity UI
     * hover state. Move 4 logical pixels inside the client, then return. */
    zwlr_virtual_pointer_v1_motion_absolute(pointer,milliseconds(),p[5],p[6],p[2],p[3]);
    zwlr_virtual_pointer_v1_frame(pointer);if(wl_display_roundtrip(display)<0)goto done;
    pause_ms(100);
    if(interrupted)goto done;
    zwlr_virtual_pointer_v1_motion_absolute(pointer,milliseconds(),p[0],p[1],p[2],p[3]);
    zwlr_virtual_pointer_v1_frame(pointer);if(wl_display_roundtrip(display)<0)goto done;
    pause_ms(400);
    if(interrupted)goto done;
    zwlr_virtual_pointer_v1_button(pointer,milliseconds(),p[4],WL_POINTER_BUTTON_STATE_PRESSED);
    held=true;
    zwlr_virtual_pointer_v1_frame(pointer);
    if(drag) {
        if(wl_display_roundtrip(display)<0||interrupted)goto done;
        /* Let a slow Unity UI frame pick the source slot before it moves. */
        pause_ms(150);
    }
    else wl_display_flush(display);
    if(drag) {
        /* Convex interpolation stays within the validated client rectangle. */
        for(int step=1;step<=10;step++) {
            pause_ms(50);
            if(interrupted)goto done;
            uint32_t x=(uint32_t)(((uint64_t)p[0]*(10-step)+(uint64_t)p[7]*step)/10);
            uint32_t y=(uint32_t)(((uint64_t)p[1]*(10-step)+(uint64_t)p[8]*step)/10);
            zwlr_virtual_pointer_v1_motion_absolute(pointer,milliseconds(),x,y,p[2],p[3]);
            zwlr_virtual_pointer_v1_frame(pointer);
            if(wl_display_roundtrip(display)<0||interrupted)goto done;
        }
    } else {
        /* PeInputExt.IsClickNoMove requires release strictly before 250ms. */
        pause_ms(150);
        if(interrupted)goto done;
    }
    zwlr_virtual_pointer_v1_button(pointer,milliseconds(),p[4],WL_POINTER_BUTTON_STATE_RELEASED);
    held=false;
    zwlr_virtual_pointer_v1_frame(pointer);if(wl_display_roundtrip(display)<0)goto done;
    pause_ms(400);if(!interrupted)result=0;
done:
    /* Best effort release on transport errors or SIGINT/SIGTERM. Destroying
     * the virtual pointer also drops its state if the display disconnected. */
    if(pointer&&held) {
        zwlr_virtual_pointer_v1_button(pointer,milliseconds(),p[4],WL_POINTER_BUTTON_STATE_RELEASED);
        zwlr_virtual_pointer_v1_frame(pointer);wl_display_flush(display);
    }
    if(pointer)zwlr_virtual_pointer_v1_destroy(pointer);
    if(manager)zwlr_virtual_pointer_manager_v1_destroy(manager);
    wl_registry_destroy(registry);wl_display_flush(display);wl_display_disconnect(display);
    return result;
}
