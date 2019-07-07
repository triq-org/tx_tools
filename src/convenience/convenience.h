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
#ifndef CONVENIENCE_H
#define CONVENIENCE_H

#ifdef _MSC_VER
#define bool _Bool
#define false 0
#define true 1
#define strcasecmp _stricmp
char * strsep(char **sp, char *sep);
#include <time.h>
struct tm *localtime_r (const time_t *timer, struct tm *result);
#endif

#include <stdint.h>
#include <SoapySDR/Device.h>


/* a collection of user friendly tools */

/*!
 * Set device frequency and report status on stderr
 *
 * \param dev the device handle
 * \param direction RX/TX
 * \param frequency in Hz
 * \return 0 on success
 */

int verbose_set_frequency(SoapySDRDevice *dev, const int direction, double frequency);

/*!
 * Set device sample rate and report status on stderr
 *
 * \param dev the device handle
 * \param direction RX/TX
 * \param samp_rate in samples/second
 * \return 0 on success
 */

int verbose_set_sample_rate(SoapySDRDevice *dev, const int direction, double samp_rate);

/*!
 * Set device bandwidth and report status on stderr
 *
 * \param dev the device handle
 * \param direction RX/TX
 * \param bandwidth in Hz
 * \return 0 on success
 */

int verbose_set_bandwidth(SoapySDRDevice *dev, const int direction, double bandwidth);

/*!
 * Set tuner gain elements by a key/value string
 *
 * \param dev the device handle
 * \param gain_str string of gain element pairs (example LNA=40,VGA=20,AMP=0), or string of overall gain, in dB
 * \return 0 on success
 */
int verbose_gain_str_set(SoapySDRDevice *dev, char const *gain_str);

/*!
 * Set the frequency correction value for the device and report status on stderr.
 *
 * \param dev the device handle
 * \param ppm_error correction value in parts per million (ppm)
 * \return 0 on success
 */

int verbose_ppm_set(SoapySDRDevice *dev, int ppm_error);

/*!
 * Find the closest matching device.
 *
 * \param s a string to be parsed
 * \param devOut device output returned
 * \param direction RX/TX
 * \return devOut, 0 if successful
 */

int verbose_device_search(char const *s, SoapySDRDevice **devOut, const int direction);

/*!
 * Setup a stream on the device.
 *
 * \param dev the device handle
 * \param streamOut stream output returned
 * \param direction RX/TX
 * \param format stream format (such as SOAPY_SDR_CS16)
 * \return streamOut, 0 if successful
 */

int verbose_setup_stream(SoapySDRDevice *dev, SoapySDRStream **streamOut, const int direction, const char *format);

/*!
 * Parse a comma-separated list of key/value pairs into SoapySDRKwargs
 *
 * \param s String of key=value pairs, separated by commas
 * \param args Parsed keyword arguments
 */
void parse_kwargs(char const *s, SoapySDRKwargs *args);

#endif /*CONVENIENCE_H*/
