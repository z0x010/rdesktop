/*
   rdesktop: A Remote Desktop Protocol client.
   Common data types
   Copyright (C) Matthew Chapman 1999-2002
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

typedef int BOOL;

#ifndef True
#define True  (1)
#define False (0)
#endif

typedef unsigned char uint8;
typedef signed char sint8;
typedef unsigned short uint16;
typedef signed short sint16;
typedef unsigned int uint32;
typedef signed int sint32;

typedef void *HBITMAP;
typedef void *HGLYPH;
typedef void *HCOLOURMAP;
typedef void *HCURSOR;

typedef struct _COLOURENTRY
{
	uint8 red;
	uint8 green;
	uint8 blue;

}
COLOURENTRY;

typedef struct _COLOURMAP
{
	uint16 ncolours;
	COLOURENTRY *colours;

}
COLOURMAP;

typedef struct _BOUNDS
{
	sint16 left;
	sint16 top;
	sint16 right;
	sint16 bottom;

}
BOUNDS;

typedef struct _PEN
{
	uint8 style;
	uint8 width;
	uint32 colour;

}
PEN;

typedef struct _BRUSH
{
	uint8 xorigin;
	uint8 yorigin;
	uint8 style;
	uint8 pattern[8];

}
BRUSH;

typedef struct _FONTGLYPH
{
	sint16 offset;
	sint16 baseline;
	uint16 width;
	uint16 height;
	HBITMAP pixmap;

}
FONTGLYPH;

typedef struct _DATABLOB
{
	void *data;
	int size;

}
DATABLOB;

typedef struct _key_translation
{
	uint8 scancode;
	uint16 modifiers;
}
key_translation;

typedef struct _VCHANNEL
{
	uint16 mcs_id;
	char name[8];
	uint32 flags;
	struct stream in;
	void (*process) (STREAM);
}
VCHANNEL;

/* RDPSND */
typedef struct {
    uint16 wFormatTag;
    uint16 nChannels;
    uint32 nSamplesPerSec;
    uint32 nAvgBytesPerSec;
    uint16 nBlockAlign;
    uint16 wBitsPerSample;
    uint16 cbSize;
} WAVEFORMATEX;

/* RDPDR */
typedef uint32 NTSTATUS;
typedef uint32 HANDLE;

typedef struct _DEVICE_FNS
{
	NTSTATUS(*create) (HANDLE * handle);
	NTSTATUS(*close) (HANDLE handle);
	NTSTATUS(*read) (HANDLE handle, uint8 * data, uint32 length, uint32 * result);
	NTSTATUS(*write) (HANDLE handle, uint8 * data, uint32 length, uint32 * result);
	NTSTATUS(*device_control) (HANDLE handle, uint32 request, STREAM in, STREAM out);
}
DEVICE_FNS;
