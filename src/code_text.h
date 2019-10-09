/** @file
    tx_tools - code_text, a simple waveform spec parser and printer.

    Copyright (C) 2018 by Christian Zuckschwerdt <zany@triq.net>

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

#ifndef INCLUDE_CODETEXT_H_
#define INCLUDE_CODETEXT_H_

#include "common.h" /* to get tone_t */

typedef struct {
    int tones;
    tone_t tone[1000]; // TODO: really should be dynamic
} symbol_t;

// parsing a code from string or reading in

symbol_t *parse_code(char const *code, symbol_t *symbols);

symbol_t *parse_code_file(char const *filename, symbol_t *symbols);

void free_symbols(symbol_t *symbols);

// debug output to stdout

void output_tone(tone_t const *t);

void output_symbol(symbol_t const *s);

#endif /* INCLUDE_CODETEXT_H_ */
