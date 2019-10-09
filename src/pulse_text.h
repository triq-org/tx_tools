/** @file
    tx_tools - pulse_text, a simple pulse spec parser and printer.

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

#ifndef INCLUDE_PULSETEXT_H_
#define INCLUDE_PULSETEXT_H_

#include "tone_text.h"
#include "read_text.h"

typedef struct pulse_setup {
    unsigned time_base; ///< 1/unit of width, usually 1000000 for us.
    int freq_mark;      ///< frequency offset for mark
    int freq_space;     ///< frequency offset for space, 0 otherwise
    int att_mark;       ///< attenuation for mark (dB)
    int att_space;      ///< attenuation for space (dB), -100 for silence
    int phase_mark;     ///< phase offset for mark, 0 otherwise
    int phase_space;    ///< phase offset for space, 0 otherwise
} pulse_setup_t;

// parsing pulse data from string or reading in

void pulse_setup_defaults(pulse_setup_t *params, char const *name);

void pulse_setup_print(pulse_setup_t *params);

tone_t *parse_pulses(char const *pulses, pulse_setup_t *defaults);

tone_t *parse_pulses_file(char const *filename, pulse_setup_t *defaults);

// debug output to stdout

void output_pulses(tone_t const *tones);

#endif /* INCLUDE_PULSETEXT_H_ */
