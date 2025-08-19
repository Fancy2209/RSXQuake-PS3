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

#include "../../generic/quakedef.h"

#include <malloc.h>

// ELUTODO: GL_Upload32 and GL_Update32 could use some optimizations
// ELUTODO: mipmap and texture filters

cvar_t		gl_max_size = {"gl_max_size", "1024"};

int		texels;

gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

void *texture_heap_ptr;
u32 texture_heap_size;

void R_InitTextureHeap (void)
{
	// TODO: Code for PS3
	return;
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

void GL_Bind (int texnum)
{
	return;
}

void QGX_ZMode(qboolean state)
{
return;
}

void QGX_Alpha(qboolean state)
{
return;
}

void QGX_AlphaMap(qboolean state)
{
	return;
}

void QGX_Blend(qboolean state)
{
return;
}

void QGX_BlendMap(qboolean state)
{
return;
}

void QGX_BlendTurb(qboolean state)
{
	return;
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
static unsigned scaled[1920*1080];
static unsigned  trans[1920*1080];

/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (gltexture_t *destination, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	return;
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

//Diabolickal TGA Begin

int lhcsumtable[256];
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, qboolean keep, int bytesperpixel)
{
	int			i, s, lhcsum;
	gltexture_t	*glt;
	// occurances. well this isn't exactly a checksum, it's better than that but
	// not following any standards.
	lhcsum = 0;
	s = width*height*bytesperpixel;
	
	for (i = 0;i < 256;i++) lhcsumtable[i] = i + 1;
	for (i = 0;i < s;i++) lhcsum += (lhcsumtable[data[i] & 255]++);

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
						//if (!__lwp_heap_free(&texture_heap, glt->data))
						//	Sys_Error("GL_ClearTextureCache: Error freeing data.");
						//goto reload; // best way to do it
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
	
	if (bytesperpixel == 1) {
			GL_Upload8 (glt, data, width, height, mipmap, alpha);
		}
		else if (bytesperpixel == 4) {
#if 1
			// Baker: this applies our -gamma parameter table
			//extern	byte	vid_gamma_table[256];
			for (i = 0; i < s; i++){
				data[4 * i +2] = vid_gamma_table[data[4 * i+2]];
				data[4 * i + 1] = vid_gamma_table[data[4 * i + 1]];
				data[4 * i] = vid_gamma_table[data[4 * i]];
			}
#endif 
			GL_Upload32 (glt, (unsigned*)data, width, height, mipmap, alpha);
		}
		else {
			Sys_Error("GL_LoadTexture: unknown bytesperpixel\n");
		}
		
	//GL_Upload8 (glt, data, width, height, mipmap, alpha);

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
	//Con_Printf("gltexnum: %i", numgltextures);
	strcpy (glt->identifier, identifier);
	//Con_Printf("Identifier: %s", identifier);
	glt->texnum = numgltextures;
	glt->width = width;
	glt->height = height;
	glt->mipmap = false; // ELUTODO
	glt->type = 0;
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
	return;
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
	return;
}
extern int lightmap_textures;
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
	
	//Con_Printf("Displaying: %i\n", pic_id);

	GL_UpdateLightmapTextureRegion32 (destination, (unsigned *)data, width, height, xoffset, yoffset, false, true);
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
	// ELUTODO: loading too much with "" fills the memory with repeated data? Hope not... Check later.
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true, true, 1);
}

// ELUTODO: clean the disable/enable multitexture calls around the engine

void GL_DisableMultitexture(void)
{
	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	return;
}

void GL_EnableMultitexture(void)
{
	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	return;
}

void GL_ClearTextureCache(void)
{
	return;
}
/*
//
//
//
//
//
//
*/
int		image_width;
int		image_height;

int COM_OpenFile (char *filename, int *hndl);

/*small function to read files with stb_image - single-file image loader library.
** downloaded from: https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
** only use jpeg+png formats, because tbh there's not much need for the others.
** */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
//#define STBI_ONLY_JPEG
//#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
//#define STBI_ONLY_PIC
#include "stb_image.h"

byte* loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight)
{
	int bpp;
	int width, height;
	int i;
	
	byte* rgba_data;
	
	//va(filename, basename);
	
	// Figure out the length
    int handle;
    int len = COM_OpenFile (filename, &handle);
    COM_CloseFile(handle);
	
	// Load the raw data into memory, then store it
    rgba_data = COM_LoadFile(filename, 5);

	if (rgba_data == NULL) {
		Con_Printf("NULL: %s", filename);
		return NULL;
	}

    byte *image = stbi_load_from_memory(rgba_data, len, &width, &height, &bpp, 4);
	
	if(image == NULL) {
		Con_Printf("%s\n", stbi_failure_reason());
		return NULL;
	}
	
	//Swap the colors the lazy way
	for (i = 0; i < (width*height)*4; i++) {
		image[i] = image[i+3];
		image[i+1] = image[i+2];
		image[i+2] = image[i+1];
		image[i+3] = image[i];
	}
	
	free(rgba_data);
	
	//set image width/height for texture uploads
	image_width = width;
	image_height = height;

	return image;
}
extern char	skybox_name[32];
extern char skytexname[32];
int loadtextureimage (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap)
{
	int	f;
	int texnum;
	char basename[128], name[132];
	
	int image_size = image_width * image_height;
	
	byte* data = (byte*)malloc(image_size * 4);
	byte *c;
	
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
	
	if (strcmp(skybox_name, ""))
		return 0;
	
	//Try PCX
	sprintf (name, "%s.pcx", basename);
	COM_FOpenFile (name, &f);
	if (f > 0) {
		data = loadimagepixels (name, complain, matchwidth, matchheight);
		texnum = GL_LoadTexture (skytexname, image_width, image_height, data, mipmap, false, false, 4);

		free(data);
		return texnum;
	}
	
	//Try TGA
	sprintf (name, "%s.tga", basename);
	COM_FOpenFile (name, &f);
	if (f){
		data = loadimagepixels (name, complain, matchwidth, matchheight);	
		texnum = GL_LoadTexture (skytexname, image_width, image_height, data, mipmap, false, false, 4);
		
		free(data);
		return texnum;
	}
	//Try PNG
	sprintf (name, "%s.png", basename);
	COM_FOpenFile (name, &f);
	if (f){
		data = loadimagepixels (name, complain, matchwidth, matchheight);
		texnum = GL_LoadTexture (skytexname, image_width, image_height, data, mipmap, false, false, 4);
		
		free(data);
		return texnum;
	}
	//Try JPEG
	sprintf (name, "%s.jpeg", basename);
	COM_FOpenFile (name, &f);
	if (f){
		data = loadimagepixels (name, complain, matchwidth, matchheight);
		texnum = GL_LoadTexture (skytexname, image_width, image_height, data, mipmap, false, false, 4);
		
		free(data);
		return texnum;
	}
	sprintf (name, "%s.jpg", basename);
	COM_FOpenFile (name, &f);
	if (f){
		data = loadimagepixels (name, complain, matchwidth, matchheight);
		texnum = GL_LoadTexture (skytexname, image_width, image_height, data, mipmap, false, false, 4);
		
		free(data);
		return texnum;
	}
	
	if (data == NULL) { 
		Con_Printf("Cannot load image %s\n", filename);
		return 0;
	}
	
	return 0;
}
// Tomaz || TGA End

