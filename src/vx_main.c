
#include "roto_matrix.h"

#include "i_system.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>

#include "r_data.h"
#include "r_draw.h"
#include "r_voxel.h"
#include "z_zone.h"
#include "deh_str.h"
#include "w_wad.h"
#include "r_main.h"
#include "doomstat.h"
#include "m_random.h"
#include "p_local.h"

#include "vx_main.h"

volume_double_buffer_t* volume_buffer;

#define UI_DIVISOR ((SCREENHEIGHT + (VOXELS_Z / 2)) / VOXELS_Z)
#define UI_WIDTH (SCREENWIDTH / UI_DIVISOR)
#define UI_HEIGHT (SCREENHEIGHT / UI_DIVISOR)

typedef struct {
    int16_t x, y;
} coord2_t;
coord2_t perimeter[UI_WIDTH];

static const float vga_squash = 240.0f / 200.0f;
static const float voxvol_radius = 0.25f * (VOXELS_X + VOXELS_Y);

float vx_scale = 0.35f;
static float world_to_voxel[3] = {1.0f, 1.0f, vga_squash};
static float voxel_to_world[3] = {1.0f, 1.0f, 1.0f / vga_squash};


static inline int min(int a, int b) {return a < b ? a : b;}
static inline int max(int a, int b) {return a > b ? a : b;}
static inline float sqrf(float a) {return a * a;}
static inline float minf(float a, float b) {return a < b ? a : b;}
static inline float maxf(float a, float b) {return a > b ? a : b;}
static inline float clampf(float v, float a, float b) {if (v < a) return a; if (v > b) return b; return v;}
static inline float lerpf(float a, float b, float t) {return a + (b - a) * t;}

typedef struct {
    vertex_t aabb[2];
    float centre[2];
} vx_state_t;

static vx_state_t vx_state;

static color_t default_palette[256];

void VX_Init(void) {
    int fd = shm_open("/vortex_double_buffer", O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(1);
    }

    volume_buffer = mmap(NULL, sizeof(*volume_buffer), PROT_WRITE, MAP_SHARED, fd, 0);
    if (volume_buffer == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

	memset(volume_buffer, 0, sizeof(volume_buffer->volume));
	//volume_buffer->bits_per_channel = 2;

    float pixang = ((float)countof(perimeter) / (float)(VOXELS_X)) * 2.0f;
    float frontang = 2 * M_PI * 0.75f;
    for (unsigned i = 0; i < countof(perimeter); ++i) {
        float t = ((float)(i)/(float)(countof(perimeter)-1) - 0.5f);
        t = (t * pixang) + frontang;
        perimeter[i].x = (int)roundf(cosf(t) * 63.5 + 63.5);
        perimeter[i].y = (int)roundf(sinf(t) * 63.5 + 63.5);
    }

    byte* pal = W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE);
    for (int i = 0; i < 256; ++i) {
        default_palette[i].a = 0;
        default_palette[i].r = gammatable[usegamma][*pal++];
        default_palette[i].g = gammatable[usegamma][*pal++];
        default_palette[i].b = gammatable[usegamma][*pal++];
    }
}

void VX_Clear() {
	memset(volume_buffer->volume[!volume_buffer->page], 0, sizeof(*volume_buffer->volume));
}

void VX_Swap() {
	volume_buffer->page = !volume_buffer->page;

    //I_Sleep(15);
    
    static uint32_t swap_prev = 0;
    uint32_t ms = I_GetTimeMS();
    uint32_t elapsed = ms - swap_prev;
    swap_prev = ms;

    if (elapsed < 25) {
        I_Sleep(25 - elapsed);
    }
    

    /*
    static int pms = 0;
    static int accum = 0;
    if (++accum >= 64) {
        int ms = I_GetTimeMS();
        int tot = ms - pms;
        pms = ms;
        printf("%d ms\n", tot / accum);
        accum = 0;
    }
    */
    
}


