
extern "C" {
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/types.h>

#include <setjmp.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
}

#include "capture_drm.h"


void fourcc_decode(unsigned int code, char *name) 
{
    char a, b, c, d;
    if(!code || !name)
        return;

    a = (char)(code & 0xFF);
    b = (char)(code >> 8 & 0xFF);
    c = (char)(code >> 16 & 0xFF);
    d = (char)(code >> 24 & 0xFF);
    sprintf(name, "%c%c%c%c", a,b,c,d);
}


void dump_buf_file(uint8_t *fb_addr, int len, char *fname)
{
    FILE *fp;
    int rlen = 0;

    fp = fopen(fname, "wb");
    if (!fp) {
        printf("Error opening file: %s, error: %s\n", fname, strerror(errno));
        exit(4);
    }

    rlen = fwrite((void *)fb_addr, len, 1, fp);
    if(rlen <=0) {
        printf("Error writing file (%d/%d)! error: %s\n", rlen, len, strerror(errno));
    }

    fclose(fp);
}

uint8_t convert_yu12_to_rgba8888( uint8_t* rgba8888, uint8_t* yuvSpace, uint32_t width, uint32_t height )
{
    uint8_t* ySpace = yuvSpace;
    uint8_t* uSpace = yuvSpace + ( width * height );
    uint8_t* vSpace = yuvSpace + ( width * height ) + ( width / 2 * height / 2 );

    uint32_t rgb_ptr = 0;

    for ( uint32_t row = 0; row < height; row++ ) {
        uint32_t y_row = row * width;
        uint32_t uv_row = ( row / 2 ) * ( width / 2 );
        for ( uint32_t col = 0; col < width; col++ ) {
            uint8_t* y = ySpace + y_row + col;
            uint8_t* u = uSpace + uv_row + ( col >> 1 );
            uint8_t* v = vSpace + uv_row + ( col >> 1 );

            int r = (298 * (*y - 16) + 409 * (*v - 128) + 128) >> 8;
            int g = (298 * (*y - 16) - 100 * (*u - 128) - 208 * (*v - 128) + 128) >> 8;
            int b = (298 * (*y - 16) + 516 * (*u - 128) + 128) >> 8;

            rgba8888[rgb_ptr] = (uint8_t)( r > 255 ? 255 : r < 0 ? 0 : r );
            rgba8888[rgb_ptr + 1] = (uint8_t)( g > 255 ? 255 : g < 0 ? 0 : g );
            rgba8888[rgb_ptr + 2] = (uint8_t)( b > 255 ? 255 : b < 0 ? 0 : b );
            rgba8888[rgb_ptr + 3] = 0xff;

            rgb_ptr += 4;
        }
    }

    return 0;
}

int dump_plane_yuv( uint32_t fd, drmModePlane * ovr, drm_capture_ctx_t* ctx )
{
    drmModeFB2Ptr fb2;

    char sBuffer[256];
    uint32_t imgsize;
    //unsigned int ylen, uvlen;
    uint8_t *pFb_p0 = NULL;
    int iBpp = 0;
    int ret = 0;

    fb2 = drmModeGetFB2(fd, ovr->fb_id);
    if(!fb2) {
        printf("Failed to get FB from DRM, error: %s\n", strerror(errno));
        return errno;
    }

    // Detect the format
    fourcc_decode(fb2->pixel_format, sBuffer);
    sBuffer[4]='\0'; /* terminate */
    switch(fb2->pixel_format) {
        case DRM_FORMAT_YUV420:
            // "YU12": 2x2 subsampled chroma. Y -> U -> V planar. 
            iBpp = 12;
            break;
        default:
            printf("Unsupported format detected: '%s'\n", sBuffer);
            return 1;
    }

    imgsize = fb2->pitches[0] * fb2->height + fb2->pitches[1] * fb2->height / 2 + fb2->pitches[2] * fb2->height / 2;

    uint64_t value;
    ret = drmGetCap(fd, DRM_CAP_PRIME, &value);
    if ( ret < 0 ) {
        printf( "failed cap prime.\n" );
    }

    if ( !(value & DRM_PRIME_CAP_EXPORT) ) {
        printf( "can not get prime buffer\n" );
        
    }

    // get YUV420 Planar.
    struct drm_prime_handle map = { };
    for ( int i = 0; i < 4; i++ ) {
        if ( fb2->handles[i] != 0 ) {
            // printf( "handles[%d] = %d\n", i, fb2->handles[i] );
            map.handle = fb2->handles[i];
            map.flags = DRM_CLOEXEC | DRM_RDWR;
            ret = drmIoctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &map);
            if (ret) {
                printf("Failed to map the Y buffer from DRM (%d)! error: %s\n", ret, strerror(errno));
                return errno;
            } else {
                // printf("get dma-buf fd: 0x%x\n", map.fd);
                break;
            }
        } else {
            printf( "handles[%d] is unused.\n", i );
        }
    }

    // printf( "planar %d: handle=0x%x, pitch=%d, height=%d, size=%d\n", 0, fb2->handles[0], fb2->pitches[0], fb2->height, imgsize );
    pFb_p0 = (uint8_t *) mmap(0, imgsize, PROT_READ, MAP_SHARED, map.fd, 0);
    if(!pFb_p0) {
        printf("Failed to map the Y memory! error: %s\n", strerror(errno));
        return errno;
    } else if ( pFb_p0 == MAP_FAILED ) {
        perror( "mmap failed" );
    }
    
    sprintf(sBuffer, "%s/P%d_%dx%d-%d_FB%d.yuv", ".", ovr->plane_id, fb2->width, fb2->height, iBpp, ovr->fb_id);
    printf("-> Output: %s (%d)\n", sBuffer, imgsize);
    dump_buf_file(pFb_p0, imgsize, sBuffer);

    // convert YUV to RGBA8888
    if ( ctx->rgbaImageBuffer ) {
        convert_yu12_to_rgba8888( ctx->rgbaImageBuffer, pFb_p0, fb2->width, fb2->height );
        sprintf(sBuffer, "%s/P%d_%dx%d-%d_FB%d.rgb", ".", ovr->plane_id, fb2->width, fb2->height, iBpp, ovr->fb_id);
        printf("-> Output: %s (%d)\n", sBuffer, imgsize);
        dump_buf_file(ctx->rgbaImageBuffer, imgsize, sBuffer);
    }

    munmap(pFb_p0, imgsize);
    close(map.fd);
    drmModeFreeFB2(fb2);

    return 0;
}


