/** @file
    tx_tools - pulse_gen, pulse data I/Q waveform generator.

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

#include "pulse_parse.h"
#include "iq_render.h"
#include "common.h"

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

#include "optparse.h"

static void print_version(void)
{
    fprintf(stderr, "pulse_gen version 0.1\n");
    fprintf(stderr, "Use -h for usage help and see https://triq.org/ for documentation.\n");
}

__attribute__((noreturn))
static void usage(int exitcode)
{
    fprintf(stderr,
            "pulse_gen, pulse data I/Q waveform generator\n\n"
            "Usage:"
            "\t[-h] Output this usage help and exit\n"
            "\t[-V] Output the version string and exit\n"
            "\t[-v] Increase verbosity (can be used multiple times).\n"
            "\t[-s sample_rate (default: 2048000 Hz)]\n"
            "\t[-m OOK|ASK|FSK|PSK] preset mode defaults\n"
            "\t[-f frequency Hz] adds a base frequency (use twice with e.g. 2FSK)\n"
            "\t[-a attenuation dB] adds a base attenuation (use twice with e.g. ASK)\n"
            "\t[-p phase deg] adds a base phase (use twice with e.g. PSK)\n"
            "\t[-n noise floor dBFS or multiplier]\n"
            "\t[-N noise on signal dBFS or multiplier]\n"
            "\t Noise level < 0 for attenuation in dBFS, otherwise amplitude multiplier, 0 is off.\n"
            "\t[-g signal gain dBFS or multiplier]\n"
            "\t Gain level < 0 for attenuation in dBFS, otherwise amplitude multiplier, 0 is 0 dBFS.\n"
            "\t Levels as dbFS or multiplier are peak values, e.g. 0 dB or 1.0 x are equivalent to -3 dB RMS.\n"
            "\t[-b output_block_size (default: 16 * 16384) bytes]\n"
            "\t[-r file_path (default: '-', read code from stdin)]\n"
            "\t[-t pulse_text] parse given code text\n"
            "\t[-S rand_seed] set random seed for reproducible output\n"
            "\tfilename (a '-' writes samples to stdout)\n\n");
    exit(exitcode);
}

#ifdef _WIN32
BOOL WINAPI
sighandler(int signum)
{
    if (CTRL_C_EVENT == signum) {
        fprintf(stderr, "Signal caught, exiting!\n");
        abort_render = 1;
        return TRUE;
    }
    return FALSE;
}
#else
static void sighandler(int signum)
{
    fprintf(stderr, "Signal caught, exiting!\n");
    abort_render = 1;
}
#endif

static void set_defaults(pulse_setup_t *params, char const *name)
{
    if (name && (*name == 'F' || *name == 'f')) {
        // FSK
        params->freq_mark   = 50000;
        params->freq_space  = -50000;
        params->att_mark    = -1;
        params->att_space   = -1;
        params->phase_mark  = 0;
        params->phase_space = 0;
        params->time_base   = 1000000;
    }
    else if (name && (*name == 'A' || *name == 'a')) {
        // ASK
        params->freq_mark   = 100000;
        params->freq_space  = 100000;
        params->att_mark    = -1;
        params->att_space   = -18;
        params->phase_mark  = 0;
        params->phase_space = 0;
        params->time_base   = 1000000;
    }
    else if (name && (*name == 'P' || *name == 'p')) {
        // PSK
        params->freq_mark   = 100000;
        params->freq_space  = 100000;
        params->att_mark    = -1;
        params->att_space   = -1;
        params->phase_mark  = 180;
        params->phase_space = 180;
        params->time_base   = 1000000;
    }
    else {
        // OOK
        params->freq_mark   = 100000;
        params->freq_space  = 0;
        params->att_mark    = -1;
        params->att_space   = -100;
        params->phase_mark  = 0;
        params->phase_space = 0;
        params->time_base   = 1000000;
    }
}

int main(int argc, char **argv)
{
    int verbosity = 0;

    double base_f[16] = {10000.0, -10000.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double *next_f = base_f;
    char *filename = NULL;

    iq_render_t spec = {0};

    pulse_setup_t defaults;
    set_defaults(&defaults, "OOK");

    char *pulse_text = NULL;
    unsigned rand_seed = 1;

    print_version();

    int opt;
    while ((opt = getopt(argc, argv, "hVvs:m:f:a:p:n:N:g:b:r:t:S:")) != -1) {
        switch (opt) {
        case 'h':
            usage(0);
        case 'V':
            exit(0); // we already printed the version
        case 'v':
            verbosity++;
            break;
        case 's':
            spec.sample_rate = atod_metric(optarg, "-s: ");
            break;
        case 'm':
            set_defaults(&defaults, optarg);
            break;
        case 'f':
            *next_f++ = atod_metric(optarg, "-f: ");
            break;
        case 'a':
            *next_f++ = atod_metric(optarg, "-a: ");
            break;
        case 'p':
            *next_f++ = atod_metric(optarg, "-p: ");
            break;
        case 'n':
            spec.noise_floor = atod_metric(optarg, "-n: ");
            break;
        case 'N':
            spec.noise_signal = atod_metric(optarg, "-N: ");
            break;
        case 'g':
            spec.gain = atod_metric(optarg, "-g: ");
            break;
        case 'b':
            spec.frame_size = atouint32_metric(optarg, "-b: ");
            break;
        case 'r':
            pulse_text = read_text_file(optarg);
            break;
        case 't':
            pulse_text = strdup(optarg);
            break;
        case 'S':
            rand_seed = (unsigned)atoi(optarg);
            break;
        default:
            usage(1);
        }
    }

    if (!pulse_text) {
        fprintf(stderr, "Input from stdin.\n");
        pulse_text = read_text_fd(fileno(stdin), "STDIN");
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
        usage(1);
    }

    spec.sample_format = file_info(&filename);
    fprintf(stderr, "Output format %s.\n", sample_format_str(spec.sample_format));

    if (spec.frame_size < MINIMAL_BUF_LENGTH ||
            spec.frame_size > MAXIMAL_BUF_LENGTH) {
        fprintf(stderr, "Output block size wrong value, falling back to default\n");
        fprintf(stderr, "Minimal length: %u\n", MINIMAL_BUF_LENGTH);
        fprintf(stderr, "Maximal length: %u\n", MAXIMAL_BUF_LENGTH);
        spec.frame_size = DEFAULT_BUF_LENGTH;
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

    srand(rand_seed);

    tone_t *tones = parse_pulses(pulse_text, &defaults);
    if (verbosity)
        output_pulses(tones);
    iq_render_file(filename, &spec, tones);
    free(tones);

    free(pulse_text);
}
