/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2022  Neil Roberts
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

#include "pcx-class-store.h"

#include <assert.h>

#include "pcx-list.h"
#include "pcx-text.h"
#include "pcx-util.h"

struct store_entry {
        struct pcx_list link;

        int ref_count;

        const void *class;
        enum pcx_text_language language;

        void *data;

        const struct pcx_class_store_callbacks *callbacks;
};

struct pcx_class_store {
        struct pcx_list entries;
};

struct pcx_class_store *
pcx_class_store_new(void)
{
        struct pcx_class_store *store = pcx_alloc(sizeof *store);

        pcx_list_init(&store->entries);

        return store;
}

void *
pcx_class_store_ref_data(struct pcx_class_store *store,
                         const struct pcx_config *config,
                         const void *class,
                         enum pcx_text_language language,
                         const struct pcx_class_store_callbacks *callbacks)
{
        struct store_entry *entry;

        /* Check if we already have the data */

        pcx_list_for_each(entry, &store->entries, link) {
                if (entry->class == class &&
                    entry->language == language) {
                        entry->ref_count++;
                        return entry->data;
                }
        }

        entry = pcx_alloc(sizeof *entry);

        entry->ref_count = 1;
        entry->class = class;
        entry->language = language;

        entry->data = callbacks->create_data(config, language);
        entry->callbacks = callbacks;

        pcx_list_insert(&store->entries, &entry->link);

        return entry->data;
}

void
pcx_class_store_unref_data(struct pcx_class_store *store,
                           void *data)
{
        struct store_entry *entry;

        pcx_list_for_each(entry, &store->entries, link) {
                if (entry->data == data) {
                        if (--entry->ref_count <= 0) {
                                entry->callbacks->free_data(entry->data);
                                pcx_list_remove(&entry->link);
                                pcx_free(entry);
                        }

                        return;
                }
        }

        assert(!"Couldn’t find entry for class data");
}

void
pcx_class_store_free(struct pcx_class_store *store)
{
        /* All of the references should have been removed before
         * freeing the store.
         */
        assert(pcx_list_empty(&store->entries));

        pcx_free(store);
}
