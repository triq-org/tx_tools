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
#include "sample.h"

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


#define MAX_STEP_SIZE 1000

/// Filter state.
typedef struct filter_state {
    double a[2 + 1]; // up to 2nd order
    double b[2 + 1]; // up to 2nd order
    double yi[2];
    double xi[2];
    double yq[2];
    double xq[2];
} filter_state_t;

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

    double step_out[MAX_STEP_SIZE];
    double step_in[MAX_STEP_SIZE];
    size_t step_len;

    filter_state_t filter_state;
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

static inline double randf(void)
{
    // hotspot:
    return (double)rand() / RAND_MAX;
}

static inline uint8_t bound_u4(int x)
{
    return x < 0 ? 0 : x > 0xf ? 0xf : (uint8_t)x;
}

static inline int8_t bound_s4(int x)
{
    return x < -0x8 ? -0x8 : x > 0x7 ? 0x7 : (int8_t)x;
}

static inline uint8_t bound_u8(int x)
{
    return x < 0 ? 0 : x > 0xff ? 0xff : (uint8_t)x;
}

static inline int8_t bound_s8(int x)
{
    return x < -0x80 ? -0x80 : x > 0x7f ? 0x7f : (int8_t)x;
}

static inline uint16_t bound_u16(int x)
{
    return x < 0 ? 0 : x > 0xffff ? 0xffff : (uint16_t)x;
}

static inline int16_t bound_s16(int x)
{
    return x < -0x8000 ? -0x8000 : x > 0x7fff ? 0x7fff : (int16_t)x;
}

static inline uint32_t bound_u32(double x)
{
    return x < 0 ? 0 : x > 0xffffffff ? 0xffffffff : (uint32_t)x;
}

static inline int32_t bound_s32(double x)
{
    return x < -0x7fffffff ? -0x7fffffff : x > 0x7fffffff ? 0x7fffffff : (int32_t)x;
}

static inline uint64_t bound_u64(double x)
{
    return x < 0 ? 0 : x > 0xffffffffffffffff ? 0xffffffffffffffff : (uint64_t)x;
}

static inline int64_t bound_s64(double x)
{
    return x < -0x7fffffffffffffff ? -0x7fffffffffffffff : x > 0x7fffffffffffffff ? 0x7fffffffffffffff : (int64_t)x;
}

// inlines

static inline void signal_out_flush(ctx_t *ctx)
{
    write(ctx->fd, ctx->frame.u8, ctx->frame_len);
    ctx->frame_pos = ctx->frame_len = 0;
}

static inline void signal_out_maybe_flush(ctx_t *ctx)
{
    if (ctx->frame_len >= ctx->frame_size) {
        signal_out_flush(ctx);
    }
}

static void signal_out_cu4(ctx_t *ctx, double i, double q)
{
    // scale [-1.0, 1.0] to [0, 15] with uniform distribution,
    // i.e. bias 7.5 -- not Excess-8
    // this exact scale prevents 1.0==16
    uint8_t i8 = bound_u4((int)(i * 7.999999 + 7.5 + 0.5));
    uint8_t q8 = bound_u4((int)(q * 7.999999 + 7.5 + 0.5));
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)(i8 << 4) | (q8);
    ctx->frame_len += 1 * sizeof(uint8_t);
}

static void signal_out_cs4(ctx_t *ctx, double i, double q)
{
    // scale [-1.0, 1.0] to [-7, 7] with uniform distribution
    // this exact scale prevents 1.0==8, -1.0==-8
    int8_t i8 = bound_s4((int)(i * 7.49999 + 8 + 0.5) - 8);
    int8_t q8 = bound_s4((int)(q * 7.49999 + 8 + 0.5) - 8);
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)(i8 << 4) | (q8 & 0xf);
    ctx->frame_len += 1 * sizeof(uint8_t);
}

