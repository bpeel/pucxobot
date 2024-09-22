/*
 * Pucxobot - A bot and website to play some card games
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

#ifndef PCX_CLASS_STORE_H
#define PCX_CLASS_STORE_H

#include "pcx-config.h"
#include "pcx-text.h"

/* This weirdly named thing is meant to support data shared between
 * multiple instances of a “class” (ie, a game type). The data is
 * indexed by a class pointer and the language used. It is reference
 * counted so that if multiple instances of the game are running they
 * can reuse the same data. This is meant to be used to share data
 * loaded off the disk between multiple instances.
 */

struct pcx_class_store;

struct pcx_class_store_callbacks {
        void *
        (* create_data)(const struct pcx_config *config,
                        enum pcx_text_language language);
        void
        (* free_data)(void *data);
};

struct pcx_class_store *
pcx_class_store_new(void);

/* Retrieves the data for the given class and language combination. If
 * the data already exists then its reference count is increased and
 * the same pointer is returned. Otherwise the data is created using
 * the callbacks provided.
 */
void *
pcx_class_store_ref_data(struct pcx_class_store *store,
                         const struct pcx_config *config,
                         const void *class,
                         enum pcx_text_language language,
                         const struct pcx_class_store_callbacks *callbacks);

/* Remove a reference on the data. If it is the last reference then
 * the data will be freed using the callbacks provided when the data
 * was created.
 */
void
pcx_class_store_unref_data(struct pcx_class_store *store,
                           void *data);

/* All of the references should have already been removed before
 * calling this.
 */
void
pcx_class_store_free(struct pcx_class_store *store);

#endif /* PCX_CLASS_STORE_H */
