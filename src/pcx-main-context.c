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

#include <errno.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <assert.h>

#include "pcx-main-context.h"
#include "pcx-list.h"
#include "pcx-util.h"
#include "pcx-slice.h"
#include "pcx-buffer.h"

struct pcx_main_context {
        /* Array for receiving events */
        struct pcx_buffer poll_array;
        bool poll_array_dirty;

        struct pcx_list timeout_sources;
        struct pcx_list poll_sources;
        struct pcx_list signal_sources;

        struct pcx_main_context_source *async_pipe_source;
        int async_pipe[2];

        int signal_buf;
        size_t signal_read;

        bool monotonic_time_valid;
        int64_t monotonic_time;

        bool wall_time_valid;
        int64_t wall_time;

        struct pcx_slice_allocator source_allocator;
};

struct pcx_main_context_source {
        enum {
                PCX_MAIN_CONTEXT_POLL_SOURCE,
                PCX_MAIN_CONTEXT_TIMEOUT_SOURCE,
                PCX_MAIN_CONTEXT_SIGNAL_SOURCE,
        } type;

        union {
                /* Poll sources */
                struct {
                        int fd;
                        enum pcx_main_context_poll_flags current_flags;
                };

                /* Timeout sources */
                struct {
                        uint64_t end_time;
                        bool busy;
                        bool removed;
                };

                /* Signal sources */
                struct {
                        void (* old_handler)(int);
                        int signal_num;
                };
        };

        void *user_data;
        void *callback;
        struct pcx_list link;

        struct pcx_main_context *mc;
};

static struct pcx_main_context *pcx_main_context_default = NULL;

struct pcx_main_context *
pcx_main_context_get_default(void)
{
        if (pcx_main_context_default == NULL)
                pcx_main_context_default = pcx_main_context_new();

        return pcx_main_context_default;
}

static void
free_source(struct pcx_main_context *mc,
            struct pcx_main_context_source *source)
{
        pcx_list_remove(&source->link);
        pcx_slice_free(&mc->source_allocator, source);
}

static void
emit_signal_source(struct pcx_main_context *mc,
                   int signal_num)
{
        struct pcx_main_context_source *source, *tmp;
        pcx_main_context_signal_callback callback;

        pcx_list_for_each_safe(source, tmp, &mc->signal_sources, link) {
                if (source->signal_num != signal_num)
                        continue;

                callback = source->callback;
                callback(source, signal_num, source->user_data);
        }
}

static void
async_pipe_cb(struct pcx_main_context_source *source,
              int fd,
              enum pcx_main_context_poll_flags flags,
              void *user_data)
{
        struct pcx_main_context *mc = user_data;
        ssize_t got = read(mc->async_pipe[0],
                           (uint8_t *) &mc->signal_buf + mc->signal_read,
                           (sizeof mc->signal_buf) - mc->signal_read);

        if (got == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                        pcx_warning("Read from quit pipe failed: %s",
                                    strerror(errno));
        } else {
                mc->signal_read += got;

                if (mc->signal_read >= sizeof mc->signal_buf) {
                        mc->signal_read = 0;
                        emit_signal_source(mc, mc->signal_buf);
                }
        }
}

static void
pcx_main_context_signal_cb(int signum)
{
        struct pcx_main_context *mc = pcx_main_context_get_default();
        size_t s = 0;

        while (s < sizeof signum) {
                ssize_t wrote = write(mc->async_pipe[1],
                                      (uint8_t *) &signum + s,
                                      (sizeof signum) - s);

                if (wrote == -1) {
                        if (errno != EINTR)
                                break;
                } else {
                        s += wrote;
                }
        }
}

