/*
Copyright (C) 2008 Eluan Miranda

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
#include <malloc.h>

// ELUTODO: blank all the framebuffers to prevent artifacts before rendering takes place. Happens between the frontend ending and the quake console showing up

#include "../../generic/quakedef.h"

static void	*host_addr;

static int scr_width, scr_height;

static bool vidmode_active = false;

/*-----------------------------------------------------------------------*/

unsigned		d_8to24table[256];

float		gldepthmin, gldepthmax;

static float vid_gamma = 1.0;

/*-----------------------------------------------------------------------*/

VmathMatrix4 perspective; 
VmathMatrix4 view, model, modelview;

cvar_t vid_tvborder = {"vid_tvborder", "0", (qboolean)true};
cvar_t vid_conmode = {"vid_conmode", "0", (qboolean)true};

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}

void VID_Shutdown(void)
{
	if (vidmode_active)
	{
		// Free the FIFO.
		//free(MEM_K1_TO_K0(gp_fifo));
		//gp_fifo = 0;

		vidmode_active = false;
	}
}

void VID_ShiftPalette(unsigned char *p)
{
//	VID_SetPalette(p);
}

void	VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	unsigned short i;
	unsigned	*table;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
	}
	//d_8to24table[255] &= 0xffffff;	// 255 is transparent
	d_8to24table[255] = 0;				// ELUTODO: will look prettier until we solve the filtering issue
}

void VID_ConModeUpdate(void)
{
	// update console resolution
	switch((int)vid_conmode.value)
	{
		default:
		case 0:
			vid.conwidth = 320;
			vid.conheight = 240;
			break;
		case 1:
			vid.conwidth = 400;
			vid.conheight = 300;
			break;
		case 2:
			vid.conwidth = 480;
			vid.conheight = 360;
			break;
		case 3:
			vid.conwidth = 560;
			vid.conheight = 420;
			break;
		case 4:
			vid.conwidth = 640;
			vid.conheight = 480;
			break;
	}
	if (vid.conheight > scr_height)
		vid.conheight = scr_height;
	if (vid.conwidth > scr_width)
		vid.conwidth = scr_width;

	conback->width = vid.conwidth;
	conback->height = vid.conheight;
}

/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
}

void GL_EndRendering (void)
{
}

// This is not the "v_gamma/gamma" cvar
static void Check_Gamma (unsigned char *pal)
{
	float	f, inf;
	unsigned char	palette[768];
	int		i;

	if ((i = COM_CheckParm("-gamma")) == 0) {
		vid_gamma = 0.7; // default to 0.7 on non-3dfx hardware
	} else
		vid_gamma = atof(com_argv[i+1]);

	for (i=0 ; i<768 ; i++)
	{
		f = pow ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
	
	Build_Gamma_Table ();
}

// ELUTODO: proper widescreen support
// ELUTODO: one more mess to remove: glwidth is the VIEWAREA and scr_width is the WHOLE SCREEN (will have to clean this up if I want to implemented split-screen multiplayer
// ELUTODO: crosshair, osk are NOT right on higher 2d resolutions and split-screen setups (is the use of scr_vrect for cl_crossx/y right?)
// scr_width/height = 3D res, vid.conwidth/conheight = 2D res, vid.width/height = "natural" resolution (320x{200,240})
// many things rely on a minimum resolution of 320x{200,240}
void VID_Init(unsigned char *palette)
{
	// Initialise the Reality Synthetizer.
	host_addr = memalign(1024*1024,HOST_SIZE);
	rsx_util_init_screen(host_addr, HOST_SIZE);
	vid.maxwarpwidth = rsx_display_width;
	vid.maxwarpheight = rsx_display_height;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

// interpret command-line params

// only multiples of eight, please
// set vid parameters
	scr_width = rsx_display_width;
	scr_height = rsx_display_height;

	vid.width = 320;
	vid.height = 240;

	if (vid.height > scr_height)
		vid.height = scr_height;
	if (vid.width > scr_width)
		vid.width = scr_width;

	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.numpages = 2;

	GL_Init();

	Check_Gamma(palette);
	VID_SetPalette(palette);

	Con_SafePrintf ("Video mode %dx%d initialized.\n", scr_width, scr_height);

	vid.recalc_refdef = 1;				// force a surface cache flush

	Cvar_RegisterVariable(&vid_tvborder);
	Cvar_RegisterVariable(&vid_conmode);

	vidmode_active = true;
}
