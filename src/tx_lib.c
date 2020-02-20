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

#include "tx_lib.h"

#include "pulse_text.h"
#include "code_text.h"
#include "iq_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#include "sdr/sdr.h"

// helpers
char const *tx_available_backends()
{
    return sdr_ctx_available_backends();
}

int tx_valid_input_format(char const *format)
{
    // we support all current formats as input
    return sample_format_for(format) != FORMAT_NONE;
}


int tx_valid_output_format(char const *format)
{
    // we support all current formats as output
    return sample_format_for(format) != FORMAT_NONE;
}

char const *tx_parse_sample_format(char const *format)
{
    return sample_format_str(sample_format_parse(format));
}

// format is 3-4 chars (plus null), compare as int.
static int is_format_equal(const void *a, const void *b)
{
    return *(const uint32_t *)a == *(const uint32_t *)b;
}

// presets

preset_t *tx_presets_load(tx_ctx_t *tx_ctx, char const *dir_name)
{
    DIR *dir;
    dir = opendir(dir_name);
    if (!dir) {
        fprintf(stderr, "presets: no such directory \"%s\".\n", dir_name);
        return NULL;
    }

    preset_t *presets = calloc(100, sizeof(*presets));
    unsigned i = 0;

    char path[256];
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        printf("%s\n", ent->d_name);
        // TODO: check if really a preset file
        if (*ent->d_name != '.') {
            snprintf(path, sizeof(path), "%s/%s", dir_name, ent->d_name);
            presets[i].name = strdup(ent->d_name);
            presets[i].text = read_text_file(path);
            presets[i].desc = parse_code_desc(presets[i].text);
            i++;
        }
    }

    closedir(dir);

    tx_presets_free(tx_ctx);
    tx_ctx->presets = presets;

    return presets;
}

void tx_presets_free(tx_ctx_t *tx_ctx)
{
    preset_t *presets = tx_ctx->presets;
    if (!presets)
        return;

    tx_ctx->presets = NULL;

    for (unsigned i = 0; presets[i].name; ++i) {
        free(presets[i].name);
        free(presets[i].desc);
        free(presets[i].text);
    }

    free(presets);
}

preset_t *tx_presets_get(tx_ctx_t *tx_ctx, char const *name)
{
    preset_t *presets = tx_ctx->presets;
    if (!presets || !name || !*name)
        return NULL;

    for (unsigned i = 0; presets[i].name; ++i) {
        if (!strcmp(presets[i].name, name))
            return &presets[i];
    }

    return NULL;
}

// api

int tx_enum_devices(tx_ctx_t *tx_ctx, const char *enum_args)
{
    return sdr_ctx_enum_devices((sdr_ctx_t *)tx_ctx, enum_args);
}

int tx_release_devices(tx_ctx_t *tx_ctx)
{
    return sdr_ctx_release_devices((sdr_ctx_t *)tx_ctx);
}

int tx_free_devices(tx_ctx_t *tx_ctx)
{
    return sdr_ctx_free_devices((sdr_ctx_t *)tx_ctx);
}

int tx_transmit(tx_ctx_t *tx_ctx, tx_cmd_t *tx)
{
    int r = sdr_tx_setup((sdr_ctx_t *)tx_ctx, (sdr_cmd_t *)tx);
    if (r) {
        perror("sdr_tx_setup");
        exit(EXIT_FAILURE);
    }
    r = tx_input_init(tx_ctx, tx);
    if (r) {
        return r;
    }
    r = sdr_tx((sdr_ctx_t *)tx_ctx, (sdr_cmd_t *)tx);
    sdr_tx_free((sdr_ctx_t *)tx_ctx, (sdr_cmd_t *)tx);
    return r;
}

