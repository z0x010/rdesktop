/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Protocol services - Clipboard functions
   Copyright (C) Erik Forsberg <forsberg@cendio.se> 2003
   Copyright (C) Matthew Chapman 2003

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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "rdesktop.h"

/*
  To gain better understanding of this code, one could be assisted by the following documents:
  - Inter-Client Communication Conventions Manual (ICCCM)
    HTML: http://tronche.com/gui/x/icccm/
    PDF:  http://ftp.xfree86.org/pub/XFree86/4.5.0/doc/PDF/icccm.pdf
  - MSDN: Clipboard Formats
    http://msdn.microsoft.com/library/en-us/winui/winui/windowsuserinterface/dataexchange/clipboard/clipboardformats.asp
*/

#define NUM_TARGETS 6

extern Display *g_display;
extern Window g_wnd;
extern Time g_last_gesturetime;

/* Atoms of the two X selections we're dealing with: CLIPBOARD (explicit-copy) and PRIMARY (selection-copy) */
static Atom clipboard_atom, primary_atom;
/* Atom of the TARGETS clipboard target */
static Atom targets_atom;
/* Atom of the TIMESTAMP clipboard target */
static Atom timestamp_atom;
/* Atom _RDESKTOP_CLIPBOARD_TARGET which has multiple uses:
   - The 'property' argument in XConvertSelection calls: This is the property of our
     window into which XConvertSelection will store the received clipboard data.
   - In a clipboard request of target _RDESKTOP_CLIPBOARD_FORMATS, an XA_INTEGER-typed
     property carrying the Windows native (CF_...) format desired by the requestor.
     Requestor set this property (e.g. requestor_wnd[_RDESKTOP_CLIPBOARD_TARGET] = CF_TEXT)
     before requesting clipboard data from a fellow rdesktop using
     the _RDESKTOP_CLIPBOARD_FORMATS target. */
static Atom rdesktop_clipboard_target_atom;
/* Atom _RDESKTOP_CLIPBOARD_FORMATS which has multiple uses:
   - The clipboard target (X jargon for "clipboard format") for rdesktop-to-rdesktop interchange
     of Windows native clipboard data.
     This target cannot be used standalone; the requestor must keep the
     _RDESKTOP_CLIPBOARD_TARGET property on his window denoting
     the Windows native clipboard format being requested.
   - The root window property set by rdesktop when it owns the clipboard,
     denoting all Windows native clipboard formats it offers via
     requests of the _RDESKTOP_CLIPBOARD_FORMATS target. */
static Atom rdesktop_clipboard_formats_atom;
/* Atom of the INCR clipboard type (see ICCCM on "INCR Properties") */
static Atom incr_atom;
/* Stores the last "selection request" (= another X client requesting clipboard data from us).
   To satisfy such a request, we request the clipboard data from the RDP server.
   When we receive the response from the RDP server (asynchronously), this variable gives us
   the context to proceed. */
static XSelectionRequestEvent selection_request;
/* Array of offered clipboard targets that will be sent to fellow X clients upon a TARGETS request. */
static Atom targets[NUM_TARGETS];
/* Denotes that this client currently holds the PRIMARY selection. */
static int have_primary = 0;
/* Denotes that an rdesktop (not this rdesktop) is owning the selection,
   allowing us to interchange Windows native clipboard data directly. */
static int rdesktop_is_selection_owner = 0;

/* Denotes that an INCR ("chunked") transfer is in progress. */
static int g_waiting_for_INCR = 0;
/* Buffers an INCR transfer. */
static uint8 *g_clip_buffer = 0;
/* Denotes the size of g_clip_buffer. */
static uint32 g_clip_buflen = 0;

/* Translate LF to CR-LF. To do this, we must allocate more memory.
   The returned string is null-terminated, as required by CF_TEXT.
   Does not stop on embedded nulls.
   The length is updated. */
static void
crlf2lf(uint8 * data, uint32 * length)
{
	uint8 *dst, *src;
	src = dst = data;
	while (src < data + *length)
	{
		if (*src != '\x0d')
			*dst++ = *src;
		src++;
	}
	*length = dst - data;
}

/* Translate LF to CR-LF. To do this, we must allocate more memory.  
   The length is updated. */
static uint8 *
lf2crlf(uint8 * data, uint32 * length)
{
	uint8 *result, *p, *o;

	/* Worst case: Every char is LF */
	result = xmalloc(*length * 2);

	p = data;
	o = result;

	while (p < data + *length)
	{
		if (*p == '\x0a')
			*o++ = '\x0d';
		*o++ = *p++;
	}
	*length = o - result;

	/* Convenience */
	*o++ = '\0';

	return result;
}


