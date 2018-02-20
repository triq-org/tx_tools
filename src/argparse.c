/*
 * Copyright (C) 2014 by Kyle Keen <keenerd@gmail.com>
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

/* a collection of user friendly tools
 * todo: use strtol/strtod for more flexible number parsing
 * */

#include "argparse.h"
#include <string.h>
#include <stdlib.h>

double atofs(char *s)
/* standard suffixes */
{
	if (!s || !*s) return 0.0;
	size_t len = strlen(s);
	char last = s[len-1];
	double suff = 1.0;
	switch (last) {
		case 'g':
		case 'G':
			suff *= 1e3;
		case 'm':
		case 'M':
			suff *= 1e3;
		case 'k':
		case 'K':
			suff *= 1e3;
			suff *= atof(s);
			return suff;
	}
	return atof(s);
}

double atoft(char *s)
/* time suffixes, returns seconds */
{
	if (!s || !*s) return 0.0;
	size_t len = strlen(s);
	char last = s[len-1];
	double suff = 1.0;
	switch (last) {
		case 'h':
		case 'H':
			suff *= 60;
		case 'm':
		case 'M':
			suff *= 60;
		case 's':
		case 'S':
			suff *= atof(s);
			return suff;
	}
	return atof(s);
}

double atofp(char *s)
/* percent suffixes */
{
	if (!s || !*s) return 0.0;
	size_t len = strlen(s);
	char last = s[len-1];
	double suff = 1.0;
	switch (last) {
		case '%':
			suff *= 0.01;
			suff *= atof(s);
			return suff;
	}
	return atof(s);
}
