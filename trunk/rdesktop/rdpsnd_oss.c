/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Sound Channel Process Functions - Open Sound System
   Copyright (C) Matthew Chapman 2003
   Copyright (C) GuoJunBo guojunbo@ict.ac.cn 2003

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

/* 
   This is a workaround for Esound bug 312665. 
   FIXME: Remove this when Esound is fixed. 
*/
#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif

#include "rdesktop.h"
#include "rdpsnd.h"
#include "rdpsnd_dsp.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEFAULTDEVICE	"/dev/dsp"
#define MAX_LEN		512

static int snd_rate;
static short samplewidth;
static char *dsp_dev;
static struct audio_driver oss_driver;
static BOOL in_esddsp;

static BOOL
detect_esddsp(void)
{
	struct stat s;
	char *preload;

	if (fstat(g_dsp_fd, &s) == -1)
		return False;

	if (S_ISCHR(s.st_mode) || S_ISBLK(s.st_mode))
		return False;

	preload = getenv("LD_PRELOAD");
	if (preload == NULL)
		return False;

	if (strstr(preload, "esddsp") == NULL)
		return False;

	return True;
}

BOOL
oss_open(void)
{
	if ((g_dsp_fd = open(dsp_dev, O_WRONLY)) == -1)
	{
		perror(dsp_dev);
		return False;
	}

	in_esddsp = detect_esddsp();

	return True;
}

void
oss_close(void)
{
	close(g_dsp_fd);
	g_dsp_busy = 0;
}

BOOL
oss_format_supported(WAVEFORMATEX * pwfx)
{
	if (pwfx->wFormatTag != WAVE_FORMAT_PCM)
		return False;
	if ((pwfx->nChannels != 1) && (pwfx->nChannels != 2))
		return False;
	if ((pwfx->wBitsPerSample != 8) && (pwfx->wBitsPerSample != 16))
		return False;

	return True;
}

BOOL
oss_set_format(WAVEFORMATEX * pwfx)
{
	int stereo, format, fragments;
	static BOOL driver_broken = False;

	ioctl(g_dsp_fd, SNDCTL_DSP_RESET, NULL);
	ioctl(g_dsp_fd, SNDCTL_DSP_SYNC, NULL);

	if (pwfx->wBitsPerSample == 8)
		format = AFMT_U8;
	else if (pwfx->wBitsPerSample == 16)
		format = AFMT_S16_LE;

	samplewidth = pwfx->wBitsPerSample / 8;

	if (ioctl(g_dsp_fd, SNDCTL_DSP_SETFMT, &format) == -1)
	{
		perror("SNDCTL_DSP_SETFMT");
		oss_close();
		return False;
	}

	if (pwfx->nChannels == 2)
	{
		stereo = 1;
		samplewidth *= 2;
	}
	else
	{
		stereo = 0;
	}

	if (ioctl(g_dsp_fd, SNDCTL_DSP_STEREO, &stereo) == -1)
	{
		perror("SNDCTL_DSP_CHANNELS");
		oss_close();
		return False;
	}

	oss_driver.need_resampling = 0;
	snd_rate = pwfx->nSamplesPerSec;
	if (ioctl(g_dsp_fd, SNDCTL_DSP_SPEED, &snd_rate) == -1)
	{
		int rates[] = { 44100, 48000, 0 };
		int *prates = rates;

		while (*prates != 0)
		{
			if ((pwfx->nSamplesPerSec != *prates)
			    && (ioctl(g_dsp_fd, SNDCTL_DSP_SPEED, prates) != -1))
			{
				oss_driver.need_resampling = 1;
				snd_rate = *prates;
				if (rdpsnd_dsp_resample_set
				    (snd_rate, pwfx->wBitsPerSample, pwfx->nChannels) == False)
				{
					error("rdpsnd_dsp_resample_set failed");
					oss_close();
					return False;
				}

				break;
			}
			prates++;
		}

		if (*prates == 0)
		{
			perror("SNDCTL_DSP_SPEED");
			oss_close();
			return False;
		}
	}

	/* try to get 12 fragments of 2^12 bytes size */
	fragments = (12 << 16) + 12;
	ioctl(g_dsp_fd, SNDCTL_DSP_SETFRAGMENT, &fragments);

	if (!driver_broken)
	{
		audio_buf_info info;

		memset(&info, 0, sizeof(info));
		if (ioctl(g_dsp_fd, SNDCTL_DSP_GETOSPACE, &info) == -1)
		{
			perror("SNDCTL_DSP_GETOSPACE");
			oss_close();
			return False;
		}

		if (info.fragments == 0 || info.fragstotal == 0 || info.fragsize == 0)
		{
			fprintf(stderr,
				"Broken OSS-driver detected: fragments: %d, fragstotal: %d, fragsize: %d\n",
				info.fragments, info.fragstotal, info.fragsize);
			driver_broken = True;
		}
	}

	return True;
}

