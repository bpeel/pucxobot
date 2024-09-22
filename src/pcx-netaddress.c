/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
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

#include "config.h"

#include "pcx-netaddress.h"

#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>

#include "pcx-util.h"
#include "pcx-buffer.h"

static void
pcx_netaddress_to_native_ipv4(const struct pcx_netaddress *address,
                              struct sockaddr_in *native)
{
        native->sin_family = AF_INET;
        native->sin_addr = address->ipv4;
        native->sin_port = htons(address->port);
}

static void
pcx_netaddress_to_native_ipv6(const struct pcx_netaddress *address,
                              struct sockaddr_in6 *native)
{
        native->sin6_family = AF_INET6;
        native->sin6_addr = address->ipv6;
        native->sin6_flowinfo = 0;
        native->sin6_scope_id = 0;
        native->sin6_port = htons(address->port);
}

void
pcx_netaddress_to_native(const struct pcx_netaddress *address,
                         struct pcx_netaddress_native *native)
{
        if (address->family == AF_INET6) {
                pcx_netaddress_to_native_ipv6(address, &native->sockaddr_in6);
                native->length = sizeof native->sockaddr_in6;
        } else {
                pcx_netaddress_to_native_ipv4(address, &native->sockaddr_in);
                native->length = sizeof native->sockaddr_in;
        }
}

static void
pcx_netaddress_from_native_ipv4(struct pcx_netaddress *address,
                                const struct sockaddr_in *native)
{
        address->family = AF_INET;
        address->ipv4 = native->sin_addr;
        address->port = ntohs(native->sin_port);
}

static void
pcx_netaddress_from_native_ipv6(struct pcx_netaddress *address,
                                const struct sockaddr_in6 *native)
{
        address->family = AF_INET6;
        address->ipv6 = native->sin6_addr;
        address->port = ntohs(native->sin6_port);
}

void
pcx_netaddress_from_native(struct pcx_netaddress *address,
                           const struct pcx_netaddress_native *native)
{
        switch (native->sockaddr.sa_family) {
        case AF_INET:
                pcx_netaddress_from_native_ipv4(address,
                                                &native->sockaddr_in);
                break;

        case AF_INET6:
                pcx_netaddress_from_native_ipv6(address,
                                                &native->sockaddr_in6);
                break;

        default:
                memset(address, 0, sizeof *address);
                break;
        }
}

char *
pcx_netaddress_to_string(const struct pcx_netaddress *address)
{
        const int buffer_length = (8 * 5 + /* length of ipv6 address */
                                   2 + /* square brackets */
                                   1 + /* colon separator */
                                   5 + /* port number */
                                   1 + /* null terminator */
                                   16 /* ... and one for the pot */);
        char *buf = pcx_alloc(buffer_length);
        static const uint8_t ipv4_mapped_address_prefix[] = {
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0xff, 0xff
        };
        int len;

        if (address->family == AF_INET6) {
                if (memcmp(&address->ipv6,
                           ipv4_mapped_address_prefix,
                           sizeof ipv4_mapped_address_prefix)) {
                        buf[0] = '[';
                        inet_ntop(AF_INET6,
                                  &address->ipv6,
                                  buf + 1,
                                  buffer_length - 1);
                        len = strlen(buf);
                        buf[len++] = ']';
                } else {
                        /* IPv6-mapped IPv4 address */
                        inet_ntop(AF_INET,
                                  (const uint8_t *) &address->ipv6 +
                                  sizeof ipv4_mapped_address_prefix,
                                  buf,
                                  buffer_length);
                        len = strlen(buf);
                }
        } else {
                inet_ntop(AF_INET,
                          &address->ipv4,
                          buf,
                          buffer_length);
                len = strlen(buf);
        }

        snprintf(buf + len, buffer_length - len,
                 ":%" PRIu16,
                 address->port);

        return buf;
}

bool
pcx_netaddress_from_string(struct pcx_netaddress *address,
                           const char *str,
                           int default_port)
{
        struct pcx_buffer buffer;
        const char *addr_end;
        char *port_end;
        unsigned long port;
        bool ret = true;

        pcx_buffer_init(&buffer);

        if (*str == '[') {
                /* IPv6 address */
                addr_end = strchr(str + 1, ']');

                if (addr_end == NULL) {
                        ret = false;
                        goto out;
                }

                pcx_buffer_append(&buffer, str + 1, addr_end - str - 1);
                pcx_buffer_append_c(&buffer, '\0');

                address->family = AF_INET6;

                if (inet_pton(AF_INET6,
                              (char *) buffer.data,
                              &address->ipv6) != 1) {
                        ret = false;
                        goto out;
                }

                addr_end++;
        } else {
                addr_end = strchr(str + 1, ':');
                if (addr_end == NULL)
                        addr_end = str + strlen(str);

                pcx_buffer_append(&buffer, str, addr_end - str);
                pcx_buffer_append_c(&buffer, '\0');

                address->family = AF_INET;

                if (inet_pton(AF_INET,
                              (char *) buffer.data,
                              &address->ipv4) != 1) {
                        ret = false;
                        goto out;
                }
        }

        if (*addr_end == ':') {
                errno = 0;
                port = strtoul(addr_end + 1, &port_end, 10);
                if (errno ||
                    port > 0xffff ||
                    port_end == addr_end + 1 ||
                    *port_end) {
                        ret = false;
                        goto out;
                }
                address->port = port;
        } else if (*addr_end != '\0') {
                ret = false;
                goto out;
        } else {
                address->port = default_port;
        }

out:
        pcx_buffer_destroy(&buffer);

        return ret;
}
