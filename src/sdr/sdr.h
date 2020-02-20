/** @file
    tx_tools - SDR backends.

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

#ifndef INCLUDE_SDR_H_
#define INCLUDE_SDR_H_

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

typedef struct sdr_dev {
    char const *backend;
    void *device;
    char *dev_kwargs;
    char *context_name;
    char *context_description;
    char *driver_key;
    char *hardware_key;
    char *hardware_info;
} sdr_dev_t;

typedef struct sdr_ctx {
    size_t devs_len;
    sdr_dev_t *devs;
} sdr_ctx_t;

typedef union sdr_buffer {
    uint8_t *u8;
    int8_t *s8;
    uint16_t *u16;
    int16_t *s16;
    uint32_t *u32;
    int32_t *s32;
    uint64_t *u64;
    int64_t *s64;
    float *f32;
    double *f64;
} sdr_buffer_t;

typedef struct sdr_cmd {
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
    // private
    double fullScale;
    int flag_abort; ///< private
    sdr_buffer_t conv_buf;
} sdr_cmd_t;

/// Show all available backends.
char const *sdr_ctx_available_backends(void);

// API: device support

/// Enumerate all devices and acquire them.
int sdr_ctx_enum_devices(sdr_ctx_t *sdr_ctx, const char *enum_args);

/// Release all devices.
int sdr_ctx_release_devices(sdr_ctx_t *sdr_ctx);

/// Release and free all devices.
int sdr_ctx_free_devices(sdr_ctx_t *sdr_ctx);

/// Release and free a given device.
int sdr_dev_free(sdr_dev_t *sdr_dev);

// API: commands

/// Find a given device.
sdr_dev_t *sdr_ctx_find_device(sdr_ctx_t *sdr_ctx, char const *kwargs);

/// Release a given device.
int sdr_dev_release(sdr_dev_t *sdr_dev);

/// Acquire a given device.
int sdr_dev_acquire(sdr_dev_t *sdr_dev);

/// Acquire device if needed and setup for transmit.
int sdr_tx_setup(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx);

/// Transmit data.
int sdr_tx(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx);

/// Free transmit data.
int sdr_tx_free(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx);

#endif /* INCLUDE_SDR_H_ */
