/** @file
    tx_tools - tx_sdr, play data through SoapySDR TX

    Copyright (C) 2017 by Christian Zuckschwerdt <zany@triq.net>

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
#include "tx_lib.h"
#include "convenience.h"
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#define DEFAULT_SAMPLE_RATE 2048000

static void print_version()
{
    fprintf(stderr,
            "tx_sdr -- an I/Q player for SoapySDR devices.\n");
}

static void usage(int exit_code)
{
    fprintf(stderr,
            "\nUsage:\t -f frequency_to_tune_to [Hz]\n"
            "\t[-s samplerate (default: 2048000 Hz)]\n"
            "\t[-d device key/value query (ex: 0, 1, driver=lime, driver=hackrf)]\n"
            "\t[-g tuner gain(s) (ex: 20, 40, PAD=-10)]\n"
            "\t[-a antenna (ex: BAND2)]\n"
            "\t[-C channel]\n"
            "\t[-K master clock rate (ex: 80M)]\n"
            "\t[-B bandwidth (ex: 5M)]\n"
            "\t[-p ppm_error (default: 0)]\n"
            "\t[-b output_block_size (default: 16384)]\n"
            "\t[-n number of samples to write (default: 0, infinite)]\n"
            "\t[-l loops count of times to write (default: 0, use -1 for infinite)]\n"
            "\t[-F force input format, CU8|CS8|CS12|CS16|CF32 (default: use file extension)]\n"
            "\t[-V] Output the version string and exit\n"
            "\t[-v] Increase verbosity (can be used multiple times)\n"
            "\t\t-v : verbose, -vv : debug, -vvv : trace\n"
            "\t[-h] Output this usage help and exit\n"
            "\tfilename (a '-' reads samples from stdin)\n\n");
    exit(exit_code);
}

static int *do_exit;

#ifdef _WIN32
BOOL WINAPI
sighandler(int signum)
{
    if (CTRL_C_EVENT == signum) {
        fprintf(stderr, "Signal caught, exiting!\n");
        *do_exit = 1;
        return TRUE;
    }
    return FALSE;
}
#else
static void sighandler(int signum)
{
    fprintf(stderr, "Signal caught, exiting!\n");
    *do_exit = 1;
}
#endif

int main(int argc, char **argv)
{
#ifndef _WIN32
    struct sigaction sigact;
#endif
    tx_cmd_t tx = {0};
    char *filename = NULL;
    int verbose = 0;
    int r, opt;

    tx.stream_fd = -1;
    tx.sample_rate = DEFAULT_SAMPLE_RATE;
    do_exit = &tx.flag_abort;

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

    print_version();

    while ((opt = getopt(argc, argv, "Vvhd:f:g:a:s:C:K:B:b:n:l:p:F:")) != -1) {
        switch (opt) {
        case 'V':
            exit(0);
        case 'v':
            verbose++;
            break;
        case 'h':
            usage(0);
        case 'd':
            tx.dev_query = optarg;
            break;
        case 'f':
            tx.center_frequency = atofs(optarg);
            break;
        case 'g':
            tx.gain_str = optarg;
            break;
        case 'a':
            tx.antenna = optarg;
            break;
        case 's':
            tx.sample_rate = atofs(optarg);
            break;
        case 'K':
            tx.master_clock_rate = atofs(optarg);
            break;
        case 'B':
            tx.bandwidth = atofs(optarg);
            break;
        case 'p':
            tx.ppm_error = atoi(optarg);
            break;
        case 'b':
            tx.block_size = (size_t)atof(optarg);
            break;
        case 'n':
            // each sample is one I/Q pair (half the count of I and Q bytes)
            tx.samples_to_write = (size_t)atofs(optarg);
            break;
        case 'l':
            tx.loops = (unsigned)atoi(optarg);
            break;
        case 'F':
            tx.input_format = tx_parse_soapy_format(optarg);
            if (!tx.input_format) {
                fprintf(stderr, "Unsupported output format: %s\n", optarg);
                exit(1);
            }
            break;
        default:
            usage(1);
            break;
        }
    }

    if (tx.center_frequency == 0.0) {
        fprintf(stderr, "Frequency not set!\n");
        usage(1);
    }

    if (argc <= optind) {
        fprintf(stderr, "Input from stdin.\n");
        filename = "-";
    }
    else if (argc == optind + 1) {
        filename = argv[optind];
    }
    else {
        fprintf(stderr, "Extra arguments? \"%s\"...\n", argv[optind + 1]);
        usage(1);
    }

    const char *ext = strrchr(filename, '.');
    if (ext) {
        ext++;
    }
    else {
        ext = "";
    }
    // detect input format if not forced
    if (!tx.input_format) {
        tx.input_format = tx_parse_soapy_format(ext);
    }
    if (!tx.input_format) {
        fprintf(stderr, "Unknown input format \"%s\", falling back to CU8.\n", ext);
        tx.input_format = SOAPY_SDR_CU8;
    }

    if (strcmp(filename, "-") == 0) { /* Read samples from stdin */
        tx.stream_fd = fileno(stdin);
        fcntl(tx.stream_fd, F_SETFL, fcntl(tx.stream_fd, F_GETFL) | O_NONBLOCK);
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
    }
    else {
        tx.stream_fd = open(filename, O_RDONLY | O_NONBLOCK);
        if (tx.stream_fd < 0) {
            fprintf(stderr, "Failed to open %s\n", filename);
            return 1;
        }
    }

    r = tx_transmit(NULL, &tx);

    if (tx.stream_fd >= 0 && tx.stream_fd != fileno(stdin))
        close(tx.stream_fd);

    return r ? 1 : 0;
}