static void
xclip_provide_selection(XSelectionRequestEvent * req, Atom type, unsigned int format, uint8 * data,
			uint32 length)
{
	XEvent xev;

	XChangeProperty(g_display, req->requestor, req->property,
			type, format, PropModeReplace, data, length);

	xev.xselection.type = SelectionNotify;
	xev.xselection.serial = 0;
	xev.xselection.send_event = True;
	xev.xselection.requestor = req->requestor;
	xev.xselection.selection = req->selection;
	xev.xselection.target = req->target;
	xev.xselection.property = req->property;
	xev.xselection.time = req->time;
	XSendEvent(g_display, req->requestor, False, NoEventMask, &xev);
}

/* This function is called for SelectionNotify events.
   The SelectionNotify message is sent from the clipboard owner to the requestor
   after his request was satisfied.
   If this function is called, we're the requestor side. */
#ifndef MAKE_PROTO
void
xclip_handle_SelectionNotify(XSelectionEvent * event)
{
	unsigned long nitems, bytes_left;
	XWindowAttributes wa;
	Atom type, best_target, text_target;
	Atom *supported_targets;
	int res, i, format;
	uint8 *data;

	if (event->property == None)
		goto fail;

	DEBUG_CLIPBOARD(("xclip_handle_SelectionNotify: selection=%s, target=%s, property=%s\n",
			 XGetAtomName(g_display, event->selection),
			 XGetAtomName(g_display, event->target),
			 XGetAtomName(g_display, event->property)));

	if (event->property == None)
		goto fail;

	res = XGetWindowProperty(g_display, g_wnd, rdesktop_clipboard_target_atom,
				 0, XMaxRequestSize(g_display), False, AnyPropertyType,
				 &type, &format, &nitems, &bytes_left, &data);

	if (res != Success)
	{
		DEBUG_CLIPBOARD(("XGetWindowProperty failed!\n"));
		goto fail;
	}


	if (type == incr_atom)
	{
		DEBUG_CLIPBOARD(("Received INCR.\n"));

		XGetWindowAttributes(g_display, g_wnd, &wa);
		if ((wa.your_event_mask | PropertyChangeMask) != wa.your_event_mask)
		{
			XSelectInput(g_display, g_wnd, (wa.your_event_mask | PropertyChangeMask));
		}
		XDeleteProperty(g_display, g_wnd, rdesktop_clipboard_target_atom);
		XFree(data);
		g_waiting_for_INCR = 1;
		return;
	}

	XDeleteProperty(g_display, g_wnd, rdesktop_clipboard_target_atom);

	/* Negotiate target format */
	if (event->target == targets_atom)
	{
		/* FIXME: We should choose format here based on what the server wanted */
		best_target = XA_STRING;
		if (type != None)
		{
			supported_targets = (Atom *) data;
			text_target = XInternAtom(g_display, "TEXT", False);
			for (i = 0; i < nitems; i++)
			{
				DEBUG_CLIPBOARD(("Target %d: %s\n", i,
						 XGetAtomName(g_display, supported_targets[i])));
				if (supported_targets[i] == text_target)
				{
					DEBUG_CLIPBOARD(("Other party supports TEXT, choosing that as best_target\n"));
					best_target = text_target;
					break;
				}
			}
			XFree(data);
		}

		XConvertSelection(g_display, primary_atom, best_target,
				  rdesktop_clipboard_target_atom, g_wnd, event->time);
		return;
	}

	/* Translate linebreaks, but only if not getting data from
	   other rdesktop instance */
	if (event->target != rdesktop_clipboard_formats_atom)
	{
		uint8 *translated_data;
		uint32 length = nitems;

		DEBUG_CLIPBOARD(("Translating linebreaks before sending data\n"));
		translated_data = lf2crlf(data, &length);
		cliprdr_send_data(translated_data, length + 1);
		xfree(translated_data);	/* Not the same thing as XFree! */
	}
	else
	{
		cliprdr_send_data(data, nitems + 1);
	}
	XFree(data);

	if (!rdesktop_is_selection_owner)
		cliprdr_send_simple_native_format_announce(CF_TEXT);
	return;

      fail:
	XDeleteProperty(g_display, g_wnd, rdesktop_clipboard_target_atom);
	XFree(data);
	cliprdr_send_data(NULL, 0);
}

/* This function is called for SelectionRequest events.
   The SelectionRequest message is sent from the requestor to the clipboard owner
   to request clipboard data.
 */
