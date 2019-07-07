/** @file
    tx_tools - tx_lib, common TX functions

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

#include "tx_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "convenience.h"
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#define DEFAULT_BUF_LENGTH (1 * 16384)
#define MINIMAL_BUF_LENGTH 512
#define MAXIMAL_BUF_LENGTH (256 * 16384)

// missing prototypes
SoapySDRDevice **SoapySDRDevice_make_each(SoapySDRKwargs *argsList, const size_t length);
int SoapySDRDevice_unmake_each(SoapySDRDevice **devices, const size_t length);

// helpers

char const *tx_parse_soapy_format(char const *format)
{
    if (strcasecmp(optarg, "CU8") == 0) {
        return SOAPY_SDR_CU8;
    }
    else if (strcasecmp(optarg, "CS8") == 0) {
        return SOAPY_SDR_CS8;
    }
    else if (strcasecmp(optarg, "CS16") == 0) {
        return SOAPY_SDR_CS16;
    }
    else if (strcasecmp(optarg, "CF32") == 0) {
        return SOAPY_SDR_CF32;
    }
    else {
        return NULL;
    }
}

// format is 3-4 chars (plus null), compare as int.
static int is_format_equal(const void *a, const void *b)
{
    return *(const uint32_t *)a == *(const uint32_t *)b;
}

static int check_format(stream_format_t format)
{
    if (format == CU8
            || format == CS8
            || format == CS12
            || format == CS16
            || format == CF32) {
        return 0;
    }
    // unknown format
    fprintf(stderr, "Unknown format \"%.*s\".\n", 4, (char const *)&format);
    return -1;
}

// api

void tx_enum_devices(tx_ctx_t *tx_ctx, const char *enum_args)
{
    if (tx_ctx->devs_len) {
        fprintf(stderr, "device list is in use (%zu).\n", tx_ctx->devs_len);
        exit(1);
    }

    fprintf(stderr, "SoapySDRDevice_enumerateStrArgs(%s)\n", enum_args);
    size_t devs_len = 0;
    SoapySDRKwargs *devs_kwargs = SoapySDRDevice_enumerateStrArgs(enum_args, &devs_len);
    fprintf(stderr, "found %u devices\n", (unsigned)devs_len);
    for (size_t i = 0; i < devs_len; ++i) {
        char *p = SoapySDRKwargs_toString(devs_kwargs + i);
        fprintf(stderr, "%u : %s\n", (unsigned)i, p);
        free(p);
    }

    // make all devices
    fprintf(stderr, "SoapySDRDevice_make_each()...\n");
    SoapySDRDevice **devs = SoapySDRDevice_make_each(devs_kwargs, devs_len);
    SoapySDRKwargsList_clear(devs_kwargs, devs_len);

    fprintf(stderr, "SoapySDRDevice_getDriverKey()...\n");
    for (size_t i = 0; i < devs_len; ++i) {
        if (!devs[i])
            continue;
        char *d_key         = SoapySDRDevice_getDriverKey(devs[i]);
        char *h_key         = SoapySDRDevice_getHardwareKey(devs[i]);
        SoapySDRKwargs info = SoapySDRDevice_getHardwareInfo(devs[i]);
        char *p             = SoapySDRKwargs_toString(&info);
        fprintf(stderr, "%u : %s : %s : %s\n", (unsigned)i, d_key, h_key, p);
        free(d_key);
        free(h_key);
        free(p);
    }

    tx_ctx->devs_len = devs_len;
    tx_ctx->devs = devs;
}

void tx_free_devices(tx_ctx_t *tx_ctx)
{
    fprintf(stderr, "SoapySDRDevice_unmake_each()...\n");
    SoapySDRDevice **devs = (SoapySDRDevice **)tx_ctx->devs;
    SoapySDRDevice_unmake_each(devs, tx_ctx->devs_len);
}

int tx_transmit(tx_ctx_t *tx_ctx, tx_cmd_t *tx)
{
    SoapySDRDevice *dev    = NULL;
    SoapySDRStream *stream = NULL;
    int16_t *buf16         = NULL;
    uint8_t *buf8          = NULL;
    float *fbuf            = NULL; // assumed 32-bit
    double fullScale       = 0.0;
    int r;

    if (!tx->block_size) {
        tx->block_size = DEFAULT_BUF_LENGTH;
    }
    if (tx->block_size < MINIMAL_BUF_LENGTH || tx->block_size > MAXIMAL_BUF_LENGTH) {
        fprintf(stderr, "Output block size wrong value, falling back to default\n");
        fprintf(stderr, "Minimal length: %u\n", MINIMAL_BUF_LENGTH);
        fprintf(stderr, "Maximal length: %u\n", MAXIMAL_BUF_LENGTH);
        tx->block_size = DEFAULT_BUF_LENGTH;
    }

    r = verbose_device_search(tx->dev_query, &dev, SOAPY_SDR_TX);
    if (r != 0) {
        fprintf(stderr, "Failed to open sdr device matching '%s'.\n", tx->dev_query);
        goto out;
    }

    char const *nativeFormat = SoapySDRDevice_getNativeStreamFormat(dev, SOAPY_SDR_TX, 0, &fullScale);
    if (!nativeFormat) {
        fprintf(stderr, "No TX capability '%s'.\n", tx->dev_query);
        goto out;
    }
    size_t format_count;
    char **formats = SoapySDRDevice_getStreamFormats(dev, SOAPY_SDR_TX, 0, &format_count);
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

    buf16 = malloc(tx->block_size * SoapySDR_formatToSize(SOAPY_SDR_CS16));
    if (is_format_equal(tx->input_format, SOAPY_SDR_CS8) || is_format_equal(tx->input_format, SOAPY_SDR_CU8)) {
        buf8 = malloc(tx->block_size * SoapySDR_formatToSize(SOAPY_SDR_CS8));
    }
    else if (is_format_equal(tx->input_format, SOAPY_SDR_CF32)) {
        fbuf = malloc(tx->block_size * SoapySDR_formatToSize(SOAPY_SDR_CF32));
    }
    else if (is_format_equal(tx->input_format, SOAPY_SDR_CS16)) {
        // nothing to do
    }
    else {
        fprintf(stderr, "Unhandled format '%s'.\n", tx->input_format);
        r = -1;
        goto out;
    }

    r = verbose_setup_stream(dev, &stream, SOAPY_SDR_TX, tx->output_format);
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
    verbose_set_frequency(dev, SOAPY_SDR_TX, 3e9);

    /* Set the sample rate */
    verbose_set_sample_rate(dev, SOAPY_SDR_TX, tx->sample_rate);

    fprintf(stderr, "Waiting for TX to settle...\n");
    sleep(1);

    /* note: needs sample rate set */
    bool hasHwTime = SoapySDRDevice_hasHardwareTime(dev, "");
    fprintf(stderr, "SoapySDRDevice_hasHardwareTime: %d\n", hasHwTime);
    long long hwTime = SoapySDRDevice_getHardwareTime(dev, "");
    fprintf(stderr, "SoapySDRDevice_getHardwareTime: %lld\n", hwTime);

    /* Set the center frequency */
    verbose_set_frequency(dev, SOAPY_SDR_TX, tx->center_frequency);

    verbose_ppm_set(dev, tx->ppm_error);

    verbose_gain_str_set(dev, "0");

    fprintf(stderr, "Writing samples in sync mode...\n");
    SoapySDRKwargs args = {0};
    r = SoapySDRDevice_activateStream(dev, stream, 0, 0, 0);
    if (r != 0) {
        fprintf(stderr, "Failed to activate stream\n");
        goto out;
    }

    // TODO: save current gain
    if (tx->gain_str) {
        verbose_gain_str_set(dev, tx->gain_str);
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
        ssize_t n_read   = 0;
        size_t n_samps   = 0, i;

        if (is_format_equal(tx->input_format, SOAPY_SDR_CS16)) {
            n_read  = read(tx->stream_fd, buf16, sizeof(int16_t) * 2 * tx->block_size);
            n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(uint16_t) / 2;
            // The "native" format we read in, write out no conversion needed
            if (fullScale < 32767.0) // actually we expect 32768.0
                for (i = 0; i < n_samps * 2; ++i) {
                    buf16[i] *= fullScale / 32768.0;
                }
        }
        else if (is_format_equal(tx->input_format, SOAPY_SDR_CS8)) {
            n_read  = read(tx->stream_fd, buf8, sizeof(int8_t) * 2 * tx->block_size);
            n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(int8_t) / 2;
            for (i = 0; i < n_samps * 2; ++i) {
                buf16[i] = (int16_t)(((int8_t)buf8[i] + 0.4) / 128.0 * fullScale);
            }
        }
        else if (is_format_equal(tx->input_format, SOAPY_SDR_CU8)) {
            n_read  = read(tx->stream_fd, buf8, sizeof(uint8_t) * 2 * tx->block_size);
            n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(uint8_t) / 2;
            for (i = 0; i < n_samps * 2; ++i) {
                buf16[i] = (int16_t)((buf8[i] + 127.4) / 128.0 * fullScale);
            }
        }
        else if (is_format_equal(tx->input_format, SOAPY_SDR_CF32)) {
            n_read  = read(tx->stream_fd, fbuf, sizeof(float) * 2 * tx->block_size);
            n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(float) / 2;
            if (!is_format_equal(tx->output_format, SOAPY_SDR_CF32))
                for (i = 0; i < n_samps * 2; ++i) {
                    buf16[i] = (int16_t)(fbuf[i] / 1.0f * (float)fullScale);
                }
        }
        else {
            fprintf(stderr, "Unsupported input format: %s (output format: %s)\n", tx->input_format, tx->output_format);
            r = -1;
            goto out;
        }
        //fprintf(stderr, "Input was %zd bytes %zu samples\n", n_read, n_samps);
        if (n_read == -1) {
            if (tx->stream_fd == fileno(stdin)) {
                fprintf(stderr, "Pipe end?\n");
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "Input read underflow.\n");
                continue; // non-blocking
            }
            else {
                fprintf(stderr, "Input read error (%d)\n", errno);
                break; //error
            }
        }
        if (n_read == 0) {
            if (tx->loops) {
                lseek(tx->stream_fd, 0, SEEK_SET);
                tx->loops--;
            }
            else {
                break; // EOF
            }
        }

        if (tx->samples_to_write > 0)
            tx->samples_to_write -= n_samps;

        if ((tx->samples_to_write > 0) && (tx->samples_to_write < (uint32_t)n_samps)) {
            n_samps = tx->samples_to_write;
            tx->flag_abort = 1;
        }

        //long long hwTime = SoapySDRDevice_getHardwareTime(dev, "");
        //timeNs =  hwTime + (0.001e9); //100ms
        timeNs = 0; //(long long)(n_written * 1e9 / tx->sample_rate);
        flags  = 0; //SOAPY_SDR_HAS_TIME;
        r      = 0; // clean ret should we exit
        for (size_t pos = 0; pos < n_samps && !tx->flag_abort;) {
            if (is_format_equal(tx->output_format, SOAPY_SDR_CF32))
                buffs[0] = &fbuf[2 * pos];
            else
                buffs[0] = &buf16[2 * pos];

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
        r = SoapySDRDevice_readStreamStatus(dev, stream, &channel, &flags, &timeNs, (long)(1e6 / tx->sample_rate * tx->block_size / 2));
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
    verbose_gain_str_set(dev, "0");
    verbose_set_frequency(dev, SOAPY_SDR_TX, 3e9);

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

    if (dev)
        SoapySDRDevice_unmake(dev);

    free(buf16);
    free(buf8);
    free(fbuf);

    return r >= 0 ? r : -r;
}
