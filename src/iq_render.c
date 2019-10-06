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
#include "getopt/getopt.h"
#define F_OK 0
#endif
#endif
#ifndef _MSC_VER
#include <unistd.h>
#include <getopt.h>
#endif

#include <time.h>

#include "nco.h"

int abort_render = 0;

static double sample_rate = DEFAULT_SAMPLE_RATE;
static double noise_floor = 0.1 * 2; // peak-to-peak
static double noise_signal = 0.05 * 2; // peak-to-peak
static double gain = 1.0;
static int fd = -1;

static enum sample_format sample_format = FORMAT_NONE;
static size_t frame_size = DEFAULT_BUF_LENGTH;
static size_t out_block_len = 0;
static size_t out_block_pos = 0;
static frame_t out_block;

static double randf(void)
{
    // hotspot:
    return (double)rand() / RAND_MAX;
}

static int bound_u8(int x)
{
    return x < 0 ? 0 : x > 255 ? 255 : x;
}

static int bound_u16(int x)
{
    return x < 0 ? 0 : x > 65535 ? 65535 : x;
}

static int bound_i16(int x)
{
    return x < -32768 ? -32768 : x > 32767 ? 32767 : x;
}

typedef void (*signal_out_fn)(double i, double q);

static void signal_out_cu8(double i, double q)
{
    double scale = 127.5; // scale to u8
    uint8_t i8 = (uint8_t)bound_u8((int)((i + 1.0) * scale));
    uint8_t q8 = (uint8_t)bound_u8((int)((q + 1.0) * scale));
    out_block.u8[out_block_pos++] = i8;
    out_block.u8[out_block_pos++] = q8;
    out_block_len += 2 * sizeof(uint8_t);
    if (out_block_len >= frame_size) {
        write(fd, out_block.u8, frame_size);
        out_block_pos = out_block_len = 0;
    }
}

static void signal_out_cs8(double i, double q)
{
    double scale = 127.5; // scale to s8
    int8_t i8 = (int8_t)bound_u8((int)(i * scale));
    int8_t q8 = (int8_t)bound_u8((int)(q * scale));
    out_block.s8[out_block_pos++] = i8;
    out_block.s8[out_block_pos++] = q8;
    out_block_len += 2 * sizeof(int8_t);
    if (out_block_len >= frame_size) {
        write(fd, out_block.u8, frame_size);
        out_block_pos = out_block_len = 0;
    }
}

static void signal_out_cs16(double i, double q)
{
    double scale = 32767.5; // scale to s16
    int16_t i8 = (int16_t)bound_i16((int)(i * scale));
    int16_t q8 = (int16_t)bound_i16((int)(q * scale));
    out_block.s16[out_block_pos++] = i8;
    out_block.s16[out_block_pos++] = q8;
    out_block_len += 2 * sizeof(int16_t);
    if (out_block_len >= frame_size) {
        write(fd, out_block.u8, frame_size);
        out_block_pos = out_block_len = 0;
    }
}

static void signal_out_cf32(double i, double q)
{
    out_block.f32[out_block_pos++] = (float)i;
    out_block.f32[out_block_pos++] = (float)q;
    out_block_len += 2 * sizeof(float);
    if (out_block_len >= frame_size) {
        write(fd, out_block.u8, frame_size);
        out_block_pos = out_block_len = 0;
    }
}

static void signal_out_flush()
{
    write(fd, out_block.u8, out_block_len);
    out_block_pos = out_block_len = 0;
}

static signal_out_fn signal_out;
static signal_out_fn format_out[] = {signal_out_cu8, signal_out_cu8, signal_out_cs8, signal_out_cs16, signal_out_cf32};

static void add_noise(size_t time_us, int db)
{
    size_t end =(size_t)(time_us * sample_rate / 1000000.0);
    for (size_t t = 0; t < end; ++t) {
        double x = (randf() - 0.5) * noise_floor;
        double y = (randf() - 0.5) * noise_floor;
        signal_out(x, y);
    }
}

static int g_db     = -40; // continuous db
static double g_hz  = 0;   // continuous freq
static uint32_t phi = 0;   // continuous phase

static double filter_out[100] = {0};
static double filter_in[100] = {0};
static size_t filter_len  = 0;

static void init_filter(size_t time_us)
{
    filter_len = (size_t)(time_us * sample_rate / 1000000.0);
    //uint32_t l_phi = 0;
    //uint32_t d_phi = nco_d_phase(1000000 / 2 / time_us, (size_t)sample_rate);
    for (size_t t = 0; t < filter_len; ++t) {
        filter_out[t] = (filter_len - t) / (double)filter_len;
        filter_in[t]  = t / (double)filter_len;

        //filter_out[t] = (nco_cos(l_phi) + 1) / 2;
        //printf("at %2d : %f\n", t, filter_out[t]);
        //filter_in[t]  = 1.0 - filter_out[t];
        //l_phi += d_phi;
    }
}

static void add_sine(double freq_hz, size_t time_us, int db)
{
    uint32_t d_phi = nco_d_phase((ssize_t)freq_hz, (size_t)sample_rate);
    // uint32_t phi = nco_phase((ssize_t)freq_hz, (size_t)sample_rate, global_time_us); // absolute phase
    // uint32_t phi = 0; // relative phase

    double n_att = db_to_mag(db);
    double g_att = db_to_mag(g_db);
    g_db = db;
    g_hz = freq_hz;

    size_t end = (size_t)(time_us * sample_rate / 1000000.0);
    for (size_t t = 0; t < end; ++t) {

        // ramp in and out
        double att = t < filter_len ? filter_out[t] * g_att + filter_in[t] * n_att : n_att;

        // complex I/Q
        double x = nco_cos(phi) * gain * att;
        double y = nco_sin(phi) * gain * att;
        phi += d_phi;

        // disturb
        x += (randf() - 0.5) * noise_signal;
        y += (randf() - 0.5) * noise_signal;

        signal_out(x, y);
    }
}

void iq_render_file(char *outpath, iq_render_t *spec, tone_t *tones)
{
    init_db_lut();
    nco_init();
    init_filter(50);

    signal_out = format_out[sample_format];

    if (!outpath || !*outpath || !strcmp(outpath, "-"))
        fd = fileno(stdout);
    else
        fd = open(outpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    out_block.u8 = malloc(frame_size);
    if (!out_block.u8) {
        fprintf(stderr, "Failed to allocate output buffer of %zu bytes.\n", frame_size);
        exit(1);
    }

    clock_t start = clock();

    size_t signal_length_us = 0;
    for (tone_t *tone = tones; (tone->us || tone->hz) && !abort_render; ++tone) {
        if (tone->db < -24) {
            //add_noise((size_t)tone->us, tone->db);
            add_sine(g_hz, (size_t)tone->us, tone->db);
        }
        else {
            add_sine(tone->hz, (size_t)tone->us, tone->db);
        }
        signal_length_us += (size_t)tone->us;
    }
    signal_out_flush();

    clock_t stop = clock();
    double elapsed = (double)(stop - start) * 1000.0 / CLOCKS_PER_SEC;
    printf("Time elapsed %g ms, signal lenght %g ms, speed %gx\n", elapsed, signal_length_us / 1000.0, signal_length_us / 1000.0 / elapsed);

    free(out_block.u8);
    if (fd != fileno(stdout))
        close(fd);
}


int iq_render_buf(tone_t *tones, iq_render_t *spec, void **out_buf, size_t *out_len)
{
    return -1;
}