pixel_t dithered_colour(uint8_t palcol) {
    color_t colour = default_palette[palcol];
    int d = M_Random();
    int dither = ((d) >> (volume_buffer->bits_per_channel)) & 0b11111110;

    uint8_t r = min(0xff, ((int)colour.r) + dither);
    uint8_t g = min(0xff, ((int)colour.g) + dither);
    uint8_t b = min(0xff, ((int)colour.b) + dither);
    return RGBPIX(r, g, b);
}

void VX_UI() {
	pixel_t* content = volume_buffer->volume[!volume_buffer->page];

    int dither;
    int d1 = 0b10101010;
    int d2 = 0b01010101;
    uint8_t r, g, b;
    color_t colour;

    for (unsigned row = 0; row < VOXELS_Z; ++row) {
        int y = ((SCREENHEIGHT + (VOXELS_Z * UI_DIVISOR)) / 2) - (row * UI_DIVISOR);
        if (y >= 0 && y < SCREENHEIGHT) {
            for (unsigned col = 0; col < UI_WIDTH; ++col) {
                int x = (col * UI_DIVISOR) + (SCREENWIDTH - (UI_WIDTH * UI_DIVISOR)) / 2;
                byte* samp = &I_VideoBuffer[x + y*SCREENWIDTH];

                /*uint32_t c = ((*(uint32_t*)&default_palette[samp[0]]) & 0xfcfcfcfc) >> 2;
                        c += ((*(uint32_t*)&default_palette[samp[1]]) & 0xfcfcfcfc) >> 2;
                        c += ((*(uint32_t*)&default_palette[samp[SCREENWIDTH]]) & 0xfcfcfcfc) >> 2;
                        c += ((*(uint32_t*)&default_palette[samp[SCREENWIDTH+1]]) & 0xfcfcfcfc) >> 2;
                color_t colour = *(color_t*)&c;*/

                coord2_t coord = perimeter[col];

                colour = default_palette[samp[0]];
                dither = ((row^col^d1) >> (volume_buffer->bits_per_channel)) & 0b11111110;
                r = min(0xff, ((int)colour.r) + dither);
                g = min(0xff, ((int)colour.g) + dither);
                b = min(0xff, ((int)colour.b) + dither);            
                content[VOXEL_INDEX(coord.x, coord.y, row)] = RGBPIX(r, g, b);

                colour = default_palette[samp[SCREENWIDTH]];
                dither = ((row^col^d2) >> (volume_buffer->bits_per_channel)) & 0b11111110;
                r = min(0xff, ((int)colour.r) + dither);
                g = min(0xff, ((int)colour.g) + dither);
                b = min(0xff, ((int)colour.b) + dither);            
                content[VOXEL_INDEX(coord.x, coord.y+1, row)] = RGBPIX(r, g, b);
            }
        }
    }
}


static float* vec2_subtract(float* result, const float* v0, const float* v1) {
    result[0] = v0[0] - v1[0];
    result[1] = v0[1] - v1[1];
    return result;
}

static float* vec2_add(float* result, const float* v0, const float* v1) {
    result[0] = v0[0] + v1[0];
    result[1] = v0[1] + v1[1];
    return result;
}

static float* vec2_multiply_f(float* result, const float* v0, const float f) {
    result[0] = v0[0] * f;
    result[1] = v0[1] * f;
    return result;
}

static float vec2_dot(const float* v0, const float* v1) {
    return v0[0]*v1[0] + v0[1]*v1[1];
}

static float vec2_length(const float* v) {
    return sqrtf(v[0]*v[0] + v[1]*v[1]);
}

