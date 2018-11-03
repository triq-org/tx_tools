/*
 * tx_sdr, play data through SoapySDR TX
 * Copyright (C) 2017 by Christian Zuckschwerdt <zany@triq.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#ifdef _MSC_VER
#include "getopt/getopt.h"
#define F_OK 0
#endif
#endif
#ifndef _MSC_VER
#include <unistd.h>
#include <getopt.h>
#endif

#include "argparse.h"
#include "convenience.h"
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#define DEFAULT_SAMPLE_RATE 2048000
#define DEFAULT_BUF_LENGTH (1 * 16384)
#define MINIMAL_BUF_LENGTH 512
#define MAXIMAL_BUF_LENGTH (256 * 16384)

// format is 3-4 chars (plus null), compare as int.
static int is_format_equal(const void *a, const void *b)
{
    return *(const uint32_t *)a == *(const uint32_t *)b;
}

static void usage(void)
{
    fprintf(stderr,
            "tx_sdr (based on rtl_sdr), an I/Q player for SoapySDR devices\n\n"
            "Usage:\t -f frequency_to_tune_to [Hz]\n"
            "\t[-s samplerate (default: 2048000 Hz)]\n"
            "\t[-d device key/value query (ex: 0, 1, driver=lime, driver=hackrf)]\n"
            "\t[-g tuner gain(s) (ex: 20, 40, PAD=-10)]\n"
            "\t[-a antenna (ex: BAND2)]\n"
            "\t[-C channel]\n"
            "\t[-K master clock rate (ex: 80M)]\n"
            "\t[-B bandwidth (ex: 5M)]\n"
            "\t[-p ppm_error (default: 0)]\n"
            "\t[-b output_block_size (default: 16 * 16384)]\n"
            "\t[-n number of samples to write (default: 0, infinite)]\n"
            "\t[-l loops count of times to write (default: 0, use -1 for infinite)]\n"
            "\t[-F force input format, CU8|CS8|CS16|CF32 (default: use file extension)]\n"
            "\tfilename (a '-' reads samples from stdin)\n\n");
    exit(1);
}

static int do_exit = 0;

#ifdef _WIN32
BOOL WINAPI
sighandler(int signum)
{
    if (CTRL_C_EVENT == signum) {
        fprintf(stderr, "Signal caught, exiting!\n");
        do_exit = 1;
        return TRUE;
    }
    return FALSE;
}
#else
static void sighandler(int signum)
{
    fprintf(stderr, "Signal caught, exiting!\n");
    do_exit = 1;
}
#endif

int main(int argc, char **argv)
{
#ifndef _WIN32
    struct sigaction sigact;
#endif
    size_t samples_to_write = 0;
    SoapySDRDevice *dev = NULL;
    SoapySDRStream *stream = NULL;
    char *filename = NULL;
    int loops = 0;
    int r, opt;
    char *gain_str = NULL;
    char *antenna = NULL;
    int ppm_error = 0;
    int fd;
    int16_t *buf16;
    uint8_t *buf8 = NULL;
    float *fbuf = NULL; // assumed 32-bit
    char *dev_query = "";
    double frequency = 0.0;
    double fullScale = 2048.0;
    double masterclk = 0.0;
    double bandwidth = 0.0;
    double samp_rate = DEFAULT_SAMPLE_RATE;
    size_t out_block_size = DEFAULT_BUF_LENGTH;
    const char *input_format = NULL;
    const char *output_format;

    while ((opt = getopt(argc, argv, "d:f:g:a:s:C:K:B:b:n:l:p:F:")) != -1) {
        switch (opt) {
        case 'd':
            dev_query = optarg;
            break;
        case 'f':
            frequency = atofs(optarg);
            break;
        case 'g':
            gain_str = optarg;
            break;
        case 'a':
            antenna = optarg;
            break;
        case 's':
            samp_rate = atofs(optarg);
            break;
        case 'K':
            masterclk = atofs(optarg);
            break;
        case 'B':
            bandwidth = atofs(optarg);
            break;
        case 'p':
            ppm_error = atoi(optarg);
            break;
        case 'b':
            out_block_size = (size_t)atof(optarg);
            break;
        case 'n':
            // each sample is one I/Q pair (half the count of I and Q bytes)
            samples_to_write = (size_t)atofs(optarg);
            break;
        case 'l':
            loops = atoi(optarg);
            break;
        case 'F':
            if (strcasecmp(optarg, "CU8") == 0) {
                input_format = SOAPY_SDR_CU8;
            } else if (strcasecmp(optarg, "CS8") == 0) {
                input_format = SOAPY_SDR_CS8;
            } else if (strcasecmp(optarg, "CS16") == 0) {
                input_format = SOAPY_SDR_CS16;
            } else if (strcasecmp(optarg, "CF32") == 0) {
                input_format = SOAPY_SDR_CF32;
            } else {
                fprintf(stderr, "Unsupported output format: %s\n", optarg);
                exit(1);
            }
            break;
        default:
            usage();
            break;
        }
    }

    if (frequency == 0.0) {
        fprintf(stderr, "Frequency not set!\n");
        usage();
    }

    if (argc <= optind) {
        fprintf(stderr, "Input form stdin.\n");
        filename = "-";
    } else if (argc == optind + 1) {
        filename = argv[optind];
    } else {
        fprintf(stderr, "Extra arguments? \"%s\"...\n", argv[optind + 1]);
        usage();
    }

    if (out_block_size < MINIMAL_BUF_LENGTH ||
            out_block_size > MAXIMAL_BUF_LENGTH) {
        fprintf(stderr, "Output block size wrong value, falling back to default\n");
        fprintf(stderr, "Minimal length: %u\n", MINIMAL_BUF_LENGTH);
        fprintf(stderr, "Maximal length: %u\n", MAXIMAL_BUF_LENGTH);
        out_block_size = DEFAULT_BUF_LENGTH;
    }

    const char *ext = strrchr(optarg, '.');
    if (!ext) {
        ext = "";
    }
    if (input_format) {
        // use forced format
    } else if (strcasecmp(ext, "CU8") == 0) {
        input_format = SOAPY_SDR_CU8;
    } else if (strcasecmp(ext, "CS8") == 0) {
        input_format = SOAPY_SDR_CS8;
    } else if (strcasecmp(ext, "CS16") == 0) {
        input_format = SOAPY_SDR_CS16;
    } else if (strcasecmp(ext, "CF32") == 0) {
        input_format = SOAPY_SDR_CF32;
    } else {
        fprintf(stderr, "Unknown input format, falling back to CU8.\n");
        input_format = SOAPY_SDR_CU8;
    }

    if (is_format_equal(input_format, SOAPY_SDR_CF32)) {
        output_format = input_format;
    } else {
        output_format = SOAPY_SDR_CS16;
    }
    output_format = SOAPY_SDR_CS16; // TODO

    buf16 = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CS16));
    if (is_format_equal(input_format, SOAPY_SDR_CS8) || is_format_equal(input_format, SOAPY_SDR_CU8)) {
        buf8 = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CS8));
    } else if (is_format_equal(input_format, SOAPY_SDR_CF32)) {
        fbuf = malloc(out_block_size * SoapySDR_formatToSize(SOAPY_SDR_CF32));
    } else {
        fprintf(stderr, "Unhandled format '%s'.\n", input_format);
        exit(1);
    }

    // TODO: allow choosing output format
    r = verbose_device_search(dev_query, &dev, &stream, SOAPY_SDR_TX, output_format);

    if (r != 0) {
        fprintf(stderr, "Failed to open sdr device matching '%s'.\n", dev_query);
        exit(1);
    }

    fprintf(stderr, "Using input format: %s (output format %s)\n", input_format, output_format);

#ifndef _WIN32
    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGPIPE, &sigact, NULL);
#else
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)sighandler, TRUE);
#endif

    if (antenna && *antenna) {
        char *ant = SoapySDRDevice_getAntenna(dev, SOAPY_SDR_TX, 0);
        fprintf(stderr, "Antenna was: %s\n", ant);
        r = SoapySDRDevice_setAntenna(dev, SOAPY_SDR_TX, 0, antenna);
        if (r != 0)
            fprintf(stderr, "SoapySDRDevice_setAntenna: %s (%d)\n", SoapySDR_errToStr(r), r);
        antenna = SoapySDRDevice_getAntenna(dev, SOAPY_SDR_TX, 0);
        fprintf(stderr, "Antenna set to: %s\n", antenna);
    }

    if (masterclk != 0.0) {
        double clk = SoapySDRDevice_getMasterClockRate(dev);
        fprintf(stderr, "MasterClockRate was: %.0f\n", clk);
        r = SoapySDRDevice_setMasterClockRate(dev, masterclk);
        if (r != 0)
            fprintf(stderr, "SoapySDRDevice_setMasterClockRate: %s (%d)\n", SoapySDR_errToStr(r), r);
        masterclk = SoapySDRDevice_getMasterClockRate(dev);
        fprintf(stderr, "MasterClockRate set to: %.0f\n", masterclk);
    }

    if (bandwidth != 0.0) {
        double bw = SoapySDRDevice_getBandwidth(dev, SOAPY_SDR_TX, 0);
        fprintf(stderr, "Bandwidth was: %.0f\n", bw);
        r = SoapySDRDevice_setBandwidth(dev, SOAPY_SDR_TX, 0, bandwidth);
        if (r != 0)
            fprintf(stderr, "SoapySDRDevice_setBandwidth: %s (%d)\n", SoapySDR_errToStr(r), r);
        bandwidth = SoapySDRDevice_getBandwidth(dev, SOAPY_SDR_TX, 0);
        fprintf(stderr, "Bandwidth set to: %.0f\n", bandwidth);
    }

    /* Set the sample rate */
    verbose_set_sample_rate(dev, SOAPY_SDR_TX, samp_rate);

    /* note: needs sample rate set */
    bool hasHwTime = SoapySDRDevice_hasHardwareTime(dev, "");
    fprintf(stderr, "SoapySDRDevice_hasHardwareTime: %d\n", hasHwTime);
    long long hwTime = SoapySDRDevice_getHardwareTime(dev, "");
    fprintf(stderr, "SoapySDRDevice_getHardwareTime: %lld\n", hwTime);

    /* Set the frequency */
    verbose_set_frequency(dev, SOAPY_SDR_TX, frequency);

    if (ppm_error)
        verbose_ppm_set(dev, ppm_error);

    if (strcmp(filename, "-") == 0) { /* Read samples from stdin */
        fd = fileno(stdin);
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
    } else {
        fd = open(filename, O_RDONLY|O_NONBLOCK);
        if (fd < 0) {
            fprintf(stderr, "Failed to open %s\n", filename);
            goto out;
        }
    }

    fprintf(stderr, "Writing samples in sync mode...\n");
    SoapySDRKwargs args = {0};
    if (SoapySDRDevice_activateStream(dev, stream, 0, 0, 0) != 0) {
        fprintf(stderr, "Failed to activate stream\n");
        exit(1);
    }

    if (NULL == gain_str) {
        /* Enable automatic gain */
        verbose_auto_gain(dev);
    } else {
        /* Enable manual gain */
        verbose_gain_str_set(dev, gain_str);
    }

    size_t n_written = 0;
    while (!do_exit) {
        const void *buffs[1];
        int flags = 0;
        long long timeNs = 0;
        long timeoutUs = 1000000; // 1 second
        ssize_t n_read = 0;
        size_t n_samps = 0, i;

        if (is_format_equal(input_format, SOAPY_SDR_CS16)) {
            n_read = read(fd, buf16, sizeof(int16_t) * 2 * out_block_size);
            // The "native" format we read in, write out no conversion needed
        } else if (is_format_equal(input_format, SOAPY_SDR_CS8)) {
            n_read = read(fd, buf8, sizeof(uint8_t) * 2 * out_block_size);
            n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(uint8_t) / 2;
            for (i = 0; i < n_samps * 2; ++i) {
                buf16[i] = (int16_t)(((int8_t)buf8[i] + 0.4) / 128.0 * fullScale);
            }
        } else if (is_format_equal(input_format, SOAPY_SDR_CU8)) {
            n_read = read(fd, buf8, sizeof(uint8_t) * 2 * out_block_size);
            n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(uint8_t) / 2;
            for (i = 0; i < n_samps * 2; ++i) {
                buf16[i] = (int16_t)((buf8[i] + 127.4) / 128.0 * fullScale);
            }
        } else if (is_format_equal(input_format, SOAPY_SDR_CF32)) {
            n_read = read(fd, fbuf, sizeof(float) * 2 * out_block_size);
            n_samps = n_read < 0 ? 0 : (size_t)n_read / sizeof(float) / 2;
            for (i = 0; i < n_samps * 2; ++i) {
                buf16[i] = (int16_t)(fbuf[i] / 1.0f * (float)fullScale);
            }
        } else {
            fprintf(stderr, "Unsupported input format: %s\n", input_format);
            exit(1);
        }
        //fprintf(stderr, "Input was %zd bytes %zu samples\n", n_read, n_samps);
        if (n_read == -1) {
            if (fd == fileno(stdin)) {
                fprintf(stderr, "Pipe end?\n");
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "Input read underflow.\n");
                continue; // non-blocking
            } else {
                fprintf(stderr, "Input read error (%d)\n", errno);
                break; //error
            }
        }
        if (n_read == 0) {
            if (loops) {
                lseek(fd, 0, SEEK_SET);
                loops--;
            } else {
                break; // EOF
            }
        }

        if (samples_to_write > 0)
            samples_to_write -= n_samps;

        if ((samples_to_write > 0) && (samples_to_write < (uint32_t)n_samps)) {
            n_samps = samples_to_write;
            do_exit = 1;
        }

        //long long hwTime = SoapySDRDevice_getHardwareTime(dev, "");
        //timeNs =  hwTime + (0.001e9); //100ms
        timeNs = 0;//(long long)(n_written * 1e9 / samp_rate);
        flags = 0;//SOAPY_SDR_HAS_TIME;
        for (size_t pos = 0; pos < n_samps && !do_exit;) {
            buffs[0] = &buf16[2 * pos];
            r = SoapySDRDevice_writeStream(dev, stream, buffs, n_samps-pos, &flags, timeNs, timeoutUs);
            if (r < 0) {
                fprintf(stderr, "WARNING: sync write failed. %s (%d)\n", SoapySDR_errToStr(r), r);
                break;
            }
            if (r >= 0)
                pos += (size_t)r;
        }

        //fprintf(stderr, "writeStream ret=%d (%zu of %zu), flags=%d, timeNs=%lld\n", r, n_samps, out_block_size, flags, timeNs);
        if (r >= 0) {
            n_written += n_samps;
        } else {
            if (r == SOAPY_SDR_OVERFLOW) {
                fprintf(stderr, "O");
                fflush(stderr);
                continue;
            }
            fprintf(stderr, "WARNING: sync write failed. %s (%d)\n", SoapySDR_errToStr(r), r);
        }

        //size_t channel = 0;
        //r = SoapySDRDevice_readStreamStatus(dev, stream, &channel, &flags, &timeNs, (long)(1e6 / samp_rate * out_block_size / 2));
        //if (r && r != SOAPY_SDR_TIMEOUT) {
        //    fprintf(stderr, "readStreamStatus %s (%d), channel=%zu flags=%d, timeNs=%lld\n", SoapySDR_errToStr(r), r, channel, flags, timeNs);
        //}
    }
    fprintf(stderr, "%zu samples written\n", n_written);

    if (do_exit)
        fprintf(stderr, "\nUser cancel, exiting...\n");
    else
        fprintf(stderr, "\nLibrary error %d, exiting...\n", r);

    if (fd != fileno(stdin))
        close(fd);

    SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
    SoapySDRDevice_closeStream(dev, stream);
    SoapySDRDevice_unmake(dev);
    free(buf16);
out:
    return r >= 0 ? r : -r;
}
