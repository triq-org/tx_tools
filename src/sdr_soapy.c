/*
 * Copyright (C) 2014 by Kyle Keen <keenerd@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* a collection of user friendly tools
 * todo: use strtol/strtod for more flexible number parsing
 * */

#include "sdr_soapy.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#define _USE_MATH_DEFINES
#endif

#include <math.h>

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#ifdef _MSC_VER

//http://unixpapa.com/incnote/string.html
char * strsep(char **sp, char *sep)
{
    char *p, *s;
    if (sp == NULL || *sp == NULL || **sp == '\0') return(NULL);
    s = *sp;
    p = s + strcspn(s, sep);
    if (*p != '\0') *p++ = '\0';
    *sp = p;
    return(s);
}
#endif

int verbose_set_frequency(SoapySDRDevice *dev, const int direction, double frequency)
{
	int r;

	SoapySDRKwargs args = {0};
	r = SoapySDRDevice_setFrequency(dev, direction, 0, frequency, &args);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set center freq.\n");
	} else {
		fprintf(stderr, "Tuned to %.0f Hz.\n", frequency);
	}
	return r;
}

int verbose_set_sample_rate(SoapySDRDevice *dev, const int direction, double samp_rate)
{
	int r;
	r = SoapySDRDevice_setSampleRate(dev, direction, 0, samp_rate);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set sample rate.\n");
	} else {
		fprintf(stderr, "Sampling at %.0f S/s.\n", samp_rate);
	}
	return r;
}

int verbose_set_bandwidth(SoapySDRDevice *dev, const int direction, double bandwidth)
{
	int r;
	r = SoapySDRDevice_setBandwidth(dev, direction, 0, bandwidth);
	double applied_bw = 0;
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set bandwidth.\n");
	} else if (bandwidth > 0) {
		applied_bw = SoapySDRDevice_getBandwidth(dev, direction, 0);
		if (applied_bw > 0.0)
			fprintf(stderr, "Bandwidth parameter %.0f Hz resulted in %.0f Hz.\n", bandwidth, applied_bw);
		else
			fprintf(stderr, "Set bandwidth parameter %.0f Hz.\n", bandwidth);
	} else {
		fprintf(stderr, "Bandwidth set to automatic resulted in %.0f Hz.\n", applied_bw);
	}
	return r;
}

int verbose_gain_str_set(SoapySDRDevice *dev, char const *gain_str)
{
	SoapySDRKwargs args = {0};
	size_t i;
	int r = 0;

	/* TODO: manual gain mode
	r = rtlsdr_set_tuner_gain_mode(dev, 1);
	if (r < 0) {
		fprintf(stderr, "WARNING: Failed to enable manual gain.\n");
		return r;
	}
	*/

	if (strchr(gain_str, '=')) {
		// Set each gain individually (more control)
		parse_kwargs(gain_str, &args);

		for (i = 0; i < args.size; ++i) {
			char *name = args.keys[i];
			double value = atof(args.vals[i]);

			fprintf(stderr, "Setting gain element %s: %f dB\n", name, value);
			r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_TX, 0, name, value);
			if (r != 0) {
				fprintf(stderr, "WARNING: setGainElement(%s, %f) failed: %d\n", name, value, r);
			}
		}
	} else {
		// Set overall gain and let SoapySDR distribute amongst components
		double value = atof(gain_str);
		r = SoapySDRDevice_setGain(dev, SOAPY_SDR_TX, 0, value);
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
		} else {
			fprintf(stderr, "Tuner gain set to %0.2f dB.\n", value);
		}
		// TODO: read back and print each individual getGainElement()s
	}

	return r;
}

int verbose_ppm_set(SoapySDRDevice *dev, double ppm_error)
{
	int r;
	if (ppm_error == 0.0) {
		return 0;}
	r = SoapySDRDevice_setFrequencyComponent(dev, SOAPY_SDR_RX, 0, "CORR", ppm_error, NULL);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set ppm error.\n");
	} else {
		fprintf(stderr, "Tuner error set to %f ppm.\n", ppm_error);
	}
	return r;
}

