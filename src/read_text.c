/** @file
    tx_tools - read_text, read text files.

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

#include "read_text.h"

#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// file helper

char *read_text_fd(int fd, char const *file_hint)
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

char *read_text_file(char const *filename)
{
    int fd = open(filename, O_RDONLY);
    char *text = read_text_fd(fd, filename);
    close(fd);
    return text;
}
