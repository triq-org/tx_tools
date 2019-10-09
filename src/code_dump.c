/** @file
    tx_tools - code_dump, parse symbolic waveform and dump content.

    Copyright (C) 2018 by Christian Zuckschwerdt <zany@triq.net>

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

#include <stdlib.h>
#include <stdio.h>

#include "read_text.h"
#include "code_text.h"

int main(int argc, char **argv)
{
    symbol_t *s = NULL;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            fprintf(stderr, "Reading from \"%s\"...\n", argv[i]);
            s = parse_code_file(argv[i], s);
            printf("Reading done.\n");
            output_symbol(s);
            free_symbols(s);
        }
    }
    else {
        fprintf(stderr, "Reading from stdin...\n");
        s = parse_code(read_text_fd(fileno(stdin), "STDIN"), s);
        printf("Reading done.\n");
        output_symbol(s);
        free_symbols(s);
    }
}
