/*
 * tx_tools - transform, data transformation helpers
 *
 * Copyright (C) 2018 by Christian Zuckschwerdt <zany@triq.net>
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

#include <stdlib.h>

size_t encode_mc_thomas(char const *data, char *buf, size_t size);

size_t encode_mc_ieee(char const *data, char *buf, size_t size);

size_t encode_dmc_lo(char const *data, char *buf, size_t size);

size_t encode_dmc_hi(char const *data, char *buf, size_t size);

size_t encode_ascii(char const *data, char *buf, size_t size);

size_t encode_hex(char const *data, char *buf, size_t size);

char *named_transform_dup(char const *arg);