void
xclip_handle_SelectionRequest(XSelectionRequestEvent * event)
{
	unsigned long nitems, bytes_left;
	unsigned char *prop_return;
	uint32 *wanted_format;
	int format, res;
	Atom type;

	DEBUG_CLIPBOARD(("xclip_handle_SelectionRequest: selection=%s, target=%s, property=%s\n",
			 XGetAtomName(g_display, event->selection),
			 XGetAtomName(g_display, event->target),
			 XGetAtomName(g_display, event->property)));

	if (event->target == targets_atom)
	{
		xclip_provide_selection(event, XA_ATOM, 32, (uint8 *) & targets, NUM_TARGETS);
		return;
	}
	else if (event->target == timestamp_atom)
	{
		xclip_provide_selection(event, XA_INTEGER, 32, (uint8 *) & g_last_gesturetime, 1);
		return;
	}
	else if (event->target == rdesktop_clipboard_formats_atom)
	{
		res = XGetWindowProperty(g_display, event->requestor,
					 rdesktop_clipboard_target_atom, 0, 1, True, XA_INTEGER,
					 &type, &format, &nitems, &bytes_left, &prop_return);
		wanted_format = (uint32 *) prop_return;
		format = (res == Success) ? *wanted_format : CF_TEXT;
		/* FIXME: Need to free returned data? */
	}
	else
	{
		format = CF_TEXT;
	}

	cliprdr_send_data_request(format);
	selection_request = *event;
	/* wait for data */
}

/* When this rdesktop holds ownership over the clipboard, it means the clipboard data
   is offered by the RDP server (and when its pasted inside RDP, there's no network
   roundtrip). This event symbolizes this rdesktop lost onwership of the clipboard
   to some other X client. We should find out what clipboard formats this other
   client offers and announce that to RDP. */
void
xclip_handle_SelectionClear(void)
{
	DEBUG_CLIPBOARD(("xclip_handle_SelectionClear\n"));
	have_primary = 0;
	XDeleteProperty(g_display, DefaultRootWindow(g_display), rdesktop_clipboard_formats_atom);
	cliprdr_send_simple_native_format_announce(CF_TEXT);
}

/* Called when any property changes in our window or the root window. */
void
xclip_handle_PropertyNotify(XPropertyEvent * event)
{
	unsigned long nitems;
	unsigned long offset = 0;
	unsigned long bytes_left = 1;
	int format, res;
	XWindowAttributes wa;
	uint8 *data;
	Atom type;

	if (event->state == PropertyNewValue && g_waiting_for_INCR)
	{
		DEBUG_CLIPBOARD(("x_clip_handle_PropertyNotify: g_waiting_for_INCR != 0\n"));

		while (bytes_left > 0)
		{
			if ((XGetWindowProperty
			     (g_display, g_wnd, rdesktop_clipboard_target_atom, offset, 4096L,
			      False, AnyPropertyType, &type, &format, &nitems, &bytes_left,
			      &data) != Success))
			{
				XFree(data);
				return;
			}

			if (nitems == 0)
			{
				XGetWindowAttributes(g_display, g_wnd, &wa);
				XSelectInput(g_display, g_wnd,
					     (wa.your_event_mask ^ PropertyChangeMask));
				XFree(data);
				g_waiting_for_INCR = 0;

				if (g_clip_buflen > 0)
				{
					cliprdr_send_data(g_clip_buffer, g_clip_buflen + 1);

					if (!rdesktop_is_selection_owner)
						cliprdr_send_simple_native_format_announce(CF_TEXT);

					xfree(g_clip_buffer);
					g_clip_buffer = 0;
					g_clip_buflen = 0;
				}
			}
			else
			{
				uint8 *translated_data;
				uint32 length = nitems;
				uint8 *tmp;

				offset += (length / 4);
				DEBUG_CLIPBOARD(("Translating linebreaks before sending data\n"));
				translated_data = lf2crlf(data, &length);

				tmp = xmalloc(length + g_clip_buflen);
				strncpy((char *) tmp, (char *) g_clip_buffer, g_clip_buflen);
				xfree(g_clip_buffer);

				strncpy((char *) (tmp + g_clip_buflen), (char *) translated_data,
					length);
				xfree(translated_data);

				g_clip_buffer = tmp;
				g_clip_buflen += length;

				XFree(data);
			}
		}
		XDeleteProperty(g_display, g_wnd, rdesktop_clipboard_target_atom);
	}

	if (event->atom != rdesktop_clipboard_formats_atom)
		return;

	if (have_primary)	/* from us */
		return;

	if (event->state == PropertyNewValue)
	{
		res = XGetWindowProperty(g_display, DefaultRootWindow(g_display),
					 rdesktop_clipboard_formats_atom, 0,
					 XMaxRequestSize(g_display), False, XA_STRING, &type,
					 &format, &nitems, &bytes_left, &data);

		if ((res == Success) && (nitems > 0))
		{
			cliprdr_send_native_format_announce(data, nitems);
			rdesktop_is_selection_owner = 1;
			return;
		}
	}

	/* PropertyDelete, or XGetWindowProperty failed */
	cliprdr_send_simple_native_format_announce(CF_TEXT);
	rdesktop_is_selection_owner = 0;
}
#endif


