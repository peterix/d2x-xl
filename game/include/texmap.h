/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#ifndef _TEXMAP_H
#define _TEXMAP_H

#include "fix.h"
#include "3d.h"
#include "gr.h"

#define NUM_LIGHTING_LEVELS 32
#define MAX_TMAP_VERTS 25
#define MAX_LIGHTING_VALUE ((NUM_LIGHTING_LEVELS - 1) * I2X(1) / NUM_LIGHTING_LEVELS)
#define MIN_LIGHTING_VALUE (I2X(1) / NUM_LIGHTING_LEVELS)

#define FIX_RECIP_TABLE_SIZE \
    641 // increased from 321 to 641, since this res is now quite achievable.. slight fps boost -MM
// -------------------------------------------------------------------------------------------------------
extern fix compute_lightingValue(CRenderPoint *vertptr);

// -------------------------------------------------------------------------------------------------------
// This is the main texture mapper call.
// tmap_num references a texture map defined in Texmap_ptrs.
// nverts = number of vertices
// vertbuf is a pointer to an array of vertex pointers
extern void draw_tmap(CBitmap *bp, int32_t nverts, CRenderPoint **vertbuf);

// -------------------------------------------------------------------------------------------------------
// Texture map vertex.
// The fields r,g,b and l are mutually exclusive.  r,g,b are used for rgb lighting.
// l is used for intensity based lighting.
typedef struct g3ds_vertex {
    fix x, y, z;
    fix u, v;
    fix x2d, y2d;
    fix l;
    fix r, g, b;
} g3ds_vertex;

// A texture map is defined as a polygon with u,v coordinates associated with
// one point in the polygon, and a pair of vectors describing the orientation
// of the texture map in the world, from which the deltas Du_dx, Dv_dy, etc.
// are computed.
typedef struct g3ds_tmap {
    int32_t nv; // number of vertices
    g3ds_vertex verts[MAX_TMAP_VERTS]; // up to 8 vertices, this is inefficient, change
} g3ds_tmap;

// -------------------------------------------------------------------------------------------------------

// Note:	Not all interpolation method and lighting combinations are supported.
// Set Interpolation_method to 0/1/2 for linear/linear, perspective/linear, perspective/perspective
extern int32_t Interpolation_method;

// Set Lighting_on to 0/1/2 for no lighting/intensity lighting/rgb lighting
extern int32_t Lighting_on;

// HACK INTERFACE: how far away the current CSegment (& thus texture) is
extern int32_t nCurrentSegDepth;
extern int32_t Max_perspective_depth; // Deepest CSegment at which perspective interpolation will be used.
extern int32_t Max_linear_depth; // Deepest CSegment at which linear interpolation will be used.
extern int32_t
    Max_flat_depth; // Deepest CSegment at which flat shading will be used. (If not flat shading, then what?)

// These are pointers to texture maps.  If you want to render texture map #7, then you will render
// the texture map defined by Texmap_ptrs[7].
extern CBitmap Texmap_ptrs[];
extern CBitmap Texmap4_ptrs[];

// Interface for sky renderer
void texture_map_lin_lin_sky(CBitmap *srcb, g3ds_tmap *t);
void texture_map_lin_lin_sky_v(CBitmap *srcb, g3ds_tmap *t);
void texture_map_hyp_lin_v(CBitmap *srcb, g3ds_tmap *t);

void ntexture_map_lighted_linear(CBitmap *srcb, g3ds_tmap *t);

// This is the gr_upoly-like interface to the texture mapper which uses texture-mapper compatible
// (ie, avoids cracking) edge/delta computation.
void gr_upoly_tmap(int32_t nverts, int32_t *vert);

// This is like gr_upoly_tmap() but instead of drawing, it calls the specified
// function with ylr values
void gr_upoly_tmap_ylr(int32_t nverts, int32_t *vert, void (*ylr_func)(int32_t, fix, fix));

extern int32_t Transparency_on, per2Flag;

// Set to !0 to enable Sim City 2000 (or Eric's Drive Through, or Eric's Game) specific code.
extern int32_t SC2000;

extern int32_t nWindowClipLeft, nWindowClipBot, nWindowClipRight, nWindowClipTop;

// for ugly hack put in to be sure we don't overflow render buffer

#define FIX_XLIMIT (639 * I2X(1))
#define FIX_YLIMIT (479 * I2X(1))

extern void init_interface_vars_to_assembler(void);

#endif
