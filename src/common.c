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

// format helper

char const *sample_format_str(enum sample_format format)
{
    switch (format) {
    case FORMAT_NONE:
        return "none";
    case FORMAT_CU8:
        return "CU8";
    case FORMAT_CS8:
        return "CS8";
    case FORMAT_CS16:
        return "CS16";
    case FORMAT_CF32:
        return "CF32";
    }
    return "unknown";
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
    else if ((colon && (!strcmp(colon, "CS16") || !strcmp(colon, "cs16")))
            || (ext && (!strcmp(ext, ".CS16") || !strcmp(ext, ".cs16")))) {
        return FORMAT_CS16;
    }
    else if ((colon && (!strcmp(colon, "CF32") || !strcmp(colon, "cf32")))
            || (ext && (!strcmp(ext, ".CF32") || !strcmp(ext, ".cf32")))) {
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
