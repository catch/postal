/* postal-bson.h
 *
 * Copyright (C) 2012 Catch.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef POSTAL_BSON_H
#define POSTAL_BSON_H

#include <glib.h>
#include <json-glib/json-glib.h>
#include <mongo-glib.h>

G_BEGIN_DECLS

JsonNode *postal_bson_to_json (const MongoBson *bson);

G_END_DECLS

#endif /* POSTAL_BSON_H */
