/*
 * Pucxobot - A bot and website to play some card games
 * Copyright (C) 2011, 2013, 2020  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef __PCX_LOG_H__
#define __PCX_LOG_H__

#include <stdbool.h>

#include "pcx-util.h"
#include "pcx-log.h"
#include "pcx-error.h"

extern struct pcx_error_domain
pcx_log_error;

enum pcx_log_error {
        PCX_LOG_ERROR_FILE
};

PCX_PRINTF_FORMAT(1, 2) void
pcx_log(const char *format, ...);

bool
pcx_log_set_file(const char *filename,
                 struct pcx_error **error);

void
pcx_log_start(void);

void
pcx_log_close(void);

#endif /* __PCX_LOG_H__ */
