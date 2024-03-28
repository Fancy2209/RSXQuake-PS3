/*
Copyright (C) 2008 Eluan Costa Miranda

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <ogc/cache.h>
#include <ogc/system.h>
#include <ogc/lwp_heap.h>
#include <ogc/lwp_mutex.h>

#include "../../generic/quakedef.h"

#include <gccore.h>
#include <malloc.h>
#include "gxutils.h"

// ELUTODO: GL_Upload32 and GL_Update32 could use some optimizations
// ELUTODO: mipmap and texture filters

cvar_t		gl_max_size = {"gl_max_size", "1024"};

int		texels;

gltexture_t	gltextures[MAX_GLTEXTURES];
gxtexobj_t	gxtexobjs[MAX_GLTEXTURES];
int			numgltextures;

heap_cntrl texture_heap;
void *texture_heap_ptr;
u32 texture_heap_size;

void R_InitTextureHeap (void)
{
	u32 level, size;

	_CPU_ISR_Disable(level);
	texture_heap_ptr = SYS_GetArena2Lo();
	texture_heap_size = 32 * 1024 * 1024;
	if ((u32)texture_heap_ptr + texture_heap_size > (u32)SYS_GetArena2Hi())
	{
		_CPU_ISR_Restore(level);
		Sys_Error("texture_heap + texture_heap_size > (u32)SYS_GetArena2Hi()");
	}	
	else
	{
		SYS_SetArena2Lo(texture_heap_ptr + texture_heap_size);
		_CPU_ISR_Restore(level);
	}

	memset(texture_heap_ptr, 0, texture_heap_size);

	size = __lwp_heap_init(&texture_heap, texture_heap_ptr, texture_heap_size, PPC_CACHE_ALIGNMENT);

	Con_Printf("Allocated %dM texture heap.\n", size / (1024 * 1024));
}

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x,y, m;
	byte	*dest;

	R_InitTextureHeap();

	Cvar_RegisterVariable (&gl_max_size);

	numgltextures = 0;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
}

void GL_Bind0 (int texnum)
{
	if (currenttexture0 == texnum)
		return;

	if (!gltextures[texnum].used)
		Sys_Error("Tried to bind a inactive texture0.");

	currenttexture0 = texnum;
	GX_LoadTexObj(&(gltextures[texnum].gx_tex), GX_TEXMAP0);
}

void GL_Bind1 (int texnum)
{
	if (currenttexture1 == texnum)
		return;

	if (!gltextures[texnum].used)
		Sys_Error("Tried to bind a inactive texture1.");

	currenttexture1 = texnum;
	GX_LoadTexObj(&(gltextures[texnum].gx_tex), GX_TEXMAP1);
}

void QGX_ZMode(qboolean state)
{
	if (state)
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	else
		GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_TRUE);
}

void QGX_Alpha(qboolean state)
{
	if (state)
		GX_SetAlphaCompare(GX_GREATER,0,GX_AOP_AND,GX_ALWAYS,0);
	else
		GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
}

void QGX_Blend(qboolean state)
{
	if (state)
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	else
		GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}

//More GX helpers :)

static u8 oldtarget = GX_TEXMAP0;
int		gx_tex_allocated; // To track amount of memory used for textures

qboolean GX_ReallocTex(int length, int width, int height)
{
	qboolean changed = false;
	if(gxtexobjs[currenttexture1].length < length)
	{
		if(gxtexobjs[currenttexture1].data != NULL)
		{
			free(gxtexobjs[currenttexture1].data);
			gxtexobjs[currenttexture1].data = NULL;
			gx_tex_allocated -= gxtexobjs[currenttexture1].length;
		};
		gxtexobjs[currenttexture1].data = memalign(32, length);
		if(gxtexobjs[currenttexture1].data == NULL)
		{
			Sys_Error("GX_ReallocTex: allocation failed on %i bytes", length);
		};
		gxtexobjs[currenttexture1].length = length;
		gx_tex_allocated += length;
		changed = true;
	};
	gxtexobjs[currenttexture1].width = width;
	gxtexobjs[currenttexture1].height = height;
	return changed;
}

void GX_BindCurrentTex(qboolean changed, int format, int mipmap)
{
	DCFlushRange(gxtexobjs[currenttexture1].data, gxtexobjs[currenttexture1].length);
	GX_InitTexObj(&gxtexobjs[currenttexture1].texobj, gxtexobjs[currenttexture1].data, gxtexobjs[currenttexture1].width, gxtexobjs[currenttexture1].height, format, GX_REPEAT, GX_REPEAT, mipmap);
	GX_LoadTexObj(&gxtexobjs[currenttexture1].texobj, oldtarget - GX_TEXMAP0);
	if(changed)
		GX_InvalidateTexAll();
}

void GX_LoadAndBind (void* data, int length, int width, int height, int format)
{
	qboolean changed = GX_ReallocTex(length, width, height);
	switch(format)
	{
	case GX_TF_RGBA8:
		GXU_CopyTexRGBA8((byte*)data, width, height, (byte*)(gxtexobjs[currenttexture1].data));
		break;
	case GX_TF_RGB5A3:
		GXU_CopyTexRGB5A3((byte*)data, width, height, (byte*)(gxtexobjs[currenttexture1].data));
		break;
	}
/*
	case GX_TF_CI8:
	case GX_TF_I8:
	case GX_TF_A8:
		GXU_CopyTexV8((byte*)data, width, height, (byte*)(gxtexobjs[currenttexture].data));
		break;
	case GX_TF_IA4:
		GXU_CopyTexIA4((byte*)data, width, height, (byte*)(gxtexobjs[currenttexture].data));
		break;
	};
*/
	GX_BindCurrentTex(changed, format, GX_FALSE);
}


