/*
 * tx_tools - numerically controlled oscillator (NCO)
 *
 * Copyright (C) 2018 by Christian Zuckschwerdt <zany@triq.net>
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
#include <stdint.h>

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// numerically controlled oscillator (NCO)

static double nco_sin_lut[1024];

static void nco_init(void)
{
    for (int i = 0; i < 1024; ++i) {
        nco_sin_lut[i] = sin(2.0 * M_PI * i / 1024.0);
    }
}

static double nco_sin_ratio(double x)
{
    unsigned int i = (unsigned int)(x * 1023.999) % 1024; // round
    return nco_sin_lut[i];
}

static double nco_cos_ratio(double x)
{
    unsigned int i = ((unsigned int)(x * 1023.999) + 256) % 1024; // round
    return nco_sin_lut[i];
}

// delta phase per sample
static uint32_t nco_d_phase(ssize_t f, size_t sample_rate)
{
    long long ll = (1LL << 32) * f / (long long)sample_rate;
    return (uint32_t)ll;
}

// delta phase per N samples
static uint32_t nco_phase(ssize_t f, size_t sample_rate, size_t sample_pos)
{
    unsigned long long ll = sample_pos * nco_d_phase(f, sample_rate);
    return (uint32_t)ll;
}

static double nco_sin(uint32_t phi)
{
    unsigned int i = ((phi + (1 << 21)) >> 22) & 0x3ff; // round
    return nco_sin_lut[i];
}

static double nco_cos(uint32_t phi)
{
    unsigned int i = ((phi + (1 << 21)) >> 22) & 0x3ff; // round
    i = (i + 256) & 0x3ff;
    return nco_sin_lut[i];
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
