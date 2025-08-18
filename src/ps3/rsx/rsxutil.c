#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>

#include <sysutil/video.h>

#include "rsxutil.h"

#define GCM_LABEL_INDEX		255

videoResolution res;
gcmContextData *rsx_context = NULL;

u32 rsx_curr_fb = 0;
u32 first_fb = 1;

u32 rsx_display_width;
u32 rsx_display_height;

u32 rsx_util_depth_pitch;
u32 rsx_util_depth_offset;
u32 *depth_buffer;

u32 rsx_color_pitch;
u32 rsx_color_offset[2];
u32 *color_buffer[2];

static u32 sLabelVal = 1;

static void waitFinish()
{
	rsxSetWriteBackendLabel(context,GCM_LABEL_INDEX,sLabelVal);

	rsxFlushBuffer(context);

	while(*(vu32*)gcmGetLabelAddress(GCM_LABEL_INDEX)!=sLabelVal)
		usleep(30);

	++sLabelVal;
}

static void waitRSXIdle()
{
	rsxSetWriteBackendLabel(context,GCM_LABEL_INDEX,sLabelVal);
	rsxSetWaitLabel(context,GCM_LABEL_INDEX,sLabelVal);

	++sLabelVal;

	waitFinish();
}

void rsx_util_set_render_target(u32 index)
{
	gcmSurface sf;

	sf.colorFormat		= GCM_SURFACE_X8R8G8B8;
	sf.colorTarget		= GCM_SURFACE_TARGET_0;
	sf.colorLocation[0]	= GCM_LOCATION_RSX;
	sf.colorOffset[0]	= rsx_color_offset[index];
	sf.colorPitch[0]	= rsx_color_pitch;

	sf.colorLocation[1]	= GCM_LOCATION_RSX;
	sf.colorLocation[2]	= GCM_LOCATION_RSX;
	sf.colorLocation[3]	= GCM_LOCATION_RSX;
	sf.colorOffset[1]	= 0;
	sf.colorOffset[2]	= 0;
	sf.colorOffset[3]	= 0;
	sf.colorPitch[1]	= 64;
	sf.colorPitch[2]	= 64;
	sf.colorPitch[3]	= 64;

	sf.depthFormat		= GCM_SURFACE_ZETA_Z16;
	sf.depthLocation	= GCM_LOCATION_RSX;
	sf.depthOffset		= rsx_depth_offset;
	sf.depthPitch		= rsx_depth_pitch;

	sf.type				= GCM_SURFACE_TYPE_LINEAR;
	sf.antiAlias		= GCM_SURFACE_CENTER_1;

	sf.width			= rsx_display_width;
	sf.height			= rsx_display_height;
	sf.x				= 0;
	sf.y				= 0;

	rsxSetSurface(context,&sf);
}

void rsx_util_init_screen(void *host_addr,u32 size)
{
	rsxInit(&context,CB_SIZE,size,host_addr);

	videoState state;
	videoGetState(0,0,&state);

	videoGetResolution(state.displayMode.resolution,&res);

	videoConfiguration vconfig;
	memset(&vconfig,0,sizeof(videoConfiguration));

	vconfig.resolution = state.displayMode.resolution;
	vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
	vconfig.pitch = res.width*sizeof(u32);

	waitRSXIdle();

	videoConfigure(0,&vconfig,NULL,0);
	videoGetState(0,0,&state);

	gcmSetFlipMode(GCM_FLIP_VSYNC);

	rsx_display_width = res.width;
	rsx_display_height = res.height;

	rsx_color_pitch = rsx_display_width*sizeof(u32);
	color_buffer[0] = (u32*)rsxMemalign(64,(rsx_display_height*rsx_color_pitch));
	color_buffer[1] = (u32*)rsxMemalign(64,(rsx_display_height*rsx_color_pitch));

	rsxAddressToOffset(color_buffer[0],&rsx_color_offset[0]);
	rsxAddressToOffset(color_buffer[1],&rsx_color_offset[1]);

	gcmSetDisplayBuffer(0,rsx_color_offset[0],rsx_color_pitch,rsx_display_width,rsx_display_height);
	gcmSetDisplayBuffer(1,rsx_color_offset[1],rsx_color_pitch,rsx_display_width,rsx_display_height);

	rsx_depth_pitch = rsx_display_width*sizeof(u32);
	depth_buffer = (u32*)rsxMemalign(64,(rsx_display_height*rsx_depth_pitch)*2);
	rsxAddressToOffset(depth_buffer,&rsx_depth_offset);
}

void rsx_util_waitflip()
{
	while(gcmGetFlipStatus()!=0)
		usleep(200);
	gcmResetFlipStatus();
}

void rsx_util_flip()
{
	if(!first_fb) waitflip();
	else gcmResetFlipStatus();

	gcmSetFlip(context,rsx_curr_fb);
	rsxFlushBuffer(context);

	gcmSetWaitFlip(context);

	rsx_curr_fb ^= 1;
	rsx_util_set_render_target(rsx_curr_fb);

	first_fb = 0;
}
