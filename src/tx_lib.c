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

#include "pulse_parse.h"
#include "code_parse.h"
#include "iq_render.h"

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

int tx_valid_input_format(char const *format)
{
    // we support all current formats as input
    return sample_format_for(format) != FORMAT_NONE;
}


int tx_valid_output_format(char const *format)
{
    // we support all current formats as output
    return sample_format_for(format) != FORMAT_NONE;
}

char const *tx_parse_sample_format(char const *format)
{
    if (strcasecmp(optarg, "CU8") == 0)
        return "CU8";
    else if (strcasecmp(optarg, "CS8") == 0)
        return "CS8";
    else if (strcasecmp(optarg, "CS12") == 0)
        return "CS12";
    else if (strcasecmp(optarg, "CS16") == 0)
        return "CS16";
    else if (strcasecmp(optarg, "CS32") == 0)
        return "CS32";
    else if (strcasecmp(optarg, "CF32") == 0)
        return "CF32";
    else if (strcasecmp(optarg, "CF64") == 0)
        return "CF64";
    else
        return NULL;
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
            || format == CS32
            || format == CF32
            || format == CF64) {
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

    dev_info_t *dev_infos = calloc(devs_len, sizeof(dev_info_t));

    for (size_t i = 0; i < devs_len; ++i) {
        char *p = SoapySDRKwargs_toString(devs_kwargs + i);
        fprintf(stderr, "%u : %s\n", (unsigned)i, p);
        dev_infos[i].dev_kwargs = p;
    }

    // make all devices
    fprintf(stderr, "SoapySDRDevice_make_each()...\n");
    SoapySDRDevice **devs = SoapySDRDevice_make_each(devs_kwargs, devs_len);
    SoapySDRKwargsList_clear(devs_kwargs, devs_len); // frees entries and struct

    fprintf(stderr, "SoapySDRDevice_getDriverKey()...\n");
    for (size_t i = 0; i < devs_len; ++i) {
        if (!devs[i])
            continue;
        char *d_key         = SoapySDRDevice_getDriverKey(devs[i]);
        char *h_key         = SoapySDRDevice_getHardwareKey(devs[i]);
        SoapySDRKwargs info = SoapySDRDevice_getHardwareInfo(devs[i]);
        char *p             = SoapySDRKwargs_toString(&info);
        fprintf(stderr, "%u : %s : %s : %s\n", (unsigned)i, d_key, h_key, p);
        dev_infos[i].driver_key    = d_key;
        dev_infos[i].hardware_key  = h_key;
        dev_infos[i].hardware_info = p;
    }

    tx_ctx->devs_len = devs_len;
    tx_ctx->devs = devs;
    tx_ctx->dev_infos = dev_infos;
}

void tx_free_devices(tx_ctx_t *tx_ctx)
{
    for (size_t i = 0; i < tx_ctx->devs_len; ++i) {
        free(tx_ctx->dev_infos[i].dev_kwargs);
        free(tx_ctx->dev_infos[i].driver_key);
        free(tx_ctx->dev_infos[i].hardware_key);
        free(tx_ctx->dev_infos[i].hardware_info);
    }
    free(tx_ctx->dev_infos);

    fprintf(stderr, "SoapySDRDevice_unmake_each()...\n");
    SoapySDRDevice **devs = (SoapySDRDevice **)tx_ctx->devs;
    SoapySDRDevice_unmake_each(devs, tx_ctx->devs_len);
}