static void show_device_info(SoapySDRDevice *dev, const int direction)
{
	size_t len = 0, i = 0;
	char **antennas = NULL;
	char **gains = NULL;
	char **frequencies = NULL;
	SoapySDRRange gainRange = {0};
	SoapySDRRange *frequencyRanges = NULL;
	SoapySDRRange *rates = NULL;
	SoapySDRRange *bandwidths = NULL;
	double fullScale;
	char **stream_formats = NULL;
	char *native_stream_format = NULL;
	SoapySDRKwargs args;
	char *hwkey = NULL;

	size_t channel = 0;

	hwkey = SoapySDRDevice_getHardwareKey(dev);
	fprintf(stderr, "Using device %s: ", hwkey);

	args = SoapySDRDevice_getHardwareInfo(dev);
	for (i = 0; i < args.size; ++i) {
		fprintf(stderr, "%s=%s ", args.keys[i], args.vals[i]);
	}
	fprintf(stderr, "\n");

	antennas = SoapySDRDevice_listAntennas(dev, direction, channel, &len);
	fprintf(stderr, "Found %zu antenna(s): ", len);
	for (i = 0; i < len; ++i) {
		fprintf(stderr, "%s ", antennas[i]);
	}
	fprintf(stderr, "\n");
/*
  Antennas: A
  Corrections: DC removal
  Full gain range: [0, 89] dB
    PGA gain range: [0, 89] dB
  Full freq range: [70, 6000] MHz
    RF freq range: [70, 6000] MHz
  Sample rates: 0.065105, 1, 2, 3, 4, 6, 7, 8, 9, 10 MSps
  Filter bandwidths: 0.2, 1, 2, 3, 4, 6, 7, 8, 9, 10 MHz
*/
	gainRange = SoapySDRDevice_getGainRange(dev, direction, channel);
	fprintf(stderr, "Gain range: %.0f - %.0f (step %.0f)\n", gainRange.minimum, gainRange.maximum, gainRange.step);

	gains = SoapySDRDevice_listGains(dev, direction, channel, &len);
	fprintf(stderr, "Found %zu gain(s): ", len);
	for (i = 0; i < len; ++i) {
		fprintf(stderr, "%s ", gains[i]);
	}
	fprintf(stderr, "\n");

	frequencies = SoapySDRDevice_listFrequencies(dev, direction, channel, &len);
	fprintf(stderr, "Found %zu frequencies: ", len);
	for (i = 0; i < len; ++i) {
		fprintf(stderr, "%s ", frequencies[i]);
	}
	fprintf(stderr, "\n");

	frequencyRanges = SoapySDRDevice_getFrequencyRange(dev, direction, channel, &len);
	fprintf(stderr, "Found %zu frequency range(s): ", len);
	for (i = 0; i < len; ++i) {
		fprintf(stderr, "%.0f - %.0f (step %.0f) ", frequencyRanges[i].minimum, frequencyRanges[i].maximum, frequencyRanges[i].step);
	}
	fprintf(stderr, "\n");

	rates = SoapySDRDevice_getSampleRateRange(dev, direction, channel, &len);
	fprintf(stderr, "Found %zu sample rate range(s): ", len);
	for (i = 0; i < len; ++i) {
		if (rates[i].minimum == rates[i].maximum)
			fprintf(stderr, "%.0f ", rates[i].minimum);
		else
			fprintf(stderr, "%.0f - %.0f (step %.0f) ", rates[i].minimum, rates[i].maximum, rates[i].step);
	}
	fprintf(stderr, "\n");

	bandwidths = SoapySDRDevice_getBandwidthRange(dev, direction, channel, &len);
    fprintf(stderr, "Found %zu bandwidth range(s): ", len);
    for (i = 0; i < len; ++i) {
		fprintf(stderr, "%.0f - %.0f (step %.0f) ", bandwidths[i].minimum, bandwidths[i].maximum, bandwidths[i].step);
    }
    fprintf(stderr, "\n");

    double bandwidth = SoapySDRDevice_getBandwidth(dev, direction, channel);
    fprintf(stderr, "Found current bandwidth %.0f\n", bandwidth);

    stream_formats = SoapySDRDevice_getStreamFormats(dev, direction, channel, &len);
    fprintf(stderr, "Found %zu stream format(s): ", len);
    for (i = 0; i < len; ++i) {
		fprintf(stderr, "%s ", stream_formats[i]);
    }
    fprintf(stderr, "\n");

    native_stream_format = SoapySDRDevice_getNativeStreamFormat(dev, direction, channel, &fullScale);
    fprintf(stderr, "Found native stream format: %s (full scale: %f)\n", native_stream_format, fullScale);
}

int verbose_device_search(char const *s, SoapySDRDevice **devOut, const int direction)
{
	size_t device_count = 0;
	size_t i = 0;
	int device, offset;
	char *s2;
	char vendor[256], product[256], serial[256];
	SoapySDRDevice *dev = NULL;

	dev = SoapySDRDevice_makeStrArgs(s);
	if (!dev) {
		fprintf(stderr, "SoapySDRDevice_make failed\n");
		return -1;
	}

	show_device_info(dev, direction);

	*devOut = dev;
	return 0;
}

int verbose_setup_stream(SoapySDRDevice *dev, SoapySDRStream **streamOut, const int direction, const char *format)
{
	SoapySDRKwargs stream_args = {0};

	// request exactly the first channel, e.g. SoapyPlutoSDR has strange ideas about "the default channel"
	size_t channels[] = {0};
	size_t numChanns = 1;
	if (SoapySDRDevice_setupStream(dev, streamOut, direction, format, channels, numChanns, &stream_args) != 0) {
		fprintf(stderr, "SoapySDRDevice_setupStream failed\n");
		return -3;
	}

	return 0;
}

void parse_kwargs(char const *s, SoapySDRKwargs *args)
{
	char *copied, *cursor, *pair, *equals;

	copied = strdup(s);
	cursor = copied;
	while ((pair = strsep(&cursor, ",")) != NULL) {
		char *key, *value;
		//printf("pair = %s\n", pair);

		equals = strchr(pair, '=');
		if (equals) {
			key = pair;
			*equals = '\0';
			value = equals + 1;
		} else {
			key = pair;
			value = "";
		}
		//printf("key=|%s|, value=|%s|\n", key, value);
		SoapySDRKwargs_set(args, key, value);
	}

	free(copied);
}

// vim: tabstop=8:softtabstop=8:shiftwidth=8:noexpandtab
