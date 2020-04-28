/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
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

#include "config.h"

#include <stdint.h>

#include "pcx-slab.h"
#include "pcx-util.h"

/* All of the allocations are made out of slabs of 2kb. That way all
 * of the memory can be freed by just freeing the few slabs */
struct pcx_slab {
        struct pcx_slab *next;
};

void
pcx_slab_init(struct pcx_slab_allocator *allocator)
{
        static const struct pcx_slab_allocator init = PCX_SLAB_STATIC_INIT;

        *allocator = init;
}

static size_t
pcx_slab_align(size_t base, int alignment)
{
        return (base + alignment - 1) & ~(alignment - 1);
}

void *
pcx_slab_allocate(struct pcx_slab_allocator *allocator,
                  size_t size, int alignment)
{
        struct pcx_slab *slab;
        size_t offset;

        offset = pcx_slab_align(allocator->slab_used, alignment);

        if (size + offset > PCX_SLAB_SIZE) {
                /* Start a new slab */
                slab = pcx_alloc(PCX_SLAB_SIZE);
                slab->next = allocator->slabs;
                allocator->slabs = slab;

                offset = pcx_slab_align(sizeof(struct pcx_slab), alignment);
        } else {
                slab = allocator->slabs;
        }

        allocator->slab_used = offset + size;

        return (uint8_t *) slab + offset;
}

void
pcx_slab_destroy(struct pcx_slab_allocator *allocator)
{
        struct pcx_slab *slab, *next;

        for (slab = allocator->slabs; slab; slab = next) {
                next = slab->next;
                pcx_free(slab);
        }
}