static void signal_out_cu8(ctx_t *ctx, double i, double q)
{
    // scale [-1.0, 1.0] to [0, 255] with uniform distribution,
    // i.e. bias 127.5 -- not Excess-128
    // this exact scale prevents 1.0==256
    uint8_t i8 = bound_u8((int)(i * 127.999999 + 127.5 + 0.5));
    uint8_t q8 = bound_u8((int)(q * 127.999999 + 127.5 + 0.5));
    ctx->frame.u8[ctx->frame_pos++] = i8;
    ctx->frame.u8[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(uint8_t);
}

static void signal_out_cs8(ctx_t *ctx, double i, double q)
{
    // scale [-1.0, 1.0] to [-127, 127] with uniform distribution
    // this exact scale prevents 1.0==128, -1.0==-128
    int8_t i8 = bound_s8((int)(i * 127.4999 + 128 + 0.5) - 128);
    int8_t q8 = bound_s8((int)(q * 127.4999 + 128 + 0.5) - 128);
    ctx->frame.s8[ctx->frame_pos++] = i8;
    ctx->frame.s8[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(int8_t);
}

static void signal_out_cu12(ctx_t *ctx, double i, double q)
{
    uint16_t i8 = bound_u16((int)((i + 1.0) * ctx->full_scale));
    uint16_t q8 = bound_u16((int)((q + 1.0) * ctx->full_scale));
    // produce 24 bit (iiqIQQ), note the input is LSB aligned, scale=2048
    // note: byte0 = i[7:0]; byte1 = {q[3:0], i[11:8]}; byte2 = q[11:4];
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)(i8);
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)((q8 << 4) | ((i8 >> 8) & 0x0f));
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)(q8 >> 4);
    ctx->frame_len += 3 * sizeof(uint8_t);
    // NOTE: frame_size needs to be a multiple of 3!
}

static void signal_out_cs12(ctx_t *ctx, double i, double q)
{
    int16_t i8 = bound_s16((int)(i * ctx->full_scale + 2048 + 0.5) - 2048);
    int16_t q8 = bound_s16((int)(q * ctx->full_scale + 2048 + 0.5) - 2048);
    // produce 24 bit (iiqIQQ), note the input is LSB aligned, scale=2048
    // note: byte0 = i[7:0]; byte1 = {q[3:0], i[11:8]}; byte2 = q[11:4];
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)(i8);
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)((q8 << 4) | ((i8 >> 8) & 0x0f));
    ctx->frame.u8[ctx->frame_pos++] = (uint8_t)(q8 >> 4);
    ctx->frame_len += 3 * sizeof(uint8_t);
    // NOTE: frame_size needs to be a multiple of 3!
}

