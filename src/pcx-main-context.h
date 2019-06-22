/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2011, 2013, 2019  Neil Roberts
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

#ifndef PCX_MAIN_CONTEXT_H
#define PCX_MAIN_CONTEXT_H

#include <stdint.h>

#include "pcx-util.h"

enum pcx_main_context_poll_flags {
        PCX_MAIN_CONTEXT_POLL_IN = 1 << 0,
        PCX_MAIN_CONTEXT_POLL_OUT = 1 << 1,
        PCX_MAIN_CONTEXT_POLL_ERROR = 1 << 2,
};

struct pcx_main_context;
struct pcx_main_context_source;

typedef void
(* pcx_main_context_poll_callback) (struct pcx_main_context_source *source,
                                    int fd,
                                    enum pcx_main_context_poll_flags flags,
                                    void *user_data);

typedef void
(* pcx_main_context_timer_callback) (struct pcx_main_context_source *source,
                                     void *user_data);

typedef void
(* pcx_main_context_idle_callback) (struct pcx_main_context_source *source,
                                    void *user_data);

typedef void
(* pcx_main_context_quit_callback) (struct pcx_main_context_source *source,
                                    void *user_data);

struct pcx_main_context *
pcx_main_context_new(void);

struct pcx_main_context *
pcx_main_context_get_default(void);

struct pcx_main_context_source *
pcx_main_context_add_poll(struct pcx_main_context *mc,
                          int fd,
                          enum pcx_main_context_poll_flags flags,
                          pcx_main_context_poll_callback callback,
                          void *user_data);

void
pcx_main_context_modify_poll(struct pcx_main_context_source *source,
                             enum pcx_main_context_poll_flags flags);

struct pcx_main_context_source *
pcx_main_context_add_quit(struct pcx_main_context *mc,
                          pcx_main_context_quit_callback callback,
                          void *user_data);

struct pcx_main_context_source *
pcx_main_context_add_timer(struct pcx_main_context *mc,
                           int minutes,
                           pcx_main_context_timer_callback callback,
                           void *user_data);

struct pcx_main_context_source *
pcx_main_context_add_idle(struct pcx_main_context *mc,
                          pcx_main_context_idle_callback callback,
                          void *user_data);

void
pcx_main_context_remove_source(struct pcx_main_context_source *source);

void
pcx_main_context_poll(struct pcx_main_context *mc);

uint64_t
pcx_main_context_get_monotonic_clock(struct pcx_main_context *mc);

int64_t
pcx_main_context_get_wall_clock(struct pcx_main_context *mc);

void
pcx_main_context_free(struct pcx_main_context *mc);

#endif /* PCX_MAIN_CONTEXT_H */
