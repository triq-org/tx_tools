/** @file
    tx_tools - SoapySDR backend.

    Copyright (C) 2019 by Christian Zuckschwerdt <zany@triq.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sdr_soapy.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

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

#include "sdr_backend.h"

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

// format is 3-4 chars (plus null), compare as int.
static int is_format_equal(const void *a, const void *b)
{
    return *(const uint32_t *)a == *(const uint32_t *)b;
}

int soapy_set_frequency(SoapySDRDevice *dev, const int direction, double frequency)
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

int soapy_set_sample_rate(SoapySDRDevice *dev, const int direction, double samp_rate)
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

int soapy_set_bandwidth(SoapySDRDevice *dev, const int direction, double bandwidth)
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

int soapy_gain_str_set(SoapySDRDevice *dev, char const *gain_str)
{
	SoapySDRKwargs args = {0};
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

		for (size_t i = 0; i < args.size; ++i) {
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

int soapy_ppm_set(SoapySDRDevice *dev, double ppm_error)
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
		// avoid a -Wfloat-equal warning
		if (rates[i].maximum - rates[i].minimum < 1 )
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

int soapy_device_search(char const *s, SoapySDRDevice **devOut, const int direction)
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

int soapy_setup_stream(SoapySDRDevice *dev, SoapySDRStream **streamOut, const int direction, const char *format)
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

// API

int soapy_enum_devices(sdr_ctx_t *sdr_ctx, const char *enum_args)
{
    fprintf(stderr, "SoapySDRDevice_enumerateStrArgs(%s)\n", enum_args);
    size_t devs_len             = 0;
    SoapySDRKwargs *devs_kwargs = SoapySDRDevice_enumerateStrArgs(enum_args, &devs_len);
    fprintf(stderr, "found %u devices\n", (unsigned)devs_len);

    for (size_t i = 0; i < devs_len; ++i) {
        char *kwargs = SoapySDRKwargs_toString(devs_kwargs + i);
        fprintf(stderr, "%u : %s\n", (unsigned)i, kwargs);
        free(kwargs);
    }

    // make all devices
    fprintf(stderr, "SoapySDRDevice_make_list()...\n");
    SoapySDRDevice **devs = SoapySDRDevice_make_list(devs_kwargs, devs_len);
    if (!devs) {
        SoapySDRKwargsList_clear(devs_kwargs, devs_len); // frees entries and struct
        return -1;
    }

    for (size_t i = 0; i < devs_len; ++i) {
        sdr_dev_t *sdr_dev = &sdr_ctx->devs[sdr_ctx->devs_len++];

        sdr_dev->backend = "soapy";
        char *kwargs = SoapySDRKwargs_toString(devs_kwargs + i);
        sdr_dev->dev_kwargs = kwargs;
        sdr_dev->device = devs[i];
        if (!devs[i])
            continue;
        char *d_key           = SoapySDRDevice_getDriverKey(devs[i]);
        char *h_key           = SoapySDRDevice_getHardwareKey(devs[i]);
        SoapySDRKwargs h_info = SoapySDRDevice_getHardwareInfo(devs[i]);
        char *p               = SoapySDRKwargs_toString(&h_info);
        fprintf(stderr, "%u : %s : %s : %s\n", (unsigned)i, d_key, h_key, p);
        sdr_dev->driver_key    = d_key;
        sdr_dev->hardware_key  = h_key;
        sdr_dev->hardware_info = p;
    }
    SoapySDRKwargsList_clear(devs_kwargs, devs_len); // frees entries and struct
    free(devs);

    return 0;
}

int soapy_release_device(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev || !sdr_dev->backend || strcmp(sdr_dev->backend, "soapy")) {
        return -1;
    }

    SoapySDRDevice *device = (SoapySDRDevice *)sdr_dev->device;
    if (!device) {
        return 0;
    }
    sdr_dev->device = NULL;

    fprintf(stderr, "SoapySDRDevice_unmake()...\n");
    return SoapySDRDevice_unmake(device);
}

int soapy_acquire_device(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev || !sdr_dev->backend || strcmp(sdr_dev->backend, "soapy")) {
        return -1;
    }
    if (sdr_dev->device) {
        return 0;
    }

    int r = soapy_device_search(sdr_dev->dev_kwargs, (SoapySDRDevice **)&sdr_dev->device, SOAPY_SDR_TX);
    if (r != 0) {
        fprintf(stderr, "Failed to open sdr device matching '%s'.\n", sdr_dev->dev_kwargs);
        return -1;
    }

    return 0;
}

int soapy_free_device(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev || !sdr_dev->backend || strcmp(sdr_dev->backend, "soapy")) {
        return -1;
    }

    soapy_release_device(sdr_dev);
    sdr_dev->backend = NULL;
    free(sdr_dev->dev_kwargs);
    free(sdr_dev->driver_key);
    free(sdr_dev->hardware_key);
    free(sdr_dev->hardware_info);

    return 0;
}

int soapy_transmit_setup(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx)
{
    if (!tx) return -1;
    if (!sdr_dev) return -1;

    int ret = soapy_acquire_device(sdr_dev);
    if (ret) {
        return ret;
    }

    tx->fullScale            = 0.0;
    char const *nativeFormat = SoapySDRDevice_getNativeStreamFormat(sdr_dev->device, SOAPY_SDR_TX, 0, &tx->fullScale);
    if (!nativeFormat) {
        fprintf(stderr, "No TX capability '%s'.\n", sdr_dev->dev_kwargs);
        return -1;
    }
    size_t format_count;
    char **formats = SoapySDRDevice_getStreamFormats(sdr_dev->device, SOAPY_SDR_TX, 0, &format_count);
    fprintf(stderr, "Supported formats:");
    for (size_t i = 0; i < format_count; ++i) {
        fprintf(stderr, " %s", formats[i]);
    }
    fprintf(stderr, "\n");

    // TODO: allow forced output format
    if (is_format_equal(tx->input_format, SOAPY_SDR_CF32)) {
        tx->output_format = tx->input_format;
    }
    else {
        tx->output_format = nativeFormat;
    }

    return 0;
}

int soapy_transmit(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx)
{
    SoapySDRDevice *dev    = sdr_dev->device;
    SoapySDRStream *stream = NULL;
    uint8_t *txbuf         = {0};
    int r;

    size_t sample_size = SoapySDR_formatToSize(tx->output_format);
    txbuf = malloc(tx->block_size * sample_size);
    if (!txbuf) {
        perror("malloc txbuf");
        exit(EXIT_FAILURE);
    }

    r = soapy_setup_stream(dev, &stream, SOAPY_SDR_TX, tx->output_format);
    if (r != 0) {
        fprintf(stderr, "Failed to setup sdr stream '%s'.\n", tx->output_format);
        goto out;
    }

    fprintf(stderr, "Using input format: %s (output format %s)\n", tx->input_format, tx->output_format);

    if (tx->antenna && *tx->antenna) {
        char *ant = SoapySDRDevice_getAntenna(dev, SOAPY_SDR_TX, 0);
        fprintf(stderr, "Antenna was: %s\n", ant);
        r = SoapySDRDevice_setAntenna(dev, SOAPY_SDR_TX, 0, tx->antenna);
        if (r != 0)
            fprintf(stderr, "SoapySDRDevice_setAntenna: %s (%d)\n", SoapySDR_errToStr(r), r);
        tx->antenna = SoapySDRDevice_getAntenna(dev, SOAPY_SDR_TX, 0);
        fprintf(stderr, "Antenna set to: %s\n", tx->antenna);
    }

    if (tx->master_clock_rate != 0.0) {
        double clk = SoapySDRDevice_getMasterClockRate(dev);
        fprintf(stderr, "MasterClockRate was: %.0f\n", clk);
        r = SoapySDRDevice_setMasterClockRate(dev, tx->master_clock_rate);
        if (r != 0)
            fprintf(stderr, "SoapySDRDevice_setMasterClockRate: %s (%d)\n", SoapySDR_errToStr(r), r);
        tx->master_clock_rate = SoapySDRDevice_getMasterClockRate(dev);
        fprintf(stderr, "MasterClockRate set to: %.0f\n", tx->master_clock_rate);
    }

    if (tx->bandwidth != 0.0) {
        double bw = SoapySDRDevice_getBandwidth(dev, SOAPY_SDR_TX, 0);
        fprintf(stderr, "Bandwidth was: %.0f\n", bw);
        r = SoapySDRDevice_setBandwidth(dev, SOAPY_SDR_TX, 0, tx->bandwidth);
        if (r != 0)
            fprintf(stderr, "SoapySDRDevice_setBandwidth: %s (%d)\n", SoapySDR_errToStr(r), r);
        tx->bandwidth = SoapySDRDevice_getBandwidth(dev, SOAPY_SDR_TX, 0);
        fprintf(stderr, "Bandwidth set to: %.0f\n", tx->bandwidth);
    }

    /* At SoapySDRDevice_setSampleRate the PlutoSDR will blast out garbage for 1.5s at full gain. */
    /* tune away and wait */
    soapy_set_frequency(dev, SOAPY_SDR_TX, 3e9);

    /* Set the sample rate */
    soapy_set_sample_rate(dev, SOAPY_SDR_TX, tx->sample_rate);

    fprintf(stderr, "Waiting for TX to settle...\n");
    sleep(1);

    /* note: needs sample rate set */
    bool hasHwTime = SoapySDRDevice_hasHardwareTime(dev, "");
    fprintf(stderr, "SoapySDRDevice_hasHardwareTime: %d\n", hasHwTime);
    long long hwTime = SoapySDRDevice_getHardwareTime(dev, "");
    fprintf(stderr, "SoapySDRDevice_getHardwareTime: %lld\n", hwTime);

    /* Set the center frequency */
    soapy_set_frequency(dev, SOAPY_SDR_TX, tx->center_frequency);

    soapy_ppm_set(dev, tx->ppm_error);

    soapy_gain_str_set(dev, "0");

    fprintf(stderr, "Writing samples in sync mode...\n");
    SoapySDRKwargs args = {0};
    r                   = SoapySDRDevice_activateStream(dev, stream, 0, 0, 0);
    if (r != 0) {
        fprintf(stderr, "Failed to activate stream\n");
        goto out;
    }

    // TODO: save current gain
    if (tx->gain_str) {
        soapy_gain_str_set(dev, tx->gain_str);
    }

    size_t mtu = SoapySDRDevice_getStreamMTU(dev, stream);
    fprintf(stderr, "Stream MTU: %u\n", (unsigned)mtu);

    size_t n_written = 0;
    int timeouts     = 0;
    while (!tx->flag_abort) {
        const void *buffs[1];
        int flags        = 0;
        long long timeNs = 0;
        long timeoutUs   = 1000000; // 1 second

        size_t n_samps = 0;
        ssize_t n_read = sdr_input_read(sdr_ctx, tx, txbuf, &n_samps, tx->fullScale);

        if (n_read < 0) {
            fprintf(stderr, "Input end\n");
            break; // EOF
        }
        if (n_read == 0) {
            continue; // retry
        }

        //long long hwTime = SoapySDRDevice_getHardwareTime(dev, "");
        //timeNs =  hwTime + (0.001e9); //100ms
        timeNs = 0; //(long long)(n_written * 1e9 / tx->sample_rate);
        flags  = 0; //SOAPY_SDR_HAS_TIME;
        r      = 0; // clean ret should we exit
        for (size_t pos = 0; pos < n_samps && !tx->flag_abort;) {
            buffs[0] = &txbuf[pos * sample_size];

            // flush TX buffer?
            if (n_samps < tx->block_size)
                flags = SOAPY_SDR_END_BURST;
            r = SoapySDRDevice_writeStream(dev, stream, buffs, n_samps - pos, &flags, timeNs, timeoutUs);
            //fprintf(stderr, "writeStream ret=%d (%zu of %zu in %zu), flags=%d, timeNs=%lld\n", r, n_samps - pos, n_samps, tx->block_size, flags, timeNs);
            if (r < 0) {
                break;
            }
            //usleep(r * 1e6 / tx->sample_rate);
            pos += (size_t)r;
        }

        //fprintf(stderr, "last writeStream ret=%d (%zu of %zu), flags=%d, timeNs=%lld\n", r, n_samps, tx->block_size, flags, timeNs);
        if (r >= 0) {
            n_written += n_samps;
            timeouts = 0;
        }
        else {
            if (r == SOAPY_SDR_OVERFLOW) {
                fprintf(stderr, "O");
                fflush(stderr);
                continue;
            }
            if (r == SOAPY_SDR_TIMEOUT) {
                if (++timeouts > 3) {
                    fprintf(stderr, "ERROR: too many timeouts.\n");
                    break;
                }
            }
            fprintf(stderr, "WARNING: sync write failed. %s (%d)\n", SoapySDR_errToStr(r), r);
        }

        size_t channel = 0;
        r              = SoapySDRDevice_readStreamStatus(dev, stream, &channel, &flags, &timeNs, (long)(1e6 / tx->sample_rate * tx->block_size / 2));
        if (r == SOAPY_SDR_NOT_SUPPORTED) {
            r = 0;
        }
        else if (r && r != SOAPY_SDR_TIMEOUT) {
            fprintf(stderr, "readStreamStatus %s (%d), channel=%zu flags=%d, timeNs=%lld\n", SoapySDR_errToStr(r), r, channel, flags, timeNs);
        }
    }
    fprintf(stderr, "%zu samples written\n", n_written);

    // TODO: restore previous gain
    if (tx->gain_str) {
        //verbose_gain_str_set(dev, saved_gain_str);
    }
    soapy_gain_str_set(dev, "0");
    soapy_set_frequency(dev, SOAPY_SDR_TX, 3e9);

    fprintf(stderr, "Waiting for TX to settle...\n");
    sleep(1);

    if (tx->flag_abort)
        fprintf(stderr, "\nUser cancel, exiting...\n");
    else if (r)
        fprintf(stderr, "\nLibrary error %d, exiting...\n", r);

out:
    if (stream) {
        SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
        SoapySDRDevice_closeStream(dev, stream);
    }

    free(txbuf);

    return r >= 0 ? r : -r;
}

int soapy_transmit_done(sdr_cmd_t *tx)
{
    // ...

    return 0;
}
