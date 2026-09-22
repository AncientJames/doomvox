//
// Copyright(C)      2023 Andrew Apted
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#ifndef __R_VOXEL__
#define __R_VOXEL__

void VX_InitVoxels (void);

void VX_ClearVoxels (void);

void VX_NearbySprites (void);

bool VX_ProjectVoxel (mobj_t * thing);

void VX_DrawVoxel (vissprite_t * vis);

bool VX_HasModel(mobj_t* thing);
void VX_WorldBounds(mobj_t* thing, vertex_t corners[4]);
uint8_t VX_SampleModel(mobj_t* thing, fixed_t wx, fixed_t wy, fixed_t wz);

void VX_Blit(mobj_t * thing, fixed_t scaleh, fixed_t scalev, angle_t rotation);

#endif  /* __R_VOXEL__ */