void tx_print(tx_ctx_t *tx_ctx, tx_cmd_t *tx)
{
    printf("TX command:\n");
    printf("  device selection\n");
    printf("    dev_query=\"%s\"\n", tx->dev_query);
    printf("  device setup\n");
    printf("    gain_str=\"%s\"\n", tx->gain_str);
    printf("    antenna=\"%s\"\n", tx->antenna);
    printf("    channel=%zu\n", tx->channel);
    printf("  rf setup\n");
    printf("    ppm_error=%f\n", tx->ppm_error);
    printf("    center_frequency=%f\n", tx->center_frequency);
    printf("    sample_rate=%f\n", tx->sample_rate);
    printf("    bandwidth=%f\n", tx->bandwidth);
    printf("    master_clock_rate=%f\n", tx->master_clock_rate);
    printf("    output_format=\"%s\"\n", tx->output_format);
    printf("    block_size=%zu\n", tx->block_size);
    printf("  transmit control\n");
    printf("    initial_delay=%u\n", tx->initial_delay);
    printf("    repeats=%u\n", tx->repeats);
    printf("    repeat_delay=%u\n", tx->repeat_delay);
    printf("    loops=%u\n", tx->loops);
    printf("    loop_delay=%u\n", tx->loop_delay);
    printf("  input from file descriptor\n");
    printf("    input_format=\"%s\"\n", tx->input_format);
    printf("    stream_fd=%i\n", tx->stream_fd);
    printf("    samples_to_write=%zu\n", tx->samples_to_write);
    printf("  input from buffer\n");
    printf("    stream_buffer=%p\n", tx->stream_buffer);
    printf("    buffer_size=%zu\n", tx->buffer_size);
    printf("  input from text\n");
    printf("    freq_mark=%i\n", tx->freq_mark);
    printf("    freq_space=%i\n", tx->freq_space);
    printf("    att_mark=%i\n", tx->att_mark);
    printf("    att_space=%i\n", tx->att_space);
    printf("    phase_mark=%i\n", tx->phase_mark);
    printf("    phase_space=%i\n", tx->phase_space);
    printf("    pulses=\"%s\"\n", tx->pulses);
}

void tx_cmd_free(tx_cmd_t *tx)
{
    //free(tx->dev_query);
    //free(tx->gain_str);
    //free(tx->antenna);
    //free(tx->output_format);
    //free(tx->input_format);
    //free(tx->preset);
    //free(tx->codes);
    //free(tx->pulses);
}

// input processing

int tx_input_init(tx_ctx_t *tx_ctx, tx_cmd_t *tx)
{
    // unpack codes if requested
    if (tx->codes) {
        iq_render_t iq_render = {0};
        iq_render_defaults(&iq_render);
        iq_render.sample_rate   = tx->sample_rate;
        iq_render.sample_format = sample_format_for(tx->output_format);

        symbol_t *symbols = NULL;
        preset_t *preset  = NULL;
        if (tx->preset) {
            preset = tx_presets_get(tx_ctx, tx->preset);
        }
        if (preset) {
            symbols = parse_code(preset->text, symbols);
        }

        symbols = parse_code(tx->codes, symbols);
        output_symbol(symbols); // debug

        iq_render_buf(&iq_render, symbols->tone, &tx->stream_buffer, &tx->buffer_size);
        free(symbols);

        return 0;
    }

    // unpack pulses if requested
    if (tx->pulses) {
        iq_render_t iq_render = {0};
        iq_render_defaults(&iq_render);
        iq_render.sample_rate   = tx->sample_rate;
        iq_render.sample_format = sample_format_for(tx->output_format);

        pulse_setup_t pulse_setup = {0};
        pulse_setup_defaults(&pulse_setup, "OOK");
        pulse_setup.freq_mark   = tx->freq_mark;
        pulse_setup.freq_space  = tx->freq_space;
        pulse_setup.att_mark    = tx->att_mark;
        pulse_setup.att_space   = tx->att_space;
        pulse_setup.phase_mark  = tx->phase_mark;
        pulse_setup.phase_space = tx->phase_space;

        tone_t *tones = parse_pulses(tx->pulses, &pulse_setup);
        output_pulses(tones); // debug

        iq_render_buf(&iq_render, tones, &tx->stream_buffer, &tx->buffer_size);
        free(tones);

        return 0;
    }

    // otherwise: setup stream conversion

    if (!tx_valid_input_format(tx->input_format)) {
        fprintf(stderr, "Unhandled input format '%s'.\n", tx->input_format);
        return -1;
    }
    if (!tx_valid_output_format(tx->output_format)) {
        fprintf(stderr, "Unhandled output format '%s'.\n", tx->output_format);
        return -1;
    }

    if (!is_format_equal(tx->input_format, tx->output_format)) {
        size_t elem_size = sample_format_length(sample_format_for(tx->input_format));
        tx->conv_buf.u8  = malloc(tx->block_size * elem_size);
        if (!tx->conv_buf.u8) {
            perror("tx_input_init");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
