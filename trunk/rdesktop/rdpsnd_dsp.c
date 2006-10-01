/*
   rdesktop: A Remote Desktop Protocol client.
   Sound DSP routines
   Copyright (C) Michael Gernoth 2006

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

#include <strings.h>

#include "rdesktop.h"
#include "rdpsnd.h"
#include "rdpsnd_dsp.h"

#ifdef HAVE_LIBSAMPLERATE
#include <samplerate.h>

#define SRC_CONVERTER SRC_SINC_MEDIUM_QUALITY
#endif

#define MAX_VOLUME 65535

static uint16 softvol_left = MAX_VOLUME;
static uint16 softvol_right = MAX_VOLUME;
static uint32 resample_to_srate = 44100;
static uint16 resample_to_bitspersample = 16;
static uint16 resample_to_channels = 2;
#ifdef HAVE_LIBSAMPLERATE
static SRC_STATE *src_converter = NULL;
#endif

void
rdpsnd_dsp_softvol_set(uint16 left, uint16 right)
{
	softvol_left = left;
	softvol_right = right;
	DEBUG(("rdpsnd_dsp_softvol_set: left: %u, right: %u\n", left, right));
}

void
rdpsnd_dsp_softvol(unsigned char *buffer, unsigned int size, WAVEFORMATEX * format)
{
	unsigned int factor_left, factor_right;
	unsigned char *posin = buffer;
	unsigned char *posout = buffer;

	if ((softvol_left == MAX_VOLUME) && (softvol_right == MAX_VOLUME))
		return;

	factor_left = (softvol_left * 256) / MAX_VOLUME;
	factor_right = (softvol_right * 256) / MAX_VOLUME;

	if (format->nChannels == 1)
	{
		factor_left = factor_right = (factor_left + factor_right) / 2;
	}

	if (format->wBitsPerSample == 8)
	{
		sint8 val;

		while (posout < buffer + size)
		{
			/* Left */
			val = *posin++;
			val = (val * factor_left) >> 8;
			*posout++ = val;

			/* Right */
			val = *posin++;
			val = (val * factor_right) >> 8;
			*posout++ = val;
		}
	}
	else
	{
		sint16 val;

		while (posout < buffer + size)
		{
			/* Left */
			val = *posin++;
			val |= *posin++ << 8;
			val = (val * factor_left) >> 8;
			*posout++ = val & 0xff;
			*posout++ = val >> 8;

			/* Right */
			val = *posin++;
			val |= *posin++ << 8;
			val = (val * factor_right) >> 8;
			*posout++ = val & 0xff;
			*posout++ = val >> 8;
		}
	}

	DEBUG(("using softvol with factors left: %d, right: %d (%d/%d)\n", factor_left,
	       factor_right, format->wBitsPerSample, format->nChannels));
}

void
rdpsnd_dsp_swapbytes(unsigned char *buffer, unsigned int size, WAVEFORMATEX * format)
{
	int i;
	uint8 swap;

	if (format->wBitsPerSample == 8)
		return;

	for (i = 0; i < size; i += 2)
	{
		swap = *(buffer + i);
		*(buffer + i) = *(buffer + i + 1);
		*(buffer + i + 1) = swap;
	}
}

BOOL
rdpsnd_dsp_resample_set(uint32 device_srate, uint16 device_bitspersample, uint16 device_channels)
{
#ifdef HAVE_LIBSAMPLERATE
	int err;
#endif

	if (device_bitspersample != 16 && device_bitspersample != 8)
		return False;

	if (device_channels != 1 && device_channels != 2)
		return False;

	resample_to_srate = device_srate;
	resample_to_bitspersample = device_bitspersample;
	resample_to_channels = device_channels;

#ifdef HAVE_LIBSAMPLERATE
	if (src_converter != NULL)
		src_converter = src_delete(src_converter);

	if ((src_converter = src_new(SRC_CONVERTER, device_channels, &err)) == NULL)
	{
		warning("src_new failed: %d!\n", err);
		return False;
	}
#endif

	return True;
}

BOOL
rdpsnd_dsp_resample_supported(WAVEFORMATEX * format)
{
	if (format->wFormatTag != WAVE_FORMAT_PCM)
		return False;
	if ((format->nChannels != 1) && (format->nChannels != 2))
		return False;
	if ((format->wBitsPerSample != 8) && (format->wBitsPerSample != 16))
		return False;

	return True;
}