struct pcx_main_context *
pcx_main_context_new(void)
{
        struct pcx_main_context *mc = pcx_alloc(sizeof *mc);

        pcx_slice_allocator_init(&mc->source_allocator,
                                 sizeof(struct pcx_main_context_source),
                                 alignof(struct pcx_main_context_source));
        mc->monotonic_time_valid = false;
        mc->wall_time_valid = false;
        mc->poll_array_dirty = true;
        pcx_buffer_init(&mc->poll_array);
        pcx_list_init(&mc->poll_sources);
        pcx_list_init(&mc->timeout_sources);
        pcx_list_init(&mc->signal_sources);

        mc->signal_read = 0;

        if (pipe(mc->async_pipe) == -1) {
                pcx_warning("Failed to create pipe: %s",
                            strerror(errno));
        } else {
                mc->async_pipe_source
                        = pcx_main_context_add_poll(mc, mc->async_pipe[0],
                                                    PCX_MAIN_CONTEXT_POLL_IN,
                                                    async_pipe_cb,
                                                    mc);
        }

        return mc;
}

struct pcx_main_context_source *
pcx_main_context_add_poll(struct pcx_main_context *mc,
                          int fd,
                          enum pcx_main_context_poll_flags flags,
                          pcx_main_context_poll_callback callback,
                          void *user_data)
{
        struct pcx_main_context_source *source;

        if (mc == NULL)
                mc = pcx_main_context_get_default();

        source = pcx_slice_alloc(&mc->source_allocator);

        source->mc = mc;
        source->fd = fd;
        source->callback = callback;
        source->type = PCX_MAIN_CONTEXT_POLL_SOURCE;
        source->user_data = user_data;
        source->current_flags = flags;
        pcx_list_insert(&mc->poll_sources, &source->link);

        mc->poll_array_dirty = true;

        return source;
}

void
pcx_main_context_modify_poll(struct pcx_main_context_source *source,
                             enum pcx_main_context_poll_flags flags)
{
        assert(source->type == PCX_MAIN_CONTEXT_POLL_SOURCE);

        if (source->current_flags == flags)
                return;

        source->current_flags = flags;
        source->mc->poll_array_dirty = true;
}

struct pcx_main_context_source *
pcx_main_context_add_signal_source(struct pcx_main_context *mc,
                                   int signal_num,
                                   pcx_main_context_signal_callback callback,
                                   void *user_data)
{
        struct pcx_main_context_source *source;

        if (mc == NULL)
                mc = pcx_main_context_get_default();

        source = pcx_slice_alloc(&mc->source_allocator);

        source->mc = mc;
        source->callback = callback;
        source->type = PCX_MAIN_CONTEXT_SIGNAL_SOURCE;
        source->user_data = user_data;
        source->signal_num = signal_num;
        source->old_handler = signal(signal_num, pcx_main_context_signal_cb);

        pcx_list_insert(&mc->signal_sources, &source->link);

        return source;
}

struct pcx_main_context_source *
pcx_main_context_add_timeout(struct pcx_main_context *mc,
                             long ms,
                             pcx_main_context_timeout_callback callback,
                             void *user_data)
{
        struct pcx_main_context_source *source;

        if (mc == NULL)
                mc = pcx_main_context_get_default();

        source = pcx_slice_alloc(&mc->source_allocator);

        source->mc = mc;
        source->callback = callback;
        source->type = PCX_MAIN_CONTEXT_TIMEOUT_SOURCE;
        source->user_data = user_data;
        source->removed = false;
        source->busy = false;
        source->end_time = (pcx_main_context_get_monotonic_clock(mc) +
                            ms * UINT64_C(1000));

        pcx_list_insert(&mc->timeout_sources, &source->link);

        return source;
}

void
pcx_main_context_remove_source(struct pcx_main_context_source *source)
{
        struct pcx_main_context *mc = source->mc;

        switch (source->type) {
        case PCX_MAIN_CONTEXT_POLL_SOURCE:
                free_source(mc, source);
                mc->poll_array_dirty = true;
                break;

        case PCX_MAIN_CONTEXT_SIGNAL_SOURCE:
                signal(source->signal_num, source->old_handler);
                free_source(mc, source);
                break;

        case PCX_MAIN_CONTEXT_TIMEOUT_SOURCE:
                /* Timer sources need to be able to be removed while
                 * iterating the source list to emit, so we need to
                 * handle them specially during iteration. */
                assert(!source->removed);
                if (source->busy)
                        source->removed = true;
                else
                        free_source(mc, source);
                break;
        }
}

