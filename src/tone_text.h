/** @file
    tx_tools - tone_text, a simple tone spec parser and printer.

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

#ifndef INCLUDE_TONETEXT_H_
#define INCLUDE_TONETEXT_H_

typedef struct {
    int hz; ///< Tone frequency (Hz)
    int db; ///< Tone attenuation (dB)
    int ph; ///< Tone phase (deg offset)
    int us; ///< Tone length (us)
} tone_t;

// parsing tone data from string or reading in

tone_t *parse_tones(char const *tones);

tone_t *parse_tones_file(char const *filename);

// debug output to stdout

void output_tone(tone_t const *t);

void output_tones(tone_t const *tones);

#endif /* INCLUDE_TONETEXT_H_ */