//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (gltextures[i].used)
			if (!strcmp (identifier, glt->identifier))
				return gltextures[i].texnum;
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}

// FIXME, temporary
static	unsigned	scaled[640*480];
static	unsigned	trans[640*480];

/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (gltexture_t *destination, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, x, y, s;
	u8			*pos;
	int			samples;
	int			scaled_width, scaled_height;

	for (scaled_width = 1 << 5 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 << 5 ; scaled_height < height ; scaled_height<<=1)
		;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	// ELUTODO: gl_max_size should be multiple of 32?
	// ELUTODO: mipmaps

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_Upload32: too big");

	// ELUTODO samples = alpha ? GX_TF_RGBA8 : GX_TF_RGBA8;

	texels += scaled_width * scaled_height;

	if (scaled_width != width || scaled_height != height)
	{
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}
	else
	{
		memcpy(scaled, data, scaled_width * scaled_height * sizeof(unsigned));
	}

	destination->data = __lwp_heap_allocate(&texture_heap, scaled_width * scaled_height * sizeof(unsigned));
	if (!destination->data)
		Sys_Error("GL_Upload32: Out of memory.");

	destination->scaled_width = scaled_width;
	destination->scaled_height = scaled_height;

	s = scaled_width * scaled_height;
	if (s & 31)
		Sys_Error ("GL_Upload32: s&31");

	if ((int)destination->data & 31)
		Sys_Error ("GL_Upload32: destination->data&31");

	pos = (u8 *)destination->data;
	for (y = 0; y < scaled_height; y += 4)
	{
		u8* row1 = (u8 *)&(scaled[scaled_width * (y + 0)]);
		u8* row2 = (u8 *)&(scaled[scaled_width * (y + 1)]);
		u8* row3 = (u8 *)&(scaled[scaled_width * (y + 2)]);
		u8* row4 = (u8 *)&(scaled[scaled_width * (y + 3)]);

		for (x = 0; x < scaled_width; x += 4)
		{
			u8 AR[32];
			u8 GB[32];

			for (i = 0; i < 4; i++)
			{
				u8* ptr1 = &(row1[(x + i) * 4]);
				u8* ptr2 = &(row2[(x + i) * 4]);
				u8* ptr3 = &(row3[(x + i) * 4]);
				u8* ptr4 = &(row4[(x + i) * 4]);

				AR[(i * 2) +  0] = ptr1[0];
				AR[(i * 2) +  1] = ptr1[3];
				AR[(i * 2) +  8] = ptr2[0];
				AR[(i * 2) +  9] = ptr2[3];
				AR[(i * 2) + 16] = ptr3[0];
				AR[(i * 2) + 17] = ptr3[3];
				AR[(i * 2) + 24] = ptr4[0];
				AR[(i * 2) + 25] = ptr4[3];

				GB[(i * 2) +  0] = ptr1[2];
				GB[(i * 2) +  1] = ptr1[1];
				GB[(i * 2) +  8] = ptr2[2];
				GB[(i * 2) +  9] = ptr2[1];
				GB[(i * 2) + 16] = ptr3[2];
				GB[(i * 2) + 17] = ptr3[1];
				GB[(i * 2) + 24] = ptr4[2];
				GB[(i * 2) + 25] = ptr4[1];
			}

			memcpy(pos, AR, sizeof(AR));
			pos += sizeof(AR);
			memcpy(pos, GB, sizeof(GB));
			pos += sizeof(GB);
		}
	}

	GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, /*mipmap ? GX_TRUE :*/ GX_FALSE);

	DCFlushRange(destination->data, scaled_width * scaled_height * sizeof(unsigned));
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (gltexture_t *destination, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, s;
	qboolean	noalpha;
	int			p;

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	}
	else
	{
		if (s&3)
			Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

	GL_Upload32 (destination, trans, width, height, mipmap, alpha);
}