extern fixed_t viewsin, viewcos;
void ViewTransform(float* point, vertex_t* vertex) {
    point[0] = FIXED2FLOAT(vertex->x - viewx);
    point[1] = FIXED2FLOAT(vertex->y - viewy);

#ifdef VOXEL_ROTATE_VIEW
    {
        float s = FIXED2FLOAT(viewsin);
        float c = FIXED2FLOAT(viewcos);
        float x = point[0] * s + point[1] * -c;
        float y = point[0] * c + point[1] * s;
        point[0] = x;
        point[1] = y;
    }
#endif

    point[0] = point[0] * world_to_voxel[0] + ((float)VOXELS_X) * 0.5f;
    point[1] = point[1] * world_to_voxel[1] + ((float)VOXELS_Y) * 0.5f;
}

void InvViewTransform(float* result, const float* point) {
    result[0] = (point[0] - ((float)VOXELS_X) * 0.5f) * voxel_to_world[0];
    result[1] = (point[1] - ((float)VOXELS_Y) * 0.5f) * voxel_to_world[1];

#ifdef VOXEL_ROTATE_VIEW
    {
        float s = FIXED2FLOAT(viewsin);
        float c = FIXED2FLOAT(viewcos);
        float x = result[0] * s + result[1] * c;
        float y = result[0] * -c + result[1] * s;
        result[0] = x;
        result[1] = y;
    }
#endif

    result[0] += FIXED2FLOAT(viewx);
    result[1] += FIXED2FLOAT(viewy);
}

static float ViewTransformZ(fixed_t z) {
    return FIXED2FLOAT(z - viewplayer->mo->z) * world_to_voxel[2] + (float)VOXEL_GROUND;
}

static float InvViewTransformZ(float z) {
    return ((z - (float)VOXEL_GROUND) * voxel_to_world[2]) + FIXED2FLOAT(viewplayer->mo->z);
}

static bool PointInSubSector(subsector_t* subsector, float* point) {
    fixed_t x = FLOAT2FIXED(point[0]);
    fixed_t y = FLOAT2FIXED(point[1]);
    for (int i = 0; i < subsector->numlines; ++i) {
        seg_t* line = &segs[subsector->firstline + i];
        if (R_PointOnSegSide(x, y, line)) {
            return false;
        }
    }
    return true;
}

static uint8_t ditherseq = 0;

#define B0(b) 0
#define B1(b) ((b * 6) + 25)
#define B2(b) ((b * 4) + 10)
#define B3(b) ((b * 1) + 25)

static int bayer[4][8] = {
    {B0(6), B0(0), B0(4), B0(2), B0(3), B0(5), B0(1), B0(7)},
    {B1(6), B1(0), B1(4), B1(2), B1(3), B1(5), B1(1), B1(7)},
    {B2(6), B2(0), B2(4), B2(2), B2(3), B2(5), B2(1), B2(7)},
    {B3(6), B0(0), B0(4), B0(2), B0(3), B0(5), B0(1), B0(7)},
};
void VX_Plot(int vx, int vy, int vz, uint8_t palcol) {
    pixel_t* content = volume_buffer->volume[!volume_buffer->page];


    /*if (dc_colormap) {
        palcol = dc_colormap[palcol];
    }*/
    color_t colour = default_palette[palcol];
    if (dc_colormap) {
        //average full bright & colormap
        color_t mapped = default_palette[dc_colormap[palcol]];
        colour.r = (colour.r + mapped.r) >> 1;
        colour.g = (colour.g + mapped.g) >> 1;
        colour.b = (colour.b + mapped.b) >> 1;
    }

    const int dither = bayer[volume_buffer->bits_per_channel & 3][(((vx&1) | ((vy&1)<<1) | ((vz&1)<<2)) + (ditherseq*4)) & 7];

    uint8_t r = min(0xff, ((int)colour.r) + dither);
    uint8_t g = min(0xff, ((int)colour.g) + dither);
    uint8_t b = min(0xff, ((int)colour.b) + dither);

    content[VOXEL_INDEX(vx, vy, vz)] = RGBPIX(r, g, b);
}