// allocate memory for rgb image.
uint8_t* alloc_rgb_image( uint32_t width, uint32_t height )
{
    return (uint8_t*)malloc( width * height * 4 );
}

int release_rgb_image( uint8_t* rgb )
{
    if ( rgb ) {
        free( rgb );
    }

    return 0;
}

drm_capture_ctx_t* open_drm_device( uint32_t width, uint32_t height )
{
    drm_capture_ctx_t* ctx = new drm_capture_ctx_t();

    ctx->width = width;
    ctx->height = height;
    ctx->rgbaImageBuffer = alloc_rgb_image( width, height );

    return ctx;
}

int close_drm_device( drm_capture_ctx_t* ctx )
{
    if ( ctx == nullptr ) {
        return -1;
    }
    
    if ( ctx->fd >= 0 ) {
        close( ctx->fd );
    }

    release_rgb_image( ctx->rgbaImageBuffer );

    delete ctx;

    return 0;
}


#define MAX_PLANE 64

int capture_rgb_image( drm_capture_ctx_t* ctx )
{
    int ret = 0;
    drmModeResPtr res;
    drmModePlaneResPtr planeRes;
    drmModePlanePtr plane;

    int drmFd = open("/dev/dri/card0", O_RDWR);
    if ( drmFd < 0 ) {
        printf("Failed to open DRM device, error: %s\n", strerror(errno));
        return -1;
    }

    ctx->fd = drmFd; // Assign the opened file descriptor to ctx->fd
    drmSetClientCap(ctx->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    res = drmModeGetResources(ctx->fd);
    if (res == 0) {
        fprintf(stderr, "Failed to get DRM resources, error: %s\n", strerror(errno));
        return -1;
    }

    planeRes = drmModeGetPlaneResources(ctx->fd);
    if (!planeRes ) {
        fprintf( stderr, "Failed to get plane resources, error: %s\n", strerror(errno));
        return -1;
    }

    if ( planeRes->count_planes > MAX_PLANE ) {
        planeRes->count_planes = MAX_PLANE;
    }

    plane = NULL;
    for ( uint32_t i = 0; i < planeRes->count_planes; i++) {
        plane = drmModeGetPlane(ctx->fd, planeRes->planes[i]);
        if (!plane) {
            continue;
        }

        if ( plane->fb_id > 0 ) {
            fprintf(stderr, "Found a valid plane with fb_id: %d\n", plane->fb_id);
            break;
        }
    }
    
    // ctx->rgbImageBufferにRGBA8888のフォーマットでキャプチャ画像が格納される.
    dump_plane_yuv( ctx->fd, plane, ctx );
 
    if ( plane ) {
        drmModeFreePlane(plane);
    }

    if ( planeRes ) {
        drmModeFreePlaneResources(planeRes);
    }
    
    if ( res ) {
        drmModeFreeResources(res);
    }

    if (  ctx->fd >= 0 ) {
        close(ctx->fd);
    }

    return ret;
}