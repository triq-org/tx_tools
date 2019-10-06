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

#ifndef INCLUDE_COMMON_H_
#define INCLUDE_COMMON_H_

#include <stdint.h>

#define READ_CHUNK_SIZE 8192

typedef union frame {
    uint8_t *u8;
    int8_t *s8;
    int16_t *s16;
    int32_t *s32;
    float *f32;
} frame_t;

enum sample_format {
    FORMAT_NONE,
    FORMAT_CU8,
    FORMAT_CS8,
    FORMAT_CS16,
    FORMAT_CF32,
};

// helper for sample formats

char const *sample_format_str(enum sample_format format);

enum sample_format file_info(char **path);

// helper to get file contents

char const *read_text_fd(int fd, char const *file_hint);

char const *read_text_file(char const *filename);

#endif /* INCLUDE_COMMON_H_ */
