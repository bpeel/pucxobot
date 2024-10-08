/*
 * Pucxobot - A bot and website to play some card games
 * Copyright (C) 2024  Neil Roberts
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

#ifndef PCX_LISTEN_SOCKET_H
#define PCX_LISTEN_SOCKET_H

#include <stdbool.h>

#include "pcx-error.h"
#include "pcx-netaddress.h"

int
pcx_listen_socket_create_for_netaddress(const struct pcx_netaddress *netaddress,
                                        struct pcx_error **error);

int
pcx_listen_socket_create_for_port(int port,
                                  struct pcx_error **error);

#endif /* PCX_LISTEN_SOCKET_H */
