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

#ifndef INCLUDE_IQRENDER_H_
#define INCLUDE_IQRENDER_H_

#include <stddef.h> /* size_t */
#include "common.h" /* to get tone_t, sample_format_t */

#define DEFAULT_SAMPLE_RATE 1000000
#define DEFAULT_BUF_LENGTH (1 * 16384)
#define MINIMAL_BUF_LENGTH 512
#define MAXIMAL_BUF_LENGTH (256 * 16384)

typedef struct iq_render {
    double sample_rate;
    double noise_floor;  ///< peak-to-peak
    double noise_signal; ///< peak-to-peak
    double gain;         ///< usually a little below 0
    enum sample_format sample_format;
    size_t frame_size; ///< default will be used if 0
} iq_render_t;

// parsing a code from string or reading in

extern int abort_render;

void iq_render_defaults(iq_render_t *spec);

size_t iq_render_length_us(tone_t *tones);

size_t iq_render_length_smp(iq_render_t *spec, tone_t *tones);

int iq_render_file(char *outpath, iq_render_t *spec, tone_t *tones);

int iq_render_buf(iq_render_t *spec, tone_t *tones, void **out_buf, size_t *out_len);

#endif /* INCLUDE_IQRENDER_H_ */
