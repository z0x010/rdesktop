/* 
   rdesktop: A Remote Desktop Protocol client.
   Sound Channel Process Functions - libao-driver
   Copyright (C) Matthew Chapman 2003
   Copyright (C) GuoJunBo guojunbo@ict.ac.cn 2003
   Copyright (C) Michael Gernoth mike@zerfleddert.de 2005

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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ao/ao.h>

#define MAX_QUEUE	10
#define WAVEOUTBUF	32

int g_dsp_fd;
ao_device *o_device = NULL;
int default_driver;
int g_samplerate;
BOOL g_dsp_busy = False;
static short g_samplewidth;

static struct audio_packet
{
	struct stream s;
	uint16 tick;
	uint8 index;
} packet_queue[MAX_QUEUE];
static unsigned int queue_hi, queue_lo;

BOOL
wave_out_open(void)
{
	ao_sample_format format;

	ao_initialize();
	default_driver = ao_default_driver_id();

	format.bits = 16;
	format.channels = 2;
	format.rate = 44100;
	g_samplerate = 44100;
	format.byte_format = AO_FMT_LITTLE;

	o_device = ao_open_live(default_driver, &format, NULL);
	if (o_device == NULL)
	{
		return False;
	}

	g_dsp_fd = 0;
	queue_lo = queue_hi = 0;

	return True;
}

void
wave_out_close(void)
{
	/* Ack all remaining packets */
	while (queue_lo != queue_hi)
	{
		rdpsnd_send_completion(packet_queue[queue_lo].tick, packet_queue[queue_lo].index);
		free(packet_queue[queue_lo].s.data);
		queue_lo = (queue_lo + 1) % MAX_QUEUE;
	}

	if (o_device != NULL)
		ao_close(o_device);
	ao_shutdown();
}

BOOL
wave_out_format_supported(WAVEFORMATEX * pwfx)
{
	if (pwfx->wFormatTag != WAVE_FORMAT_PCM)
		return False;
	if ((pwfx->nChannels != 1) && (pwfx->nChannels != 2))
		return False;
	if ((pwfx->wBitsPerSample != 8) && (pwfx->wBitsPerSample != 16))
		return False;
	/* The only common denominator between libao output drivers is a sample-rate of
	   44100, we need to upsample 22050 to it */
	if ((pwfx->nSamplesPerSec != 44100) && (pwfx->nSamplesPerSec != 22050))
		return False;

	return True;
}

BOOL
wave_out_set_format(WAVEFORMATEX * pwfx)
{
	ao_sample_format format;

	format.bits = pwfx->wBitsPerSample;
	format.channels = pwfx->nChannels;
	format.rate = 44100;
	g_samplerate = pwfx->nSamplesPerSec;
	format.byte_format = AO_FMT_LITTLE;

	g_samplewidth = pwfx->wBitsPerSample / 8;

	if(o_device != NULL)
		ao_close(o_device);

	o_device = ao_open_live(default_driver, &format, NULL);
	if (o_device == NULL)
	{
		return False;
	}


	return True;
}

void
wave_out_volume(uint16 left, uint16 right)
{
}

void
wave_out_write(STREAM s, uint16 tick, uint8 index)
{
	struct audio_packet *packet = &packet_queue[queue_hi];
	unsigned int next_hi = (queue_hi + 1) % MAX_QUEUE;

	if (next_hi == queue_lo)
	{
		error("No space to queue audio packet\n");
		return;
	}

	queue_hi = next_hi;

	packet->s = *s;
	packet->tick = tick;
	packet->index = index;
	packet->s.p += 4;

	/* we steal the data buffer from s, give it a new one */
	s->data = malloc(s->size);

	if (!g_dsp_busy)
		wave_out_play();
}

void
wave_out_play(void)
{
	struct audio_packet *packet;
	STREAM out;
	unsigned char expanded[WAVEOUTBUF];
	int offset,len,i;

	if (queue_lo == queue_hi)
	{
		g_dsp_busy = 0;
		return;
	}

	packet = &packet_queue[queue_lo];
	out = &packet->s;

	len = 0;

	if (g_samplerate == 22050 )
	{
		/* Resample to 44100 */
		for(i=0; (i<((WAVEOUTBUF/8)*(3-g_samplewidth))) && (out->p < out->end); i++)
		{
			offset=i*4*g_samplewidth;
			memcpy(&expanded[0*g_samplewidth+offset],out->p,g_samplewidth);
			memcpy(&expanded[2*g_samplewidth+offset],out->p,g_samplewidth);
			out->p += 2;

			memcpy(&expanded[1*g_samplewidth+offset],out->p,g_samplewidth);
			memcpy(&expanded[3*g_samplewidth+offset],out->p,g_samplewidth);
			out->p += 2;
			len += 4*g_samplewidth;
		}
	}
	else
	{
		len = (WAVEOUTBUF > (out->end - out->p)) ? (out->end - out->p) : WAVEOUTBUF;
		memcpy(expanded,out->p,len);
		out->p += len;
	}

	ao_play(o_device, expanded, len);

	if (out->p == out->end)
	{
		rdpsnd_send_completion(packet->tick, packet->index);
		free(out->data);
		queue_lo = (queue_lo + 1) % MAX_QUEUE;
	}

	g_dsp_busy = 1;
	return;
}
