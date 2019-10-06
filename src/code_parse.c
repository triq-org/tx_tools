/** @file
    tx_tools - code_parse, a simple waveform spec parser.

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

#include "code_parse.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h> /* size_t */

#include "common.h"
#include "transform.h"

static void skip_ws(char const **buf)
{
    char const *p = *buf;

    // skip whitespace and comments
    while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '#')) {
        if (*p == '#')
            while (*p && *p != '\r' && *p != '\n')
                ++p;
        if (*p)
            ++p;
    }

    *buf = p;
}

static void parse_tone(char const **buf, tone_t *tone, symbol_t *symbols)
{
    char const *p = *buf;

    // skip opening paren
    if (*p == '(')
        ++p;
    skip_ws(&p);
    // if the first character is not a number use it as reference
    if ((*p < '0' || *p > '9') && *p != '-' && *p != '.') {
        char c = *p++;
        skip_ws(&p);
        tone_t *r = symbols[(int)c].tone;
        tone->hz = r->hz;
        tone->db = r->db;
        tone->us = r->us;
    }
    else {
        tone->hz = 0;
        tone->db = -200;
        tone->us = 0;
    }

    // read stuff until closing paren
    while (p && *p != ')') {
        char *end;
        int v = (int)strtol(p, &end, 10);
        //printf("strtol '%c' %d '%c'\n", *p, v, *end);

        if (p == end) {
            // no number
            ++p;
        }
        else if (end[0] == 'H' && end[1] == 'z') {
            tone->hz = v;
            p = end + 1;
            if (tone->db == -200)
                tone->db = 0;
        }
        else if (end[0] == 'k' && end[1] == 'H' && end[2] == 'z') {
            tone->hz = v * 1000;
            p = end + 2;
            if (tone->db == -200)
                tone->db = 0;
        }
        else if (end[0] == 'd' && end[1] == 'B') {
            tone->db = v;
            p = end + 1;
            //if (tone->hz == INT32_MAX)
            //    tone->hz = 10000; // ?
        }
        else if (end[0] == 'u' && end[1] == 's') {
            tone->us = v;
            p = end + 1;
        }
        else if (end[0] == 'm' && end[1] == 's') {
            tone->us = v * 1000;
            p = end + 1;
        }
        else if (end[0] == 's') {
            tone->us = v * 1000000;
            p = end + 1;
        }
        //printf("READ %dHz %ddB %dus\n", tone->hz, tone->db, tone->us);

        ++p;
        skip_ws(&p);
    }
    if (tone->db == -200)
        tone->db = -99;
    if (p && *p == ')')
        ++p;

    //printf("TONE %dHz %ddB %dus ", tone->hz, tone->db, tone->us);
    *buf = p;
}

static void append_tone(tone_t **t, tone_t *j)
{
    (*t)->hz = j->hz;
    (*t)->db = j->db;
    (*t)->us = j->us;
    ++(*t);
}

static void append_symbol(tone_t **t, symbol_t *s)
{
    for (tone_t *j = s->tone; j->us; ++j) {
        append_tone(t, j);
    }
}

static void append_transform(tone_t **t, char const **buf, symbol_t *s)
{
    // skip opening brace
    if (**buf == '{')
        ++(*buf);

    char const *end = strchr(*buf, '}');
    if (!end)
        return;
    char *dup = strndup(*buf, (size_t)(end - *buf));
    *buf = end + 1;
    char *res = named_transform_dup(dup);
    free(dup);

    for (char const *b = res; *b; ++b) {
        append_symbol(t, &s[(int)*b]);
    }

    if (res)
        free(res);
}

static void parse_define(char const **buf, symbol_t *symbols)
{
    char const *p = *buf;

    // skip opening bracket
    if (*p == '[')
        ++p;
    skip_ws(&p);
    // use the first character as target
    char c = *p++;
    symbol_t *s = &symbols[(int)c];
    tone_t *t = s->tone;
    //printf("DEFINE %c: ", c);

    skip_ws(&p);
    // read stuff until closing bracket
    while (p && *p != ']') {
        skip_ws(&p);
        if (*p == '(') {
            parse_tone(&p, t, symbols);
            ++t;
        }
        else {
            append_symbol(&t, &symbols[(int)*p++]);
        }
        skip_ws(&p);
    }

    if (p && *p == ']')
        ++p;

    //printf("\n");
    *buf = p;
}

void output_tone(tone_t const *t)
{
    if (t->hz == 0) {
        printf("(%dus) ", t->us);
    }
    else if (t->db == 0) {
        printf("(%dHz %dus) ", t->hz, t->us);
    }
    else {
        printf("(%dHz %ddB %dus) ", t->hz, t->db, t->us);
    }
}

void output_symbol(symbol_t const *s)
{
    for (tone_t const *t = s->tone; t->us; ++t)
        output_tone(t);
}

symbol_t *parse_code(char const *code, symbol_t *symbols)
{
    if (!code)
        return symbols;

    if (!symbols) {
        // enough room for 7-bit ASCII
        symbols = calloc(128, sizeof(symbol_t));

        // preset a base tone
        symbols['~'].tone->hz = 10000;
        symbols['~'].tone->db = 0;
        symbols['~'].tone->us = 1;
    }

    tone_t *out_tone = symbols[0].tone;
    char const *p = code;

    while (*p) {
        skip_ws(&p);
        if (*p == '[') {
            // definition mode
            parse_define(&p, symbols);
        }
        else if (*p == '(') {
            // direct output
            tone_t t;
            parse_tone(&p, &t, symbols);
            append_tone(&out_tone, &t);
        }
        else if (*p == '{') {
            // hex output
            append_transform(&out_tone, &p, symbols);
        }
        else if (*p) {
            // symbol output
            char c = *p++; // symbol
            symbol_t *s = &symbols[(int)c];
            append_symbol(&out_tone, s);
        }
    }

    return symbols;
}

void free_symbols(symbol_t *symbols)
{
    if (symbols)
        free(symbols);
}

symbol_t *parse_code_file(char const *filename, symbol_t *symbols)
{
    char const *text = read_text_file(filename);
    return parse_code(text, symbols);
}
