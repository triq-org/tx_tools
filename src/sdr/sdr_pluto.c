/** @file
    tx_tools - Pluto SDR backend.

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
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <iio.h>
#ifdef HAS_AD9361_IIO
#include <ad9361.h>
#endif

#include "sdr_backend.h"

#define MHZ(x) ((long long)(x*1000000.0 + .5))

static char const *query_args(char const *enum_args)
{
    if (enum_args && !strncmp(enum_args, "pluto:", 6)) {
        return &enum_args[6];
    }
    return enum_args;
}

static int is_uri(char const *enum_args)
{
    return enum_args
            && (!strncmp(enum_args, "local:", 6)
            || !strncmp(enum_args, "xml:", 4)
            || !strncmp(enum_args, "ip:", 3)
            || !strncmp(enum_args, "usb:", 4)
            || !strncmp(enum_args, "serial:", 7));
}

static struct iio_context *create_context(char const *enum_args)
{
    struct iio_context *ctx = NULL;
    char err_str[1024];
    char const *query = query_args(enum_args);

    fprintf(stderr, "* Acquiring IIO context \"%s\"\n", query);
    if (is_uri(query)) {
        ctx = iio_create_context_from_uri(query);
    }
    else if (query && *query) {
        ctx = iio_create_network_context(query);
    }
    else {
        ctx = iio_create_default_context();
        if (ctx == NULL) {
            // fallback to common hostname
            ctx = iio_create_network_context("pluto.local");
        }
    }

    if (ctx == NULL) {
        iio_strerror(errno, err_str, sizeof(err_str));
        fprintf(stderr, "Failed creating IIO context: %s\n", err_str);
        return NULL;
    }

    fprintf(stderr, "* Acquiring devices\n");
    unsigned device_count = iio_context_get_devices_count(ctx);
    if (!device_count) {
        fprintf(stderr, "No supported PlutoSDR devices found.\n");
    }
    fprintf(stderr, "* Context has %u device(s).\n", device_count);

    return ctx;
}

int pluto_enum_devices(sdr_ctx_t *sdr_ctx, char const *enum_args)
{
    struct iio_context *ctx = create_context(enum_args);
    if (ctx == NULL) {
        return -1;
    }
    fprintf(stderr, "* Context name: %s\n", iio_context_get_name(ctx));
    fprintf(stderr, "* Context description: %s\n", iio_context_get_description(ctx));

    // assumes a single device per context and that it is a Pluto
    sdr_dev_t *sdr_dev = &sdr_ctx->devs[sdr_ctx->devs_len++];

    sdr_dev->backend             = "pluto";
    sdr_dev->device              = ctx;
    sdr_dev->dev_kwargs          = strdup(enum_args);
    sdr_dev->context_name        = strdup(iio_context_get_name(ctx));
    sdr_dev->context_description = strdup(iio_context_get_description(ctx));
    sdr_dev->driver_key          = "Pluto";
    sdr_dev->hardware_key        = "ADALM-PLUTO";

    struct iio_scan_context *scan_ctx;
    struct iio_context_info **info;
    scan_ctx = iio_create_scan_context(NULL, 0);
    if (scan_ctx) {
        ssize_t info_count = iio_scan_context_get_info_list(scan_ctx, &info);
        if (info_count > 0) {
            fprintf(stderr, "* Found %s\n", iio_context_info_get_description(info[0]));
            fprintf(stderr, "* URI %s\n", iio_context_info_get_uri(info[0]));

            //sdr_dev->dev_index     = str(i);
            //sdr_dev->dev_uri       = strdup(iio_context_info_get_uri(info[0]));
            sdr_dev->hardware_info = strdup(iio_context_info_get_description(info[0]));

            iio_context_info_list_free(info);
        }
        iio_scan_context_destroy(scan_ctx);
    }

    return 0;
}

int pluto_release_device(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev || !sdr_dev->backend || strcmp(sdr_dev->backend, "pluto")) {
        return -1;
    }

    struct iio_context *ctx = (struct iio_context *)sdr_dev->device;
    if (!ctx) {
        return 0;
    }
    sdr_dev->device = NULL;

    iio_context_destroy(ctx);
    return 0;
}

int pluto_acquire_device(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev || !sdr_dev->backend || strcmp(sdr_dev->backend, "pluto")) {
        return -1;
    }
    if (sdr_dev->device) {
        return 0;
    }

    struct iio_context *ctx = create_context(sdr_dev->dev_kwargs);
    if (ctx == NULL) {
        return -1;
    }

    sdr_dev->device = ctx;
    return 0;
}

int pluto_free_device(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev || !sdr_dev->backend || strcmp(sdr_dev->backend, "pluto")) {
        return -1;
    }

    pluto_release_device(sdr_dev);
    sdr_dev->backend = NULL;
    free(sdr_dev->dev_kwargs);
    free(sdr_dev->context_name);
    free(sdr_dev->context_description);
    free(sdr_dev->hardware_info);

    return 0;
}

int pluto_transmit_setup(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx)
{
    if (!tx) return -1;
    if (!sdr_dev) return -1;

    int ret = pluto_acquire_device(sdr_dev);
    if (ret) {
        return ret;
    }

    tx->output_format = "CS16";
    tx->fullScale     = 32768.0; // MSB aligned 12-bit

    return 0;
}

int pluto_transmit(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx)
{
    if (!tx) return -1;

    int ret = 0;
    char err_str[1024];

    long long cfg_bw_hz;    // Analog banwidth in Hz
    long long cfg_fs_hz;    // Baseband sample rate in Hz
    long long cfg_lo_hz;    // Local oscillator frequency in Hz
    char const *cfg_rfport; // Port name
    double cfg_gain_db;     // Hardware gain

    struct iio_context *ctx = (struct iio_context *)sdr_dev->device;
    struct iio_device *txdev = NULL;
    struct iio_device *phydev = NULL;
    struct iio_channel *tx0_i = NULL;
    struct iio_channel *tx0_q = NULL;
    struct iio_buffer *tx_buffer = NULL;

    // TX stream default config
    cfg_fs_hz = (long long)tx->sample_rate;
    int interpolation = false;
    // The minimum sampling rate that can be set without enabling the decimation/interpolation of the FIRs is 2.083 MSPS,
    // the minimum ADC rate is 25 MHz and the maximum decimation of the half band filters is 12.
    /*
    We have three decimation/interpolation stages and the FIR stage.
    The decimation/interpolation can be 2,bypass at two stages and one stage with 3,2,bypass.
    The FIR can be 4,2,1,bypass. The ADC/DAC minimum rate is 25 MHz.
    Without enabling the decimation/interpolation of the FIRs we can go down to 25M/12 = 2.083 MSps.
    Then we scale by 8, thus should be able to go down to 25M/12/8 = 260.417kSps without needing a FIR.
    iio_attr --auto -d ad9361-phy tx_path_rates
    iio_attr --auto -o -c ad9361-phy voltage0 sampling_frequency [<rate>]
    iio_attr --auto -o -c cf-ad9361-dds-core-lpc voltage0 sampling_frequency [<rate|rate/8>]
    will accept lower rates (down to 25M/48 and 25M/48/8) but those will enable FIR and probably not work without loading a FIR.
    */
    if (cfg_fs_hz < (25e6 / 12)) {
        if (cfg_fs_hz * 8 < (25e6 / 12)) {
            fprintf(stderr, "Error low sample rate needs FIR and is not supported.\n");
            return -1;
        }

        interpolation = true;
        cfg_fs_hz = cfg_fs_hz * 8;
    }

    cfg_lo_hz = (long long)tx->center_frequency;

    cfg_rfport = tx->antenna;
    // phy_chn "rf_port_select_available" value: A B
    if (!cfg_rfport || !*cfg_rfport) {
        cfg_rfport = "A";
    }

    if (!tx->gain_str || !*tx->gain_str) {
        cfg_gain_db = -20.0;
    } else {
        char const *gain_str = tx->gain_str;
        if (!strncmp(gain_str, "PGA=", 4))
            gain_str = &gain_str[4];
        cfg_gain_db = atof(gain_str);
        // IIO TX gain is attenuation [0; -89.75]
        // flip and clamp to match Soapy
        if (cfg_gain_db >= 0)
            cfg_gain_db = cfg_gain_db - 89.0;
    }
    // phy_chn "hardwaregain_available" value: [-89.750000 0.250000 0.000000]
    if (cfg_gain_db > 0.0) cfg_gain_db = 0.0;
    if (cfg_gain_db < -89.0) cfg_gain_db = -89.0;

    cfg_bw_hz = (long long)tx->bandwidth;
    if (cfg_bw_hz <= 0) cfg_bw_hz = cfg_fs_hz;
    // phy_chn "rf_bandwidth_available" value: [200000 1 40000000]
    if (cfg_bw_hz > MHZ(40.0)) cfg_bw_hz = MHZ(40.0);
    if (cfg_bw_hz < MHZ(0.2)) cfg_bw_hz = MHZ(0.2);

    fprintf(stderr, "* Acquiring TX device\n");
    txdev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");
    if (txdev == NULL) {
        iio_strerror(errno, err_str, sizeof(err_str));
        fprintf(stderr, "Error opening PlutoSDR TX device: %s\n", err_str);
        ret = -1;
        goto error_exit;
    }

    iio_device_set_kernel_buffers_count(txdev, 8);

    phydev = iio_context_find_device(ctx, "ad9361-phy");
    if (phydev == NULL) {
        iio_strerror(errno, err_str, sizeof(err_str));
        fprintf(stderr, "Error opening PlutoSDR PHY device: %s\n", err_str);
        ret = -1;
        goto error_exit;
    }

    struct iio_channel* phy_chn = iio_device_find_channel(phydev, "voltage0", true);
    iio_channel_attr_write(phy_chn, "rf_port_select", cfg_rfport);
    iio_channel_attr_write_longlong(phy_chn, "rf_bandwidth", cfg_bw_hz);
    iio_channel_attr_write_longlong(phy_chn, "sampling_frequency", cfg_fs_hz);
    iio_channel_attr_write_double(phy_chn, "hardwaregain", cfg_gain_db);

    iio_channel_attr_write_bool(
        iio_device_find_channel(phydev, "altvoltage0", true), "powerdown", true); // Turn OFF RX LO

    iio_channel_attr_write_longlong(
        iio_device_find_channel(phydev, "altvoltage1", true), "frequency", cfg_lo_hz); // Set TX LO frequency

    fprintf(stderr, "* Initializing streaming channels\n");
    tx0_i = iio_device_find_channel(txdev, "voltage0", true);
    if (!tx0_i)
        tx0_i = iio_device_find_channel(txdev, "altvoltage0", true);
    iio_channel_attr_write_longlong(tx0_i, "sampling_frequency", interpolation ? cfg_fs_hz / 8 : cfg_fs_hz);

    tx0_q = iio_device_find_channel(txdev, "voltage1", true);
    if (!tx0_q)
        tx0_q = iio_device_find_channel(txdev, "altvoltage1", true);

    fprintf(stderr, "* Enabling IIO streaming channels\n");
    iio_channel_enable(tx0_i);
    iio_channel_enable(tx0_q);