byte		vid_gamma_table[256];
void Build_Gamma_Table (void) {
	int		i;
	float		inf;
	float   in_gamma;

	if ((i = COM_CheckParm("-gamma")) != 0 && i+1 < com_argc) {
		in_gamma = Q_atof(com_argv[i+1]);
		if (in_gamma < 0.3) in_gamma = 0.3;
		if (in_gamma > 1) in_gamma = 1.0;
	} else {
		in_gamma = 1;
	}

	if (in_gamma != 1) {
		for (i=0 ; i<256 ; i++) {
			inf = min(255 * pow((i + 0.5) / 255.5, in_gamma) + 0.5, 255);
			vid_gamma_table[i] = inf;
		}
	} else {
		for (i=0 ; i<256 ; i++)
			vid_gamma_table[i] = i;
	}

}

/*
================
GL_LoadTexture
================
*/
//sB modified for Wii
int GL_LoadTexture32 (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, qboolean keep)
{
	
	int			i;
	gltexture_t	*glt;
	int image_size = width * height;

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (glt->used)
			{
				// ELUTODO: causes problems if we compare to a texture with NO name?
				if (!strcmp (identifier, glt->identifier))
				{
					if (width != glt->width || height != glt->height)
					{
						//Con_DPrintf ("GL_LoadTexture: cache mismatch, reloading");
						if (!__lwp_heap_free(&texture_heap, glt->data))
							Sys_Error("GL_ClearTextureCache: Error freeing data.");
						goto reload; // best way to do it
					}
					return glt->texnum;
				}
			}
		}
	}

	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (!glt->used)
			break;
	}

	if (i == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: numgltextures == MAX_GLTEXTURES\n");

reload:
	strcpy (glt->identifier, identifier);
	glt->texnum = i;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;
	glt->type = 0;
	glt->keep = keep;
	glt->used = true;

	// Baker: this applies our -gamma parameter table
	if (1) {
		//extern	byte	vid_gamma_table[256];
		for (i = 0; i < image_size; i++){
			data[4 * i] = vid_gamma_table[data[4 * i]];
			data[4 * i + 1] = vid_gamma_table[data[4 * i + 1]];
			data[4 * i + 2] = vid_gamma_table[data[4 * i + 2]];
		}
	}

	GL_Upload32 (glt, (unsigned *)data, width, height, mipmap, alpha); 

	if (glt->texnum == numgltextures)
		numgltextures++;

	return glt->texnum;
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, qboolean keep)
{
	int			i;
	gltexture_t	*glt;

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (glt->used)
			{
				// ELUTODO: causes problems if we compare to a texture with NO name?
				if (!strcmp (identifier, glt->identifier))
				{
					if (width != glt->width || height != glt->height)
					{
						//Con_DPrintf ("GL_LoadTexture: cache mismatch, reloading");
						if (!__lwp_heap_free(&texture_heap, glt->data))
							Sys_Error("GL_ClearTextureCache: Error freeing data.");
						goto reload; // best way to do it
					}
					return glt->texnum;
				}
			}
		}
	}

	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (!glt->used)
			break;
	}

	if (i == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: numgltextures == MAX_GLTEXTURES\n");

reload:
	strcpy (glt->identifier, identifier);
	glt->texnum = i;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;
	glt->type = 0;
	glt->keep = keep;
	glt->used = true;

	GL_Upload8 (glt, data, width, height, mipmap, alpha);

	if (glt->texnum == numgltextures)
		numgltextures++;

	return glt->texnum;
}

