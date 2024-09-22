/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2014, 2020  Neil Roberts
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

#include "pcx-base64.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "pcx-error.h"

struct pcx_error_domain
pcx_base64_error;

static int
alphabet_value(int ch)
{
        if (ch == '/') {
                return 63;
        } else if (ch == '+') {
                return 62;
        } else if (ch <= '9') {
                if (ch < '0')
                        return -1;
                return ch - '0' + 26 * 2;
        } else if (ch <= 'Z') {
                if (ch < 'A')
                        return -1;
                return ch - 'A';
        } else if (ch <= 'z') {
                if (ch < 'a')
                        return -1;
                return ch - 'a' + 26;
        } else {
                return -1;
        }
}

void
pcx_base64_decode_start(struct pcx_base64_data *data)
{
        memset(data, 0, sizeof *data);
}

static ssize_t
padding_error(struct pcx_error **error)
{
        pcx_set_error(error,
                      &pcx_base64_error,
                      PCX_BASE64_ERROR_INVALID_PADDING,
                      "The padding in the base64-encoded data is invalid");
        return -1;
}

static bool
handle_padding(struct pcx_base64_data *data,
               const uint8_t *in_buffer,
               size_t length,
               struct pcx_error **error)
{
        const uint8_t *in;

        for (in = in_buffer; in - in_buffer < length; in++) {
                if (*in == '=') {
                        if (++data->n_padding > 2) {
                                padding_error(error);
                                return false;
                        }
                } else if (alphabet_value(*in) != -1) {
                        padding_error(error);
                        return false;
                }
        }

        return true;
}

ssize_t
pcx_base64_decode(struct pcx_base64_data *data,
                  const uint8_t *in_buffer,
                  size_t length,
                  uint8_t *out_buffer,
                  struct pcx_error **error)
{
        uint8_t *out = out_buffer;
        const uint8_t *in;
        int ch_value;

        if (data->n_padding > 0)
                return handle_padding(data, in_buffer, length, error) ? 0 : -1;

        for (in = in_buffer; in - in_buffer < length; in++) {
                ch_value = alphabet_value(*in);

                if (ch_value >= 0) {
                        data->value = (data->value << 6) | ch_value;

                        if (++data->n_chars >= 4) {
                                *(out++) = data->value >> 16;
                                *(out++) = data->value >> 8;
                                *(out++) = data->value;
                                data->n_chars = 0;
                        }
                } else if (*in == '=') {
                        if (!handle_padding(data,
                                            in,
                                            in_buffer + length - in,
                                            error))
                                return -1;

                        break;
                }
        }

        return out - out_buffer;
}

ssize_t
pcx_base64_decode_end(struct pcx_base64_data *data,
                      uint8_t *buffer,
                      struct pcx_error **error)
{
        switch (data->n_padding) {
        case 0:
                if (data->n_chars != 0)
                        return padding_error(error);
                return 0;
        case 1:
                if (data->n_chars != 3 ||
                    (data->value & 3) != 0)
                        return padding_error(error);
                *(buffer++) = data->value >> 10;
                *(buffer++) = data->value >> 2;
                return 2;
        case 2:
                if (data->n_chars != 2 ||
                    (data->value & 15) != 0)
                        return padding_error(error);
                *(buffer++) = data->value >> 4;
                return 1;
        }

        assert(false);

        return 0;
}

static int
to_alphabet_value(int value)
{
        if (value < 26)
                return value + 'A';
        else if (value < 52)
                return value - 26 + 'a';
        else if (value < 62)
                return value - 52 + '0';
        else if (value == 62)
                return '+';
        else
                return '/';
}

size_t
pcx_base64_encode(const uint8_t *data_in,
                  size_t data_in_length,
                  char *data_out)
{
        char *out = data_out;
        int value;

        while (data_in_length >= 3) {
                value = data_in[0] << 16 | data_in[1] << 8 | data_in[2];

                *(out++) = to_alphabet_value(value >> 18);
                *(out++) = to_alphabet_value((value >> 12) & 63);
                *(out++) = to_alphabet_value((value >> 6) & 63);
                *(out++) = to_alphabet_value(value & 63);

                data_in += 3;
                data_in_length -= 3;
        }

        switch (data_in_length) {
        case 0:
                break;

        case 1:
                value = data_in[0] << 16;
                *(out++) = to_alphabet_value(value >> 18);
                *(out++) = to_alphabet_value((value >> 12) & 63);
                *(out++) = '=';
                *(out++) = '=';
                break;

        case 2:
                value = (data_in[0] << 16) | (data_in[1] << 8);
                *(out++) = to_alphabet_value(value >> 18);
                *(out++) = to_alphabet_value((value >> 12) & 63);
                *(out++) = to_alphabet_value((value >> 6) & 63);
                *(out++) = '=';
                break;
        }

        return out - data_out;
}
