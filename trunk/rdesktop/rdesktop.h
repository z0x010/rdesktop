/*
   rdesktop: A Remote Desktop Protocol client.
   Master include file
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define VERSION "1.2.0"

#ifdef WITH_DEBUG
#define DEBUG(args)	printf args;
#else
#define DEBUG(args)
#endif

#ifdef WITH_DEBUG_KBD
#define DEBUG_KBD(args) printf args;
#else
#define DEBUG_KBD(args)
#endif

#ifdef WITH_DEBUG_RDP5
#define DEBUG_RDP5(args) printf args;
#else
#define DEBUG_RDP5(args)
#endif

#ifdef WITH_DEBUG_CLIPBOARD
#define DEBUG_CLIPBOARD(args) printf args;
#else
#define DEBUG_CLIPBOARD(args)
#endif

#define STRNCPY(dst,src,n) { strncpy(dst,src,n-1); dst[n-1] = 0; }

#include "constants.h"
#include "types.h"
#include "parse.h"

/* Declared here since it uses STREAM that's declared in parse.h */
typedef struct _rdp5_channel
{
	uint16 channelno;
	char name[8];
	uint32 channelflags;
	void (*channelcallback) (STREAM, uint16);
}
rdp5_channel;

#ifndef MAKE_PROTO
#include "proto.h"
#endif
