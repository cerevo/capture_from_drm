#pragma once

#include <stdint.h>


typedef struct __drm_capture_ctx {
    int fd;
    uint8_t* rgbaImageBuffer;    // RGBA8888.
    uint32_t width;
    uint32_t height;
} drm_capture_ctx_t;


drm_capture_ctx_t* open_drm_device( uint32_t width, uint32_t height );
int close_drm_device( drm_capture_ctx_t* ctx );
int capture_rgb_image( drm_capture_ctx_t* ctx );