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
#ifndef ARGPARSE_H
#define ARGPARSE_H

/* a collection of user friendly tools */

/*!
 * Convert standard suffixes (k, M, G) to double
 *
 * \param s a string to be parsed
 * \return double
 */

double atofs(char *s);

/*!
 * Convert time suffixes (s, m, h) to double
 *
 * \param s a string to be parsed
 * \return seconds as double
 */

double atoft(char *s);

/*!
 * Convert percent suffixe (%) to double
 *
 * \param s a string to be parsed
 * \return double
 */

double atofp(char *s);

#endif /*ARGPARSE_H*/
