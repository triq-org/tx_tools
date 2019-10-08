/** @file
    tx_tools - iq_render, render tone date to I/Q data.

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

#include "iq_render.h"
#include "common.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#ifdef _MSC_VER
#define F_OK 0
#endif
#endif
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <time.h>

#include "nco.h"

int abort_render = 0;

// render context

typedef struct ctx ctx_t;

typedef void (*signal_out_fn)(ctx_t *ctx, double i, double q);

struct ctx {
    double sample_rate;
    double noise_floor;  ///< peak-to-peak (-19 dB)
    double noise_signal; ///< peak-to-peak (-25 dB)
    double gain;         ///< sine-peak (-0 dB)

    enum sample_format sample_format;
    double full_scale;
    size_t frame_size;

    size_t frame_len;
    size_t frame_pos;
    frame_t frame;
    int fd;

    signal_out_fn signal_out;

    int g_db;     ///< continuous db
    double g_hz;  ///< continuous freq
    uint32_t phi; ///< continuous phase

    double filter_out[100];
    double filter_in[100];
    size_t filter_len;
};

// helper

static double noise_pp_level(double level)
{
    if (level < 0)
        level = pow(10.0, 1.0 / 20.0 * level);
    // correct for RMS to equal a sine
    return level * 2 * sqrt(1.0 / 2.0 * 3.0 / 2.0);
}

static double sine_pk_level(double level)
{
    if (level <= 0)
        level = pow(10.0, 1.0 / 20.0 * level);
    return level;
}

static double randf(void)
{
    // hotspot:
    return (double)rand() / RAND_MAX;
}

static int bound_u8(int x)
{
    return x < 0 ? 0 : x > 0xff ? 0xff : x;
}

static int bound_s8(int x)
{
    return x < -0x80 ? -0x80 : x > 0x7f ? 0x7f : x;
}

static int bound_u16(int x)
{
    return x < 0 ? 0 : x > 0xffff ? 0xffff : x;
}

static int bound_s16(int x)
{
    return x < -0x8000 ? -0x8000 : x > 0x7fff ? 0x7fff : x;
}

static int32_t bound_s32(double x)
{
    return x < -0x7fffffff ? -0x7fffffff : x > 0x7fffffff ? 0x7fffffff : (int32_t)x;
}

static int64_t bound_s64(double x)
{
    return x < -0x7fffffffffffffff ? -0x7fffffffffffffff : x > 0x7fffffffffffffff ? 0x7fffffffffffffff : (int64_t)x;
}

// inlines

static void signal_out_cu8(ctx_t *ctx, double i, double q)
{
    uint8_t i8 = (uint8_t)bound_u8((int)((i + 1.0) * ctx->full_scale));
    uint8_t q8 = (uint8_t)bound_u8((int)((q + 1.0) * ctx->full_scale));
    ctx->frame.u8[ctx->frame_pos++] = i8;
    ctx->frame.u8[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(uint8_t);
    if (ctx->frame_len >= ctx->frame_size) {
        write(ctx->fd, ctx->frame.u8, ctx->frame_size);
        ctx->frame_pos = ctx->frame_len = 0;
    }
}

static void signal_out_cs8(ctx_t *ctx, double i, double q)
{
    int8_t i8 = (int8_t)bound_s8((int)(i * ctx->full_scale));
    int8_t q8 = (int8_t)bound_s8((int)(q * ctx->full_scale));
    ctx->frame.s8[ctx->frame_pos++] = i8;
    ctx->frame.s8[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(int8_t);
    if (ctx->frame_len >= ctx->frame_size) {
        write(ctx->fd, ctx->frame.u8, ctx->frame_size);
        ctx->frame_pos = ctx->frame_len = 0;
    }
}

static void signal_out_cs12(ctx_t *ctx, double i, double q)
{
    int16_t i8 = (int16_t)bound_s16((int)(i * ctx->full_scale));
    int16_t q8 = (int16_t)bound_s16((int)(q * ctx->full_scale));
    // produce 24 bit (iiqIQQ), note the input is LSB aligned, scale=2048
    // note: byte0 = i[7:0]; byte1 = {q[3:0], i[11:8]}; byte2 = q[11:4];
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)(i8);
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)((q8 << 4) | ((i8 >> 8) & 0x0f));
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)(q8 >> 4);
    ctx->frame_len += 3 * sizeof(uint8_t);
    // NOTE: frame_size needs to be a multiple of 3!
    if (ctx->frame_len >= ctx->frame_size) {
        write(ctx->fd, ctx->frame.u8, ctx->frame_size);
        ctx->frame_pos = ctx->frame_len = 0;
    }
}

static void signal_out_cs16(ctx_t *ctx, double i, double q)
{
    int16_t i8 = (int16_t)bound_s16((int)(i * ctx->full_scale));
    int16_t q8 = (int16_t)bound_s16((int)(q * ctx->full_scale));
    ctx->frame.s16[ctx->frame_pos++] = i8;
    ctx->frame.s16[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(int16_t);
    if (ctx->frame_len >= ctx->frame_size) {
        write(ctx->fd, ctx->frame.u8, ctx->frame_size);
        ctx->frame_pos = ctx->frame_len = 0;
    }
}

static void signal_out_cs32(ctx_t *ctx, double i, double q)
{
    int32_t i8 = bound_s32(i * ctx->full_scale);
    int32_t q8 = bound_s32(q * ctx->full_scale);
    ctx->frame.s32[ctx->frame_pos++] = i8;
    ctx->frame.s32[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(int32_t);
    if (ctx->frame_len >= ctx->frame_size) {
        write(ctx->fd, ctx->frame.u8, ctx->frame_size);
        ctx->frame_pos = ctx->frame_len = 0;
    }
}

static void signal_out_cs64(ctx_t *ctx, double i, double q)
{
    int64_t i8 = bound_s64(i * ctx->full_scale);
    int64_t q8 = bound_s64(q * ctx->full_scale);
    ctx->frame.s64[ctx->frame_pos++] = i8;
    ctx->frame.s64[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(int64_t);
    if (ctx->frame_len >= ctx->frame_size) {
        write(ctx->fd, ctx->frame.u8, ctx->frame_size);
        ctx->frame_pos = ctx->frame_len = 0;
    }
}

static void signal_out_cf32(ctx_t *ctx, double i, double q)
{
    ctx->frame.f32[ctx->frame_pos++] = (float)(i * ctx->full_scale);
    ctx->frame.f32[ctx->frame_pos++] = (float)(q * ctx->full_scale);
    ctx->frame_len += 2 * sizeof(float);
    if (ctx->frame_len >= ctx->frame_size) {
        write(ctx->fd, ctx->frame.u8, ctx->frame_size);
        ctx->frame_pos = ctx->frame_len = 0;
    }
}

static void signal_out_cf64(ctx_t *ctx, double i, double q)
{
    ctx->frame.f64[ctx->frame_pos++] = (double)(i * ctx->full_scale);
    ctx->frame.f64[ctx->frame_pos++] = (double)(q * ctx->full_scale);
    ctx->frame_len += 2 * sizeof(double);
    if (ctx->frame_len >= ctx->frame_size) {
        write(ctx->fd, ctx->frame.u8, ctx->frame_size);
        ctx->frame_pos = ctx->frame_len = 0;
    }
}

static void signal_out_flush(ctx_t *ctx)
{
    write(ctx->fd, ctx->frame.u8, ctx->frame_len);
    ctx->frame_pos = ctx->frame_len = 0;
}

static signal_out_fn format_out[] = {signal_out_cu8, signal_out_cu8, signal_out_cs8, signal_out_cs12, signal_out_cs16, signal_out_cs32, signal_out_cs64, signal_out_cf32, signal_out_cf64};
static double scale_defaults[]    = {127.5, 127.5, 127.5, 2047.5, 32767.5, 2147483647.5, 9223372036854775999.5, 1.0, 1.0};

// signal gen

static void add_noise(ctx_t *ctx, size_t time_us, int db)
{
    size_t end =(size_t)(time_us * ctx->sample_rate / 1000000.0);
    for (size_t t = 0; t < end; ++t) {
        double x = (randf() - 0.5) * ctx->noise_floor;
        double y = (randf() - 0.5) * ctx->noise_floor;
        ctx->signal_out(ctx, x, y);
    }
}

static void init_filter(ctx_t *ctx, size_t time_us)
{
    ctx->filter_len = (size_t)(time_us * ctx->sample_rate / 1000000.0);
    //uint32_t l_phi = 0;
    //uint32_t d_phi = nco_d_phase(1000000 / 2 / time_us, (size_t)ctx->sample_rate);
    for (size_t t = 0; t < ctx->filter_len; ++t) {
        ctx->filter_out[t] = (ctx->filter_len - t) / (double)ctx->filter_len;
        ctx->filter_in[t]  = t / (double)ctx->filter_len;

        //ctx->filter_out[t] = (nco_cos(l_phi) + 1) / 2;
        //printf("at %2d : %f\n", t, ctx->filter_out[t]);
        //ctx->filter_in[t]  = 1.0 - ctx->filter_out[t];
        //l_phi += d_phi;
    }
}

static void add_sine(ctx_t *ctx, double freq_hz, size_t time_us, int db, int ph)
{
    uint32_t d_phi = nco_d_phase((ssize_t)freq_hz, (size_t)ctx->sample_rate);
    // uint32_t phi = nco_phase((ssize_t)freq_hz, (size_t)ctx->sample_rate, global_time_us); // absolute phase
    // uint32_t phi = 0; // relative phase

    // phase offset if requested
    while (ph < 0)
        ph += 360;
    while (ph >= 360)
        ph -= 360;
    if (ph) {
        ctx->phi += 11930465 * (uint32_t)ph; // (0x100000000 / 360)
    }

    double n_att = db_to_mag(db);
    double g_att = db_to_mag(ctx->g_db);
    ctx->g_db = db;
    ctx->g_hz = freq_hz;

    size_t end = (size_t)(time_us * ctx->sample_rate / 1000000.0);
    for (size_t t = 0; t < end; ++t) {

        // ramp in and out
        double att = t < ctx->filter_len ? ctx->filter_out[t] * g_att + ctx->filter_in[t] * n_att : n_att;

        // complex I/Q
        double x = nco_cos(ctx->phi) * ctx->gain * att;
        double y = nco_sin(ctx->phi) * ctx->gain * att;
        ctx->phi += d_phi;

        // disturb
        x += (randf() - 0.5) * ctx->noise_signal;
        y += (randf() - 0.5) * ctx->noise_signal;

        ctx->signal_out(ctx, x, y);
    }
}

// api

size_t iq_render_length_us(tone_t *tones)
{
    size_t signal_length_us = 0;

    for (tone_t *tone = tones; (tone->us || tone->hz) && !abort_render; ++tone) {
        signal_length_us += (size_t)tone->us;
    }

    return signal_length_us;
}

size_t iq_render_length_smp(iq_render_t *spec, tone_t *tones)
{
    if (spec->sample_rate == 0.0)
        spec->sample_rate = DEFAULT_SAMPLE_RATE;
    double sample_rate = spec->sample_rate;

    size_t signal_length_samples = 0;

    for (tone_t *tone = tones; (tone->us || tone->hz) && !abort_render; ++tone) {
        size_t len = (size_t)(tone->us * sample_rate / 1000000.0);
        signal_length_samples += len;
    }

    return signal_length_samples;
}

void iq_render_defaults(iq_render_t *spec)
{
    spec->sample_rate  = DEFAULT_SAMPLE_RATE;
    spec->noise_floor  = -19;
    spec->noise_signal = -25;
    spec->gain         = -3;
    spec->frame_size   = DEFAULT_BUF_LENGTH;
}

static void iq_render_init(ctx_t *ctx, iq_render_t *spec)
{
    if (spec->sample_rate == 0.0)
        spec->sample_rate = DEFAULT_SAMPLE_RATE;

    if (spec->frame_size == 0)
        spec->frame_size = DEFAULT_BUF_LENGTH;

    if (spec->sample_format < FORMAT_CU8 || spec->sample_format > FORMAT_CF64) {
        fprintf(stderr, "Bad sample format (%d).\n",
                spec->sample_format);
        exit(1);
    }

    if (spec->full_scale == 0.0)
        spec->full_scale = scale_defaults[spec->sample_format];
    // fprintf(stderr, "Full scale is %.1f.\n", spec->full_scale);

    size_t unit = sample_format_length(spec->sample_format);
    if (spec->frame_size % unit != 0) {
        fprintf(stderr, "Adjusting frame size from %zu to %zu bytes.\n",
                spec->frame_size, spec->frame_size - spec->frame_size % unit);
        spec->frame_size -= spec->frame_size % unit;
    }

    ctx->sample_rate   = spec->sample_rate;
    ctx->noise_floor   = noise_pp_level(spec->noise_floor);
    ctx->noise_signal  = noise_pp_level(spec->noise_signal);
    ctx->gain          = sine_pk_level(spec->gain);
    ctx->sample_format = spec->sample_format;
    ctx->full_scale    = spec->full_scale;
    ctx->frame_size    = spec->frame_size;
    ctx->signal_out    = format_out[ctx->sample_format];

    ctx->g_db = -40;
    ctx->g_hz = 0;
    ctx->phi  = 0;

    init_db_lut();
    nco_init();
    init_filter(ctx, 50);
}

static size_t iq_render(ctx_t *ctx, tone_t *tones)
{
    size_t signal_length_us = 0;

    for (tone_t *tone = tones; (tone->us || tone->hz) && !abort_render; ++tone) {
        if (tone->db < -24) {
            //add_noise((size_t)tone->us, tone->db);
            add_sine(ctx, ctx->g_hz, (size_t)tone->us, tone->db, tone->ph);
        }
        else {
            add_sine(ctx, tone->hz, (size_t)tone->us, tone->db, tone->ph);
        }
        signal_length_us += (size_t)tone->us;
    }

    return signal_length_us;
}

int iq_render_file(char *outpath, iq_render_t *spec, tone_t *tones)
{
    ctx_t ctx = {0};
    ctx.fd    = -1;

    iq_render_init(&ctx, spec);

    if (!outpath || !*outpath || !strcmp(outpath, "-"))
        ctx.fd = fileno(stdout);
    else
        ctx.fd = open(outpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    ctx.frame.u8 = malloc(ctx.frame_size);
    if (!ctx.frame.u8) {
        fprintf(stderr, "Failed to allocate output buffer of %zu bytes.\n", ctx.frame_size);
        exit(1);
    }

    clock_t start = clock();

    size_t signal_length_us = iq_render(&ctx, tones);
    signal_out_flush(&ctx);

    clock_t stop = clock();
    double elapsed = (double)(stop - start) * 1000.0 / CLOCKS_PER_SEC;
    printf("Time elapsed %g ms, signal lenght %g ms, speed %gx\n", elapsed, signal_length_us / 1000.0, signal_length_us / 1000.0 / elapsed);

    free(ctx.frame.u8);
    if (ctx.fd != fileno(stdout))
        close(ctx.fd);

    return 0;
}

int iq_render_buf(iq_render_t *spec, tone_t *tones, void **out_buf, size_t *out_len)
{
    ctx_t ctx = {0};
    ctx.fd    = -1;

    iq_render_init(&ctx, spec);

    size_t smp = iq_render_length_smp(spec, tones);
    ctx.frame_size = smp * sample_format_length(ctx.sample_format);

    ctx.frame.u8 = malloc(ctx.frame_size);
    if (!ctx.frame.u8) {
        fprintf(stderr, "Failed to allocate output buffer of %zu bytes.\n", ctx.frame_size);
        exit(1);
    }

    ctx.frame_size += 1; // this way we never try to flush

    clock_t start = clock();

    size_t signal_length_us = iq_render(&ctx, tones);

    clock_t stop = clock();
    double elapsed = (double)(stop - start) * 1000.0 / CLOCKS_PER_SEC;
    printf("Time elapsed %g ms, signal lenght %g ms, speed %gx\n", elapsed, signal_length_us / 1000.0, signal_length_us / 1000.0 / elapsed);

    if (out_buf)
        *out_buf = ctx.frame.u8;
    if (out_len)
        *out_len = ctx.frame_size - 1;
    return 0;
}
