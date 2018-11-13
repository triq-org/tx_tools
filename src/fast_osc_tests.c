/*
 * tx_tools - fast_osc, optimized oscillators
 *
 * Copyright (C) 2017 by Christian Zuckschwerdt <zany@triq.net>
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

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fast_osc.h"

// plain

static double plain_oscc(double f, double sample_rate, size_t t)
{
    return cos(f * 2.0 * M_PI * t / sample_rate);
}

static double plain_oscs(double f, double sample_rate, size_t t)
{
    return sin(f * 2.0 * M_PI * t / sample_rate);
}

// approx

static double approx_oscc(double f, double sample_rate, size_t t)
{
    double p = f * t / sample_rate + 0.25;
    p = p - (int)p;
    return approx_sin(2.0 * M_PI * p); // mock
}

static double approx_oscs(double f, double sample_rate, size_t t)
{
    double p = f * t / sample_rate;
    p = p - (int)p;
    return approx_sin(2.0 * M_PI * p);
}

// tests

static void plain_add_sine(double *buf, ssize_t freq_hz, size_t sample_rate, size_t time_us, double att_db)
{
    lut_osc_t *lut = get_lut_osc(freq_hz, sample_rate);
    size_t end = (size_t)(time_us * sample_rate / 1000000);
    for (size_t t = 0; t < end; ++t) {

        double x = plain_oscc(freq_hz, sample_rate, t) * att_db;
        double y = plain_oscs(freq_hz, sample_rate, t) * att_db;

        *buf++ = x;
        *buf++ = y;
    }
}

static void approx_add_sine(double *buf, ssize_t freq_hz, size_t sample_rate, size_t time_us, double att_db)
{
    lut_osc_t *lut = get_lut_osc(freq_hz, sample_rate);
    size_t end = (size_t)(time_us * sample_rate / 1000000);
    for (size_t t = 0; t < end; ++t) {

        double x = approx_oscc(freq_hz, sample_rate, t) * att_db;
        double y = approx_oscs(freq_hz, sample_rate, t) * att_db;

        *buf++ = x;
        *buf++ = y;
    }
}

static void nco_add_sine(double *buf, ssize_t freq_hz, size_t sample_rate, size_t time_us, double att_db)
{
    uint32_t d_phi = nco_d_phase(freq_hz, sample_rate);
    uint32_t phi = 0;
    size_t end = (size_t)(time_us * sample_rate / 1000000);
    for (size_t t = 0; t < end; ++t) {

        double x = nco_cos(phi) * att_db;
        double y = nco_sin(phi) * att_db;
        phi += d_phi;

        *buf++ = x;
        *buf++ = y;
    }
}

static void osc_add_sine(double *buf, ssize_t freq_hz, size_t sample_rate, size_t time_us, double att_db)
{
    lut_osc_t *lut = get_lut_osc(freq_hz, sample_rate);
    size_t end = (size_t)(time_us * sample_rate / 1000000);
    for (size_t t = 0; t < end; ++t) {

        double x = lut_oscc(lut, t) * att_db;
        double y = lut_oscs(lut, t) * att_db;

        *buf++ = x;
        *buf++ = y;
    }
}

#include <time.h>

#define SAMPLE_RATE 1000000
#define SAMPLE_COUNT 100000
#define LOOPS 100

static void print_summary(char const *label, clock_t start, clock_t stop, double *buf, size_t len)
{
    double elapsed = (double)(stop - start) * 1000.0 / CLOCKS_PER_SEC;
    printf("%s: Time elapsed %f ms\t\t", label, elapsed);

    double avg_i = 0, avg_q = 0;
    for (size_t i = 0; i < len; ++i) {
        avg_i += buf[2 * i];
        avg_q += buf[2 * i + 1];
    }
    printf("Sum I %f Q %f\n", avg_i, avg_q);
}

int main(int argc, char **argv)
{
    double sample_rate = SAMPLE_RATE;
    size_t samples = SAMPLE_COUNT;
    size_t out_block_size = 2 * samples * sizeof(double);

    double *out_block = malloc(out_block_size);
    if (!out_block) {
        fprintf(stderr, "Failed to allocate output buffer of %zu bytes.\n", out_block_size);
        exit(1);
    }

    init_db_lut();
    nco_init();

    clock_t start, stop;

    // Plain

    start = clock();

    for (size_t i = 0; i < LOOPS; ++i) {
        plain_add_sine(out_block, 10000, (size_t)sample_rate, samples, 1.0);
        plain_add_sine(out_block, 20000, (size_t)sample_rate, samples, 1.0);
        plain_add_sine(out_block, 30000, (size_t)sample_rate, samples, 1.0);
    }

    stop = clock();
    print_summary("Plain ", start, stop, out_block, samples);

    // Approx

    start = clock();

    for (size_t i = 0; i < LOOPS; ++i) {
        approx_add_sine(out_block, 10000, (size_t)sample_rate, samples, 1.0);
        approx_add_sine(out_block, 20000, (size_t)sample_rate, samples, 1.0);
        approx_add_sine(out_block, 30000, (size_t)sample_rate, samples, 1.0);
    }

    stop = clock();
    print_summary("Approx", start, stop, out_block, samples);

    // NCO

    start = clock();

    for (size_t i = 0; i < LOOPS; ++i) {
        nco_add_sine(out_block, 10000, (size_t)sample_rate, samples, 1.0);
        nco_add_sine(out_block, 20000, (size_t)sample_rate, samples, 1.0);
        nco_add_sine(out_block, 30000, (size_t)sample_rate, samples, 1.0);
    }

    stop = clock();
    print_summary("NCO   ", start, stop, out_block, samples);

    // LUT osc

    start = clock();

    for (size_t i = 0; i < LOOPS; ++i) {
        osc_add_sine(out_block, 10000, (size_t)sample_rate, samples, 1.0);
        osc_add_sine(out_block, 20000, (size_t)sample_rate, samples, 1.0);
        osc_add_sine(out_block, 30000, (size_t)sample_rate, samples, 1.0);
    }

    stop = clock();
    print_summary("Osc   ", start, stop, out_block, samples);

    free(out_block);
}