/*
======================
GL_LoadLightmapTexture
======================
*/
int GL_LoadLightmapTexture (char *identifier, int width, int height, byte *data)
{
	gltexture_t	*glt;

	// They need to be allocated sequentially
	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadLightmapTexture: numgltextures == MAX_GLTEXTURES\n");

	glt = &gltextures[numgltextures];
	strcpy (glt->identifier, identifier);
	glt->texnum = numgltextures;
	glt->width = width;
	glt->height = height;
	glt->mipmap = true; // ELUTODO
	glt->type = 1;
	glt->keep = false;
	glt->used = true;

	GL_Upload32 (glt, (unsigned *)data, width, height, true, false);

	if (width != glt->scaled_width || height != glt->scaled_height)
		Sys_Error("GL_LoadLightmapTexture: Tried to scale lightmap\n");

	numgltextures++;

	return glt->texnum;
}

/*
===============
GL_Update32
===============
*/
void GL_Update32 (gltexture_t *destination, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, x, y, s;
	u8			*pos;
	int			samples;
	int			scaled_width, scaled_height;

	for (scaled_width = 1 << 5 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 << 5 ; scaled_height < height ; scaled_height<<=1)
		;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	// ELUTODO: gl_max_size should be multiple of 32?
	// ELUTODO: mipmaps

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_Update32: too big");

	// ELUTODO samples = alpha ? GX_TF_RGBA8 : GX_TF_RGBA8;

	if (scaled_width != width || scaled_height != height)
	{
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}
	else
	{
		memcpy(scaled, data, scaled_width * scaled_height * sizeof(unsigned));
	}

	s = scaled_width * scaled_height;
	if (s & 31)
		Sys_Error ("GL_Update32: s&31");

	if ((int)destination->data & 31)
		Sys_Error ("GL_Update32: destination->data&31");

	pos = (u8 *)destination->data;
	for (y = 0; y < scaled_height; y += 4)
	{
		u8* row1 = (u8 *)&(scaled[scaled_width * (y + 0)]);
		u8* row2 = (u8 *)&(scaled[scaled_width * (y + 1)]);
		u8* row3 = (u8 *)&(scaled[scaled_width * (y + 2)]);
		u8* row4 = (u8 *)&(scaled[scaled_width * (y + 3)]);

		for (x = 0; x < scaled_width; x += 4)
		{
			u8 AR[32];
			u8 GB[32];

			for (i = 0; i < 4; i++)
			{
				u8* ptr1 = &(row1[(x + i) * 4]);
				u8* ptr2 = &(row2[(x + i) * 4]);
				u8* ptr3 = &(row3[(x + i) * 4]);
				u8* ptr4 = &(row4[(x + i) * 4]);

				AR[(i * 2) +  0] = ptr1[0];
				AR[(i * 2) +  1] = ptr1[3];
				AR[(i * 2) +  8] = ptr2[0];
				AR[(i * 2) +  9] = ptr2[3];
				AR[(i * 2) + 16] = ptr3[0];
				AR[(i * 2) + 17] = ptr3[3];
				AR[(i * 2) + 24] = ptr4[0];
				AR[(i * 2) + 25] = ptr4[3];

				GB[(i * 2) +  0] = ptr1[2];
				GB[(i * 2) +  1] = ptr1[1];
				GB[(i * 2) +  8] = ptr2[2];
				GB[(i * 2) +  9] = ptr2[1];
				GB[(i * 2) + 16] = ptr3[2];
				GB[(i * 2) + 17] = ptr3[1];
				GB[(i * 2) + 24] = ptr4[2];
				GB[(i * 2) + 25] = ptr4[1];
			}

			memcpy(pos, AR, sizeof(AR));
			pos += sizeof(AR);
			memcpy(pos, GB, sizeof(GB));
			pos += sizeof(GB);
		}
	}

	DCFlushRange(destination->data, scaled_width * scaled_height * sizeof(unsigned));
	GX_InvalidateTexAll();
}

