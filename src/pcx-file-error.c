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

#include "config.h"

#include "pcx-file-error.h"

#include <errno.h>

struct pcx_error_domain
pcx_file_error;

enum pcx_file_error
pcx_file_error_from_errno(int errnum)
{
        switch (errnum) {
        case EEXIST:
                return PCX_FILE_ERROR_EXIST;
        case EISDIR:
                return PCX_FILE_ERROR_ISDIR;
        case EACCES:
                return PCX_FILE_ERROR_ACCES;
        case ENAMETOOLONG:
                return PCX_FILE_ERROR_NAMETOOLONG;
        case ENOENT:
                return PCX_FILE_ERROR_NOENT;
        case ENOTDIR:
                return PCX_FILE_ERROR_NOTDIR;
        case EAGAIN:
#if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:
#endif
                return PCX_FILE_ERROR_AGAIN;
        case EINTR:
                return PCX_FILE_ERROR_INTR;
        case EPERM:
                return PCX_FILE_ERROR_PERM;
        case EPFNOSUPPORT:
                return PCX_FILE_ERROR_PFNOSUPPORT;
        case EAFNOSUPPORT:
                return PCX_FILE_ERROR_AFNOSUPPORT;
        case EMFILE:
                return PCX_FILE_ERROR_MFILE;
        }

        return PCX_FILE_ERROR_OTHER;
}

PCX_PRINTF_FORMAT(3, 4) void
pcx_file_error_set(struct pcx_error **error,
                   int errnum,
                   const char *format,
                   ...)
{
        va_list ap;

        va_start(ap, format);
        pcx_set_error_va_list(error,
                              &pcx_file_error,
                              pcx_file_error_from_errno(errnum),
                              format,
                              ap);
        va_end(ap);
}
