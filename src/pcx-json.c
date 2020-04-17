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

#include "pcx-json.h"

#include <stdarg.h>

bool
pcx_json_get(struct json_object *obj,
             ...)
{
        if (!json_object_is_type(obj, json_type_object))
                return false;

        bool ret = true;
        va_list ap;

        va_start(ap, obj);

        const char *key;

        while ((key = va_arg(ap, const char *))) {
                struct json_object *value;

                if (!json_object_object_get_ex(obj, key, &value)) {
                        ret = false;
                        break;
                }

                enum json_type type = va_arg(ap, enum json_type);

                if (!json_object_is_type(value, type)) {
                        ret = false;
                        break;
                }

                switch (type) {
                case json_type_boolean:
                        *va_arg(ap, bool *) = json_object_get_boolean(value);
                        break;
                case json_type_string:
                        *va_arg(ap, const char **) =
                                json_object_get_string(value);
                        break;
                case json_type_double:
                        *va_arg(ap, double *) = json_object_get_double(value);
                        break;
                case json_type_int:
                        *va_arg(ap, int64_t *) = json_object_get_int64(value);
                        break;
                case json_type_object:
                case json_type_array:
                        *va_arg(ap, struct json_object **) = value;
                        break;
                default:
                        pcx_fatal("Unexpected json type");
                }
        }

        va_end(ap);

        return ret;
}