/*
===============
GL_Update8
===============
*/
void GL_Update8 (gltexture_t *destination, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, s;
	qboolean	noalpha;
	int			p;

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	}
	else
	{
		if (s&3)
			Sys_Error ("GL_Update8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

	GL_Update32 (destination, trans, width, height, mipmap, alpha);
}

/*
================
GL_UpdateTexture
================
*/
void GL_UpdateTexture (int pic_id, char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha)
{
	gltexture_t	*glt;

	// see if the texture is allready present
	glt = &gltextures[pic_id];

	if (strcmp (identifier, glt->identifier) || width != glt->width || height != glt->height || mipmap != glt->mipmap || glt->type != 0 || !glt->used)
			Sys_Error ("GL_UpdateTexture: cache mismatch");

	GL_Update8 (glt, data, width, height, mipmap, alpha);
}

const int lightblock_datamap[128*128*4] =
{
#include "128_128_datamap.h"
};

/*
================================
GL_UpdateLightmapTextureRegion32
================================
*/
void GL_UpdateLightmapTextureRegion32 (gltexture_t *destination, unsigned *data, int width, int height, int xoffset, int yoffset, qboolean mipmap, qboolean alpha)
{
	int			x, y, pos;
	int			samples;
	int			realwidth = width + xoffset;
	int			realheight = height + yoffset;
	u8			*dest = (u8 *)destination->data, *src = (u8 *)data;

	// ELUTODO: mipmaps
	// ELUTODO samples = alpha ? GX_TF_RGBA8 : GX_TF_RGBA8;

	if ((int)destination->data & 31)
		Sys_Error ("GL_Update32: destination->data&31");

	for (y = yoffset; y < realheight; y++)
	{
		for (x = xoffset; x < realwidth; x++)
		{
			pos = (x + y * realwidth) * 4;
			dest[lightblock_datamap[pos]] = src[pos];
			dest[lightblock_datamap[pos + 1]] = src[pos + 1];
			dest[lightblock_datamap[pos + 2]] = src[pos + 2];
			dest[lightblock_datamap[pos + 3]] = src[pos + 3];
		}
	}

	// ELUTODO: flush region only
	DCFlushRange(destination->data, destination->scaled_width * destination->scaled_height * sizeof(unsigned));
	GX_InvalidateTexAll();
}

/*
==============================
GL_UpdateLightmapTextureRegion
==============================
*/
// ELUTODO: doesn't work if the texture doesn't follow the default quake format. Needs improvements.
void GL_UpdateLightmapTextureRegion (int pic_id, int width, int height, int xoffset, int yoffset, byte *data)
{
	gltexture_t	*destination;

	// see if the texture is allready present
	destination = &gltextures[pic_id];

	GL_UpdateLightmapTextureRegion32 (destination, (unsigned *)data, width, height, xoffset, yoffset, true, true);
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
	// ELUTODO: loading too much with "" fills the memory with repeated data? Hope not... Check later.
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true, true);
}

// ELUTODO: clean the disable/enable multitexture calls around the engine

void GL_DisableMultitexture(void)
{
	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
}

void GL_EnableMultitexture(void)
{
	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX1, GX_DIRECT);

	GX_SetNumTexGens(2);
	GX_SetNumTevStages(2);
}

void GL_ClearTextureCache(void)
{
	int i;
	int oldnumgltextures = numgltextures;
	void *newdata;

	numgltextures = 0;

	for (i = 0; i < oldnumgltextures; i++)
	{
		if (gltextures[i].used)
		{
			if (gltextures[i].keep)
			{
				numgltextures = i + 1;

				newdata = __lwp_heap_allocate(&texture_heap, gltextures[i].scaled_width * gltextures[i].scaled_height * sizeof(unsigned));
				if (!newdata)
					Sys_Error("GL_ClearTextureCache: Out of memory.");

				// ELUTODO Pseudo-defragmentation that helps a bit :)
				memcpy(newdata, gltextures[i].data, gltextures[i].scaled_width * gltextures[i].scaled_height * sizeof(unsigned));

				if (!__lwp_heap_free(&texture_heap, gltextures[i].data))
					Sys_Error("GL_ClearTextureCache: Error freeing data.");

				gltextures[i].data = newdata;
				GX_InitTexObj(&gltextures[i].gx_tex, gltextures[i].data, gltextures[i].scaled_width, gltextures[i].scaled_height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, /*mipmap ? GX_TRUE :*/ GX_FALSE);

				DCFlushRange(gltextures[i].data, gltextures[i].scaled_width * gltextures[i].scaled_height * sizeof(unsigned));
			}
			else
			{
				gltextures[i].used = false;
				if (!__lwp_heap_free(&texture_heap, gltextures[i].data))
					Sys_Error("GL_ClearTextureCache: Error freeing data.");
			}
		}
	}

	GX_InvalidateTexAll();
}

/*
//
//
//
//
//
//
*/

//Diabolickal TGA Begin
int		image_width;
int		image_height;
#define	IMAGE_MAX_DIMENSIONS	512

/*
=================================================================
  PCX Loading
=================================================================
*/

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned 	data;			// unbounded
} pcx_t;