void bayer_tweak(char ch) {
    static int brightness = 0;
    static int contrast = 1;

    switch (ch) {
        case 'q': ++brightness; break;
        case 'a': --brightness; break;
        case 'w': ++contrast; break;
        case 's': --contrast; break;
    }

    printf("%d %d\n", brightness, contrast);

    int b = volume_buffer->bits_per_channel & 3;
    bayer[b][0] = 6 * contrast + brightness;
    bayer[b][1] = 0 * contrast + brightness;
    bayer[b][2] = 4 * contrast + brightness;
    bayer[b][3] = 2 * contrast + brightness;
    bayer[b][4] = 3 * contrast + brightness;
    bayer[b][5] = 5 * contrast + brightness;
    bayer[b][6] = 1 * contrast + brightness;
    bayer[b][7] = 7 * contrast + brightness;

}



static uint8_t SampleFlat(short flatpic, float* worldpos) {
    int lumpnum = firstflat + flattranslation[flatpic];
	byte* tex = W_CacheLumpNum(lumpnum, PU_CACHE);
    return (tex[((int)worldpos[0] & 63) + ((int)worldpos[1] & 63) * 64]);
}

static bool SampleColumn(uint8_t* palcol, column_t* column, int row) {
    while (column->topdelta != 0xff) {
        int r = row - column->topdelta;
        if (r < 0) {
            return false;
        }
        if (r < column->length) {
            byte* tex = (byte*)column + 3;
            *palcol = tex[r];
            return true;
        }
        column = (column_t*)((byte*)column + column->length + 4);
    }

    return false;
}

static bool SamplePatch(uint8_t* palcol, patch_t* patch, int x, int y) {
    if (x >= patch->width || y >= patch->height) {
        return false;
    }

    column_t* column = (column_t*)((byte*)patch + (patch->columnofs[x]));
    return SampleColumn(palcol, column, y);
}