void
oss_volume(uint16 left, uint16 right)
{
	uint32 volume;

	volume = left / (65536 / 100);
	volume |= right / (65536 / 100) << 8;

	if (ioctl(g_dsp_fd, MIXER_WRITE(SOUND_MIXER_PCM), &volume) == -1)
	{
		warning("hardware volume control unavailable, falling back to software volume control!\n");
		oss_driver.wave_out_volume = rdpsnd_dsp_softvol_set;
		rdpsnd_dsp_softvol_set(left, right);
		return;
	}
}

void
oss_play(void)
{
	struct audio_packet *packet;
	ssize_t len;
	STREAM out;

	if (rdpsnd_queue_empty())
	{
		g_dsp_busy = 0;
		return;
	}

	packet = rdpsnd_queue_current_packet();
	out = &packet->s;

	len = out->end - out->p;

	len = write(g_dsp_fd, out->p, (len > MAX_LEN) ? MAX_LEN : len);
	if (len == -1)
	{
		if (errno != EWOULDBLOCK)
			perror("write audio");
		g_dsp_busy = 1;
		return;
	}

	out->p += len;

	if (out->p == out->end)
	{
		int delay_bytes;
		unsigned long delay_us;
		audio_buf_info info;

		if (in_esddsp)
		{
			/* EsounD has no way of querying buffer status, so we have to
			 * go with a fixed size. */
			delay_bytes = out->size;
		}
		else
		{
#ifdef SNDCTL_DSP_GETODELAY
			delay_bytes = 0;
			if (ioctl(g_dsp_fd, SNDCTL_DSP_GETODELAY, &delay_bytes) == -1)
				delay_bytes = -1;
#else
			delay_bytes = -1;
#endif

			if (delay_bytes == -1)
			{
				if (ioctl(g_dsp_fd, SNDCTL_DSP_GETOSPACE, &info) != -1)
					delay_bytes = info.fragstotal * info.fragsize - info.bytes;
				else
					delay_bytes = out->size;
			}
		}

		delay_us = delay_bytes * (1000000 / (samplewidth * snd_rate));
		rdpsnd_queue_next(delay_us);
	}
	else
	{
		g_dsp_busy = 1;
	}

	return;
}

struct audio_driver *
oss_register(char *options)
{
	oss_driver.wave_out_open = oss_open;
	oss_driver.wave_out_close = oss_close;
	oss_driver.wave_out_format_supported = oss_format_supported;
	oss_driver.wave_out_set_format = oss_set_format;
	oss_driver.wave_out_volume = oss_volume;
	oss_driver.wave_out_play = oss_play;
	oss_driver.name = xstrdup("oss");
	oss_driver.description =
		xstrdup("OSS output driver, default device: " DEFAULTDEVICE " or $AUDIODEV");
	oss_driver.need_byteswap_on_be = 0;
	oss_driver.need_resampling = 0;
	oss_driver.next = NULL;

	if (options)
	{
		dsp_dev = xstrdup(options);
	}
	else
	{
		dsp_dev = getenv("AUDIODEV");

		if (dsp_dev == NULL)
		{
			dsp_dev = xstrdup(DEFAULTDEVICE);
		}
	}

	return &oss_driver;
}