/*
============
LoadPCX
============
*/
byte* LoadPCX (FILE *f, int matchwidth, int matchheight)
{
	pcx_t	*pcx, pcxbuf;
	byte	palette[768];
	byte	*pix, *image_rgba;
	int		x, y;
	int		dataByte, runLength;
	int		count;

//
// parse the PCX file
//
	fread (&pcxbuf, 1, sizeof(pcxbuf), f);
	pcx = &pcxbuf;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 514
		|| pcx->ymax >= 514)
	{
		Con_Printf ("Bad pcx file\n");
		return NULL;
	}
	if (matchwidth && (pcx->xmax+1) != matchwidth)
		return NULL;
	if (matchheight && (pcx->ymax+1) != matchheight)
		return NULL;
	// seek to palette
	fseek (f, -768, SEEK_END);
	fread (palette, 1, 768, f);
	fseek (f, sizeof(pcxbuf) - 4, SEEK_SET);
	count = (pcx->xmax+1) * (pcx->ymax+1);
	image_rgba = (byte*)malloc( count * 4);

	for (y=0 ; y<=pcx->ymax ; y++)
	{
		pix = image_rgba + 4*y*(pcx->xmax+1);
		for (x=0 ; x<=pcx->xmax ; ) // muff - fixed - was referencing ymax
		{
			dataByte = fgetc(f);
			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = fgetc(f);
			}
			else
				runLength = 1;

			while(runLength-- > 0)
			{
				pix[0] = palette[dataByte*3];
				pix[1] = palette[dataByte*3+1];
				pix[2] = palette[dataByte*3+2];
				pix[3] = 255;
				pix += 4;
				x++;
			}
		}
	}
	image_width = pcx->xmax+1;
	image_height = pcx->ymax+1;

	fclose(f);
	return image_rgba;
}


/*
=========================================================

			Targa

=========================================================
*/

#define TGA_MAXCOLORS 16384

/* Definitions for image types. */
#define TGA_Null	0	/* no image data */
#define TGA_Map		1	/* Uncompressed, color-mapped images. */
#define TGA_RGB		2	/* Uncompressed, RGB images. */
#define TGA_Mono	3	/* Uncompressed, black and white images. */
#define TGA_RLEMap	9	/* Runlength encoded color-mapped images. */
#define TGA_RLERGB	10	/* Runlength encoded RGB images. */
#define TGA_RLEMono	11	/* Compressed, black and white images. */
#define TGA_CompMap	32	/* Compressed color-mapped data, using Huffman, Delta, and runlength encoding. */
#define TGA_CompMap4	33	/* Compressed color-mapped data, using Huffman, Delta, and runlength encoding. 4-pass quadtree-type process. */

/* Definitions for interleave flag. */
#define TGA_IL_None	0	/* non-interleaved. */
#define TGA_IL_Two	1	/* two-way (even/odd) interleaving */
#define TGA_IL_Four	2	/* four way interleaving */
#define TGA_IL_Reserved	3	/* reserved */

/* Definitions for origin flag */
#define TGA_O_UPPER	0	/* Origin in lower left-hand corner. */
#define TGA_O_LOWER	1	/* Origin in upper left-hand corner. */

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

int fgetLittleShort (FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + b2*256);
}

int fgetLittleLong (FILE *f)
{
	byte	b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}