static void DrawWall(line_t* line) {
    float v0[2];
    float v1[2];
    ViewTransform(v0, line->v1);
    ViewTransform(v1, line->v2);
    if (minf(v0[0], v1[0]) >= VOXELS_X || maxf(v0[0], v1[0]) < 0 || minf(v0[1], v1[1]) >= VOXELS_Y || maxf(v0[1], v1[1]) < 0) {
        return;
    }

    float diff[2];
    vec2_subtract(diff, v1, v0);
    float length = vec2_length(diff);
    if (length < 1.0f) {
        return;
    }

    float step;
    if (fabsf(diff[0]) > fabsf(diff[1])) {
        step = length / fabsf(diff[0]);
    } else {
        step = length / fabsf(diff[1]);
    }

    float vstep[2];
    vec2_multiply_f(vstep, diff, step / length);


    int zmidbot = (int)ViewTransformZ(line->frontsector->floorheight);
    int zmidtop = (int)ViewTransformZ(line->frontsector->ceilingheight);
    int zbot = zmidbot;
    int ztop = zmidtop;

    side_t* sidedef = &sides[line->sidenum[0]];
    int texbot = sidedef->bottomtexture;
    int texmid = sidedef->midtexture;
    int textop = sidedef->toptexture;
    float texcoloff = FIXED2FLOAT(sidedef->textureoffset);
    float texrowoff = FIXED2FLOAT(sidedef->rowoffset);

    bool maskedmidtexture = false;

    if (line->backsector) {
        if (texbot && line->backsector->floorheight > line->frontsector->floorheight) {
            zmidbot = (int)ViewTransformZ(line->backsector->floorheight);
        }
        if (textop && line->backsector->ceilingheight < line->frontsector->ceilingheight) {
            zmidtop = (int)ViewTransformZ(line->backsector->ceilingheight);
        }
        maskedmidtexture = texmid != 0;
    }
    if (ztop < 0 || zbot >= VOXELS_Z || zbot >= ztop) {
        return;
    }

    float dty = voxel_to_world[2];

    float vp[2] = {v0[0], v0[1]};
    float p = 0.0f;
    while (p < length) {
        const float thickness = 0.125f;
        int x[2] = {(int)(vp[0] - vstep[1] * thickness), (int)(vp[0] + vstep[1] * thickness)};
        int y[2] = {(int)(vp[1] + vstep[0] * thickness), (int)(vp[1] - vstep[0] * thickness)};
        if (x[1] < 0 || x[1] >= VOXELS_X || y[1] < 0 || y[1] >= VOXELS_Y) {
            x[1] = x[0];
            y[1] = y[0];
        }
        if (x[0] >= 0 && y[0] >= 0 && x[0] < VOXELS_X && y[0] < VOXELS_Y) {
            int thicken = (x[0] != x[1] || y[0] != y[1]);
            int col = (p * voxel_to_world[0]) + texcoloff;
            if (texbot) {
                byte* tex = R_GetColumn(texbot, col);
                float ty = texrowoff;
                for (int z = zmidbot; z > zbot; --z) {
                    if (z > 0 && z < VOXELS_Z) {
                        for (int t = 0; t <= thicken; ++t) {
                            VX_Plot(x[t], y[t], z, tex[((int)ty)&127]);
                        }
                    }
                    ty += dty;
                }
            }
            if (texmid) {
                byte* tex = R_GetColumn(texmid, col);
                float ty = texrowoff;
                for (int z = zmidtop; z > zmidbot; --z) {
                    if (z > 0 && z < VOXELS_Z) {
                        for (int t = 0; t <= thicken; ++t) {
                            if (maskedmidtexture) {
                                uint8_t palcol;
                                if (SampleColumn(&palcol, (column_t*)(tex-3), ((int)ty)&127)) {
                                    VX_Plot(x[t], y[t], z, palcol);
                                }
                            } else {
                                VX_Plot(x[t], y[t], z, tex[((int)ty)&127]);
                            }
                        }
                    }
                    ty += dty;
                }
            }
            if (textop) {
                byte* tex = R_GetColumn(textop, col);
                float ty = texrowoff;
                for (int z = ztop; z > zmidtop; --z) {
                    if (z > 0 && z < VOXELS_Z) {
                        for (int t = 0; t <= thicken; ++t) {
                            VX_Plot(x[t], y[t], z, tex[((int)ty)&127]);
                        }
                    }
                    ty += dty;
                }
            }
        }

        vec2_add(vp, vp, vstep);
        p += step;
    }
}


static void DrawFloor() {
    const float rsq = voxvol_radius * voxvol_radius;
    const float centre[] = {0.5f * VOXELS_X, 0.5f * VOXELS_Y};
    for (int y = 0; y < VOXELS_Y; ++y) {
        for (int x = 0; x < VOXELS_X; ++x) {
            const float voxpos[] = {x + 0.5f, y + 0.5f};
            const float circle[] = {voxpos[0] - centre[0], voxpos[1] - centre[1]};
            if (vec2_dot(circle, circle) < rsq) {
                float worldpos[2];
                InvViewTransform(worldpos, voxpos);

                subsector_t* sub = R_PointInSubsector(FLOAT2FIXED(worldpos[0]), FLOAT2FIXED(worldpos[1]));
                if (sub->sector->validcount != validcount) {
                    continue;
                }
                if (sub->sector->floorheight >= sub->sector->ceilingheight) {
                    continue;
                }
                if (!PointInSubSector(sub, worldpos)) {
                    continue;
                }

                int z = (int)ViewTransformZ(sub->sector->floorheight);
                if (z >= 0 && z < VOXELS_Z) {
                    VX_Plot(x, y, z, SampleFlat(sub->sector->floorpic, worldpos));
                }
/*
                if (sub->sector->ceilingpic != skyflatnum) {
                    z = (int)ViewTransformZ(sub->sector->ceilingheight);
                    if (z >= 0 && z < VOXELS_Z) {
                        VX_Plot(x, y, z, SampleFlat(sub->sector->ceilingpic, worldpos));
                    }
                }
*/
            }
        }
    }
}

