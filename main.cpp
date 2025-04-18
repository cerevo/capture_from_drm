#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "capture_drm.h"

int main (int argc, char *argv[])
{
    drm_capture_ctx_t* ctx = open_drm_device(1920, 1080);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to open DRM device\n");
        return -1;
    }

    if (capture_rgb_image(ctx) != 0) {
        fprintf(stderr, "Failed to capture RGB image\n");
        close_drm_device(ctx);
        return -1;
    }

    close_drm_device(ctx);
    return 0;
}