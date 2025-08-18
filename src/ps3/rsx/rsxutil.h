#ifndef __RSXUTIL_H__
#define __RSXUTIL_H__

#include <ppu-types.h>

#include <rsx/rsx.h>

#define CB_SIZE		0x100000
#define HOST_SIZE	(32*1024*1024)

extern gcmContextData *rsx_context;
extern u32 rsx_display_width;
extern u32 rsx_display_height;
extern u32 rsx_curr_fb;

extern u32 rsx_color_pitch;
extern u32 rsx_color_offset[2];

extern u32 rsx_depth_pitch;
extern u32 rsx_depth_offset;

void rsx_util_set_render_target(u32 index);
void rsx_util_init_screen(void *host_addr,u32 size);
void rsx_util_waitflip();
void rsx_util_flip();

#endif