static int
get_timeout(struct pcx_main_context *mc)
{
        if (pcx_list_empty(&mc->timeout_sources))
                return -1;

        uint64_t now = pcx_main_context_get_monotonic_clock(mc);
        int min_ms = INT_MAX;

        struct pcx_main_context_source *source;

        pcx_list_for_each(source, &mc->timeout_sources, link) {
                if (source->end_time <= now)
                        return 0;
                uint64_t ms_to_wait = (source->end_time - now) / 1000;

                if (ms_to_wait < INT_MAX && (int) ms_to_wait < min_ms)
                        min_ms = (int) ms_to_wait;
        }

        return min_ms;
}

static void
check_timer_sources(struct pcx_main_context *mc)
{
        if (pcx_list_empty(&mc->timeout_sources))
                return;

        uint64_t now = pcx_main_context_get_monotonic_clock(mc);

        /* Collect all of the sources to emit into a list and mark
         * them as busy. That way if they are removed they will just
         * be marked as removed instead of actually modifying the
         * timeout list. That way any timers can be removed as a
         * result of invoking any callback.
         */
        struct pcx_list to_emit;
        pcx_list_init(&to_emit);

        struct pcx_main_context_source *source, *tmp_source;

        pcx_list_for_each_safe(source, tmp_source, &mc->timeout_sources, link) {
                if (source->end_time <= now) {
                        pcx_list_remove(&source->link);
                        pcx_list_insert(&to_emit, &source->link);
                        source->busy = true;
                }
        }

        pcx_list_for_each(source, &to_emit, link) {
                if (source->removed)
                        continue;
                pcx_main_context_timeout_callback callback = source->callback;
                callback(source, source->user_data);
        }

        /* The timeouts are one-shot so we always remove all of them
         * after emitting.
         */
        pcx_list_for_each_safe(source, tmp_source, &to_emit, link) {
                free_source(mc, source);
        }
}

static struct pcx_main_context_source *
get_source_for_fd(struct pcx_main_context *mc,
                  int fd)
{
        struct pcx_main_context_source *source;

        pcx_list_for_each(source, &mc->poll_sources, link) {
                if (source->fd == fd)
                        return source;
        }

        pcx_fatal("Poll result found for unknown fd");
}

static void
handle_poll_result(struct pcx_main_context *mc,
                   const struct pollfd *pollfd)
{
        struct pcx_main_context_source *source;
        pcx_main_context_poll_callback callback;
        enum pcx_main_context_poll_flags flags;

        if (pollfd->revents == 0)
                return;

        source = get_source_for_fd(mc, pollfd->fd);

        callback = source->callback;
        flags = 0;

        if (pollfd->revents & POLLOUT)
                flags |= PCX_MAIN_CONTEXT_POLL_OUT;
        if (pollfd->revents & POLLIN)
                flags |= PCX_MAIN_CONTEXT_POLL_IN;
        if (pollfd->revents & POLLHUP) {
                /* If the source is polling for read then we'll
                 * just mark it as ready for reading so that any
                 * error or EOF will be handled by the read call
                 * instead of immediately aborting */
                if ((source->current_flags & PCX_MAIN_CONTEXT_POLL_IN))
                        flags |= PCX_MAIN_CONTEXT_POLL_IN;
                else
                        flags |= PCX_MAIN_CONTEXT_POLL_ERROR;
        }
        if (pollfd->revents & POLLERR)
                flags |= PCX_MAIN_CONTEXT_POLL_ERROR;

        callback(source, source->fd, flags, source->user_data);
}

