/** @file
    tx_tools - Lime SDR backend.

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
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include <lime/LimeSuite.h>

#include "sdr_backend.h"

#define EXIT_CODE_NO_DEVICE (-2)
#define EXIT_CODE_LMS_OPEN  (-1)

#define DEFAULT_ANTENNA 1 // antenna with BW [30MHz .. 2000MHz]

static char const *query_args(char const *enum_args)
{
    if (enum_args && !strncmp(enum_args, "lime:", 5)) {
        return &enum_args[5];
    }
    return enum_args;
}

int lime_enum_devices(sdr_ctx_t *sdr_ctx, const char *enum_args)
{

    const char *query = query_args(enum_args);

    int index = -1;
    if (query && *query) {
        char *endptr = NULL;
        index = (int)strtol(query, &endptr, 0);
        if (endptr == query) {
            index = -1;
        }
    }

    int device_count = LMS_GetDeviceList(NULL);
    if (device_count < 1) {
        fprintf(stderr, "No Lime device found\n");
        return -1;
    }
    lms_info_str_t *device_list = malloc(sizeof(lms_info_str_t) * (unsigned)device_count);
    device_count = LMS_GetDeviceList(device_list);

    for (int i = 0; i < device_count; ++i) {
        sdr_dev_t *sdr_dev = &sdr_ctx->devs[sdr_ctx->devs_len++];

        if (index >= 0 && index != i) {
            continue;
        }

        char kwargs[64] = {0};
        snprintf(kwargs, sizeof(kwargs), "lime:%i", i);

        fprintf(stderr, "device[%d/%d]=%s\n", i + 1, device_count, device_list[i]);

        lms_device_t *device = NULL;
        lms_dev_info_t const *info = NULL;
        int ret = LMS_Open(&device, device_list[i], NULL);
        if (ret == 0) {
            info = LMS_GetDeviceInfo(device);
            sdr_dev->hardware_key = strdup(info->deviceName);
            //fprintf(stderr, "* deviceName = %s\n", info->deviceName);
            //fprintf(stderr, "* expansionName = %s\n", info->expansionName);
            //fprintf(stderr, "* firmwareVersion = %s\n", info->firmwareVersion);
            //fprintf(stderr, "* hardwareVersion = %s\n", info->hardwareVersion);
            //fprintf(stderr, "* protocolVersion = %s\n", info->protocolVersion);
            //fprintf(stderr, "* boardSerialNumber = %llu\n", info->boardSerialNumber);
            //fprintf(stderr, "* gatewareVersion = %s\n", info->gatewareVersion);
            //fprintf(stderr, "* gatewareTargetBoard = %s\n", info->gatewareTargetBoard);
        }

        sdr_dev->backend       = "lime";
        sdr_dev->device        = device;
        sdr_dev->dev_kwargs    = strdup(kwargs);
        //sdr_dev->dev_index     = strdup(device_list[i]);
        sdr_dev->driver_key    = "Lime";
        sdr_dev->hardware_info = strdup(device_list[i]);
    }

    free(device_list);

    return 0;
}

int lime_release_device(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev || !sdr_dev->backend || strcmp(sdr_dev->backend, "lime")) {
        return -1;
    }

    lms_device_t *device = (lms_device_t *)sdr_dev->device;
    if (!device) {
        return 0;
    }
    sdr_dev->device = NULL;

    return LMS_Close(device);
}

int lime_acquire_device(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev || !sdr_dev->backend || strcmp(sdr_dev->backend, "lime")) {
        return -1;
    }
    if (sdr_dev->device) {
        return 0;
    }

    const char *query = query_args(sdr_dev->dev_kwargs);
    fprintf(stderr, "Acquiring Lime device %s\n", query);

    int device_count = LMS_GetDeviceList(NULL);
    if (device_count < 1) {
        return EXIT_CODE_NO_DEVICE;
    }
    lms_info_str_t *device_list = malloc(sizeof(lms_info_str_t) * (unsigned)device_count);

    device_count = LMS_GetDeviceList(device_list);

    for (int i = 0; i < device_count; ++i) {
        fprintf(stderr, "device[%d/%d]=%s\n", i + 1, device_count, device_list[i]);
    }

    int32_t index = 0;
    if (query && *query)
        index = (int32_t)strtol(query, NULL, 0);

    // Use correct values
    // Use existing device
    if (index < 0 || index > device_count) {
        free(device_list);
        return -1;
    }
    fprintf(stderr, "Using device index %d [%s]\n", index, device_list[index]);

    lms_device_t *device = NULL;
    int ret              = LMS_Open(&device, device_list[index], NULL);
    free(device_list);
    if (ret) {
        return EXIT_CODE_LMS_OPEN;
    }

    sdr_dev->device = device;

    return 0;
}

int lime_free_device(sdr_dev_t *sdr_dev)
{
    if (!sdr_dev || !sdr_dev->backend || strcmp(sdr_dev->backend, "lime")) {
        return -1;
    }

    lime_release_device(sdr_dev);
    sdr_dev->backend = NULL;
    free(sdr_dev->dev_kwargs);
    free(sdr_dev->hardware_key);
    free(sdr_dev->hardware_info);

    return 0;
}

int lime_transmit_setup(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx)
{
    if (!tx) return -1;
    if (!sdr_dev) return -1;

    int ret = lime_acquire_device(sdr_dev);
    if (ret) {
        return ret;
    }

    tx->output_format = "CS16";
    tx->fullScale     = 2048.0; // LSB aligned 12-bit

    return 0;
}

int lime_transmit(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx)
{
    if (!tx) return -1;

    double gain = 0.0;
    unsigned gain_value;
    int32_t antenna = DEFAULT_ANTENNA;
    size_t channel = tx->channel;
    double sampleRate = tx->sample_rate;
    double tx_frequency = tx->center_frequency;
    double tx_bandwidth = tx->bandwidth;

    if (tx->gain_str && *tx->gain_str)
        gain = strtod(tx->gain_str, NULL);
    if (tx->antenna && *tx->antenna)
        antenna = (int32_t)strtol(tx->antenna, NULL, 0);

    // TX gain is [-12.0; 64.0]
    // "PAD": [0.0; 52.0]
    // "IAMP": [-12.0; 12.0]
    if (gain < -12.0) {
        gain = -12.0;
    }
    if (gain > 64.0) {
        gain = 64.0;
    }
    gain_value = (unsigned)(gain + 12.5); // [0; 76]
    fprintf(stderr, "Using gain %.0f dB\n", gain);

    lms_device_t *device = (lms_device_t *)sdr_dev->device;

    int ret = LMS_Reset(device);
    if (ret) {
        fprintf(stderr, "LMS_Reset %d(%s)\n", ret, LMS_GetLastErrorMessage());
    }
    ret = LMS_Init(device);
    if (ret) {
        fprintf(stderr, "LMS_Init %d(%s)\n", ret, LMS_GetLastErrorMessage());
    }

    ret = LMS_GetNumChannels(device, LMS_CH_TX);
    if (ret < 0) {
        fprintf(stderr, "LMS_GetNumChannels %d(%s)\n", ret, LMS_GetLastErrorMessage());
    }
    size_t channel_count = (size_t)ret;
    fprintf(stderr, "Tx channel count %zu\n", channel_count);
    if (channel >= channel_count) {
        channel = 0;
    }
    fprintf(stderr, "Using channel %zu\n", channel);

    int antenna_count = LMS_GetAntennaList(device, LMS_CH_TX, channel, NULL);
    fprintf(stderr, "TX%zu Channel has %d antenna(ae)\n", channel, antenna_count);
    if (antenna_count > 0) {
        lms_name_t *antenna_name = malloc(sizeof(lms_name_t) * (unsigned)antenna_count);
        lms_range_t *antenna_bw = malloc(sizeof(lms_range_t) * (unsigned)antenna_count);
        LMS_GetAntennaList(device, LMS_CH_TX, channel, antenna_name);
        for (unsigned i = 0 ; i < (unsigned)antenna_count; ++i) {
            LMS_GetAntennaBW(device, LMS_CH_TX, channel, i, antenna_bw + i);
            fprintf(stderr, "Channel %zu, antenna [%s] has BW [%lf .. %lf] (step %lf)\n", channel, antenna_name[i], antenna_bw[i].min, antenna_bw[i].max, antenna_bw[i].step);
        }
        free(antenna_name);
        free(antenna_bw);
    }
    if (antenna < 0) {
        antenna = DEFAULT_ANTENNA;
    }
    if (antenna >= antenna_count) {
        antenna = DEFAULT_ANTENNA;
    }
    // LMS_SetAntenna(device, LMS_CH_TX, channel, antenna); // SetLOFrequency should take care of selecting the proper antenna

    ret = LMS_SetGaindB(device, LMS_CH_TX, channel, gain_value);
    if (ret) {
        fprintf(stderr, "LMS_SetGaindB %d(%s)\n", ret, LMS_GetLastErrorMessage());
    }
    // Disable all other channels
    for (size_t ch = 0; ch < channel_count; ++ch) {
        if (ch != channel) {
            LMS_EnableChannel(device, LMS_CH_TX, ch, false);
        }
    }
    // LimeSuite bug workaround (needed since LimeSuite git rev 52d6129 - or v18.06.0)
    LMS_EnableChannel(device, LMS_CH_RX, 0, true);
    for (size_t ch = 0; ch < channel_count; ++ch) {
        if (ch != channel) {
            LMS_EnableChannel(device, LMS_CH_RX, ch, false);
        }
    }
    // Enable our Tx channel
    LMS_EnableChannel(device, LMS_CH_TX, channel, true);

    ret = LMS_SetLOFrequency(device, LMS_CH_TX, channel, tx_frequency);
    if (ret) {
        fprintf(stderr, "LMS_SetLOFrequency(%lf)=%d(%s)\n", tx_frequency, ret, LMS_GetLastErrorMessage());
    }

#ifdef __USE_LPF__
    lms_range_t LPFBWRange;
    LMS_GetLPFBWRange(device, LMS_CH_TX, &LPFBWRange);
    // fprintf(stderr, "TX%zu LPFBW [%lf .. %lf] (step %lf)\n", channel, LPFBWRange.min, LPFBWRange.max, LPFBWRange.step);
    double LPFBW = tx_bandwidth;
    if (LPFBW < LPFBWRange.min) {
        LPFBW = LPFBWRange.min;
    }
    if (LPFBW > LPFBWRange.max) {
        LPFBW = LPFBWRange.min;
    }
    int setLPFBW = LMS_SetLPFBW(device, LMS_CH_TX, channel, LPFBW);
    if (setLPFBW) {
        fprintf(stderr, "setLPFBW(%lf)=%d(%s)\n", LPFBW, setLPFBW, LMS_GetLastErrorMessage());
    }
    int enableLPF = LMS_SetLPF(device, LMS_CH_TX, channel, true);
    if (enableLPF) {
        fprintf(stderr, "enableLPF=%d(%s)\n", enableLPF, LMS_GetLastErrorMessage());
    }
#endif

    lms_range_t sampleRateRange;
    ret = LMS_GetSampleRateRange(device, LMS_CH_TX, &sampleRateRange);
    if (ret) {
        fprintf(stderr, "LMS_GetSampleRateRange=%d(%s)\n", ret, LMS_GetLastErrorMessage());
    } else{
        // fprintf(stderr, "sampleRateRange [%lf MHz.. %lf MHz] (step=%lf Hz)\n", sampleRateRange.min / 1e6, sampleRateRange.max / 1e6, sampleRateRange.step);
    }

    fprintf(stderr, "Set sample rate to %lf...\n", sampleRate);
    ret = LMS_SetSampleRate(device, sampleRate, 0);
    if (ret) {
        fprintf(stderr, "LMS_SetSampleRate=%d(%s)\n", ret, LMS_GetLastErrorMessage());
    }
    double actualHostSampleRate = 0.0;
    double actualRFSampleRate = 0.0;
    ret = LMS_GetSampleRate(device, LMS_CH_TX, channel, &actualHostSampleRate, &actualRFSampleRate);
    if (ret) {
        fprintf(stderr, "LMS_GetSampleRate=%d(%s)\n", ret, LMS_GetLastErrorMessage());
    } else{
        fprintf(stderr, "actualRate %lf (Host) / %lf (RF)\n", actualHostSampleRate, actualRFSampleRate);
    }

    fprintf(stderr, "Calibrating...\n");
    ret = LMS_Calibrate(device, LMS_CH_TX, channel, tx_bandwidth, 0);
    if (ret) {
        fprintf(stderr, "LMS_Calibrate=%d(%s)\n", ret, LMS_GetLastErrorMessage());
    }

    fprintf(stderr, "Setup TX stream...\n");
    lms_stream_t tx_stream = {.channel = (uint32_t)channel, .fifoSize = 1024*1024, .throughputVsLatency = 0.5, .isTx = true, .dataFmt = LMS_FMT_I12};
    ret = LMS_SetupStream(device, &tx_stream);
    if (ret) {
        fprintf(stderr, "LMS_SetupStream=%d(%s)\n", ret, LMS_GetLastErrorMessage());
    }

    //tx->block_size = (size_t)sampleRate / 100;
    size_t bufs_per_s = (size_t)sampleRate / tx->block_size;
    if (bufs_per_s < 1) {
        bufs_per_s = 1;
    }
    int16_t *sampleBuffer = malloc(tx->block_size * 2 * sizeof(int16_t));

    LMS_StartStream(&tx_stream);

    size_t loop = 0;
    size_t n_written = 0;
    while (!tx->flag_abort) {
        size_t n_samps = 0;
        ssize_t n_read = sdr_input_read(sdr_ctx, tx, sampleBuffer, &n_samps, tx->fullScale);
        loop++;
        if (n_read < 0 || 0 == (loop % bufs_per_s)) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            fprintf(stderr, "gettimeofday()=> %ld:%06ld ; ", tv.tv_sec, (long)tv.tv_usec);
            lms_stream_status_t status;
            LMS_GetStreamStatus(&tx_stream, &status); //Obtain TX stream stats
            fprintf(stderr, "TX rate:%lf MB/s\n", status.linkRate / 1e6);
        }
        if (n_read < 0) {
            fprintf(stderr, "Input end\n");
            break; // EOF
        }
        if (n_read == 0) {
            continue; // retry
        }

        ret = LMS_SendStream(&tx_stream, sampleBuffer, n_samps, NULL, 1000);
        if (ret < 0) {
            fprintf(stderr, "LMS_SendStream %d(%s)\n", ret, LMS_GetLastErrorMessage());
        }
        else {
            n_written += (size_t)ret;
        }
    }
    fprintf(stderr, "%zu samples written\n", n_written);
    fprintf(stderr, "Release TX stream...\n");

    LMS_StopStream(&tx_stream);
    LMS_DestroyStream(device, &tx_stream);

    free(sampleBuffer);

    ret = LMS_EnableChannel(device, LMS_CH_TX, channel, false);

    return ret;
}

int lime_transmit_done(sdr_cmd_t *tx)
{
    // ...

    return 0;
}
