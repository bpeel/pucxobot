/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2013, 2020  Neil Roberts
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

#ifndef PCX_FILE_ERROR_H
#define PCX_FILE_ERROR_H

#include "pcx-error.h"

extern struct pcx_error_domain
pcx_file_error;

enum pcx_file_error {
        PCX_FILE_ERROR_EXIST,
        PCX_FILE_ERROR_ISDIR,
        PCX_FILE_ERROR_ACCES,
        PCX_FILE_ERROR_NAMETOOLONG,
        PCX_FILE_ERROR_NOENT,
        PCX_FILE_ERROR_NOTDIR,
        PCX_FILE_ERROR_AGAIN,
        PCX_FILE_ERROR_INTR,
        PCX_FILE_ERROR_PERM,
        PCX_FILE_ERROR_PFNOSUPPORT,
        PCX_FILE_ERROR_AFNOSUPPORT,
        PCX_FILE_ERROR_MFILE,

        PCX_FILE_ERROR_OTHER
};

enum pcx_file_error
pcx_file_error_from_errno(int errnum);

PCX_PRINTF_FORMAT(3, 4) void
pcx_file_error_set(struct pcx_error **error,
                   int errnum,
                   const char *format,
                   ...);

#endif /* PCX_FILE_ERROR_H */
