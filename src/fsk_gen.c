/*
 * fsk_gen, a simple I/Q waveform generator
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

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "argparse.h"

#define DEFAULT_SAMPLE_RATE 2048000
#define DEFAULT_BUF_LENGTH (1 * 16384)
#define MINIMAL_BUF_LENGTH 512
#define MAXIMAL_BUF_LENGTH (256 * 16384)

static void usage(void)
{
    fprintf(stderr,
            "fsk_gen, a simple I/Q waveform generator\n\n"
            "Usage:\t[-s sample_rate (default: 2048000 Hz)]\n"
            "\t[-f first frequency Hz]\n"
            "\t[-F second frequency Hz]\n"
            "\t[-n noise floot dB]\n"
            "\t[-N noise in signal dB]\n"
            "\t[-g tuner gain(s) (ex: 20, 40, PAD=-10)]\n"
            "\t[-b output_block_size (default: 16 * 16384)]\n"
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

static double sample_rate = 8000000.0; // DEFAULT_SAMPLE_RATE
static double noise_floor = 0.1 * 2;
static double noise_signal = 0.05 * 2;
static double gain = 1.0;
static int fd = -1;

static size_t out_block_size = DEFAULT_BUF_LENGTH;
static size_t out_block_pos = 0;
static uint8_t *out_block;

static double randf()
{
    return (double)rand() / RAND_MAX;
}

static int bound(int x)
{
    return x < 0 ? 0 : x > 255 ? 255 : x;
}

static void signal_out(double i, double q)
{
    double scale = 127.5; // scale to u8
    uint8_t i8 = (uint8_t)bound((int)((i + 1.0) * gain * scale));
    uint8_t q8 = (uint8_t)bound((int)((q + 1.0) * gain * scale));
    out_block[out_block_pos++] = i8;
    out_block[out_block_pos++] = q8;
    if (out_block_pos == out_block_size) {
        write(fd, out_block, out_block_size);
        out_block_pos = 0;
    }
}

static double oscc(double f, size_t t)
{
    return cos(f * 2.0 * M_PI * t / sample_rate);
}

static double oscs(double f, size_t t)
{
    return sin(f * 2.0 * M_PI * t / sample_rate);
}

static void add_noise(size_t time_us)
{
    for (size_t t = 0; t < (size_t)(time_us * sample_rate / 1000000); ++t) {
        signal_out((randf() - 0.5) * noise_floor,
                   (randf() - 0.5) * noise_floor);
    }
}

static void add_sine(double freq_hz, size_t time_us)
{
    for (size_t t = 0; t < (size_t)(time_us * sample_rate / 1000000); ++t) {
        signal_out(oscc(freq_hz, t) + (randf() - 0.5) * noise_signal,
                   oscs(freq_hz, t) + (randf() - 0.5) * noise_signal);
    }
}

static void gen(char *outpath, double f1, double f2)
{
    char *symbols =
            "__"
            "1010101010101010101010100010110111010100001010110100001000010011_"
            "1010101010101010101010100010110111010100001010110100001000010011_"
            "1010101010101010101010100010110111010100001010110100001000010011_"
            "1010101010101010101010100010110111010100001010110100001000010011_"
            "1010101010101010101010100010110111010100001010110100001000010011_"
            "1010101010101010101010100010110111010100001010110100001000010011_"
            "1010101010101010101010100010110111010100001010110100001000010011_"
            "1010101010101010101010100010110111010100001010110100001000010011_"
            "_";
    // 622 bit width
    // 8000 us packet gap

    fd = fileno(stdout); //io.open(outpath, mode="wb")

    out_block = malloc(out_block_size);
    if (!out_block) {
        fprintf(stderr, "Failed to allocate output buffer of %zu bytes.\n", out_block_size);
        exit(1);
    }

    //srand();
    //int len = strlen(symbols);
    for (char *symbol = symbols; *symbol && !do_exit; ++symbol) {
        if (*symbol == '_') {
            add_noise(8000);
        } else if (*symbol == '0') {
            add_sine(f1, 622);
        } else if (*symbol == '1') {
            add_sine(f2, 622);
        }
    }

    free(out_block);
    //fd.close()
}

int main(int argc, char **argv)
{
    double f1 = 100.0, f2 = 1000.0;
    char *filename = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "s:f:F:n:N:g:b:")) != -1) {
        switch (opt) {
        case 's':
            sample_rate = atofs(optarg);
            break;
        case 'f':
            f1 = atofs(optarg);
            break;
        case 'F':
            f2 = atofs(optarg);
            break;
        case 'n':
            noise_floor = atofs(optarg);
            break;
        case 'N':
            noise_signal = atofs(optarg);
            break;
        case 'g':
            gain = atofs(optarg);
            break;
        case 'b':
            out_block_size = (size_t)atofs(optarg);
            break;
        default:
            usage();
            break;
        }
    }

    if (argc <= optind) {
        usage();
    } else {
        filename = argv[optind];
    }

    if (out_block_size < MINIMAL_BUF_LENGTH ||
            out_block_size > MAXIMAL_BUF_LENGTH) {
        fprintf(stderr, "Output block size wrong value, falling back to default\n");
        fprintf(stderr, "Minimal length: %u\n", MINIMAL_BUF_LENGTH);
        fprintf(stderr, "Maximal length: %u\n", MAXIMAL_BUF_LENGTH);
        out_block_size = DEFAULT_BUF_LENGTH;
    }

#ifndef _WIN32
    struct sigaction sigact;
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

    gen(filename, f1, f2);
}