#if 1
static void DrawThingSprite(mobj_t* thing) {
    float voxpos[2];
    ViewTransform(voxpos, (vertex_t*)&(thing->x));
    //voxpos[2] = ViewTransformZ(thing->z);


    spritedef_t* spritedef = &sprites[thing->sprite];
    spriteframe_t* spriteframe = &spritedef->spriteframes[thing->frame & FF_FRAMEMASK];


	unsigned rot = -thing->angle;
#ifdef VOXEL_ROTATE_VIEW
    rot += viewangle;
#else
    rot += ANG90;
#endif
    rot = (rot + (unsigned)(ANG45/2)*9)>>29;
	int lump = spriteframe->lump[rot];
	bool flip = (bool)spriteframe->flip[rot];

    patch_t* patch = W_CacheLumpNum(firstspritelump + lump, PU_CACHE);

    float radius = patch->width * world_to_voxel[0] * 0.25f;
    int xmin = voxpos[0] - radius;
    int xmax = voxpos[0] + radius;
    xmax = min(xmax, VOXELS_X-1);

    int ztop = ViewTransformZ(thing->z + spritetopoffset[lump]);
    int zbot = ViewTransformZ(max(thing->z + spritetopoffset[lump] - (patch->height<<FRACBITS), thing->subsector->sector->floorheight));
    zbot = max(zbot, 0);

    int ymin = max(voxpos[1] - 2, 0);
    int ymax = min(voxpos[1] + 2, VOXELS_Y-1);
    if (ymax < ymin) {
        return;
    }

    float u = flip ? patch->width - 0.5f : 0.5f;
    float du = flip ? -voxel_to_world[0] : voxel_to_world[0];
    for (int x = xmin; x <= xmax; ++x) {
        if (x > 0 && u > 0 && u < patch->width) {
            float v = voxel_to_world[2];
            for (int z = ztop; z >= zbot; --z) {
                if (z < VOXELS_Z) {
                    uint8_t palcol;
                    if (SamplePatch(&palcol, patch, (int)u, (int)v)) {
                        for (int y = ymin; y <= ymax; ++y) {
                            VX_Plot(x, y, z, palcol);
                        }
                    }
                }

                v += voxel_to_world[2];
            }
        }

        u += du;
    }
}
#else
static void DrawThingSprite(mobj_t* thing) {
	pixel_t* content = volume_buffer->volume[!volume_buffer->page];

    float voxpos[2];
    ViewTransform(voxpos, (vertex_t*)&(thing->x));

    spritedef_t* spritedef = &sprites[thing->sprite];
    spriteframe_t* spriteframe = &spritedef->spriteframes[thing->frame & FF_FRAMEMASK];

    unsigned rot = (ANG270)>>29;
	int lump = spriteframe->lump[rot];
	bool flip = (bool)spriteframe->flip[rot];

    patch_t* patch = W_CacheLumpNum(firstspritelump + lump, PU_CACHE);

    float radius = patch->width * world_to_voxel[0] * 0.5f;

    int ztop = ViewTransformZ(thing->z + spritetopoffset[lump]);
    int zbot = ViewTransformZ(max(thing->z + spritetopoffset[lump] - (patch->height<<FRACBITS), thing->subsector->sector->floorheight));
    zbot = max(zbot, 0);

    float vdir[2];
    vdir[0] = FIXED2FLOAT(finecosine[thing->angle>>ANGLETOFINESHIFT]);
    vdir[1] = FIXED2FLOAT(finesine[thing->angle>>ANGLETOFINESHIFT]);

    float step;
    if (fabsf(vdir[0]) > fabsf(vdir[1])) {
        step = 1.0f / fabsf(vdir[0]);
    } else {
        step = 1.0f / fabsf(vdir[1]);
    }

    float vstep[2];
    vec2_multiply_f(vstep, vdir, step);

    float u = flip ? patch->width-1 : 0;
    float du = flip ? -step * voxel_to_world[0] : step * voxel_to_world[0];

    int wmin = -2;
    int wmax = 2;

    float vox[2] = {voxpos[0] - vdir[0] * radius, voxpos[1] - vdir[1] * radius};
    float length = radius * 2;
    while (length >= 0) {
        float v = 0;
        for (int z = ztop; z >= zbot; --z) {
            uint8_t palcol;
            if (SamplePatch(&palcol, patch, (int)u, (int)v)) {
                for (int w = wmin; w <= wmax; ++w) {
                    int x = vox[0] + vstep[1] * w;
                    int y = vox[1] - vstep[0] * w;
                    if (x >= 0 && y >= 0 && x < VOXELS_X && y < VOXELS_Y && z < VOXELS_Z) {
                        VX_Plot(x, y, z, palcol);
                    }
                }
            }

            v += voxel_to_world[2];
        }

        u += du;
        length -= step;
        vec2_add(vox, vox, vstep);
    }
}
#endif

