/*
   rdesktop: A Remote Desktop Protocol client.
   RDP order processing
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

#define RDP_ORDER_STANDARD   0x01
#define RDP_ORDER_SECONDARY  0x02
#define RDP_ORDER_BOUNDS     0x04
#define RDP_ORDER_CHANGE     0x08
#define RDP_ORDER_DELTA      0x10
#define RDP_ORDER_LASTBOUNDS 0x20
#define RDP_ORDER_SMALL      0x40
#define RDP_ORDER_TINY       0x80

enum RDP_ORDER_TYPE
{
	RDP_ORDER_DESTBLT = 0,
	RDP_ORDER_PATBLT = 1,
	RDP_ORDER_SCREENBLT = 2,
	RDP_ORDER_LINE = 9,
	RDP_ORDER_RECT = 10,
	RDP_ORDER_DESKSAVE = 11,
	RDP_ORDER_MEMBLT = 13,
	RDP_ORDER_TRIBLT = 14,
	RDP_ORDER_POLYLINE = 22,
	RDP_ORDER_TEXT2 = 27
};

enum RDP_SECONDARY_ORDER_TYPE
{
	RDP_ORDER_RAW_BMPCACHE = 0,
	RDP_ORDER_COLCACHE = 1,
	RDP_ORDER_BMPCACHE = 2,
	RDP_ORDER_FONTCACHE = 3
};

typedef struct _DESTBLT_ORDER
{
	uint16 x;
	uint16 y;
	uint16 cx;
	uint16 cy;
	uint8 opcode;

}
DESTBLT_ORDER;

typedef struct _PATBLT_ORDER
{
	uint16 x;
	uint16 y;
	uint16 cx;
	uint16 cy;
	uint8 opcode;
	uint32 bgcolour;
	uint32 fgcolour;
	BRUSH brush;

}
PATBLT_ORDER;

typedef struct _SCREENBLT_ORDER
{
	uint16 x;
	uint16 y;
	uint16 cx;
	uint16 cy;
	uint8 opcode;
	uint16 srcx;
	uint16 srcy;

}
SCREENBLT_ORDER;

typedef struct _LINE_ORDER
{
	uint16 mixmode;
	uint16 startx;
	uint16 starty;
	uint16 endx;
	uint16 endy;
	uint32 bgcolour;
	uint8 opcode;
	PEN pen;

}
LINE_ORDER;

typedef struct _RECT_ORDER
{
	uint16 x;
	uint16 y;
	uint16 cx;
	uint16 cy;
	uint32 colour;

}
RECT_ORDER;

typedef struct _DESKSAVE_ORDER
{
	uint32 offset;
	uint16 left;
	uint16 top;
	uint16 right;
	uint16 bottom;
	uint8 action;

}
DESKSAVE_ORDER;

typedef struct _TRIBLT_ORDER
{
	uint8 colour_table;
	uint8 cache_id;
	uint16 x;
	uint16 y;
	uint16 cx;
	uint16 cy;
	uint8 opcode;
	uint16 srcx;
	uint16 srcy;
	uint32 bgcolour;
	uint32 fgcolour;
	BRUSH brush;
	uint16 cache_idx;
	uint16 unknown;

}
TRIBLT_ORDER;

typedef struct _MEMBLT_ORDER
{
	uint8 colour_table;
	uint8 cache_id;
	uint16 x;
	uint16 y;
	uint16 cx;
	uint16 cy;
	uint8 opcode;
	uint16 srcx;
	uint16 srcy;
	uint16 cache_idx;

}
MEMBLT_ORDER;

#define MAX_DATA 256

typedef struct _POLYLINE_ORDER
{
	uint16 x;
	uint16 y;
	uint8 opcode;
	uint32 fgcolour;
	uint8 lines;
	uint8 datasize;
	uint8 data[MAX_DATA];

}
POLYLINE_ORDER;

#define MAX_TEXT 256

typedef struct _TEXT2_ORDER
{
	uint8 font;
	uint8 flags;
	uint8 mixmode;
	uint8 unknown;
	uint32 fgcolour;
	uint32 bgcolour;
	uint16 clipleft;
	uint16 cliptop;
	uint16 clipright;
	uint16 clipbottom;
	uint16 boxleft;
	uint16 boxtop;
	uint16 boxright;
	uint16 boxbottom;
	uint16 x;
	uint16 y;
	uint8 length;
	uint8 text[MAX_TEXT];

}
TEXT2_ORDER;

typedef struct _RDP_ORDER_STATE
{
	uint8 order_type;
	BOUNDS bounds;

	DESTBLT_ORDER destblt;
	PATBLT_ORDER patblt;
	SCREENBLT_ORDER screenblt;
	LINE_ORDER line;
	RECT_ORDER rect;
	DESKSAVE_ORDER desksave;
	MEMBLT_ORDER memblt;
	TRIBLT_ORDER triblt;
	POLYLINE_ORDER polyline;
	TEXT2_ORDER text2;

}
RDP_ORDER_STATE;

typedef struct _RDP_RAW_BMPCACHE_ORDER
{
	uint8 cache_id;
	uint8 pad1;
	uint8 width;
	uint8 height;
	uint8 bpp;
	uint16 bufsize;
	uint16 cache_idx;
	uint8 *data;

}
RDP_RAW_BMPCACHE_ORDER;

typedef struct _RDP_BMPCACHE_ORDER
{
	uint8 cache_id;
	uint8 pad1;
	uint8 width;
	uint8 height;
	uint8 bpp;
	uint16 bufsize;
	uint16 cache_idx;
	uint16 pad2;
	uint16 size;
	uint16 row_size;
	uint16 final_size;
	uint8 *data;

}
RDP_BMPCACHE_ORDER;

#define MAX_GLYPH 32

typedef struct _RDP_FONT_GLYPH
{
	uint16 character;
	uint16 unknown;
	uint16 baseline;
	uint16 width;
	uint16 height;
	uint8 data[MAX_GLYPH];

}
RDP_FONT_GLYPH;

#define MAX_GLYPHS 256

typedef struct _RDP_FONTCACHE_ORDER
{
	uint8 font;
	uint8 nglyphs;
	RDP_FONT_GLYPH glyphs[MAX_GLYPHS];

}
RDP_FONTCACHE_ORDER;

typedef struct _RDP_COLCACHE_ORDER
{
	uint8 cache_id;
	COLOURMAP map;

}
RDP_COLCACHE_ORDER;
