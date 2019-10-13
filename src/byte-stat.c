/** @file
    tx_tools - byte-stat, show byte histogram of raw binary files.

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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define READ_CHUNK_SIZE 8192

static void print_stat(char *filename)
{
    unsigned tab4l[16]   = {0};
    unsigned tab4h[16]   = {0};
    unsigned tab8[256]   = {0};
    unsigned tab16l[256] = {0};
    unsigned tab16h[256] = {0};

    uint8_t buf[READ_CHUNK_SIZE];

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error %d opening \"%s\": ", fd, filename);
        perror(NULL);
        exit(-fd);
    }

    size_t n_offs = 0;
    ssize_t n_read = 1; // just to get us started
    while (n_read) {
        n_read = read(fd, buf, READ_CHUNK_SIZE);
        if (n_read < 0) {
            fprintf(stderr, "Error %d reading \"%s\": ", (int)n_read, filename);
            perror(NULL);
            exit(-(int)n_read);
        }

        // sum
        for (size_t i = 0; i < (size_t)n_read; ++i) {
            tab4h[buf[i] >> 4] += 1;
            tab4l[buf[i] & 0xf] += 1;

            tab8[buf[i]] += 1;
        }
        for (size_t i = 0; i < (size_t)n_read / 2; ++i) {
            tab16l[buf[i * 2 + 0]] += 1;
            tab16h[buf[i * 2 + 1]] += 1;
        }

        n_offs += (size_t)n_read;
    }

    close(fd);

    printf("%zu bytes in \"%s\" are (percentages, 100=uniform distribution)\n", n_offs, filename);

    printf("\n4-bit wide low nibble:\n");
    for (int i = 0; i < 16; ++i)
        printf("%4zu", 100 * tab4l[i] * 16 / n_offs);
    printf("\n");

    printf("\n4-bit wide high nibble:\n");
    for (int i = 0; i < 16; ++i)
        printf("%4zu", 100 * tab4h[i] * 16 / n_offs);
    printf("\n");

    printf("\n8-bit wide bytes:\n");
    for (int i = 0; i < 256; ++i)
        printf("%c%4zu", i % 16 ? ' ' : '\n', 100 * tab8[i] * 256 / n_offs);
    printf("\n");

    printf("\n16-bit wide low byte:\n");
    for (int i = 0; i < 256; ++i)
        printf("%c%4zu", i % 16 ? ' ' : '\n', 100 * tab16l[i] * 256 / n_offs * 2);
    printf("\n");

    printf("\n16-bit wide high byte:\n");
    for (int i = 0; i < 256; ++i)
        printf("%c%4zu", i % 16 ? ' ' : '\n', 100 * tab16h[i] * 256 / n_offs * 2);
    printf("\n");
}

int main(int argc, char *argv[])
{
    for (int i = 1 ; i < argc; ++i)
        print_stat(argv[i]);

    return 0;
}