static short int
get_poll_events(enum pcx_main_context_poll_flags flags)
{
        short int events = 0;

        if (flags & PCX_MAIN_CONTEXT_POLL_IN)
                events |= POLLIN;
        if (flags & PCX_MAIN_CONTEXT_POLL_OUT)
                events |= POLLOUT;

        return events;
}

static void
ensure_poll_array(struct pcx_main_context *mc)
{
        struct pcx_main_context_source *source;
        struct pollfd *pollfd;

        if (!mc->poll_array_dirty)
                return;

        mc->poll_array.length = 0;

        pcx_list_for_each(source, &mc->poll_sources, link) {
                pcx_buffer_set_length(&mc->poll_array,
                                      mc->poll_array.length +
                                      sizeof (struct pollfd));
                pollfd = (struct pollfd *) (mc->poll_array.data +
                                            mc->poll_array.length -
                                            sizeof (struct pollfd));
                pollfd->fd = source->fd;
                pollfd->events = get_poll_events(source->current_flags);
        }

        mc->poll_array_dirty = false;
}

void
pcx_main_context_poll(struct pcx_main_context *mc)
{
        int n_events;

        if (mc == NULL)
                mc = pcx_main_context_get_default();

        ensure_poll_array(mc);

        n_events = poll((struct pollfd *) mc->poll_array.data,
                        mc->poll_array.length / sizeof (struct pollfd),
                        get_timeout(mc));

        /* Once we've polled we can assume that some time has passed so our
           cached values of the clocks are no longer valid */
        mc->monotonic_time_valid = false;
        mc->wall_time_valid = false;

        if (n_events == -1) {
                if (errno != EINTR)
                        pcx_warning("poll failed: %s", strerror(errno));
        } else {
                struct pollfd *pollfds = (struct pollfd *) mc->poll_array.data;
                int i;

                for (i = 0; i < mc->poll_array.length / sizeof *pollfds; i++)
                        handle_poll_result(mc, pollfds + i);

                check_timer_sources(mc);
        }
}

uint64_t
pcx_main_context_get_monotonic_clock(struct pcx_main_context *mc)
{
        struct timespec ts;

        if (mc == NULL)
                mc = pcx_main_context_get_default();

        /* Because in theory the program doesn't block between calls
           to poll, we can act as if no time passes between calls.
           That way we can cache the clock value instead of having to
           do a system call every time we need it */
        if (!mc->monotonic_time_valid) {
                clock_gettime(CLOCK_MONOTONIC, &ts);
                mc->monotonic_time = (ts.tv_sec * UINT64_C(1000000) +
                                      ts.tv_nsec / UINT64_C(1000));
                mc->monotonic_time_valid = true;
        }

        return mc->monotonic_time;
}

int64_t
pcx_main_context_get_wall_clock(struct pcx_main_context *mc)
{
        time_t now;

        if (mc == NULL)
                mc = pcx_main_context_get_default();

        /* Because in theory the program doesn't block between calls
           to poll, we can act as if no time passes between calls.
           That way we can cache the clock value instead of having to
           do a system call every time we need it */
        if (!mc->wall_time_valid) {
                time(&now);

                mc->wall_time = now;
                mc->wall_time_valid = true;
        }

        return mc->wall_time;
}

void
pcx_main_context_free(struct pcx_main_context *mc)
{
        pcx_main_context_remove_source(mc->async_pipe_source);
        pcx_close(mc->async_pipe[0]);
        pcx_close(mc->async_pipe[1]);

        assert(pcx_list_empty(&mc->poll_sources));
        assert(pcx_list_empty(&mc->timeout_sources));
        assert(pcx_list_empty(&mc->signal_sources));

        pcx_buffer_destroy(&mc->poll_array);

        pcx_slice_allocator_destroy(&mc->source_allocator);

        pcx_free(mc);

        if (mc == pcx_main_context_default)
                pcx_main_context_default = NULL;
}