#ifdef HAS_AD9361_IIO
    ad9361_set_bb_rate(phydev, (unsigned long)cfg_fs_hz);
#endif

    fprintf(stderr, "* Creating TX buffer\n");

    tx_buffer = iio_device_create_buffer(txdev, tx->block_size, false);
    if (!tx_buffer) {
        fprintf(stderr, "Could not create TX buffer.\n");
        ret = -1;
        goto error_exit;
    }

    iio_channel_attr_write_bool(
        iio_device_find_channel(phydev, "altvoltage1", true), "powerdown", false); // Turn ON TX LO

    short *ptx_buffer = (short *)iio_buffer_start(tx_buffer);

    fprintf(stderr, "* Transmit starts...\n");
    // Keep writing samples while there is more data to send and no failures have occurred.
    size_t n_written = 0;
    while (!tx->flag_abort) {
        size_t n_samps = 0;
        ssize_t n_read = sdr_input_read(sdr_ctx, tx, ptx_buffer, &n_samps, tx->fullScale);

        if (n_read < 0) {
            fprintf(stderr, "Input end\n");
            break; // EOF
        }
        if (n_read == 0) {
            continue; // retry
        }

        // Schedule TX buffer
        ssize_t ntx = iio_buffer_push(tx_buffer);
        if (ntx < 0) {
            fprintf(stderr, "Error pushing buf %zd\n", ntx);
            break;
        }
        else {
            n_written += n_samps; // or: ntx / 2 * size
        }
    }
    fprintf(stderr, "%zu samples written\n", n_written);
    fprintf(stderr, "* Transmit ended.\n");

error_exit:
    iio_channel_attr_write_bool(
        iio_device_find_channel(phydev, "altvoltage1", true), "powerdown", true); // Turn OFF TX LO

    if (tx_buffer) { iio_buffer_destroy(tx_buffer); }
    if (tx0_i) { iio_channel_disable(tx0_i); }
    if (tx0_q) { iio_channel_disable(tx0_q); }
    return ret;
}

int pluto_transmit_done(sdr_cmd_t *tx)
{
    // ...

    return 0;
}
