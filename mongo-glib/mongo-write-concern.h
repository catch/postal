/* mongo-write-concern.h
 *
 * Copyright (C) 2012 Christian Hergert <chris@dronelabs.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MONGO_WRITE_CONCERN_H
#define MONGO_WRITE_CONCERN_H

#include <glib.h>

#include "mongo-bson.h"
#include "mongo-message.h"

G_BEGIN_DECLS

typedef struct _MongoWriteConcern MongoWriteConcern;

GType              mongo_write_concern_get_type           (void) G_GNUC_CONST;
MongoWriteConcern *mongo_write_concern_new                (void);
MongoWriteConcern *mongo_write_concern_new_unsafe         (void);
MongoWriteConcern *mongo_write_concern_copy               (MongoWriteConcern *concern);
void               mongo_write_concern_free               (MongoWriteConcern *concern);

gint               mongo_write_concern_get_w              (MongoWriteConcern *concern);
void               mongo_write_concern_set_fsync          (MongoWriteConcern *concern,
                                                           gboolean           _fsync);
void               mongo_write_concern_set_journal        (MongoWriteConcern *concern,
                                                           gboolean           journal);
void               mongo_write_concern_set_w              (MongoWriteConcern *concern,
                                                           gint               w);
void               mongo_write_concern_set_w_majority     (MongoWriteConcern *concern);
void               mongo_write_concern_set_w_tags         (MongoWriteConcern *concern,
                                                           const MongoBson   *tags);
void               mongo_write_concern_set_wtimeoutms     (MongoWriteConcern *concern,
                                                           guint              wtimeoutms);

MongoMessage      *mongo_write_concern_build_getlasterror (MongoWriteConcern *concern,
                                                           const gchar       *db_and_collection);

G_END_DECLS

#endif /* MONGO_WRITE_CONCERN_H */
