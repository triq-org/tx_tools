/** @file
    tx_tools - example_gen, example symbolic I/Q waveform generator.

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

#include "code_text.h"

int main(int argc, char **argv)
{
    char const *code =
            "[~ (10kHz) ]                    # define a base frequency\n"
            "[X (~, -10dB, 10ms) ]           # define a very long preamble pulse\n"
            "[_ (1672us) ]                   # define a long gap\n"
            "[0 (472us) (~ 1332us) ]         # define a 0 symbol as short gap and long pulse\n"
            "[1 (920us) (~ 888us) ]          # define a 1 symbol as long gap and short pulse\n"
            "[O (472us) (~ 1784us) ]         # define O symbol alternate 0 symbol with longer pulse\n"
            "[I (920us) (~ 1332us) ]         # define I symbol alternate 1 symbol with longer pulse\n"
            ""
            "______                          # output some silence\n"
            "X__                             # output preamble\n"
            "10000000000010100010_           # output payload\n"
            "IOOOOOOOOOOOIOIOOOIO_           # output alternate payload\n"
            "10000000000010100010_           # ...\n"
            "IOOOOOOOOOOOIOIOOOIO_"
            "10000000000010100010_"
            "IOOOOOOOOOOOIOIOOOIO_"
            "10000000000010100010_"
            "IOOOOOOOOOOOIOIOOOIO_"
            "_"
            "______";

    symbol_t *symbol = parse_code(code, NULL);

    // do something with "symbol"...

    free_symbols(symbol);
}
