/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Protocol services - Multipoint Communications Service
   Copyright (C) Matthew Chapman 1999-2005
   Copyright (C) Erik Forsberg 2003

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

#include "rdesktop.h"

extern uint8 *g_next_packet;

extern RDPCOMP g_mppc_dict;

void
rdp5_process(STREAM s)
{
	uint16 length, count, x, y;
	uint8 type, ctype;
	uint8 *next;

	uint32 roff, rlen;
	struct stream *ns = &(g_mppc_dict.ns);
	struct stream *ts;

#if 0
	printf("RDP5 data:\n");
	hexdump(s->p, s->end - s->p);
#endif

	ui_begin_update();
	while (s->p < s->end)
	{
		in_uint8(s, type);
		if (type & RDP_COMPRESSION)
		{
			in_uint8(s, ctype);
			in_uint16_le(s, length);
			type ^= RDP_COMPRESSION;
		}
		else
		{
			ctype = 0;
			in_uint16_le(s, length);
		}
		g_next_packet = next = s->p + length;

		if (ctype & RDP_MPPC_COMPRESSED)
		{
			if (length > RDP_MPPC_DICT_SIZE)
				error("error decompressed packet size exceeds max\n");
			if (mppc_expand(s->p, length, ctype, &roff, &rlen) == -1)
				error("error while decompressing packet\n");

			/* allocate memory and copy the uncompressed data into the temporary stream */
			ns->data = (uint8 *) xrealloc(ns->data, rlen);

			memcpy((ns->data), (unsigned char *) (g_mppc_dict.hist + roff), rlen);

			ns->size = rlen;
			ns->end = (ns->data + ns->size);
			ns->p = ns->data;
			ns->rdp_hdr = ns->p;

			ts = ns;
		}
		else
			ts = s;

		switch (type)
		{
				/* Thanks to Jeroen Meijer <jdmeijer at yahoo
				   dot com> for finding out the meaning of
				   most of the opcodes here. Especially opcode
				   8! :) */
			case 0:	/* orders */
				in_uint16_le(ts, count);
				process_orders(ts, count);
				break;
			case 1:	/* bitmap update (???) */
				in_uint8s(ts, 2);	/* part length */
				process_bitmap_updates(ts);
				break;
			case 2:	/* palette */
				in_uint8s(ts, 2);	/* uint16 = 2 */
				process_palette(ts);
				break;
			case 3:	/* probably an palette with offset 3. Weird */
				break;
			case 5:
				ui_set_null_cursor();
				break;
			case 8:
				in_uint16_le(ts, x);
				in_uint16_le(ts, y);
				if (s_check(ts))
					ui_move_pointer(x, y);
				break;
			case 9:
				process_colour_pointer_pdu(ts);
				break;
			case 10:
				process_cached_pointer_pdu(ts);
				break;
			default:
				unimpl("RDP5 opcode %d\n", type);
		}

		s->p = next;
	}
	ui_end_update();
}
