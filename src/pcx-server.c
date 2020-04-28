/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2020  Neil Roberts
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

#include "pcx-server.h"

#include <json_object.h>
#include <json_tokener.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <fcntl.h>

#include "pcx-util.h"
#include "pcx-main-context.h"
#include "pcx-game.h"
#include "pcx-list.h"
#include "pcx-config.h"
#include "pcx-buffer.h"
#include "pcx-socket.h"
#include "pcx-file-error.h"
#include "pcx-netaddress.h"

#define DEFAULT_PORT 3648

struct pcx_error_domain
pcx_server_error;

struct pcx_server {
        const struct pcx_config *config;
        int listen_sock;
};

static int
create_socket_for_netaddress(const struct pcx_netaddress *netaddress,
                             struct pcx_error **error)
{
        struct pcx_netaddress_native native_address;
        const int true_value = true;
        int sock;

        pcx_netaddress_to_native(netaddress, &native_address);

        sock = socket(native_address.sockaddr.sa_family == AF_INET6 ?
                      PF_INET6 : PF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
                pcx_file_error_set(error,
                                   errno,
                                   "Failed to create socket: %s",
                                   strerror(errno));
                return -1;
        }

        setsockopt(sock,
                   SOL_SOCKET, SO_REUSEADDR,
                   &true_value, sizeof true_value);

        if (!pcx_socket_set_nonblock(sock, error))
                goto error;

        if (bind(sock, &native_address.sockaddr, native_address.length) == -1) {
                pcx_file_error_set(error,
                                   errno,
                                   "Failed to bind socket: %s",
                                   strerror(errno));
                goto error;
        }

        if (listen(sock, 10) == -1) {
                pcx_file_error_set(error,
                                   errno,
                                   "Failed to make socket listen: %s",
                                   strerror(errno));
                goto error;
        }

        return sock;

error:
        pcx_close(sock);
        return false;
}

static int
create_socket_for_port(int port,
                       struct pcx_error **error)
{
        struct pcx_netaddress netaddress;

        memset(&netaddress, 0, sizeof netaddress);

        /* First try binding it with an IPv6 address */
        netaddress.port = port;
        netaddress.family = AF_INET6;

        struct pcx_error *local_error = NULL;

        int sock = create_socket_for_netaddress(&netaddress, &local_error);

        if (sock != -1)
                return sock;

        if (local_error->domain == &pcx_file_error &&
            (local_error->code == PCX_FILE_ERROR_PFNOSUPPORT ||
             local_error->code == PCX_FILE_ERROR_AFNOSUPPORT)) {
                pcx_error_free(local_error);
        } else {
                pcx_error_propagate(error, local_error);
                return -1;
        }

        /* Some servers disable IPv6 so try IPv4 */
        netaddress.family = AF_INET;

        return create_socket_for_netaddress(&netaddress, error);
}

static int
create_socket_for_address(const char *address,
                          struct pcx_error **error)
{
        unsigned long port;
        char *tail;

        errno = 0;
        port = strtoul(address, &tail, 0);
        if (errno == 0 && port <= UINT16_MAX && *tail == '\0')
                return create_socket_for_port(port, error);

        struct pcx_netaddress netaddress;

        if (!pcx_netaddress_from_string(&netaddress,
                                        address,
                                        DEFAULT_PORT)) {
                pcx_set_error(error,
                              &pcx_server_error,
                              PCX_SERVER_ERROR_INVALID_ADDRESS,
                              "The listen address %s is invalid",
                              address);
                return -1;
        }

        return create_socket_for_netaddress(&netaddress, error);
}

struct pcx_server *
pcx_server_new(const struct pcx_config *config,
               const struct pcx_config_server *server_config,
               struct pcx_error **error)
{
        int sock;

        if (server_config->address)
                sock = create_socket_for_address(server_config->address, error);
        else
                sock = create_socket_for_port(DEFAULT_PORT, error);

        if (sock == -1)
                return NULL;

        struct pcx_server *server = pcx_calloc(sizeof *server);

        server->config = config;
        server->listen_sock = sock;

        return server;
}

void
pcx_server_free(struct pcx_server *server)
{
        if (server->listen_sock != -1)
                pcx_close(server->listen_sock);

        pcx_free(server);
}
