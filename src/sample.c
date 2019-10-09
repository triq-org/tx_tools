/** @file
    tx_tools - sample, sample format types and helpers.

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

#include "sample.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef _MSC_VER
#include <string.h>
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#else
#include <strings.h>
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// format is 3-4 chars (plus null), compare as int.
static int is_format_equal(const void *a, const void *b)
{
    return *(const uint32_t *)a == *(const uint32_t *)b;
}

// format helper

size_t sample_format_length(enum sample_format format)
{
    switch (format) {
    case FORMAT_NONE:
        return 2 * sizeof(uint8_t);
    case FORMAT_CU8:
        return 2 * sizeof(uint8_t);
    case FORMAT_CS8:
        return 2 * sizeof(int8_t);
    case FORMAT_CS12:
        return 3 * sizeof(uint8_t);
    case FORMAT_CS16:
        return 2 * sizeof(int16_t);
    case FORMAT_CS32:
        return 2 * sizeof(int32_t);
    case FORMAT_CS64:
        return 2 * sizeof(int64_t);
    case FORMAT_CF32:
        return 2 * sizeof(float);
    case FORMAT_CF64:
        return 2 * sizeof(double);
    }
    return 2 * sizeof(uint8_t);
}

char const *sample_format_str(enum sample_format format)
{
    switch (format) {
    case FORMAT_NONE:
        return "none";
    case FORMAT_CU8:
        return "CU8";
    case FORMAT_CS8:
        return "CS8";
    case FORMAT_CS12:
        return "CS12";
    case FORMAT_CS16:
        return "CS16";
    case FORMAT_CS32:
        return "CS32";
    case FORMAT_CS64:
        return "CS64";
    case FORMAT_CF32:
        return "CF32";
    case FORMAT_CF64:
        return "CF64";
    }
    return "unknown";
}

enum sample_format sample_format_for(char const *format)
{
    if (!format || !*format)
        return FORMAT_NONE;
    else if (is_format_equal(format, "CU8"))
        return FORMAT_CU8;
    else if (is_format_equal(format, "CS8"))
        return FORMAT_CS8;
    else if (is_format_equal(format, "CS12"))
        return  FORMAT_CS12;
    else if (is_format_equal(format, "CS16"))
        return  FORMAT_CS16;
    else if (is_format_equal(format, "CS32"))
        return  FORMAT_CS32;
    else if (is_format_equal(format, "CS64"))
        return  FORMAT_CS64;
    else if (is_format_equal(format, "CF32"))
        return  FORMAT_CF32;
    else if (is_format_equal(format, "CF64"))
        return  FORMAT_CF64;
    return FORMAT_NONE;
}

enum sample_format sample_format_parse(char const *format)
{
    if (!format || !*format)
        return FORMAT_NONE;
    // skip leading non-alphanums
    char const *p = format;
    while (*p && (*p < '0' || *p > '9') && (*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z'))
        ++p;
    if (strcasecmp(p, "CU8") == 0)
        return FORMAT_CU8;
    else if (strcasecmp(p, "CS8") == 0)
        return FORMAT_CS8;
    else if (strcasecmp(p, "CS12") == 0)
        return FORMAT_CS12;
    else if (strcasecmp(p, "CS16") == 0)
        return FORMAT_CS16;
    else if (strcasecmp(p, "CS32") == 0)
        return FORMAT_CS32;
    else if (strcasecmp(p, "CS64") == 0)
        return FORMAT_CS64;
    else if (strcasecmp(p, "CF32") == 0)
        return FORMAT_CF32;
    else if (strcasecmp(p, "CF64") == 0)
        return FORMAT_CF64;
    else
        return FORMAT_NONE;
}

enum sample_format file_info(char **path)
{
    // return the last colon not followed by a backslash, otherwise NULL
    char *colon = NULL;
    char *next = strchr(*path, ':');
    while (next && next[1] != '\\') {
        colon = next;
        next = strchr(next + 1, ':');
    }
    if (colon) {
        *colon = '\0';
        next = colon + 1;
        colon = *path;
        *path = next;
    }

    char const *ext = strrchr(*path, '.');
    if ((colon && !strcasecmp(colon, "CU8"))
            || (ext && !strcasecmp(ext, ".CU8"))) {
        return FORMAT_CU8;
    }
    else if ((colon && !strcasecmp(colon, "CS8"))
            || (ext && !strcasecmp(ext, ".CS8"))) {
        return FORMAT_CS8;
    }
    else if ((colon && !strcasecmp(colon, "CS12"))
            || (ext && !strcasecmp(ext, ".CS12"))) {
        return FORMAT_CS12;
    }
    else if ((colon && !strcasecmp(colon, "CS16"))
            || (ext && !strcasecmp(ext, ".CS16"))) {
        return FORMAT_CS16;
    }
    else if ((colon && !strcasecmp(colon, "CS32"))
            || (ext && !strcasecmp(ext, ".CS32"))) {
        return FORMAT_CS32;
    }
    else if ((colon && !strcasecmp(colon, "CS64"))
            || (ext && !strcasecmp(ext, ".CS64"))) {
        return FORMAT_CS64;
    }
    else if ((colon && !strcasecmp(colon, "CF32"))
            || (ext && !strcasecmp(ext, ".CF32"))) {
        return FORMAT_CF32;
    }
    else if ((colon && !strcasecmp(colon, "CF64"))
            || (ext && !strcasecmp(ext, ".CF64"))) {
        return FORMAT_CF64;
    }
    // compatibility file extensions
    else if ((colon && !strcasecmp(colon, "DATA"))
            || (ext && !strcasecmp(ext, ".DATA"))) {
        return FORMAT_CU8;
    }
    else if ((colon && !strcasecmp(colon, "CFILE"))
            || (ext && !strcasecmp(ext, ".CFILE"))) {
        return FORMAT_CF32;
    }
    else if ((colon && !strcasecmp(colon, "COMPLEX16U"))
            || (ext && !strcasecmp(ext, ".COMPLEX16U"))) {
        return FORMAT_CU8;
    }
    else if ((colon && !strcasecmp(colon, "COMPLEX16S"))
            || (ext && !strcasecmp(ext, ".COMPLEX16S"))) {
        return FORMAT_CS8;
    }
    else if ((colon && !strcasecmp(colon, "COMPLEX"))
            || (ext && !strcasecmp(ext, ".COMPLEX"))) {
        return FORMAT_CF32;
    }

    return FORMAT_NONE;
}