int tx_transmit(tx_ctx_t *tx_ctx, tx_cmd_t *tx)
{
    SoapySDRDevice *dev    = NULL;
    SoapySDRStream *stream = NULL;
    frame_t txbuf          = {0};
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

    txbuf.s16 = malloc(tx->block_size * SoapySDR_formatToSize(SOAPY_SDR_CS16));
    if (!txbuf.s16) {
        perror("tx_input_init");
        exit(EXIT_FAILURE);
    }
    r = tx_input_init(tx_ctx, tx);
    if (r) {
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

        n_read = tx_input_read(tx_ctx, tx, txbuf.s16, &n_samps, fullScale);
        if (n_read == -2) {
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
                tx_input_reset(tx_ctx, tx);
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
                buffs[0] = &txbuf.f32[2 * pos];
            else
                buffs[0] = &txbuf.s16[2 * pos];

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

    free(txbuf.s16);

    return r >= 0 ? r : -r;
}

void tx_print(tx_ctx_t *tx_ctx, tx_cmd_t *tx)
{
    printf("TX command:\n");
    printf("  device selection\n");
    printf("    dev_query=\"%s\"\n", tx->dev_query);
    printf("  device setup\n");
    printf("    gain_str=\"%s\"\n", tx->gain_str);
    printf("    antenna=\"%s\"\n", tx->antenna);
    printf("    channel=%zu\n", tx->channel);
    printf("  rf setup\n");
    printf("    ppm_error=%f\n", tx->ppm_error);
    printf("    center_frequency=%f\n", tx->center_frequency);
    printf("    sample_rate=%f\n", tx->sample_rate);
    printf("    bandwidth=%f\n", tx->bandwidth);
    printf("    master_clock_rate=%f\n", tx->master_clock_rate);
    printf("    output_format=\"%s\"\n", tx->output_format);
    printf("    block_size=%zu\n", tx->block_size);
    printf("  transmit control\n");
    printf("    initial_delay=%u\n", tx->initial_delay);
    printf("    repeats=%u\n", tx->repeats);
    printf("    repeat_delay=%u\n", tx->repeat_delay);
    printf("    loops=%u\n", tx->loops);
    printf("    loop_delay=%u\n", tx->loop_delay);
    printf("  input from file descriptor\n");
    printf("    input_format=\"%s\"\n", tx->input_format);
    printf("    stream_fd=%i\n", tx->stream_fd);
    printf("    samples_to_write=%zu\n", tx->samples_to_write);
    printf("  input from buffer\n");
    printf("    stream_buffer=%p\n", tx->stream_buffer);
    printf("    buffer_size=%zu\n", tx->buffer_size);
    printf("  input from text\n");
    printf("    freq_mark=%i\n", tx->freq_mark);
    printf("    freq_space=%i\n", tx->freq_space);
    printf("    att_mark=%i\n", tx->att_mark);
    printf("    att_space=%i\n", tx->att_space);
    printf("    phase_mark=%i\n", tx->phase_mark);
    printf("    phase_space=%i\n", tx->phase_space);
    printf("    pulses=\"%s\"\n", tx->pulses);
}

// input processing

int tx_input_init(tx_ctx_t *tx_ctx, tx_cmd_t *tx)
{
    // unpack pulses if needed

    if (tx->pulses) {
        iq_render_t iq_render = {0};
        iq_render_defaults(&iq_render);
        iq_render.sample_rate = tx->sample_rate;
        iq_render.sample_format = sample_format_for(tx->output_format);

        pulse_setup_t pulse_setup = {0};
        pulse_setup_defaults(&pulse_setup, "OOK");
        pulse_setup.freq_mark = tx->freq_mark;
        pulse_setup.freq_space = tx->freq_space;
        pulse_setup.att_mark = tx->att_mark;
        pulse_setup.att_space = tx->att_space;
        pulse_setup.phase_mark = tx->phase_mark;
        pulse_setup.phase_space = tx->phase_space;

        tone_t *tones = parse_pulses(tx->pulses, &pulse_setup);
        output_pulses(tones); // debug

        iq_render_buf(&iq_render, tones, &tx->stream_buffer, &tx->buffer_size);
        free(tones);

        return 0;
    }

    // otherwise: setup stream conversion

    if (!tx_valid_input_format(tx->input_format)) {
        fprintf(stderr, "Unhandled input format '%s'.\n", tx->input_format);
        return -1;
    }
    if (!tx_valid_output_format(tx->output_format)) {
        fprintf(stderr, "Unhandled output format '%s'.\n", tx->output_format);
        return -1;
    }

    if (!is_format_equal(tx->input_format, tx->output_format)) {
        size_t elem_size = sample_format_length(sample_format_for(tx->input_format));
        tx->conv_buf.u8  = malloc(tx->block_size * elem_size);
        if (!tx->conv_buf.u8) {
            perror("tx_input_init");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

int tx_input_destroy(tx_ctx_t *tx_ctx, tx_cmd_t *tx)
{
    free(tx->stream_buffer);

    free(tx->conv_buf.u8);

    return 0;
}

int tx_input_reset(tx_ctx_t *tx_ctx, tx_cmd_t *tx)
{
    if (tx->stream_fd >= 0) {
        lseek(tx->stream_fd, 0, SEEK_SET);
    }
    else {
        tx->buffer_offset = 0;
    }

    return 0;
}

ssize_t tx_input_read(tx_ctx_t *tx_ctx, tx_cmd_t *tx, void *buf, size_t *out_samps, double fullScale)
{
    // read from buffer

    if (!tx->stream_fd) {
        size_t n_read = sizeof(int16_t) * 2 * tx->block_size;
        if (n_read > tx->block_size - tx->buffer_offset)
            n_read = tx->block_size - tx->buffer_offset;

        memcpy(buf, (uint8_t *)(tx->stream_buffer) + tx->buffer_offset, n_read);
        tx->buffer_offset += n_read;

        *out_samps = (size_t)n_read / sizeof(uint16_t) / 2;
        return (ssize_t)n_read;
    }

    // read from stream

    ssize_t n_read;
    size_t n_samps;

    // FIXME: this still assumes CS16 output...
    if (is_format_equal(tx->input_format, "CS16")) {
        n_read  = read(tx->stream_fd, buf, sizeof(int16_t) * 2 * tx->block_size);
        n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(uint16_t) / 2;
        // The "native" format we read in, write out no conversion needed
        if (fullScale < 32767.0) // actually we expect 32768.0
            for (size_t i = 0; i < n_samps * 2; ++i) {
                ((int16_t *)buf)[i] *= fullScale / 32768.0;
            }
    }
    else if (is_format_equal(tx->input_format, "CS8")) {
        n_read  = read(tx->stream_fd, tx->conv_buf.u8, sizeof(int8_t) * 2 * tx->block_size);
        n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(int8_t) / 2;
        for (size_t i = 0; i < n_samps * 2; ++i) {
            ((int16_t *)buf)[i] = (int16_t)((tx->conv_buf.s8[i] + 0.4) / 128.0 * fullScale);
        }
    }
    else if (is_format_equal(tx->input_format, "CU8")) {
        n_read  = read(tx->stream_fd, tx->conv_buf.u8, sizeof(uint8_t) * 2 * tx->block_size);
        n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(uint8_t) / 2;
        for (size_t i = 0; i < n_samps * 2; ++i) {
            ((int16_t *)buf)[i] = (int16_t)((tx->conv_buf.u8[i] + 127.4) / 128.0 * fullScale);
        }
    }
    else if (is_format_equal(tx->input_format, "CF32")) {
        n_read  = read(tx->stream_fd, tx->conv_buf.u8, sizeof(float) * 2 * tx->block_size);
        n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(float) / 2;
        if (!is_format_equal(tx->output_format, "CF32"))
            for (size_t i = 0; i < n_samps * 2; ++i) {
                ((int16_t *)buf)[i] = (int16_t)(tx->conv_buf.f32[i] / 1.0f * (float)fullScale);
            }
    }
    else {
        fprintf(stderr, "Unsupported input format: %s (output format: %s)\n", tx->input_format, tx->output_format);
        return -2;
    }

    *out_samps = n_samps;
    return n_read;
}
