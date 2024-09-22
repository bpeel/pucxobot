/*
 * Pucxobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2013  Neil Roberts
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef PCX_SLICE_H
#define PCX_SLICE_H

#include <stdalign.h>

#include "pcx-util.h"
#include "pcx-slab.h"

struct pcx_slice {
        struct pcx_slice *next;
};

struct pcx_slice_allocator {
        size_t element_size;
        size_t element_alignment;
        struct pcx_slice *magazine;
        struct pcx_slab_allocator slab;
};

#define PCX_SLICE_ALLOCATOR(type, name)                                 \
        static struct pcx_slice_allocator                               \
        name = {                                                        \
                .element_size = MAX(sizeof(type), sizeof (struct pcx_slice)), \
                .element_alignment = alignof(type),                     \
                .magazine = NULL,                                       \
                .slab = PCX_SLAB_STATIC_INIT                            \
        }

void
pcx_slice_allocator_init(struct pcx_slice_allocator *allocator,
                         size_t size,
                         size_t alignment);

void
pcx_slice_allocator_destroy(struct pcx_slice_allocator *allocator);

void *
pcx_slice_alloc(struct pcx_slice_allocator *allocator);

void
pcx_slice_free(struct pcx_slice_allocator *allocator,
               void *ptr);

#endif /* PCX_SLICE_H */
