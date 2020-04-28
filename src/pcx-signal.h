/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2013, 2020 Neil Roberts
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

/* This file was originally borrowed from the Wayland source code */

#ifndef PCX_SIGNAL_H
#define PCX_SIGNAL_H

#include <stdbool.h>

#include "pcx-list.h"

struct pcx_listener;

typedef bool
(* pcx_notify_func)(struct pcx_listener *listener, void *data);

struct pcx_signal {
        struct pcx_list listener_list;
};

struct pcx_listener {
        struct pcx_list link;
        pcx_notify_func notify;
};

static inline void
pcx_signal_init(struct pcx_signal *signal)
{
        pcx_list_init(&signal->listener_list);
}

static inline void
pcx_signal_add(struct pcx_signal *signal,
               struct pcx_listener *listener)
{
        pcx_list_insert(signal->listener_list.prev, &listener->link);
}

static inline bool
pcx_signal_emit(struct pcx_signal *signal, void *data)
{
        struct pcx_listener *l, *next;

        pcx_list_for_each_safe(l, next, &signal->listener_list, link)
                if (!l->notify(l, data))
                        return false;

        return true;
}

#endif /* PCX_SIGNAL_H */
