/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2012, 2013 Intel Corporation
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

/* This list implementation is based on the Wayland source code */

#ifndef PCX_LIST_H
#define PCX_LIST_H

/**
 * PcxList - linked list
 *
 * The list head is of "PcxList" type, and must be initialized
 * using pcx_list_init().  All entries in the list must be of the same
 * type.  The item type must have a "PcxList" member. This
 * member will be initialized by pcx_list_insert(). There is no need to
 * call pcx_list_init() on the individual item. To query if the list is
 * empty in O(1), use pcx_list_empty().
 *
 * Let's call the list reference "PcxList foo_list", the item type as
 * "item_t", and the item member as "PcxList link". The following code
 *
 * The following code will initialize a list:
 *
 *      pcx_list_init (foo_list);
 *      pcx_list_insert (foo_list, item1);      Pushes item1 at the head
 *      pcx_list_insert (foo_list, item2);      Pushes item2 at the head
 *      pcx_list_insert (item2, item3);         Pushes item3 after item2
 *
 * The list now looks like [item2, item3, item1]
 *
 * Will iterate the list in ascending order:
 *
 *      item_t *item;
 *      pcx_list_for_each(item, foo_list, link) {
 *              Do_something_with_item(item);
 *      }
 */

struct pcx_list {
        struct pcx_list *prev;
        struct pcx_list *next;
};

void
pcx_list_init(struct pcx_list *list);

void
pcx_list_insert(struct pcx_list *list, struct pcx_list *elm);

void
pcx_list_remove(struct pcx_list *elm);

int
pcx_list_length(const struct pcx_list *list);

int
pcx_list_empty(struct pcx_list *list);

void
pcx_list_insert_list(struct pcx_list *list, struct pcx_list *other);

/* This assigns to iterator first so that taking a reference to it
 * later in the second step won't be an undefined operation. It
 * assigns the value of list_node rather than 0 so that it is possible
 * have list_node be based on the previous value of iterator. In that
 * respect iterator is just used as a convenient temporary variable.
 * The compiler optimises all of this down to a single subtraction by
 * a constant */
#define pcx_list_set_iterator(list_node, iterator, member)              \
        ((iterator) = (void *) (list_node),                             \
         (iterator) = (void *) ((char *) (iterator) -                   \
                                (((char *) &(iterator)->member) -       \
                                 (char *) (iterator))))

#define pcx_container_of(ptr, type, member)                     \
        (type *) ((char *) (ptr) - offsetof (type, member))

#define pcx_list_for_each(pos, head, member)                            \
        for (pcx_list_set_iterator ((head)->next, pos, member);         \
             &pos->member != (head);                                    \
             pcx_list_set_iterator (pos->member.next, pos, member))

#define pcx_list_for_each_safe(pos, tmp, head, member)                  \
        for (pcx_list_set_iterator ((head)->next, pos, member),         \
                     pcx_list_set_iterator ((pos)->member.next, tmp, member); \
             &pos->member != (head);                                    \
             pos = tmp,                                                 \
                     pcx_list_set_iterator (pos->member.next, tmp, member))

#define pcx_list_for_each_reverse(pos, head, member)                    \
        for (pcx_list_set_iterator ((head)->prev, pos, member);         \
             &pos->member != (head);                                    \
             pcx_list_set_iterator (pos->member.prev, pos, member))

#endif /* PCX_LIST_H */
