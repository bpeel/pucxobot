/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2015, 2020  Neil Roberts
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

/* This header is included multiple times with different definitions
 * of the PCX_PROTO_TYPE macro. The macro will be called like this:
 *
 * PCX_PROTO_TYPE(enum_name - the name of the type in pcx_proto_type
 *               type_name - the name of the C type
 *               ap_type_name - the name of a type that can be used to
 *                              retrieve the value with va_arg)
 */

#define PCX_PROTO_TYPE_SIMPLE(enum_name, type_name)      \
        PCX_PROTO_TYPE(enum_name, type_name, type_name)

PCX_PROTO_TYPE(PCX_PROTO_TYPE_UINT8, uint8_t, unsigned int)
PCX_PROTO_TYPE(PCX_PROTO_TYPE_UINT16, uint16_t, unsigned int)
PCX_PROTO_TYPE_SIMPLE(PCX_PROTO_TYPE_UINT32, uint32_t)
PCX_PROTO_TYPE_SIMPLE(PCX_PROTO_TYPE_UINT64, uint64_t)

#undef PCX_PROTO_TYPE_SIMPLE
