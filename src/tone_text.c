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

#include "tone_text.h"
#include "read_text.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void skip_ws(char const **buf)
{
    char const *p = *buf;

    // skip whitespace
    while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        ++p;
    }

    *buf = p;
}

static void skip_ws_sep(char const **buf)
{
    char const *p = *buf;

    // skip whitespace and separators
    while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '(' || *p == ')')) {
        ++p;
    }

    *buf = p;
}

static int is_num(char const *p)
{
    return (*p == '+' || *p == '-' || (*p >= '0' && *p <= '9'));
}

static int parse_num(char const **buf)
{
    char const *p = *buf;

    char *endptr;
    double val = strtod(p, &endptr);

    if (p == endptr) {
        fprintf(stderr, "invalid number argument \"%.5s\"\n", p);
        exit(1);
    }

    if (val < -0x80000000 && val >= 0x80000000 ) {
        fprintf(stderr, "out of range number argument (%f)\n", val);
        exit(1);
    }

    int ival = (int)val;

    if (val - ival > 1e-6) {
        fprintf(stderr, "integer number argument expected (%f)\n", val);
        exit(1);
    }

    *buf = endptr;
    return ival;
}

tone_t *parse_tones(char const *tones)
{
    if (!tones || !*tones)
        return NULL;

    // sum tone count

    unsigned count = 0;

    char const *p = tones;
    while (*p) {
        skip_ws_sep(&p);
        if (!*p)
            break; // eol

        // skip tone def
        while (*p != '(' && *p != ')') {
            ++p;
        }

        count += 1;
    }

    // parse and generate tones

    tone_t *ret = calloc(count + 1, sizeof(tone_t));

    int i = 0;
    p = tones;
    while (*p) {
        skip_ws_sep(&p);
        if (!*p)
            break; // eol

        // parse %dHz %ddeg %ddB %dus
        tone_t *t = &ret[i++];
        while (is_num(p)) {
            int num = parse_num(&p);
            skip_ws(&p);
            if (!strncmp(p, "Hz", 2) || !strncmp(p, "hz", 2)) {
                t->hz = num;
                p += 2;
            }
            // maybe also parse Quadrants ("L"), and Binary degree ("brad" / "br")?
            else if (!strncmp(p, "deg", 3)) {
                t->ph = num;
                p += 3;
            }
            else if (!strncmp(p, "dB", 2) || !strncmp(p, "db", 2)) {
                t->db = num;
                p += 2;
            }
            else if (!strncmp(p, "us", 2)) {
                t->us = num;
                p += 2;
            }
            else {
                fprintf(stderr, "unknown unit (%.3s) at tone %d\n", p, i);
                exit(1);
            }
            skip_ws(&p);
        }
    }

    return ret;
}

tone_t *parse_tones_file(char const *filename)
{
    char const *text = read_text_file(filename);
    return parse_tones(text);
}

void output_tone(tone_t const *t)
{
    if (!t)
        return;

    if (t->hz == 0) {
        printf("(%dus) ", t->us);
    }
    else if (t->db == 0 && t->ph == 0) {
        printf("(%dHz %dus) ", t->hz, t->us);
    }
    else if (t->db == 0) {
        printf("(%dHz %ddeg %dus) ", t->hz, t->ph, t->us);
    }
    else if (t->ph == 0) {
        printf("(%dHz %ddB %dus) ", t->hz, t->db, t->us);
    }
    else {
        printf("(%dHz %ddeg %ddB %dus) ", t->hz, t->ph, t->db, t->us);
    }
}

void output_tones(tone_t const *tones)
{
    if (!tones)
        return;

    for (tone_t const *t = tones; t->us || t->hz; ++t) {
        output_tone(t);
    }
}
