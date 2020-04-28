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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static char
read_buf[1024];

static const char
server_address[] = "/tmp/pucxobot-json-server";

static const char
header[] =
        "Status: 200\r\n"
        "Content-type: application/json\r\n"
        "\r\n";

static bool
get_content_length(unsigned *content_length)
{
        const char *content_length_str = getenv("CONTENT_LENGTH");

        if (content_length_str == NULL)
                return false;

        errno = 0;

        char *tail;
        *content_length = strtoul(content_length_str, &tail, 10);

        if (errno || *tail)
                return false;

        return true;
}

static int
connect_to_server(void)
{
        int sock = socket(AF_LOCAL, SOCK_STREAM, 0);

        if (sock == -1)
                return -1;

        size_t address_size = (offsetof(struct sockaddr_un, sun_path) +
                               sizeof(server_address));
        struct sockaddr_un *addr = malloc(address_size);

        addr->sun_family = AF_LOCAL;
        addr->sun_path[0] = '\0';
        memcpy(addr->sun_path + 1, server_address, (sizeof server_address) - 1);

        int ret = connect(sock,
                          (const struct sockaddr *) addr,
                          address_size);

        free(addr);

        if (ret == -1) {
                close(sock);
                return -1;
        }

        return sock;
}

static bool
write_all(size_t length,
          const char *buf,
          int fd)
{
        while (length > 0) {
                int wrote = write(fd, buf, length);

                if (wrote < 0)
                        return false;

                buf += wrote;
                length -= wrote;
        }

        return true;
}

static void
report_error(void)
{
        printf("Status: 500 Internal Server Error\r\n"
               "Content-type: text/plain\r\n"
               "\r\n"
               "Interal server error\r\n");
        exit(EXIT_SUCCESS);
}

static bool
copy_data(int infd, int outfd)
{
        size_t read_pos = 0;

        while (true) {
                if (read_pos < sizeof read_buf) {
                        int got = read(infd,
                                       read_buf + read_pos,
                                       (sizeof read_buf) - read_pos);

                        if (got == 0)
                                return true;
                        if (got == -1)
                                return false;

                        read_pos += got;
                }

                int wrote = write(outfd, read_buf, read_pos);

                if (wrote == -1)
                        return false;

                memcpy(read_buf, read_buf + wrote, read_pos - wrote);
                read_pos -= wrote;
        }
}

int
main(int argc, char **argv)
{
        unsigned content_length;

        if (!get_content_length(&content_length))
                report_error();

        int sock = connect_to_server();

        if (sock == -1)
                report_error();

        /* Copy the content from stdin to the server */
        while (content_length > 0) {
                unsigned to_read = content_length;

                if (to_read > sizeof read_buf)
                        to_read = sizeof read_buf;

                int got = read(STDIN_FILENO, read_buf, to_read);

                if (got <= 0)
                        report_error();

                if (!write_all(got, read_buf, sock))
                        report_error();

                content_length -= got;
        }

        /* Let the server know we’ve finished writing */
        if (shutdown(sock, SHUT_WR) == -1)
                report_error();

        if (!write_all((sizeof header) - 1, header, STDOUT_FILENO))
                return EXIT_FAILURE;

        if (!copy_data(sock, STDOUT_FILENO))
                return EXIT_FAILURE;

        close(sock);

        return EXIT_SUCCESS;
}
