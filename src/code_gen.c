/*
 * tx_tools - code_gen, symbolic I/Q waveform generator
 *
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
#include <stdint.h>
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

#include <time.h>

#include "argparse.h"
#include "code_parse.h"
#include "nco.h"

#define DEFAULT_SAMPLE_RATE 1000000
#define DEFAULT_BUF_LENGTH (1 * 16384)
#define MINIMAL_BUF_LENGTH 512
#define MAXIMAL_BUF_LENGTH (256 * 16384)

static void usage(void)
{
    fprintf(stderr,
            "code_gen, a simple I/Q waveform generator\n\n"
            "Usage:"
            "\t[-s sample_rate (default: 2048000 Hz)]\n"
            "\t[-f frequency Hz] adds a base frequency (use twice with e.g. 2FSK)\n"
            "\t[-n noise floor dBFS or multiplier]\n"
            "\t[-N noise on signal dBFS or multiplier]\n"
            "\t Noise level < 0 for attenuation in dBFS, otherwise amplitude multiplier, 0 is off.\n"
            "\t[-g signal gain dBFS or multiplier]\n"
            "\t Gain level < 0 for attenuation in dBFS, otherwise amplitude multiplier, 0 is 0 dBFS.\n"
            "\t Levels as dbFS or multiplier are peak values, e.g. 0 dB or 1.0 x are equivalent to -3 dB RMS.\n"
            "\t[-b output_block_size (default: 16 * 16384) bytes]\n"
            "\t[-r file_path (default: '-', read code from stdin)]\n"
            "\t[-c code_text] parse given code text\n"
            "\t[-S rand_seed] set random seed for reproducible output\n"
            "\tfilename (a '-' writes samples to stdout)\n\n");
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

enum sample_format {
    FORMAT_NONE,
    FORMAT_CU8,
    FORMAT_CS8,
    FORMAT_CS16,
    FORMAT_CF32,
};

static enum sample_format file_info(char **path)
{
    // return the last colon not followed by a backslash, otherwise NULL
    char *colon = NULL;
    char *next = strchr(*path, ':');
    while (next && next[1] != '\\') {
        colon = next;
        next = strchr(next + 1, ':');
    }
    if (colon) {
        *colon = '\0';
        next = colon + 1;
        colon = *path;
        *path = next;
    }

    char const *ext = strchr(*path, '.');
    if ((colon && (!strcmp(colon, "CU8") || !strcmp(colon, "cu8")))
            || (ext && (!strcmp(ext, ".CU8") || !strcmp(ext, ".cu8")))) {
        return FORMAT_CU8;
    }
    else if ((colon && (!strcmp(colon, "CS8") || !strcmp(colon, "cs8")))
            || (ext && (!strcmp(ext, ".CS8") || !strcmp(ext, ".cs8")))) {
        return FORMAT_CS8;
    }
    else if ((colon && (!strcmp(colon, "CS16") || !strcmp(colon, "cs16")))
            || (ext && (!strcmp(ext, ".CS16") || !strcmp(ext, ".cs16")))) {
        return FORMAT_CS16;
    }
    else if ((colon && (!strcmp(colon, "CF32") || !strcmp(colon, "cf32")))
            || (ext && (!strcmp(ext, ".CF32") || !strcmp(ext, ".cf32")))) {
        return FORMAT_CF32;
    }

    return FORMAT_NONE;
}

static double sample_rate = DEFAULT_SAMPLE_RATE;
static double noise_floor = 0.1 * 2; // peak-to-peak
static double noise_signal = 0.05 * 2; // peak-to-peak
static double gain = 1.0;
static int fd = -1;

typedef union {
    uint8_t *u8;
    int8_t *s8;
    int16_t *s16;
    float *cf32;

} out_block_t;

static enum sample_format sample_format = FORMAT_NONE;
static size_t out_block_size = DEFAULT_BUF_LENGTH;
static size_t out_block_len = 0;
static size_t out_block_pos = 0;
static out_block_t out_block;

static double randf(void)
{
    // hotspot:
    return (double)rand() / RAND_MAX;
}

static int bound8(int x)
{
    return x < 0 ? 0 : x > 255 ? 255 : x;
}

static int bound16(int x)
{
    return x < 0 ? 0 : x > 65535 ? 65535 : x;
}

typedef void (*signal_out_fn)(double i, double q);

static void signal_out_cu8(double i, double q)
{
    double scale = 127.5; // scale to u8
    uint8_t i8 = (uint8_t)bound8((int)((i + 1.0) * scale));
    uint8_t q8 = (uint8_t)bound8((int)((q + 1.0) * scale));
    out_block.u8[out_block_pos++] = i8;
    out_block.u8[out_block_pos++] = q8;
    out_block_len += 2 * sizeof(uint8_t);
    if (out_block_len >= out_block_size) {
        write(fd, out_block.u8, out_block_size);
        out_block_pos = out_block_len = 0;
    }
}

static void signal_out_cs8(double i, double q)
{
    double scale = 127.5; // scale to s8
    int8_t i8 = (int8_t)bound8((int)(i * scale));
    int8_t q8 = (int8_t)bound8((int)(q * scale));
    out_block.s8[out_block_pos++] = i8;
    out_block.s8[out_block_pos++] = q8;
    out_block_len += 2 * sizeof(int8_t);
    if (out_block_len >= out_block_size) {
        write(fd, out_block.u8, out_block_size);
        out_block_pos = out_block_len = 0;
    }
}

static void signal_out_cs16(double i, double q)
{
    double scale = 32767.5; // scale to s16
    int16_t i8 = (int16_t)bound16((int)(i * scale));
    int16_t q8 = (int16_t)bound16((int)(q * scale));
    out_block.s16[out_block_pos++] = i8;
    out_block.s16[out_block_pos++] = q8;
    out_block_len += 2 * sizeof(int16_t);
    if (out_block_len >= out_block_size) {
        write(fd, out_block.u8, out_block_size);
        out_block_pos = out_block_len = 0;
    }
}

static void signal_out_cf32(double i, double q)
{
    out_block.cf32[out_block_pos++] = (float)i;
    out_block.cf32[out_block_pos++] = (float)q;
    out_block_len += 2 * sizeof(float);
    if (out_block_len >= out_block_size) {
        write(fd, out_block.u8, out_block_size);
        out_block_pos = out_block_len = 0;
    }
}

static void signal_out_flush()
{
    write(fd, out_block.u8, out_block_len);
    out_block_pos = out_block_len = 0;
}

static signal_out_fn signal_out;
static signal_out_fn format_out[] = {signal_out_cu8, signal_out_cu8, signal_out_cs8, signal_out_cs16, signal_out_cf32};

static void add_noise(size_t time_us, int db)
{
    size_t end =(size_t)(time_us * sample_rate / 1000000.0);
    for (size_t t = 0; t < end; ++t) {
        double x = (randf() - 0.5) * noise_floor;
        double y = (randf() - 0.5) * noise_floor;
        signal_out(x, y);
    }
}

static void add_sine(double freq_hz, size_t time_us, int db)
{
    uint32_t d_phi = nco_d_phase((ssize_t)freq_hz, (size_t)sample_rate);
    uint32_t phi = 0; //nco_phase((ssize_t)freq_hz, (size_t)sample_rate, global_time_us);

    double att = db_to_mag(db);
    //size_t att_steps = 10;

    size_t end = (size_t)(time_us * sample_rate / 1000000.0);
    for (size_t t = 0; t < end; ++t) {

        // ramp in and out
        //double att_in = t < att_steps ? (1.0 / att_steps) * t : 1.0;
        //double att_out = t + att_steps > end ? (1.0 / att_steps) * (end - t) : 1.0;

        // complex I/Q
        double x = nco_cos(phi) * gain * att;// * att_in * att_out;
        double y = nco_sin(phi) * gain * att;// * att_in * att_out;
        phi += d_phi;

        // disturb
        x += (randf() - 0.5) * noise_signal;
        y += (randf() - 0.5) * noise_signal;

        signal_out(x, y);
    }
}

static void gen(char *outpath, symbol_t *symbol, double base_f[])
{
    init_db_lut();
    nco_init();

    if (!outpath || !*outpath || !strcmp(outpath, "-"))
        fd = fileno(stdout);
    else
        fd = open(outpath, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    out_block.u8 = malloc(out_block_size);
    if (!out_block.u8) {
        fprintf(stderr, "Failed to allocate output buffer of %zu bytes.\n", out_block_size);
        exit(1);
    }

    clock_t start = clock();

    for (tone_t *tone = symbol->tone; tone->us && !do_exit; ++tone) {
        if (tone->db < -24) {
            add_noise((size_t)tone->us, tone->db);
        }
        else {
            add_sine(tone->hz, (size_t)tone->us, tone->db);
        }
    }
    signal_out_flush();

    clock_t stop = clock();
    double elapsed = (double)(stop - start) * 1000.0 / CLOCKS_PER_SEC;
    printf("Time elapsed in ms: %f\n", elapsed);

    free(out_block.u8);
    if (fd != fileno(stdout))
        close(fd);
}

static double noise_pp_level(char *arg)
{
    double level = atofs(arg);
    if (level < 0)
        level = pow(10.0, 1.0 / 20.0 * level);
    // correct for RMS to equal a sine
    return level * 2 * sqrt(1.0 / 2.0 * 3.0 / 2.0);
}

static double sine_pk_level(char *arg)
{
    double level = atofs(arg);
    if (level <= 0)
        level = pow(10.0, 1.0 / 20.0 * level);
    return level;
}

int main(int argc, char **argv)
{
    double base_f[16] = {10000.0, -10000.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double *next_f = base_f;
    char *filename = NULL;

    symbol_t *symbols = NULL;
    unsigned rand_seed = 1;

    int opt;
    while ((opt = getopt(argc, argv, "s:f:n:N:g:b:r:c:S:")) != -1) {
        switch (opt) {
        case 's':
            sample_rate = atofs(optarg);
            break;
        case 'f':
            *next_f++ = atofs(optarg);
            break;
        case 'n':
            noise_floor = noise_pp_level(optarg);
            break;
        case 'N':
            noise_signal = noise_pp_level(optarg);
            break;
        case 'g':
            gain = sine_pk_level(optarg);
            break;
        case 'b':
            out_block_size = (size_t)atofs(optarg);
            break;
        case 'r':
            symbols = parse_code_file(optarg, symbols);
            break;
        case 'c':
            symbols = parse_code(optarg, symbols);
            break;
        case 'S':
            rand_seed = (unsigned)atoi(optarg);
            break;
        default:
            usage();
            break;
        }
    }

    if (!symbols) {
        fprintf(stderr, "Input from stdin.\n");
        symbols = parse_code(read_text_fd(fileno(stdin), "STDIN"), symbols);
    }

    if (argc <= optind) {
        fprintf(stderr, "Output to stdout.\n");
        filename = "-";
        exit(0);
    }
    else if (argc == optind + 1) {
        filename = argv[optind];
    }
    else {
        fprintf(stderr, "Extra arguments? \"%s\"...\n", argv[optind + 1]);
        usage();
    }

    sample_format = file_info(&filename);

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

    signal_out = format_out[sample_format];
    srand(rand_seed);
    gen(filename, symbols, base_f);
    free_symbols(symbols);
}