/*
=============
LoadTGA
=============
*/
byte *LoadTGA (FILE *fin, int matchwidth, int matchheight)
{
	int		w, h, x, y, realrow, truerow, baserow, i, temp1, temp2, pixel_size, map_idx;
	int		RLE_count, RLE_flag, size, interleave, origin;
	qboolean	mapped, rlencoded;
	byte		*data, *dst, r, g, b, a, j, k, l, *ColorMap;
	TargaHeader	header;

	header.id_length = fgetc (fin);
	header.colormap_type = fgetc (fin);
	header.image_type = fgetc (fin);
	header.colormap_index = fgetLittleShort (fin);
	header.colormap_length = fgetLittleShort (fin);
	header.colormap_size = fgetc (fin);
	header.x_origin = fgetLittleShort (fin);
	header.y_origin = fgetLittleShort (fin);
	header.width = fgetLittleShort (fin);
	header.height = fgetLittleShort (fin);
	header.pixel_size = fgetc (fin);
	header.attributes = fgetc (fin);

	if (header.width > IMAGE_MAX_DIMENSIONS || header.height > IMAGE_MAX_DIMENSIONS)
	{
		Con_DPrintf ("TGA image %s exceeds maximum supported dimensions\n", fin);
		fclose (fin);
		return NULL;
	}

	if ((matchwidth && header.width != matchwidth) || (matchheight && header.height != matchheight))
	{
		fclose (fin);
		return NULL;
	}

	if (header.id_length != 0)
		fseek (fin, header.id_length, SEEK_CUR);

	/* validate TGA type */
	switch (header.image_type)
	{
	case TGA_Map:
	case TGA_RGB:
	case TGA_Mono:
	case TGA_RLEMap:
	case TGA_RLERGB:
	case TGA_RLEMono:
		break;

	default:
		Con_DPrintf ("Unsupported TGA image %s: Only type 1 (map), 2 (RGB), 3 (mono), 9 (RLEmap), 10 (RLERGB), 11 (RLEmono) TGA images supported\n");
		fclose (fin);
		return NULL;
	}

	/* validate color depth */
	switch (header.pixel_size)
	{
	case 8:
	case 15:
	case 16:
	case 24:
	case 32:
		break;

	default:
		Con_DPrintf ("Unsupported TGA image %s: Only 8, 15, 16, 24 or 32 bit images (with colormaps) supported\n");
		fclose (fin);
		return NULL;
	}

	r = g = b = a = l = 0;

	/* if required, read the color map information. */
	ColorMap = NULL;
	mapped = (header.image_type == TGA_Map || header.image_type == TGA_RLEMap) && header.colormap_type == 1;
	if (mapped)
	{
		/* validate colormap size */
		switch (header.colormap_size)
		{
		case 8:
		case 15:
		case 16:
		case 32:
		case 24:
			break;

		default:
			Con_DPrintf ("Unsupported TGA image %s: Only 8, 15, 16, 24 or 32 bit colormaps supported\n");
			fclose (fin);
			return NULL;
		}

		temp1 = header.colormap_index;
		temp2 = header.colormap_length;
		if ((temp1 + temp2 + 1) >= TGA_MAXCOLORS)
		{
			fclose (fin);
			return NULL;
		}
		ColorMap = (byte*)(malloc (TGA_MAXCOLORS * 4));
		map_idx = 0;
		for (i = temp1 ; i < temp1 + temp2 ; ++i, map_idx += 4)
		{
			/* read appropriate number of bytes, break into rgb & put in map. */
			switch (header.colormap_size)
			{
			case 8:	/* grey scale, read and triplicate. */
				r = g = b = getc (fin);
				a = 255;
				break;

			case 15:	/* 5 bits each of red green and blue. */
						/* watch byte order. */
				j = getc (fin);
				k = getc (fin);
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = 255;
				break;

			case 16:	/* 5 bits each of red green and blue, 1 alpha bit. */
						/* watch byte order. */
				j = getc (fin);
				k = getc (fin);
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = (k & 0x80) ? 255 : 0;
				break;

			case 24:	/* 8 bits each of blue, green and red. */
				b = getc (fin);
				g = getc (fin);
				r = getc (fin);
				a = 255;
				l = 0;
				break;

			case 32:	/* 8 bits each of blue, green, red and alpha. */
				b = getc (fin);
				g = getc (fin);
				r = getc (fin);
				a = getc (fin);
				l = 0;
				break;
			}
			ColorMap[map_idx+0] = r;
			ColorMap[map_idx+1] = g;
			ColorMap[map_idx+2] = b;
			ColorMap[map_idx+3] = a;
		}
	}

	/* check run-length encoding. */
	rlencoded = (header.image_type == TGA_RLEMap || header.image_type == TGA_RLERGB || header.image_type == TGA_RLEMono);
	RLE_count = RLE_flag = 0;

	image_width = w = header.width;
	image_height = h = header.height;

	size = w * h * 4;
	data = (byte*)(malloc (size));

	/* read the Targa file body and convert to portable format. */
	pixel_size = header.pixel_size;
	origin = (header.attributes & 0x20) >> 5;
	interleave = (header.attributes & 0xC0) >> 6;
	truerow = baserow = 0;
	for (y=0 ; y<h ; y++)
	{
		realrow = truerow;
		if (origin == TGA_O_UPPER)
			realrow = h - realrow - 1;

		dst = data + realrow * w * 4;

		for (x=0 ; x<w ; x++)
		{
			/* check if run length encoded. */
			if (rlencoded)
			{
				if (!RLE_count)
				{
					/* have to restart run. */
					i = getc (fin);
					RLE_flag = (i & 0x80);
					if (!RLE_flag)	// stream of unencoded pixels
						RLE_count = i + 1;
					else		// single pixel replicated
						RLE_count = i - 127;
					/* decrement count & get pixel. */
					--RLE_count;
				}
				else
				{
					/* have already read count & (at least) first pixel. */
					--RLE_count;
					if (RLE_flag)
						/* replicated pixels. */
						goto PixEncode;
				}
			}

			/* read appropriate number of bytes, break into RGB. */
			switch (pixel_size)
			{
			case 8:	/* grey scale, read and triplicate. */
				r = g = b = l = getc (fin);
				a = 255;
				break;

			case 15:	/* 5 bits each of red green and blue. */
						/* watch byte order. */
				j = getc (fin);
				k = getc (fin);
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = 255;
				break;

			case 16:	/* 5 bits each of red green and blue, 1 alpha bit. */
						/* watch byte order. */
				j = getc (fin);
				k = getc (fin);
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = (k & 0x80) ? 255 : 0;
				break;

			case 24:	/* 8 bits each of blue, green and red. */
				b = getc (fin);
				g = getc (fin);
				r = getc (fin);
				a = 255;
				l = 0;
				break;

			case 32:	/* 8 bits each of blue, green, red and alpha. */
				b = getc (fin);
				g = getc (fin);
				r = getc (fin);
				a = getc (fin);
				l = 0;
				break;

			default:
				Con_DPrintf ("Malformed TGA image: Illegal pixel_size '%d'\n", pixel_size);
				fclose (fin);
				free (data);
				if (mapped)
					free (ColorMap);
				return NULL;
			}

PixEncode:
			if (mapped)
			{
				map_idx = l * 4;
				*dst++ = ColorMap[map_idx+0];
				*dst++ = ColorMap[map_idx+1];
				*dst++ = ColorMap[map_idx+2];
				*dst++ = ColorMap[map_idx+3];
			}
			else
			{
				*dst++ = r;
				*dst++ = g;
				*dst++ = b;
				*dst++ = a;
			}
		}

		if (interleave == TGA_IL_Four)
			truerow += 4;
		else if (interleave == TGA_IL_Two)
			truerow += 2;
		else
			truerow++;
		if (truerow >= h)
			truerow = ++baserow;
	}

	if (mapped)
		free (ColorMap);

	fclose (fin);

	return data;
}