[[maybe_unused]] static void DrawThingVoxel(mobj_t* thing) {
    int xinf = VOXELS_X;
    int yinf = VOXELS_Y;
    int xsup = -1;
    int ysup = -1;

    vertex_t corners[4];
    VX_WorldBounds(thing, corners);
    for (int i = 0; i < 4; ++i) {
        float voxpox[2];
        ViewTransform(voxpox, &corners[i]);

        xinf = min(xinf, (int)floorf(voxpox[0]));
        xsup = max(xsup, (int) ceilf(voxpox[0]));
        yinf = min(yinf, (int)floorf(voxpox[1]));
        ysup = max(ysup, (int) ceilf(voxpox[1]));
    }

    xinf = max(0, xinf);
    yinf = max(0, yinf);
    xsup = min(VOXELS_X-1, xsup);
    ysup = min(VOXELS_Y-1, ysup);

    for (int y = yinf; y <= ysup; ++y) {
        for (int x = xinf; x <= xsup; ++x) {
            const float voxpos[] = {x + 0.5f, y + 0.5f};
            float worldpos[2];
            InvViewTransform(worldpos, voxpos);
            fixed_t wx = FLOAT2FIXED(worldpos[0]);
            fixed_t wy = FLOAT2FIXED(worldpos[1]);

            for (int z = 0; z < VOXELS_Z; ++z) {
                fixed_t wz = FLOAT2FIXED(InvViewTransformZ(z + 0.5f));
                uint8_t colour = VX_SampleModel(thing, wx, wy, wz);
                if (colour) {
                    VX_Plot(x, y, z, colour);
                }
            }
        }
    }

}

static void DrawSector(sector_t* sector) {
    if (sector->validcount == validcount) {
        return;
    }
    sector->validcount = validcount;

    fixed_t scaleh = FLOAT2FIXED(world_to_voxel[0]);
    fixed_t scalev = FLOAT2FIXED(world_to_voxel[2]);

#ifdef VOXEL_ROTATE_VIEW
    angle_t rotation = viewangle;
#else
    angle_t rotation = ANG90;
#endif

    for (int i = 0; i < sector->linecount; ++i) {
        DrawWall(sector->lines[i]);
    }

    const float region = voxvol_radius * voxel_to_world[0];
    for (mobj_t* thing = sector->thinglist; thing; thing = thing->snext) {
        float rsq = sqrf(FIXED2FLOAT(thing->radius) + region);
        float off[] = {FIXED2FLOAT(thing->x) - vx_state.centre[0], FIXED2FLOAT(thing->y) - vx_state.centre[1]};
        if (vec2_dot(off, off) <= rsq) {
            if (VX_HasModel(thing)) {
                //if (world_to_voxel[2] > 1) {
                //    DrawThingVoxel(thing);
                //} else {

                    VX_Blit(thing, scaleh, scalev, rotation);
                //}
            } else {
                DrawThingSprite(thing);
            }
        }
    }

    for (int i = 0; i < sector->linecount; ++i) {
        line_t* line = sector->lines[i];
        if (line->validcount == validcount) {
            continue;
        }
        line->validcount = validcount;

        if (!(line->flags & ML_TWOSIDED) || !line->backsector) {
            continue;
        }

        if (max(line->frontsector->floorheight, line->backsector->floorheight) >= min(line->frontsector->ceilingheight, line->backsector->ceilingheight)) {
            continue;
        }

        if ((line->v1->x < vx_state.aabb[0].x && line->v2->x < vx_state.aabb[0].x)
         || (line->v1->x > vx_state.aabb[1].x && line->v2->x > vx_state.aabb[1].x)
         || (line->v1->y < vx_state.aabb[0].y && line->v2->y < vx_state.aabb[0].y)
         || (line->v1->y > vx_state.aabb[1].y && line->v2->y > vx_state.aabb[1].y)) {
            continue;
        }

        DrawSector(line->frontsector);
        DrawSector(line->backsector);
    }
}

