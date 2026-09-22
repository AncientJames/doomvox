#ifndef __VXMAIN__
#define __VXMAIN__

#include "roto_matrix.h"
#include "r_data.h"

#define countof(a) (sizeof(a)/sizeof(*a))
#define FIXED2FLOAT(a) ((float)(a) / (float)FRACUNIT)
#define FLOAT2FIXED(a) ((fixed_t)(a * FRACUNIT))

#define VOXEL_GROUND (VOXELS_Z/4)

extern volume_double_buffer_t* volume_buffer;
extern float vx_scale;

void VX_Plot(int vx, int vy, int vz, uint8_t palcol);

void VX_Init(void);
void VX_Clear(void);
void VX_Swap(void);

void VX_UI(void);
void VX_Draw(void);


#endif