static void signal_out_cu16(ctx_t *ctx, double i, double q)
{
    uint16_t i8 = bound_u16((int)((i + 1.0) * ctx->full_scale));
    uint16_t q8 = bound_u16((int)((q + 1.0) * ctx->full_scale));
    ctx->frame.u16[ctx->frame_pos++] = i8;
    ctx->frame.u16[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(uint16_t);
}

static void signal_out_cs16(ctx_t *ctx, double i, double q)
{
    int16_t i8 = bound_s16((int)(i * ctx->full_scale + 32768 + 0.5) - 32768);
    int16_t q8 = bound_s16((int)(q * ctx->full_scale + 32768 + 0.5) - 32768);
    ctx->frame.s16[ctx->frame_pos++] = i8;
    ctx->frame.s16[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(int16_t);
}

static void signal_out_cu32(ctx_t *ctx, double i, double q)
{
    uint32_t i8 = bound_u32((i + 1.0) * ctx->full_scale);
    uint32_t q8 = bound_u32((q + 1.0) * ctx->full_scale);
    ctx->frame.u32[ctx->frame_pos++] = i8;
    ctx->frame.u32[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(uint32_t);
}

static void signal_out_cs32(ctx_t *ctx, double i, double q)
{
    int32_t i8 = bound_s32(i * ctx->full_scale);
    int32_t q8 = bound_s32(q * ctx->full_scale);
    ctx->frame.s32[ctx->frame_pos++] = i8;
    ctx->frame.s32[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(int32_t);
}

static void signal_out_cu64(ctx_t *ctx, double i, double q)
{
    uint64_t i8 = bound_u64((i + 1.0) * ctx->full_scale);
    uint64_t q8 = bound_u64((q + 1.0) * ctx->full_scale);
    ctx->frame.u64[ctx->frame_pos++] = i8;
    ctx->frame.u64[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(uint64_t);
}

static void signal_out_cs64(ctx_t *ctx, double i, double q)
{
    int64_t i8 = bound_s64(i * ctx->full_scale);
    int64_t q8 = bound_s64(q * ctx->full_scale);
    ctx->frame.s64[ctx->frame_pos++] = i8;
    ctx->frame.s64[ctx->frame_pos++] = q8;
    ctx->frame_len += 2 * sizeof(int64_t);
}

static void signal_out_cf32(ctx_t *ctx, double i, double q)
{
    ctx->frame.f32[ctx->frame_pos++] = (float)(i * ctx->full_scale);
    ctx->frame.f32[ctx->frame_pos++] = (float)(q * ctx->full_scale);
    ctx->frame_len += 2 * sizeof(float);
}

static void signal_out_cf64(ctx_t *ctx, double i, double q)
{
    ctx->frame.f64[ctx->frame_pos++] = (double)(i * ctx->full_scale);
    ctx->frame.f64[ctx->frame_pos++] = (double)(q * ctx->full_scale);
    ctx->frame_len += 2 * sizeof(double);
}

static signal_out_fn format_out[] = {
        signal_out_cu8,
        signal_out_cu4,
        signal_out_cs4,
        signal_out_cu8,
        signal_out_cs8,
        signal_out_cu12,
        signal_out_cs12,
        signal_out_cu16,
        signal_out_cs16,
        signal_out_cu32,
        signal_out_cs32,
        signal_out_cu64,
        signal_out_cs64,
        signal_out_cf32,
        signal_out_cf64,
};
static double scale_defaults[] = {
        127.5,
        7.999999,
        7.49999,
        127.999999,
        127.4999,
        2047.999999,
        2047.4999,
        32767.999999,
        32767.4999,
        2147483647.999999,
        2147483647.4999,
        9223372036854775999.999999,
        9223372036854775999.4999,
        1.0,
        1.0,
};

// signal gen

static void init_step(ctx_t *ctx, size_t time_us)
{
    ctx->step_len = (size_t)(time_us * ctx->sample_rate / 1000000.0);
    if (ctx->step_len > MAX_STEP_SIZE)
        ctx->step_len = MAX_STEP_SIZE;
    //uint32_t l_phi = 0;
    //uint32_t d_phi = nco_d_phase(1000000 / 2 / time_us, (size_t)ctx->sample_rate);
    for (size_t t = 0; t < ctx->step_len; ++t) {
        // naive linear stepping
        ctx->step_out[t] = (ctx->step_len - t) / (double)ctx->step_len;
        ctx->step_in[t]  = t / (double)ctx->step_len;

        //ctx->step_out[t] = (nco_cos(l_phi) + 1) / 2;
        //printf("at %2d : %f\n", t, ctx->step_out[t]);
        //ctx->step_in[t]  = 1.0 - ctx->step_out[t];
        //l_phi += d_phi;
    }
}

static void init_filter(ctx_t *ctx, double wc)
{
    // wc is the ratio of cutoff and sampling freq: wc = f_cutoff / f_sampling
    // [b,a] = butter(2, Wc) # low pass filter with cutoff pi*Wc radians

    if (wc >= 0.5) {
        // flat, no filter
        ctx->filter_state = (filter_state_t){
                .a = {1.00000, 0.00000, 0.00000},
                .b = {1.00000, 0.00000, 0.00000},
        };
        return;
    }

    // Calculate coefficients of 2nd order Butterworth Low Pass Filter
    //y(n) = b0.x(n) + b1.x(n-1) + b2.x(n-2) + a1.y(n-1) + a2.y(n-2)
    double ita = 1.0 / tan(M_PI * wc);
    double q   = sqrt(2.0);
    double b0  = 1.0 / (1.0 + q * ita + ita * ita);
    double b1  = 2 * b0;
    double b2  = b0;
    double a1  = 2.0 * (ita * ita - 1.0) * b0;
    double a2  = -(1.0 - q * ita + ita * ita) * b0;
    //printf("b0: %f b1: %f b2: %f a1: %f a2: %f\n", b0, b1, b2, a1, a2);

    ctx->filter_state = (filter_state_t){
            .a = {1.00000, a1, a2},
            .b = {b0, b1, b2},
    };
}

static inline double apply_filter_i(ctx_t *ctx, double x)
{
    double *xi = ctx->filter_state.xi;
    double *yi = ctx->filter_state.yi;
    double *a = ctx->filter_state.a;
    double *b = ctx->filter_state.b;

    double y = a[1] * yi[0]
               + a[2] * yi[1]
               + b[0] * x
               + b[1] * xi[0]
               + b[2] * xi[1];

    xi[1] = xi[0];
    xi[0] = x;
    yi[1] = yi[0];
    yi[0] = y;

    return y;
}

static inline double apply_filter_q(ctx_t *ctx, double x)
{
    double *xq = ctx->filter_state.xq;
    double *yq = ctx->filter_state.yq;
    double *a  = ctx->filter_state.a;
    double *b  = ctx->filter_state.b;

    double y = a[1] * yq[0]
               + a[2] * yq[1]
               + b[0] * x
               + b[1] * xq[0]
               + b[2] * xq[1];

    xq[1] = xq[0];
    xq[0] = x;
    yq[1] = yq[0];
    yq[0] = y;

    return y;
}

static inline void add_sine(ctx_t *ctx, double freq_hz, size_t time_us, int db, int ph)
{
    //uint32_t g_phi = nco_d_phase((ssize_t)ctx->g_hz, (size_t)ctx->sample_rate);
    uint32_t d_phi = nco_d_phase((ssize_t)freq_hz, (size_t)ctx->sample_rate);
    // uint32_t phi = nco_phase((ssize_t)freq_hz, (size_t)ctx->sample_rate, global_time_us); // absolute phase
    // uint32_t phi = 0; // relative phase

    //printf("Phase delta: %u vs %u (%u to %u)\n", g_phi, d_phi,
    //        (uint32_t)(ctx->step_out[0] * g_phi + ctx->step_in[0] * d_phi),
    //        (uint32_t)(ctx->step_out[ctx->step_len-1] * g_phi + ctx->step_in[ctx->step_len-1] * d_phi));

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
        double att = t < ctx->step_len ? ctx->step_out[t] * g_att + ctx->step_in[t] * n_att : n_att;

        // complex I/Q
        double i = nco_cos(ctx->phi) * ctx->gain * att;
        double q = nco_sin(ctx->phi) * ctx->gain * att;
        ctx->phi += d_phi;
        //ctx->phi += t < ctx->step_len ? ctx->step_out[t] * g_phi + ctx->step_in[t] * d_phi : d_phi;

        // disturb
        i += (randf() - 0.5) * ctx->noise_signal;
        q += (randf() - 0.5) * ctx->noise_signal;

        // band limit
        i = apply_filter_i(ctx, i);
        q = apply_filter_q(ctx, q);

        // disturb
        i += (randf() - 0.5) * ctx->noise_floor;
        q += (randf() - 0.5) * ctx->noise_floor;

        ctx->signal_out(ctx, i, q);
        signal_out_maybe_flush(ctx);
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
    spec->noise_floor  = -36;
    spec->noise_signal = -24;
    spec->gain         = -3;
    spec->filter_wc    = 0.1;
    spec->step_width   = 50;
    spec->frame_size   = DEFAULT_BUF_LENGTH;
}

static void iq_render_init(ctx_t *ctx, iq_render_t *spec)
{
    if (spec->sample_rate == 0.0)
        spec->sample_rate = DEFAULT_SAMPLE_RATE;

    if (spec->frame_size == 0)
        spec->frame_size = DEFAULT_BUF_LENGTH;

    if (spec->sample_format < FORMAT_CU4 || spec->sample_format > FORMAT_CF64) {
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
    init_step(ctx, spec->step_width);
    init_filter(ctx, spec->filter_wc);
}

static size_t iq_render(ctx_t *ctx, tone_t *tones)
{
    size_t signal_length_us = 0;

    for (tone_t *tone = tones; (tone->us || tone->hz) && !abort_render; ++tone) {
        if (tone->db < -24) {
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

    if (!ctx.frame_size) {
        fprintf(stderr, "Warning: no samples to render.\n");
        return 0;
    }

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
    else
        free(ctx.frame.u8);
    if (out_len)
        *out_len = ctx.frame_size - 1;
    return 0;
}
