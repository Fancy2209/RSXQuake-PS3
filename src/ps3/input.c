/*
Quake GameCube port.
Copyright (C) 2007 Peter Mackay
Copyright (C) 2008 Eluan Miranda
Copyright (C) 2015 Fabio Olimpieri

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

// ELUTODO: do something about lookspring and lookstrafe
// ELUTODO: keys to: nunchuk turn and nunchuk look up/down?
// ELUTODO: osk doesn't work if client disconnected

#include "../generic/quakedef.h"

cvar_t	osk_repeat_delay = {"osk_repeat_delay","0.25"};
cvar_t	kb_repeat_delay = {"kb_repeat_delay","0.1"};
cvar_t	nunchuk_stick_as_arrows = {"nunchuk_stick_as_arrows","0"};
cvar_t  rumble = {"rumble","1"};

char keycode_normal[256] = { 
	'\0', '\0', '\0', '\0', //0-3
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', //4-29
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', //30-39
	K_ENTER, K_ESCAPE, K_BACKSPACE, K_TAB, K_SPACE, //40-44
	'-', '=', '[', ']', '\0', '\\', ';', '\'', '`', ',', '.', '/', '\0', //45-57
	K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12, '\0', '\0', K_PAUSE, //58-72
	'\0', '\0', '\0', '\0', '\0', '\0',//73-78
	K_RIGHTARROW, K_LEFTARROW, K_DOWNARROW, K_UPARROW, //79-82
	K_NUMLOCK, '/', '*', '-', '+', K_ENTER, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.', '\\', //83-100
	K_MENU
};

char keycode_shifted[256] = { 
	'\0', '\0', '\0', '\0', 
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 
	'!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
	K_ENTER, K_ESCAPE, K_BACKSPACE, K_TAB, K_SPACE,
	'_', '+', '{', '}', '\0', '|', ':', '"', '~', '<', '>', '?', '\0',
	K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12, '\0', '\0', K_PAUSE,
	'\0', '\0', '\0', '\0', '\0', '\0', //73-78
	K_RIGHTARROW, K_LEFTARROW, K_DOWNARROW, K_UPARROW, //79-82
	K_NUMLOCK, '/', '*', '-', '+', K_ENTER, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.', '\\', //83-100
	K_MENU
};

bool keyboard_shifted = FALSE;
u8 kb_last_selected = 0x0;

// pass these values to whatever subsystem wants it
float in_pitchangle;
float in_yawangle;
float in_rollangle;

// Are we inside the on-screen keyboard? (ELUTODO: refactor)
int in_osk = 0;

// \0 means not mapped...
// 5 * 15
char osk_normal[75] =
{
	'\'', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', ']', K_BACKSPACE,
	0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', 0, '[', K_ENTER, K_ENTER,
	0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0, '~', '/', K_ENTER, K_ENTER,
	0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', ';', K_ENTER, K_ENTER, K_ENTER,
	0 , 0, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, 0, 0
};

char osk_shifted[75] =
{
	'\"', '!', '@', '#', '$', '%', 0, '&', '*', '(', ')', '_', '+', '}', K_BACKSPACE,
	0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0, '{', K_ENTER, K_ENTER,
	0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0, '^', '?', K_ENTER, K_ENTER,
	0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', ':', K_ENTER, K_ENTER, K_ENTER,
	0 , 0, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, 0, 0
};

char *osk_set;
int osk_selected;
int osk_last_selected;
int osk_coords[2];

float osk_last_press_time = 0.0f;

#include <io/pad.h>
#include <io/kb.h>
#include "input_wiimote.h"

#define FORCE_KEY_BINDINGS 0

u32 wiimote_ir_res_x;
u32 wiimote_ir_res_y;

// wiimote info
u32 wpad_previous_keys = 0x0000;
u32 wpad_keys = 0x0000;

bool wiimote_connected = TRUE;
bool nunchuk_connected = FALSE;
bool classic_connected = FALSE;
bool keyboard_connected = FALSE;

typedef enum  {LEFT, CENTER_X, RIGHT} stick_x_st_t;
typedef enum   {UP, CENTER_Y, DOWN} stick_y_st_t;

stick_x_st_t stick_x_st = CENTER_X;
stick_y_st_t stick_y_st = CENTER_Y;

u16 pad_previous_keys = 0x0000;
u16 pad_keys = 0x0000;

int last_irx = -1, last_iry = -1;

static float clamp(float value, float minimum, float maximum)
{
	if (value > maximum)
	{
		return maximum;
	}
	else if (value < minimum)
	{
		return minimum;
	}
	else
	{
		return value;
	}
}

static void apply_dead_zone(float* x, float* y, float dead_zone)
{
	// Either stick out of the dead zone?
	if ((fabsf(*x) >= dead_zone) || (fabsf(*y) >= dead_zone))
	{
		// Nothing to do.
	}
	else
	{
		// Clamp to the dead zone.
		*x = 0.0f;
		*y = 0.0f;
	}
}

static s8 WPAD_StickX(u8 which)
{
	float mag = 0.0;
	float ang = 0.0;

	// TODO
}

static s8 WPAD_StickY(u8 which)
{
	float mag = 0.0;
	float ang = 0.0;

	// TODO
}

void IN_Init (void)
{
	// TODO
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
	// TODO
}

// Some things here rely upon IN_Move always being called after IN_Commands on the same frame
void IN_Move (usercmd_t *cmd)
{
	// TODO
}