uint32
rdpsnd_dsp_resample(unsigned char **out, unsigned char *in, unsigned int size,
		    WAVEFORMATEX * format, BOOL stream_be)
{
#ifdef HAVE_LIBSAMPLERATE
	SRC_DATA resample_data;
	float *infloat, *outfloat;
	int err;
#else
	int ratio1k = (resample_to_srate * 1000) / format->nSamplesPerSec;
#endif
	int innum, outnum;
	static BOOL warned = False;
	unsigned char *tmpdata = NULL;
	int samplewidth = format->wBitsPerSample / 8;
	int outsize = 0;
	int i;

	if ((resample_to_bitspersample == format->wBitsPerSample) &&
	    (resample_to_channels == format->nChannels) &&
	    (resample_to_srate == format->nSamplesPerSec))
		return 0;

#ifdef B_ENDIAN
	if (!stream_be)
		rdpsnd_dsp_swapbytes(in, size, format);
#endif

	/* Expand 8bit input-samples to 16bit */
#ifndef HAVE_LIBSAMPLERATE	/* libsamplerate needs 16bit samples */
	if (format->wBitsPerSample != resample_to_bitspersample)
#endif
	{
		/* source: 8 bit, dest: 16bit */
		if (format->wBitsPerSample == 8)
		{
			tmpdata = xmalloc(size * 2);
			for (i = 0; i < size; i++)
			{
				tmpdata[i * 2] = in[i];
				tmpdata[(i * 2) + 1] = 0x00;
			}
			in = tmpdata;
			samplewidth = 16 / 2;
			size *= 2;
		}
	}

	if (resample_to_channels != format->nChannels)
	{
		warning("unsupported resample-settings (%u -> %u/%u -> %u/%u -> %u), not resampling!\n", format->nSamplesPerSec, resample_to_srate, format->wBitsPerSample, resample_to_bitspersample, format->nChannels, resample_to_channels);
		warned = True;
	}

	innum = size / samplewidth;

	/* Do the resampling */
#ifdef HAVE_LIBSAMPLERATE
	if (src_converter == NULL)
	{
		warning("no samplerate converter available!!\n");
		return 0;
	}

	outnum = ((float) innum * ((float) resample_to_srate / (float) format->nSamplesPerSec)) + 1;

	infloat = xmalloc(sizeof(float) * innum);
	outfloat = xmalloc(sizeof(float) * outnum);

	src_short_to_float_array((short *) in, infloat, innum);

	bzero(&resample_data, sizeof(resample_data));
	resample_data.data_in = infloat;
	resample_data.data_out = outfloat;
	resample_data.input_frames = innum / resample_to_channels;
	resample_data.output_frames = outnum / resample_to_channels;
	resample_data.src_ratio = (double) resample_to_srate / (double) format->nSamplesPerSec;
	resample_data.end_of_input = 0;

	if ((err = src_process(src_converter, &resample_data)) != 0)
		error("src_process: %s", src_strerror(err));

	xfree(infloat);

	outsize = resample_data.output_frames_gen * resample_to_channels * samplewidth;
	*out = xmalloc(outsize);
	src_float_to_short_array(outfloat, (short *) *out,
				 resample_data.output_frames_gen * resample_to_channels);
	xfree(outfloat);

#else
	/* Michaels simple linear resampler */
	if (resample_to_srate < format->nSamplesPerSec)
	{
		warning("downsampling currently not supported!\n");
		return 0;
	}

	outnum = (innum * ratio1k) / 1000;

	outsize = outnum * samplewidth;
	*out = xmalloc(outsize);
	bzero(*out, outsize);

	for (i = 0; i < outsize / (resample_to_channels * samplewidth); i++)
	{
		int source = ((i * 1000) + ratio1k - 1000) / (ratio1k + 1);

		if (source * resample_to_channels + samplewidth > size)
			break;

		if (resample_to_channels == 2)
		{
			memcpy(*out + (i * resample_to_channels * samplewidth),
			       in + (source * resample_to_channels * samplewidth), samplewidth);
			memcpy(*out + (i * resample_to_channels * samplewidth) + samplewidth,
			       in + (source * resample_to_channels * samplewidth) + samplewidth,
			       samplewidth);
		}
		else
		{
			memcpy(*out + (i * samplewidth), in + (source * samplewidth), samplewidth);
		}
	}
	outsize = i * resample_to_channels * samplewidth;
#endif

	if (tmpdata != NULL)
		xfree(tmpdata);

	/* Shrink 16bit output-samples to 8bit */
#ifndef HAVE_LIBSAMPLERATE	/* libsamplerate produces 16bit samples */
	if (format->wBitsPerSample != resample_to_bitspersample)
#endif
	{
		/* source: 16 bit, dest: 8 bit */
		if (resample_to_bitspersample == 8)
		{
			for (i = 0; i < outsize; i++)
			{
				*out[i] = *out[i * 2];
			}
			outsize /= 2;
		}
	}

#ifdef B_ENDIAN
	if (!stream_be)
		rdpsnd_dsp_swapbytes(*out, outsize, format);
#endif
	return outsize;
}

STREAM
rdpsnd_dsp_process(STREAM s, struct audio_driver * current_driver, WAVEFORMATEX * format)
{
	static struct stream out;
	BOOL stream_be = False;

	/* softvol and byteswap do not change the amount of data they
	   return, so they can operate on the input-stream */
	if (current_driver->wave_out_volume == rdpsnd_dsp_softvol_set)
		rdpsnd_dsp_softvol(s->data, s->size, format);

#ifdef B_ENDIAN
	if (current_driver->need_byteswap_on_be)
	{
		rdpsnd_dsp_swapbytes(s->data, s->size, format);
		stream_be = True;
	}
#endif

	out.data = NULL;

	if (current_driver->need_resampling)
		out.size = rdpsnd_dsp_resample(&out.data, s->data, s->size, format, stream_be);

	if (out.data == NULL)
	{
		out.data = xmalloc(s->size);
		memcpy(out.data, s->data, s->size);
		out.size = s->size;
	}

	out.p = out.data;
	out.end = out.p + out.size;

	return &out;
}
