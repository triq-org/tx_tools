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

#include <stdlib.h>

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Approx sine

static double approx_sin(double x)
{
    double c3 = 1.0 / (3 * 2 * 1);
    double c5 = 1.0 / (5 * 4 * 3 * 2 * 1);
    double c7 = 1.0 / (7 * 6 * 5 * 4 * 3 * 2 * 1);
    return x - x * x * x * c3 + x * x * x * x * x * c5 - x * x * x * x * x * x * x * c7;
}

// LUT sine

static double lut_sin_table[256] = {0.0};

static void init_lut_sin(void)
{
    for (int i = 0; i < 256; ++i) {
        lut_sin_table[i] = sin(2.0 * M_PI * i / 256.0);
    }
}

static double lut_sin(double x)
{
    return lut_sin_table[(int)(x * 256)]; // use 4 quadrants
}

// LUT oscillator

typedef struct {
    ssize_t freq;
    size_t periode;
    size_t quarter;
    size_t sample_rate;
    double lut_sin[1000];
} lut_osc_t;

static lut_osc_t osc_lut[10] = {{0}};

static lut_osc_t *get_lut_osc(ssize_t f, size_t sample_rate)
{
    lut_osc_t *lut = osc_lut;
    while (lut->freq)
        if (lut->freq == f)
            return lut;
        else
            ++lut;

    lut->sample_rate = sample_rate;
    lut->freq = f;
    size_t abs_f = f < 0 ? (size_t)-f : (size_t)f;
    size_t periode = sample_rate / abs_f;
    size_t quarter = f < 0 ? periode * 3 / 4 : periode / 4;
    lut->periode = periode;
    lut->quarter = quarter;
    //printf("Freq %ld sin at %ld rate has periode %ld, quarter %ld\n", f, sample_rate, periode, quarter);
    for (size_t i = 0; i < periode; ++i)
        lut->lut_sin[i] = sin(f * 2.0 * M_PI * i / sample_rate);

    return lut;
}

static double lut_oscc(lut_osc_t *lut, size_t t)
{
    return lut->lut_sin[(t + lut->quarter) % lut->periode];
}

static double lut_oscs(lut_osc_t *lut, size_t t)
{
    return lut->lut_sin[t % lut->periode];
    //size_t i = t % lut->periode;
    //if (i < 2 * lut->quarter) {
    //    if (i < lut->quarter)
    //        return lut->lut_sin[i];
    //    else
    //        return lut->lut_sin[2 * lut->quarter - i];
    //} else {
    //    if (i < 3 * lut->quarter)
    //        return lut->lut_sin[i - 2 * lut->quarter];
    //    else
    //        return lut->lut_sin[4 * lut->quarter - i];
    //}
}

// LUT dB

static double db_lut[256];

static void init_db_lut(void)
{
    for (int db = -128; db <= 127; ++db)
        db_lut[128 + db] = pow(10.0, 1.0/20.0 * db);
}

static double db_to_mag(int db)
{
    return db_lut[128 + db];
}
