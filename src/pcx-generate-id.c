/*
 * Pucxobot - A bot and website to play some card games
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

#include "pcx-generate-id.h"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static void
xor_bytes(uint64_t *id,
          const uint8_t *data,
          size_t data_length)
{
        uint8_t *id_bytes = (uint8_t *) id;
        int data_pos = 0;
        int i;

        for (i = 0; i < sizeof (*id); i++) {
                id_bytes[i] ^= data[data_pos];
                data_pos = (data_pos + 1) % data_length;
        }
}

static int
fill_from_urandom(void *buf, size_t size)
{
        int fd = open("/dev/urandom", O_RDONLY);

        if (fd == -1)
                return 0;

        int got = read(fd, buf, size);

        close(fd);

        return got == -1 ? 0 : got;
}

uint64_t
pcx_generate_id(const struct pcx_netaddress *remote_address)
{
        uint16_t random_data;
        uint64_t id = 0;
        int i;

        int got = fill_from_urandom(&id, sizeof id);

        /* If it didn’t work, fill in the remaining data with rand() */
        for (i = got / sizeof random_data;
             i < sizeof id / sizeof random_data;
             i++) {
                random_data = rand();
                memcpy((uint8_t *) &id + i * sizeof random_data,
                       &random_data,
                       sizeof random_data);
        }

        /* XOR in the bytes of the client's address so that even if
         * the client can predict the random number sequence it'll
         * still be hard to guess a number of another client
         */
        xor_bytes(&id,
                  (uint8_t *) &remote_address->port,
                  sizeof remote_address->port);
        if (remote_address->family == AF_INET6)
                xor_bytes(&id,
                          (uint8_t *) &remote_address->ipv6,
                          sizeof remote_address->ipv6);
        else
                xor_bytes(&id,
                          (uint8_t *) &remote_address->ipv4,
                          sizeof remote_address->ipv4);

        return id;
}
