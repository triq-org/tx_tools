/** @file
    tx_tools - SDR backends.

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

#ifndef INCLUDE_SDRBACKEND_H_
#define INCLUDE_SDRBACKEND_H_

#include "sdr.h"

// Internal: input processing

/// Reset input data.
int sdr_input_reset(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx);

/// Read input data.
ssize_t sdr_input_read(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx, void *buf, size_t *out_samps, double fullScale);

/// Try to read input data.
ssize_t sdr_input_try_read(sdr_ctx_t *sdr_ctx, sdr_cmd_t *tx, void *buf, size_t *out_samps, double fullScale);

// Backends: prototypes

#ifdef HAS_SOAPY
int soapy_enum_devices(sdr_ctx_t *sdr_ctx, const char *enum_args);
int soapy_release_device(sdr_dev_t *sdr_dev);
int soapy_acquire_device(sdr_dev_t *sdr_dev);
int soapy_free_device(sdr_dev_t *sdr_dev);
int soapy_transmit_setup(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx);
int soapy_transmit(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx);
int soapy_transmit_done(sdr_cmd_t *tx);
#endif

#ifdef HAS_LIME
int lime_enum_devices(sdr_ctx_t *sdr_ctx, const char *enum_args);
int lime_release_device(sdr_dev_t *sdr_dev);
int lime_acquire_device(sdr_dev_t *sdr_dev);
int lime_free_device(sdr_dev_t *sdr_dev);
int lime_transmit_setup(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx);
int lime_transmit(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx);
int lime_transmit_done(sdr_cmd_t *tx);
#endif

#ifdef HAS_IIO
int pluto_enum_devices(sdr_ctx_t *sdr_ctx, const char *enum_args);
int pluto_release_device(sdr_dev_t *sdr_dev);
int pluto_acquire_device(sdr_dev_t *sdr_dev);
int pluto_free_device(sdr_dev_t *sdr_dev);
int pluto_transmit_setup(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx);
int pluto_transmit(sdr_ctx_t *sdr_ctx, sdr_dev_t *sdr_dev, sdr_cmd_t *tx);
int pluto_transmit_done(sdr_cmd_t *tx);
#endif

#endif /* INCLUDE_SDRBACKEND_H_ */