/*small function to read files with stb_image - single-file image loader library.
** downloaded from: https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
** only use jpeg+png formats, because tbh there's not much need for the others.
** */
/*
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"
byte* LoadSTBI(FILE *f, int width, int height)
{
	int bpp;
	int inwidth, inheight;
	byte* image = stbi_load_from_file(f, &inwidth, &inheight, &bpp, 4);
	// wtf?
	image_width = inwidth;
	image_height = inheight;
	fclose(f);
	return image;
}
*/

byte* loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight)

{
	FILE	*f;
	char	basename[128], name[132];
	byte	*c;

	if (complain == false)
		COM_StripExtension(filename, basename); // strip the extension to allow TGA
	else
		strcpy(basename, filename);

	c = (byte*)basename;
	while (*c)
	{
		if (*c == '*')
			*c = '+';
		c++;
	}

	//Try PCX
	sprintf (name, "%s.pcx", basename);
	COM_FOpenFile (name, &f);
	if (f)
		return LoadPCX (f, matchwidth, matchheight);
	//Try TGA
	sprintf (name, "%s.tga", basename);
	COM_FOpenFile (name, &f);
	if (f)
		return LoadTGA (f, matchwidth, matchheight);
	//Try PNG
	/*
	sprintf (name, "%s.png", basename);
	COM_FOpenFile (name, &f);
	if (f)
		return LoadSTBI (f, matchwidth, matchheight);
	//Try JPEG
	sprintf (name, "%s.jpeg", basename);
	COM_FOpenFile (name, &f);
	if (f)
		return LoadSTBI (f, matchwidth, matchheight);
	sprintf (name, "%s.jpg", basename);
	COM_FOpenFile (name, &f);
	if (f)
		return LoadSTBI (f, matchwidth, matchheight);
	*/
	
	//if (complain)
	//	Con_Printf ("Couldn't load %s.tga or %s.pcx \n", filename);
	
	return NULL;
}

int loadtextureimage (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap)
{
	int texnum;
	byte *data;
	if (!(data = loadimagepixels (filename, complain, matchwidth, matchheight))) { 
		Con_DPrintf("Cannot load image %s\n", filename);
		return 0;
	}
	texnum = GL_LoadTexture (filename, image_width, image_height, data, mipmap, true, false);
	                        //identifer, width, height, data, mipmap, alpha, bytesperpixel
	free(data);
	return texnum;
}
// Tomaz || TGA End

