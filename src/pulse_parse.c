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

#include "pulse_parse.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "common.h"

static void skip_ws(char const **buf)
{
    char const *p = *buf;

    // skip whitespace
    while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        ++p;
    }

    *buf = p;
}

static void skip_ws_c(char const **buf)
{
    char const *p = *buf;

    // skip whitespace and comments
    while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '#' || *p == ';')) {
        if (*p == '#' || *p == ';')
            while (*p && *p != '\r' && *p != '\n')
                ++p;
        if (*p)
            ++p;
    }

    *buf = p;
}

static void parse_param(char const **buf, pulse_setup_t *params)
{
    char const *p = *buf;

    // get "key = value" ...
    while (*p && *p != '\r' && *p != '\n')
        ++p;
    if (*p)
        ++p;

    *buf = p;
}

static int parse_len(char const **buf)
{
    char const *p = *buf;

    char *endptr;
    double val = strtod(p, &endptr);

    if (p == endptr) {
        fprintf(stderr, "invalid number argument (%c)\n", *p);
        exit(1);
    }

    if (val < -0x80000000 && val >= 0x80000000 ) {
        fprintf(stderr, "out of range number argument (%f)\n", val);
        exit(1);
    }

    int ival = (int)val;

    if (ival < 0 && ival != -1) {
        fprintf(stderr, "non-negative number argument expected (%f)\n", val);
        exit(1);
    }

    *buf = endptr;
    return ival;
}

tone_t *parse_pulses(char const *pulses, pulse_setup_t *defaults)
{
    if (!pulses || !*pulses)
        return NULL;
    if (!defaults)
        return NULL;

    // unsigned len = 0;
    unsigned count = 0;

    // read header and sum pulse length

    char const *p = pulses;
    while (*p) {
        skip_ws(&p);
        if (*p == ';') {
            // parse one parameter
            parse_param(&p, defaults);
        }
        else if (*p) {
            // parse mark and space
            int mark = parse_len(&p);
            int space = parse_len(&p);

            // len += mark + space;
            count += 2;
        }
    }

    tone_t *tones = malloc((count + 1) * sizeof(tone_t));

    // skip header and generate pulses

    int i = 0;
    p = pulses;
    while (*p) {
        skip_ws_c(&p);
        // parse mark and space
        int mark  = parse_len(&p);
        int space = parse_len(&p);

        if (mark == -1) {
            // special case: silence

            tone_t *t = &tones[i++];
            t->hz = defaults->freq_mark;
            t->db = defaults->att_mark;
            t->ph = defaults->phase_mark;
            t->us = 0;

            t = &tones[i++];
            t->hz = defaults->freq_space;
            t->db = -200;
            t->ph = defaults->phase_space;
            t->us = (int)((uint64_t)space * 1000000 / defaults->time_base);

            continue;
        }

        // gen mark
        tone_t *t = &tones[i++];
        t->hz = defaults->freq_mark;
        t->db = defaults->att_mark;
        t->ph = defaults->phase_mark;
        t->us = (int)((uint64_t)mark * 1000000 / defaults->time_base);

        // gen space
        t = &tones[i++];
        t->hz = defaults->freq_space;
        t->db = defaults->att_space;
        t->ph = defaults->phase_space;
        t->us = (int)((uint64_t)space * 1000000 / defaults->time_base);
    }

    // null terminate
    tone_t *t = &tones[i];
    t->hz = 0;
    t->db = 0;
    t->ph = 0;
    t->us = 0;

    return tones;
}

tone_t *parse_pulses_file(char const *filename, pulse_setup_t *defaults)
{
    char const *text = read_text_file(filename);
    return parse_pulses(text, defaults);
}

void output_pulses(tone_t const *tones)
{
    if (!tones || !tones[0].hz || !tones[1].us) {
        printf("Invalid pulse data\n");
        return;
    }
    printf(";pulse data\n");
    printf(";version 1\n");
    printf(";timescale 1us\n");
    printf(";freq_mark %d\n", tones[0].hz);
    printf(";freq_space %d\n", tones[1].hz);
    printf(";att_mark %d\n", tones[0].db);
    printf(";att_space %d\n", tones[1].db);
    printf(";phase_mark %d\n", tones[0].ph);
    printf(";phase_space %d\n", tones[1].ph);
    printf(";time_base %d\n", 1000000);

    for (tone_t const *t = tones; t->us || t->hz; ++t) {
        tone_t const *m = t++;
        printf("%d %d\n", m->us, t->us);
    }
}