/* Called when the RDP server announces new clipboard data formats.
   In response, we:
   - take ownership over the clipboard
   - declare those formats in their Windows native form
     to other rdesktop instances on this X server */
void
ui_clip_format_announce(uint8 * data, uint32 length)
{
	XSetSelectionOwner(g_display, primary_atom, g_wnd, g_last_gesturetime);
	if (XGetSelectionOwner(g_display, primary_atom) != g_wnd)
	{
		warning("Failed to aquire ownership of PRIMARY clipboard\n");
		return;
	}

	have_primary = 1;
	XChangeProperty(g_display, DefaultRootWindow(g_display),
			rdesktop_clipboard_formats_atom, XA_STRING, 8, PropModeReplace, data,
			length);

	XSetSelectionOwner(g_display, clipboard_atom, g_wnd, g_last_gesturetime);
	if (XGetSelectionOwner(g_display, clipboard_atom) != g_wnd)
		warning("Failed to aquire ownership of CLIPBOARD clipboard\n");
}


void
ui_clip_handle_data(uint8 * data, uint32 length)
{
	if (selection_request.target != rdesktop_clipboard_formats_atom)
	{
		uint8 *firstnull;

		/* translate linebreaks */
		crlf2lf(data, &length);

		/* Only send data up to null byte, if any */
		firstnull = (uint8 *) strchr((char *) data, '\0');
		if (firstnull)
		{
			length = firstnull - data + 1;
		}
	}

	xclip_provide_selection(&selection_request, XA_STRING, 8, data, length - 1);
}

void
ui_clip_request_data(uint32 format)
{
	Window selectionowner;

	DEBUG_CLIPBOARD(("Request from server for format %d\n", format));

	if (rdesktop_is_selection_owner)
	{
		XChangeProperty(g_display, g_wnd, rdesktop_clipboard_target_atom,
				XA_INTEGER, 32, PropModeReplace, (unsigned char *) &format, 1);

		XConvertSelection(g_display, primary_atom, rdesktop_clipboard_formats_atom,
				  rdesktop_clipboard_target_atom, g_wnd, CurrentTime);
		return;
	}

	selectionowner = XGetSelectionOwner(g_display, primary_atom);
	if (selectionowner != None)
	{
		XConvertSelection(g_display, primary_atom, targets_atom,
				  rdesktop_clipboard_target_atom, g_wnd, CurrentTime);
		return;
	}

	/* No PRIMARY, try CLIPBOARD */
	selectionowner = XGetSelectionOwner(g_display, clipboard_atom);
	if (selectionowner != None)
	{
		XConvertSelection(g_display, clipboard_atom, targets_atom,
				  rdesktop_clipboard_target_atom, g_wnd, CurrentTime);
		return;
	}

	/* No data available */
	cliprdr_send_data(NULL, 0);
}

void
ui_clip_sync(void)
{
	cliprdr_send_simple_native_format_announce(CF_TEXT);
}


void
xclip_init(void)
{
	if (!cliprdr_init())
		return;

	primary_atom = XInternAtom(g_display, "PRIMARY", False);
	clipboard_atom = XInternAtom(g_display, "CLIPBOARD", False);
	targets_atom = XInternAtom(g_display, "TARGETS", False);
	timestamp_atom = XInternAtom(g_display, "TIMESTAMP", False);
	rdesktop_clipboard_target_atom =
		XInternAtom(g_display, "_RDESKTOP_CLIPBOARD_TARGET", False);
	incr_atom = XInternAtom(g_display, "INCR", False);
	targets[0] = targets_atom;
	targets[1] = XInternAtom(g_display, "TEXT", False);
	targets[2] = XInternAtom(g_display, "STRING", False);
	targets[3] = XInternAtom(g_display, "text/unicode", False);
	targets[4] = XInternAtom(g_display, "TIMESTAMP", False);
	targets[5] = XA_STRING;

	/* rdesktop sets _RDESKTOP_CLIPBOARD_FORMATS on the root window when acquiring the clipboard.
	   Other interested rdesktops can use this to notify their server of the available formats. */
	rdesktop_clipboard_formats_atom =
		XInternAtom(g_display, "_RDESKTOP_CLIPBOARD_FORMATS", False);
	XSelectInput(g_display, DefaultRootWindow(g_display), PropertyChangeMask);
}
