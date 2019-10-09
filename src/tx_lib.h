/** @file
    tx_tools - tx_lib, common TX functions.

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
#include <stdint.h>
#include <unistd.h>

#include "sample.h"

typedef struct dev_info {
    char *dev_kwargs;
    char *driver_key;
    char *hardware_key;
    char *hardware_info;
} dev_info_t;

typedef struct tx_ctx {
    size_t devs_len;
    void *devs;
    dev_info_t *dev_infos;
} tx_ctx_t;

typedef struct tx_cmd {
    // device selection
    char const *dev_query;
    // device setup
    char const *gain_str;
    char const *antenna;
    size_t channel;
    // rf setup
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
    size_t buffer_offset;
    size_t buffer_size;
    // input from text (OOK, ASK, FSK, PSK)
    int freq_mark;   ///< frequency offset for mark
    int freq_space;  ///< frequency offset for space, 0 otherwise
    int att_mark;    ///< attenuation for mark
    int att_space;   ///< attenuation for space, 0 otherwise
    int phase_mark;  ///< phase offset for mark, 0 otherwise
    int phase_space; ///< phase offset for space, 0 otherwise
    char const *pulses;
    // private
    int flag_abort; ///< private
    frame_t conv_buf;
} tx_cmd_t;

/// Is the format string a valid input format?
int tx_valid_input_format(char const *format);

/// Is the format string a valid output format?
int tx_valid_output_format(char const *format);

/// Parse SoapySDR format string.
char const *tx_parse_sample_format(char const *format);

/// Enumerate all devices.
void tx_enum_devices(tx_ctx_t *tx_ctx, const char *enum_args);

/// Unmake all devices.
void tx_free_devices(tx_ctx_t *tx_ctx);

/// Transmit data.
int tx_transmit(tx_ctx_t *tx_ctx, tx_cmd_t *tx);

/// Print transmit data (debug).
void tx_print(tx_ctx_t *tx_ctx, tx_cmd_t *tx);

// input processing

/// Prepare input data.
int tx_input_init(tx_ctx_t *tx_ctx, tx_cmd_t *tx);

/// Discard and close input data.
int tx_input_destroy(tx_ctx_t *tx_ctx, tx_cmd_t *tx);

/// Reset input data.
int tx_input_reset(tx_ctx_t *tx_ctx, tx_cmd_t *tx);

/// Read input data.
ssize_t tx_input_read(tx_ctx_t *tx_ctx, tx_cmd_t *tx, void *buf, size_t *out_samps, double fullScale);

#endif /* INCLUDE_TXLIB_H_ */
