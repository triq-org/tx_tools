/** @file
    tx_tools - common, common types and helpers.

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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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
    if ((colon && (!strcmp(colon, "CU8") || !strcmp(colon, "cu8")))
            || (ext && (!strcmp(ext, ".CU8") || !strcmp(ext, ".cu8")))) {
        return FORMAT_CU8;
    }
    else if ((colon && (!strcmp(colon, "CS8") || !strcmp(colon, "cs8")))
            || (ext && (!strcmp(ext, ".CS8") || !strcmp(ext, ".cs8")))) {
        return FORMAT_CS8;
    }
    else if ((colon && (!strcmp(colon, "CS12") || !strcmp(colon, "cs12")))
            || (ext && (!strcmp(ext, ".CS12") || !strcmp(ext, ".cs12")))) {
        return FORMAT_CS12;
    }
    else if ((colon && (!strcmp(colon, "CS16") || !strcmp(colon, "cs16")))
            || (ext && (!strcmp(ext, ".CS16") || !strcmp(ext, ".cs16")))) {
        return FORMAT_CS16;
    }
    else if ((colon && (!strcmp(colon, "CS32") || !strcmp(colon, "cs32")))
            || (ext && (!strcmp(ext, ".CS32") || !strcmp(ext, ".cs32")))) {
        return FORMAT_CS32;
    }
    else if ((colon && (!strcmp(colon, "CS64") || !strcmp(colon, "cs64")))
            || (ext && (!strcmp(ext, ".CS64") || !strcmp(ext, ".cs64")))) {
        return FORMAT_CS64;
    }
    else if ((colon && (!strcmp(colon, "CF32") || !strcmp(colon, "cf32")))
            || (ext && (!strcmp(ext, ".CF32") || !strcmp(ext, ".cf32")))) {
        return FORMAT_CF32;
    }
    else if ((colon && (!strcmp(colon, "CF64") || !strcmp(colon, "cf64")))
            || (ext && (!strcmp(ext, ".CF64") || !strcmp(ext, ".cf64")))) {
        return FORMAT_CF64;
    }
    // compatibility file extensions
    else if ((colon && (!strcmp(colon, "DATA") || !strcmp(colon, "data")))
            || (ext && (!strcmp(ext, ".DATA") || !strcmp(ext, ".data")))) {
        return FORMAT_CU8;
    }
    else if ((colon && (!strcmp(colon, "CFILE") || !strcmp(colon, "cfile")))
            || (ext && (!strcmp(ext, ".CFILE") || !strcmp(ext, ".cfile")))) {
        return FORMAT_CF32;
    }
    else if ((colon && (!strcmp(colon, "COMPLEX16U") || !strcmp(colon, "complex16u")))
            || (ext && (!strcmp(ext, ".COMPLEX16U") || !strcmp(ext, ".complex16u")))) {
        return FORMAT_CU8;
    }
    else if ((colon && (!strcmp(colon, "COMPLEX16S") || !strcmp(colon, "complex16s")))
            || (ext && (!strcmp(ext, ".COMPLEX16S") || !strcmp(ext, ".complex16s")))) {
        return FORMAT_CS8;
    }
    else if ((colon && (!strcmp(colon, "COMPLEX") || !strcmp(colon, "complex")))
            || (ext && (!strcmp(ext, ".COMPLEX") || !strcmp(ext, ".complex")))) {
        return FORMAT_CF32;
    }

    return FORMAT_NONE;
}


// file helper

char const *read_text_fd(int fd, char const *file_hint)
{
    char *text = NULL;

    size_t n_offs = 0;
    ssize_t n_read = 1; // just to get us started
    while (n_read) {
        text = realloc((void *)text, n_offs + READ_CHUNK_SIZE);
        n_read = read(fd, (void *)&text[n_offs], READ_CHUNK_SIZE);
        if (n_read < 0) {
            fprintf(stderr, "Error %d reading \"%s\".\n", (int)n_read, file_hint);
            exit((int)n_read);
        }
        n_offs += (size_t)n_read;
    }

    return text;
}

char const *read_text_file(char const *filename)
{
    int fd = open(filename, O_RDONLY);
    char const *text = read_text_fd(fd, filename);
    close(fd);
    return text;
}
