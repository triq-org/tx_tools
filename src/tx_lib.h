/** @file
    tx_tools - tx_lib, common TX functions

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

#ifndef INCLUDE_TXLIB_H_
#define INCLUDE_TXLIB_H_

#include <stddef.h>

typedef struct tx_ctx {
    size_t devs_len;
    void *devs;
} tx_ctx_t;

typedef enum stream_format {
    CU8  = 0x43553800,
    CS8  = 0x43533800,
    CS12 = 0x43533132,
    CS16 = 0x43533136,
    CF32 = 0x43463332,
} stream_format_t;

typedef struct tx_cmd {
    // device selection
    char const *dev_query;
    // device setup
    char const *gain_str;
    char const *antenna;
    size_t channel;
    double ppm_error;
    double center_frequency;
    double sample_rate;
    double bandwidth;
    double master_clock_rate;
    char const *output_format; ///< force output format if set
    size_t block_size;         ///< force output block size if set
    // transmit control
    unsigned initial_delay;
    unsigned repeats;
    unsigned repeat_delay;
    unsigned loops;
    unsigned loop_delay;
    // input from file descriptor
    char const *input_format;
    int stream_fd;
    size_t samples_to_write;
    // input from buffer
    void *stream_buffer;
    size_t buffer_size;
    // input from text
    char const *pulses;
} tx_cmd_t;

/// enumerate all devices
void tx_enum_devices(tx_ctx_t *tx_ctx, const char *enum_args);

/// unmake all devices
void tx_free_devices(tx_ctx_t *tx_ctx);

/// transmit data
int tx_transmit(tx_ctx_t *tx_ctx, tx_cmd_t *tx);

#endif /* INCLUDE_TXLIB_H_ */
