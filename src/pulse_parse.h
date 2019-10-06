/** @file
    tx_tools - pulse_parse, a simple pulse spec parser.

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

#ifndef INCLUDE_PULSEPARSE_H_
#define INCLUDE_PULSEPARSE_H_

#include <unistd.h>

#include "code_parse.h"

#define READ_CHUNK_SIZE 8192

typedef struct pulse_setup {
    int freq_mark;   ///< frequency offset for mark
    int freq_space;  ///< frequency offset for space, 0 otherwise
    int att_mark;    ///< attenuation for mark (dB)
    int att_space;   ///< attenuation for space (dB), -100 for silence
    int phase_mark;  ///< phase offset for mark, 0 otherwise
    int phase_space; ///< phase offset for space, 0 otherwise
    unsigned time_base;   ///< 1/unit of width, usually 1000000 for us.
} pulse_setup_t;

typedef struct output_spec {
    double sample_rate;
    double noise_floor;  ///< peak-to-peak
    double noise_signal; ///< peak-to-peak
    double gain;         ///< usually a little below 1.0
    //enum sample_format sample_format;
} output_spec_t;

// parsing a code from string or reading in

tone_t *parse_pulses(char const *pulses, pulse_setup_t *defaults);

tone_t *parse_pulses_file(char const *filename, pulse_setup_t *defaults);

// debug output to stdout

void output_pulses(tone_t const *tones);

// parsing pulse data from string

//int pulse_gen(char const *pulses, pulse_setup_t *defaults, output_spec_t *out_spec, void **out_buf, size_t *out_len);

#endif /* INCLUDE_PULSEPARSE_H_ */