static void AutoScale(void) {
    [[maybe_unused]]mobj_t* attacker = NULL;
    fixed_t closest = 200 << FRACBITS;

    sector_t* sector = sectors;
    for (int i = 0; i < numsectors; ++i, ++sector) {
        for (mobj_t* thing = sector->thinglist; thing; thing = thing->snext) {
            if (thing->target == viewplayer->mo && thing->health > 0) {
                fixed_t dist = P_AproxDistance(thing->x - thing->target->x, thing->y - thing->target->y);
                dist += thing->radius;

                if (dist < closest) {
                    closest = dist;
                    attacker = thing;
                }
            }
        }
    }

    /*if (attacker) {
        float bubble = FIXED2FLOAT(closest + attacker->radius);
        float scale = (VOXELS_X / 2) / bubble;
        vx_scale = lerpf(vx_scale, scale, 0.1f);
    }*/

    float scale = (VOXELS_X * 0.5f) / (FIXED2FLOAT(closest) + 50.0f);
    vx_scale = lerpf(vx_scale, scale, 0.1f);

}

void VX_Draw(void) {
    ++ditherseq;

    if (demoplayback) {
        AutoScale();
    }

    vx_scale = clampf(vx_scale, 0.05f, 1.0f / vga_squash);

    world_to_voxel[0] = world_to_voxel[1] = vx_scale;
    world_to_voxel[2] = vx_scale * vga_squash;
    voxel_to_world[0] = voxel_to_world[1] = 1.0f / vx_scale;
    voxel_to_world[2] = 1.0f / world_to_voxel[2];

    float voxpox[2] = {0.5f*VOXELS_X, 0.5f*VOXELS_Y};
    InvViewTransform(vx_state.centre, voxpox);

    vx_state.aabb[0].x = vx_state.aabb[0].y = INT_MAX;
    vx_state.aabb[1].x = vx_state.aabb[1].y = INT_MIN;
    for (int i = 0; i < 4; ++i) {
        float worldpos[2];
        voxpox[0] = ( i     & 1) * VOXELS_X;
        voxpox[1] = ((i>>1) & 1) * VOXELS_Y;
        InvViewTransform(worldpos, voxpox);

        vx_state.aabb[0].x = min(vx_state.aabb[0].x, FLOAT2FIXED(worldpos[0]));
        vx_state.aabb[0].y = min(vx_state.aabb[0].y, FLOAT2FIXED(worldpos[1]));
        vx_state.aabb[1].x = max(vx_state.aabb[1].x, FLOAT2FIXED(worldpos[0]));
        vx_state.aabb[1].y = max(vx_state.aabb[1].y, FLOAT2FIXED(worldpos[1]));
    }
 
    ++validcount;
    DrawSector(viewplayer->mo->subsector->sector);

    DrawFloor();
}
