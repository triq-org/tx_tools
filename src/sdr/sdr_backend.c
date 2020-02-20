/** @file
    tx_tools - SDR backends.

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

#include "sdr_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#define DEFAULT_BUF_LENGTH (1 * 16384)
#define MINIMAL_BUF_LENGTH 512
#define MAXIMAL_BUF_LENGTH (256 * 16384)

// format is 3-4 chars (plus null), compare as int.
static int is_format_equal(const void *a, const void *b)
{
    return *(const uint32_t *)a == *(const uint32_t *)b;
}

char const *sdr_ctx_available_backends()
{
    return ""
#ifdef HAS_IIO
           "Pluto (iio"
#ifdef HAS_AD9361_IIO
           " ad9361"
#endif
           ") "
#endif
#ifdef HAS_LIME
           "Lime "
#endif
#ifdef HAS_SOAPY
           "SoapySDR"
#endif
            ;
}

int sdr_ctx_enum_devices(sdr_ctx_t *sdr_ctx, const char *enum_args)
{
    int ret = -1;

    if (!sdr_ctx->devs) {
        // TODO: use a dynamic list
        sdr_ctx->devs = calloc(64, sizeof(sdr_dev_t));
    }

#ifdef HAS_IIO
    if (!strncmp(enum_args, "pluto:", 6)) {
        return pluto_enum_devices(sdr_ctx, enum_args);
    }
#endif
#ifdef HAS_LIME
    if (!strncmp(enum_args, "lime:", 5)) {
        return lime_enum_devices(sdr_ctx, enum_args);
    }
#endif
#ifdef HAS_SOAPY
    ret = soapy_enum_devices(sdr_ctx, enum_args);
#endif

    return ret;
}

int sdr_ctx_release_devices(sdr_ctx_t *sdr_ctx)
{
    int ret = 0;

    for (size_t i = 0; i < sdr_ctx->devs_len; i++) {
        sdr_dev_t *sdr_dev = &sdr_ctx->devs[i];
        ret = sdr_dev_release(sdr_dev);
    }

    return ret;
}

int sdr_ctx_free_devices(sdr_ctx_t *sdr_ctx)
{
    int ret = 0;

    for (size_t i = 0; i < sdr_ctx->devs_len; i++) {
        sdr_dev_t *sdr_dev = &sdr_ctx->devs[i];
        ret = sdr_dev_free(sdr_dev);
    }

    free(sdr_ctx->devs);
    sdr_ctx->devs = NULL;

    return ret;
}

int sdr_dev_free(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev) return -1;
    if (!sdr_dev->backend) return -1;

    int ret = -1;

#ifdef HAS_IIO
    if (!strcmp(sdr_dev->backend, "pluto")) {
        return pluto_free_device(sdr_dev);
    }
#endif
#ifdef HAS_LIME
    if (!strcmp(sdr_dev->backend, "lime")) {
        return lime_free_device(sdr_dev);
    }
#endif
#ifdef HAS_SOAPY
    if (!strcmp(sdr_dev->backend, "soapy")) {
        ret = soapy_free_device(sdr_dev);
    }
#endif

    return ret;
}

int sdr_dev_release(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev) return -1;
    if (!sdr_dev->backend) return -1;

    int ret = -1;

#ifdef HAS_IIO
    if (!strcmp(sdr_dev->backend, "pluto")) {
        return pluto_release_device(sdr_dev);
    }
#endif
#ifdef HAS_LIME
    if (!strcmp(sdr_dev->backend, "lime")) {
        return lime_release_device(sdr_dev);
    }
#endif
#ifdef HAS_SOAPY
    if (!strcmp(sdr_dev->backend, "soapy")) {
        ret = soapy_release_device(sdr_dev);
    }
#endif

    return ret;
}

int sdr_dev_acquire(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev) return -1;
    if (!sdr_dev->backend) return -1;

    int ret = -1;

#ifdef HAS_IIO
    if (!strcmp(sdr_dev->backend, "pluto")) {
        return pluto_acquire_device(sdr_dev);
    }
#endif
#ifdef HAS_LIME
    if (!strcmp(sdr_dev->backend, "lime")) {
        return lime_acquire_device(sdr_dev);
    }
#endif
#ifdef HAS_SOAPY
    if (!strcmp(sdr_dev->backend, "soapy")) {
        ret = soapy_acquire_device(sdr_dev);
    }
#endif

    return ret;
}

sdr_dev_t *sdr_ctx_find_device(sdr_ctx_t *sdr_ctx, char const *kwargs)
{
    if (!sdr_ctx) return NULL;
    if (!kwargs) kwargs = "";
    size_t len = strlen(kwargs);

    for (size_t i = 0; i < sdr_ctx->devs_len; i++) {
        sdr_dev_t *info = &sdr_ctx->devs[i];
        if (info && info->dev_kwargs && !strncmp(info->dev_kwargs, kwargs, len)) {
            return info;
        }
    }
    fprintf(stderr, "Device query not found: %s\n", kwargs);
    return NULL;
}

int sdr_tx_setup(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx)
{
    if (!sdr_ctx) return -1;
    if (!tx) return -1;
    sdr_dev_t *sdr_dev = sdr_ctx_find_device(sdr_ctx, tx->dev_query);
    if (!sdr_dev) return -1;

    if (!tx->block_size) {
        tx->block_size = DEFAULT_BUF_LENGTH;
    }
    if (tx->block_size < MINIMAL_BUF_LENGTH || tx->block_size > MAXIMAL_BUF_LENGTH) {
        fprintf(stderr, "Output block size wrong value, falling back to default\n");
        fprintf(stderr, "Minimal length: %d\n", MINIMAL_BUF_LENGTH);
        fprintf(stderr, "Maximal length: %d\n", MAXIMAL_BUF_LENGTH);
        tx->block_size = DEFAULT_BUF_LENGTH;
    }

    int ret = -1;

#ifdef HAS_IIO
    if (!strcmp(sdr_dev->backend, "pluto")) {
        return pluto_transmit_setup(sdr_ctx, sdr_dev, tx);
    }
#endif
#ifdef HAS_LIME
    if (!strcmp(sdr_dev->backend, "lime")) {
        return lime_transmit_setup(sdr_ctx, sdr_dev, tx);
    }
#endif
#ifdef HAS_SOAPY
    ret = soapy_transmit_setup(sdr_ctx, sdr_dev, tx);
#endif

    return ret;
}

int sdr_tx(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx)
{
    if (!sdr_ctx) return -1;
    if (!tx) return -1;
    sdr_dev_t *sdr_dev = sdr_ctx_find_device(sdr_ctx, tx->dev_query);
    if (!sdr_dev) return -1;

    int ret = -1;

#ifdef HAS_IIO
    if (!strcmp(sdr_dev->backend, "pluto")) {
        return pluto_transmit(sdr_ctx, sdr_dev, tx);
    }
#endif
#ifdef HAS_LIME
    if (!strcmp(sdr_dev->backend, "lime")) {
        return lime_transmit(sdr_ctx, sdr_dev, tx);
    }
#endif
#ifdef HAS_SOAPY
    ret = soapy_transmit(sdr_ctx, sdr_dev, tx);
#endif

    return ret;
}

int sdr_tx_free(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx)
{
    if (!tx) return -1;
    sdr_dev_t *sdr_dev = sdr_ctx_find_device(sdr_ctx, tx->dev_query);
    if (!sdr_dev) return -1;

    int ret = -1;

#ifdef HAS_IIO
    if (!strcmp(sdr_dev->backend, "pluto")) {
        return pluto_transmit_done(tx);
    }
#endif
#ifdef HAS_LIME
    if (!strcmp(sdr_dev->backend, "lime")) {
        return lime_transmit_done(tx);
    }
#endif
#ifdef HAS_SOAPY
    ret = soapy_transmit_done(tx);
#endif

    return ret;

    //free(tx->dev_query);
    //free(tx->gain_str);
    //free(tx->antenna);
    //free(tx->output_format);
    //free(tx->input_format);
}

// input processing

int sdr_input_reset(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx)
{
    if (tx->stream_fd >= 0) {
        lseek(tx->stream_fd, 0, SEEK_SET);
    }
    else {
        tx->buffer_offset = 0;
    }

    return 0;
}

ssize_t sdr_input_read(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx, void *buf, size_t *out_samps, double fullScale)
{
    ssize_t n_read = 0;
    size_t n_samps = 0;

    n_read = sdr_input_try_read(sdr_ctx, tx, buf, &n_samps, fullScale);
    if (n_read == -2) {
        *out_samps = 0;
        return -3; // format error
    }
    //fprintf(stderr, "Input was %zd bytes %zu samples\n", n_read, n_samps);
    if (n_read == -1) {
        if (tx->stream_fd == fileno(stdin)) {
            fprintf(stderr, "Pipe end?\n");
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "Input read underflow.\n");
            *out_samps = 0;
            return 0; // non-blocking
        }
        else {
            fprintf(stderr, "Input read error (%d)\n", errno);
        }
        *out_samps = 0;
        return -2; // read error
    }
    if (n_read == 0) {
        if (tx->loops) {
            sdr_input_reset(sdr_ctx, tx);
            // TODO: render loop_delay silence here
            tx->loops--;
        }
        else {
            *out_samps = 0;
            return -1; // EOF
        }
    }

    // else n_read > 0
    if (tx->samples_to_write > n_samps) {
        tx->samples_to_write -= n_samps;
    }
    else if (tx->samples_to_write > 0) {
        n_samps = tx->samples_to_write;
        tx->samples_to_write = 0;
        tx->flag_abort = 1;
    }
    *out_samps = n_samps;
    return n_read;
}

ssize_t sdr_input_try_read(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx, void *buf, size_t *out_samps, double fullScale)
{
    if (!is_format_equal(tx->output_format, "CS16")) {
        // TODO: support other output format besides CS16
        fprintf(stderr, "Unsupported output format: %s (input format: %s)\n", tx->output_format, tx->input_format);
        return -2;
    }

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

    if (is_format_equal(tx->input_format, "CS16")) {
        n_read  = read(tx->stream_fd, buf, sizeof(int16_t) * 2 * tx->block_size);
        n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(uint16_t) / 2;
        // The "native" format we read in, write out no conversion needed
        if (fullScale >= 2047.0 && fullScale <= 2048.0) {
            // Quick and dirty, so -1 (0xFFFF) to -15 (0xFFF1) scale down to -1 instead of 0
            for (size_t i = 0; i < n_samps * 2; ++i) {
                ((int16_t *)buf)[i] >>= 4;
            }
        }
        else if (fullScale < 32767.0) { // actually we expect 32768.0
            for (size_t i = 0; i < n_samps * 2; ++i) {
                ((int16_t *)buf)[i] *= fullScale / 32768.0;
            }
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
            ((int16_t *)buf)[i] = (int16_t)((tx->conv_buf.u8[i] - 127.4) / 128.0 * fullScale);
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
