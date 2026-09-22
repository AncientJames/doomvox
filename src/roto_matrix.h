#ifndef _ROTO_MATRIX_H_
#define _ROTO_MATRIX_H_
#include <stdint.h>

#define R565(p) (((p)>>8) & 0xf8)
#define G565(p) (((p)>>3) & 0xfc)
#define B565(p) (((p)<<3) & 0xf8)
#define RGB565(r,g,b) ((((r)<<8)&0xf800) | (((g)<<3)&0x07e0) | (((b)>>3)&0x001f))

#define R332(p) ((((p)>>5)& 7)*36)
#define G332(p) ((((p)>>2)& 7)*36)
#define B332(p) (((p)     & 3)*85)
#define RGB332(r,g,b) (((r)&0xe0) | (((g)>>3)&0x1c) | (((b)>>6)&0x03))

#if 1

typedef uint8_t pixel_t;
#define R_PIX(a) R332(a)
#define G_PIX(a) G332(a)
#define B_PIX(a) B332(a)

#define RGBPIX(r,g,b) RGB332(r,g,b)

#define R_MTH_BIT(p, b) (((p)>>(7-b))&1)
#define G_MTH_BIT(p, b) (((p)>>(4-b))&1)
#define B_MTH_BIT(p, b) (((p)>>(~b&1))&1)
//#define R_MTH_BIT(p, b, s) (((p)<<(s-(7-b)))&(1<<s))
//#define G_MTH_BIT(p, b, s) (((p)<<(s-(4-b)))&(1<<s))
//#define B_MTH_BIT(p, b, s) (((p)<<(s-(1-b)))&(1<<s))

#else

typedef uint16_t pixel_t;
#define R_PIX(a) R565(a)
#define G_PIX(a) G565(a)
#define B_PIX(a) B565(a)

#define RGBPIX(r,g,b) RGB565(r,g,b)

#define R_MTH_BIT(p, b) (((p)>>(15-b))&1)
#define G_MTH_BIT(p, b) (((p)>>(10-b))&1)
#define B_MTH_BIT(p, b) (((p)>>( 4-b))&1)

#endif

#define MATRIX_WIDTH  128
#define MATRIX_HEIGHT 64

#define VOXELS_X 128
#define VOXELS_Y 128
#define VOXELS_Z 64
#define VOXEL_Z_STRIDE 1
#define VOXEL_X_STRIDE VOXELS_Z
#define VOXEL_Y_STRIDE (VOXEL_X_STRIDE * VOXELS_X)
#define VOXELS_COUNT (VOXELS_X*VOXELS_Y*VOXELS_Z)

#define VOXEL_INDEX(x,y,z) ((x)*VOXEL_X_STRIDE + (y)*VOXEL_Y_STRIDE + (z)*VOXEL_Z_STRIDE)

typedef struct {
    pixel_t volume[2][VOXELS_COUNT];
    uint8_t page;
    uint8_t bits_per_channel;
    uint16_t debug_flags;
    uint16_t revolutions_per_minute;
    uint16_t microseconds_per_frame;
} volume_double_buffer_t;

#endif
