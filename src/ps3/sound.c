/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2007 Peter Mackay

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

// ELUTODO: go to 48khz once we have hi-res sound packs

#include "../generic/quakedef.h"

// Represents a single sound sample.
typedef u32 sample;

// We copy Quake's audio into double buffered DMA buffers while it is
// being transferred to the GameCube's audio system.
static const size_t		samples_per_dma_buffer	= 2048;
static sample			dma_buffers[2][2048] __attribute__((aligned(32)));
static size_t			current_dma_buffer		= 0;

// Quake writes its audio into this mix buffer.
static const size_t		samples_per_mix_buffer	= 65536;
static sample			mix_buffer[65536];
static volatile size_t	mix_buffer_pointer		= 0;

// Called whenever more audio data is required.
static void play_more_audio()
{
	// TODO
}

qboolean SNDDMA_Init(void)
{
	// TODO
}

void SNDDMA_Shutdown(void)
{
	// TODO
}

int SNDDMA_GetDMAPos(void)
{
	// TODO
}

void SNDDMA_Submit(void)
{
}
