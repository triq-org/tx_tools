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

#include "convenience.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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
struct tm *localtime_r (const time_t *timer, struct tm *result)
{
    struct tm *local_result = localtime (timer);
    if (local_result == NULL || result == NULL) return NULL;
    memcpy (result, local_result, sizeof (struct tm));
    return result;
}

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

int verbose_direct_sampling(SoapySDRDevice *dev, int on)
{
	int r = 0;
	char *value, *set_value;
	if (on == 0)
		value = "0";
	else if (on == 1)
		value = "1";
	else if (on == 2)
		value = "2";
	else
		return -1;
	SoapySDRDevice_writeSetting(dev, "direct_samp", value);
	set_value = SoapySDRDevice_readSetting(dev, "direct_samp");

	if (set_value == NULL) {
		fprintf(stderr, "WARNING: Failed to set direct sampling mode.\n");
		return r;
	}
	if (atoi(set_value) == 0) {
		fprintf(stderr, "Direct sampling mode disabled.\n");}
	if (atoi(set_value) == 1) {
		fprintf(stderr, "Enabled direct sampling mode, input 1/I.\n");}
	if (atoi(set_value) == 2) {
		fprintf(stderr, "Enabled direct sampling mode, input 2/Q.\n");}
	if (on == 3) {
		fprintf(stderr, "Enabled no-mod direct sampling mode.\n");}
	return r;
}

int verbose_offset_tuning(SoapySDRDevice *dev)
{
	int r = 0;
	SoapySDRDevice_writeSetting(dev, "offset_tune", "true");
	char *set_value = SoapySDRDevice_readSetting(dev, "offset_tune");

	if (strcmp(set_value, "true") != 0) {
		/* TODO: detection of failure modes
		if ( r == -2 )
			fprintf(stderr, "WARNING: Failed to set offset tuning: tuner doesn't support offset tuning!\n");
		else if ( r == -3 )
			fprintf(stderr, "WARNING: Failed to set offset tuning: direct sampling not combinable with offset tuning!\n");
		else
		*/
			fprintf(stderr, "WARNING: Failed to set offset tuning.\n");
	} else {
		fprintf(stderr, "Offset tuning mode enabled.\n");
	}
	return r;
}

int verbose_auto_gain(SoapySDRDevice *dev)
{
	int r;
	r = 0;
	/* TODO: not bridged, https://github.com/pothosware/SoapyRTLSDR/search?utf8=✓&q=rtlsdr_set_tuner_gain_mode
	r = rtlsdr_set_tuner_gain_mode(dev, 0);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
	} else {
		fprintf(stderr, "Tuner gain set to automatic.\n");
	}
	*/

	// Per-driver hacks TODO: clean this up
	char *driver = SoapySDRDevice_getDriverKey(dev);
	if (strcmp(driver, "RTLSDR") == 0) {
		// For now, set 40.0 dB, high
		// Note: 26.5 dB in https://github.com/librtlsdr/librtlsdr/blob/master/src/tuner_r82xx.c#L1067 - but it's not the same
		// TODO: remove or change after auto-gain? https://github.com/pothosware/SoapyRTLSDR/issues/21 rtlsdr_set_tuner_gain_mode(dev, 0);
		r = SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, 0, 40.);
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
		} else {
			fprintf(stderr, "Tuner gain semi-automatically set to 40 dB\n");
		}

	} else if (strcmp(driver, "HackRF") == 0) {
		// HackRF has three gains LNA, VGA, and AMP, setting total distributes amongst, 116.0 dB seems to work well,
		// even though it logs HACKRF_ERROR_INVALID_PARAM? https://github.com/rxseger/rx_tools/issues/9
		// Total gain is distributed amongst all gains, 116 = 37,65,1; the LNA is OK (<40) but VGA is out of range (65 > 62)
		// TODO: generic means to set all gains, of any SDR? string parsing LNA=#,VGA=#,AMP=#?
		r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "LNA", 40.); // max 40
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set LNA tuner gain.\n");
		}
		r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "VGA", 20.); // max 65
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set VGA tuner gain.\n");
		}
		r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "AMP", 0.); // on or off
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set AMP tuner gain.\n");
		}

	} else if (strcmp(driver, "LimeSDR-USB") == 0) {
		r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "LNA", 20.); // 0.0 - 30.0
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set LNA tuner gain.\n");
		}
		r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "PGA", 10.); // -12.0 - 19.0
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set PGA tuner gain.\n");
		}
		r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, 0, "TIA", 2.); // 0.0 - 12.0
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set TIA tuner gain.\n");
		}
		r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_TX, 0, "PAD", 0.); // -52.0 - 0.0
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set PAD tuner gain.\n");
		}

	}
	// otherwise leave unset, hopefully the driver has good defaults

	return r;
}

int verbose_gain_str_set(SoapySDRDevice *dev, char *gain_str)
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

int verbose_ppm_set(SoapySDRDevice *dev, int ppm_error)
{
	int r;
	if (ppm_error == 0) {
		return 0;}
	r = SoapySDRDevice_setFrequencyComponent(dev, SOAPY_SDR_RX, 0, "CORR", (double)ppm_error, NULL);
	if (r != 0) {
		fprintf(stderr, "WARNING: Failed to set ppm error.\n");
	} else {
		fprintf(stderr, "Tuner error set to %i ppm.\n", ppm_error);
	}
	return r;
}

static void show_device_info(SoapySDRDevice *dev, const int direction)
{
	size_t len = 0, i = 0;
	char **antennas = NULL;
	char **gains = NULL;
	char **frequencies = NULL;
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

	//SOAPY_SDR_API SoapySDRRange SoapySDRDevice_getGainRange(const SoapySDRDevice *device, const int direction, const size_t channel);
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

int verbose_device_search(char *s, SoapySDRDevice **devOut, const int direction)
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

void parse_kwargs(char *s, SoapySDRKwargs *args)
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
