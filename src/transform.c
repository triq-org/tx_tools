/** @file
    tx_tools - transform, data transformation helpers.

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef _MSC_VER
#include <string.h>
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#else
#include <strings.h>
#endif

#include "transform.h"

typedef size_t (*transform_fn)(char const *data, char *buf, size_t size);

// return the required size including the terminating null, zero if data is NULL.
size_t encode_mc_thomas(char const *data, char *buf, size_t size)
{
    if (!data)
        return 0;

    size_t len = 0;
    for (; *data; ++data) {
        if (*data == ' ' || *data == '\t' || *data == '\r' || *data == '\n')
            continue;
        len += 2;
        if (buf && len <= size) {
            if (*data == '0') {
                *buf++ = '0';
                *buf++ = '1';
            }
            else {
                *buf++ = '1';
                *buf++ = '0';
            }
        }
    }
    if (buf && len < size)
        *buf = '\0';
    return len + 1;
}

// return the required size including the terminating null, zero if data is NULL.
size_t encode_mc_ieee(char const *data, char *buf, size_t size)
{
    if (!data)
        return 0;

    size_t len = 0;
    for (; *data; ++data) {
        if (*data == ' ' || *data == '\t' || *data == '\r' || *data == '\n')
            continue;
        len += 2;
        if (buf && len <= size) {
            if (*data == '0') {
                *buf++ = '1';
                *buf++ = '0';
            }
            else {
                *buf++ = '0';
                *buf++ = '1';
            }
        }
    }
    if (buf && len < size)
        *buf = '\0';
    return len + 1;
}

// return the required size including the terminating null, zero if data is NULL.
static size_t encode_dmc(char const *data, char *buf, size_t size, int state)
{
    if (!data)
        return 0;

    size_t len = 0;
    for (; *data; ++data) {
        if (*data == ' ' || *data == '\t' || *data == '\r' || *data == '\n')
            continue;
        len += 2;
        if (buf && len <= size) {
            if (*data == '0') {
                *buf++ = state ? '0' : '1';
                *buf++ = state ? '1' : '0';
            }
            else {
                *buf++ = state ? '0' : '1';
                *buf++ = state ? '0' : '1';
                state = !state;
            }
        }
    }
    if (buf && len < size)
        *buf = '\0';
    return len + 1;
}

// return the required size including the terminating null, zero if data is NULL.
size_t encode_dmc_lo(char const *data, char *buf, size_t size)
{
    return encode_dmc(data, buf, size, 1);
}

// return the required size including the terminating null, zero if data is NULL.
size_t encode_dmc_hi(char const *data, char *buf, size_t size)
{
    return encode_dmc(data, buf, size, 0);
}

// return the required size including the terminating null, zero if hex is NULL.
size_t encode_ascii(char const *data, char *buf, size_t size)
{
    if (!data)
        return 0;

    size_t len = 0;

    for (; *data; ++data) {
        if (*data == ' ' || *data == '\t' || *data == '\r' || *data == '\n')
            continue;

        len += 8;
        if (buf && len <= size) {
            *buf++ = (*data & 0x80) ? '1' : '0';
            *buf++ = (*data & 0x40) ? '1' : '0';
            *buf++ = (*data & 0x20) ? '1' : '0';
            *buf++ = (*data & 0x10) ? '1' : '0';
            *buf++ = (*data & 0x08) ? '1' : '0';
            *buf++ = (*data & 0x04) ? '1' : '0';
            *buf++ = (*data & 0x02) ? '1' : '0';
            *buf++ = (*data & 0x01) ? '1' : '0';
        }
    }
    if (buf && len < size)
        *buf = '\0';
    return len + 1;
}

// return the required size including the terminating null, zero if data is NULL.
size_t encode_hex(char const *data, char *buf, size_t size)
{
    if (!data)
        return 0;

    size_t len = 0;

    for (; *data; ++data) {
        if (*data == ' ' || *data == '\t' || *data == '\r' || *data == '\n')
            continue;

        int v = -1;
        if (*data >= '0' && *data <= '9') {
            v = *data - '0';
        }
        else if (*data >= 'A' && *data <= 'F') {
            v = *data - 'A' + 10;
        }
        else if (*data >= 'a' && *data <= 'f') {
            v = *data - 'a' + 10;
        }
        else {
            fprintf(stderr, "Not a valid hex char: \"%c\" (%d)\n", *data, *data);
            continue;
        }
        len += 4;
        if (buf && len <= size) {
            *buf++ = (v & 0x8) ? '1' : '0';
            *buf++ = (v & 0x4) ? '1' : '0';
            *buf++ = (v & 0x2) ? '1' : '0';
            *buf++ = (v & 0x1) ? '1' : '0';
        }
    }
    if (buf && len < size)
        *buf = '\0';
    return len + 1;
}

static char *transform_dup(transform_fn fn, char const *arg)
{
    size_t len = fn(arg, NULL, 0);
    char *buf = malloc(len);
    fn(arg, buf, len);
    return buf;
}

char *named_transform_dup(char const *arg)
{
    if (!strncasecmp(arg, "ASCII", 5)) {
        return transform_dup(encode_ascii, arg + 5);
    }
    else if (!strncasecmp(arg, "DMC", 3)) {
        char *bits = transform_dup(encode_hex, arg + 3);
        char *ret = transform_dup(encode_dmc_hi, bits);
        free(bits);
        return ret;
    }
    else if (!strncasecmp(arg, "MC", 2)) {
        char *bits = transform_dup(encode_hex, arg + 2);
        char *ret = transform_dup(encode_mc_thomas, bits);
        free(bits);
        return ret;
    }
    else if (!strncasecmp(arg, "IMC", 3)) {
        char *bits = transform_dup(encode_hex, arg + 3);
        char *ret = transform_dup(encode_mc_ieee, bits);
        free(bits);
        return ret;
    }
    else if (!strncasecmp(arg, "HEX", 3)) {
        return transform_dup(encode_hex, arg + 3);
    }
    else { // HEX as default
        return transform_dup(encode_hex, arg);
    }
}

#if defined(PROG_DMC) || defined(PROG_MC) || defined(PROG_IMC) || defined(PROG_HEX) || defined(PROG_ASCII)
int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
#if defined(PROG_ASCII)
        char *buf = transform_dup(encode_ascii, argv[i]);
#elif defined(PROG_HEX)
        char *buf = transform_dup(encode_hex, argv[i]);
#else
        char *bits = transform_dup(encode_hex, argv[i]);
#if defined(PROG_DMC)
        char *buf = transform_dup(encode_dmc_hi, bits);
#elif defined(PROG_MC)
        char *buf = transform_dup(encode_mc_thomas, bits);
#elif defined(PROG_IMC)
        char *buf = transform_dup(encode_mc_ieee, bits);
#endif
        free(bits);
#endif
        printf("%s\n", buf);
        free(buf);
    }
}
#endif
