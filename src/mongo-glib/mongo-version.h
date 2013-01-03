/* mongo-version.h.in
 *
 * Copyright (C) 2012 Christian Hergert <christian@hergert.me>
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

#ifndef __MONGO_VERSION_H__
#define __MONGO_VERSION_H__

/**
 * SECTION:mongo-version
 * @short_description: MONGO-GLib version checking
 *
 * MONGO-GLib provides macros to check the version of the library
 * at compile-time
 */

/**
 * MONGO_MAJOR_VERSION:
 *
 * Mongo major version component (e.g. 1 if %MONGO_VERSION is 1.2.3)
 */
#define MONGO_MAJOR_VERSION (0)

/**
 * MONGO_MINOR_VERSION:
 *
 * Mongo minor version component (e.g. 2 if %MONGO_VERSION is 1.2.3)
 */
#define MONGO_MINOR_VERSION (3)

/**
 * MONGO_MICRO_VERSION:
 *
 * Mongo micro version component (e.g. 3 if %MONGO_VERSION is 1.2.3)
 */
#define MONGO_MICRO_VERSION (5)

/**
 * MONGO_VERSION:
 *
 * Mongo version.
 */
#define MONGO_VERSION (0.3.5)

/**
 * MONGO_VERSION_S:
 *
 * Mongo version, encoded as a string, useful for printing and
 * concatenation.
 */
#define MONGO_VERSION_S "0.3.5"

/**
 * MONGO_VERSION_HEX:
 *
 * Mongo version, encoded as an hexadecimal number, useful for
 * integer comparisons.
 */
#define MONGO_VERSION_HEX (MONGO_MAJOR_VERSION << 24 | \
                           MONGO_MINOR_VERSION << 16 | \
                           MONGO_MICRO_VERSION << 8)

/**
 * MONGO_CHECK_VERSION:
 * @major: required major version
 * @minor: required minor version
 * @micro: required micro version
 *
 * Compile-time version checking. Evaluates to %TRUE if the version
 * of Mongo is greater than the required one.
 */
#define MONGO_CHECK_VERSION(major,minor,micro)   \
        (MONGO_MAJOR_VERSION > (major) || \
         (MONGO_MAJOR_VERSION == (major) && MONGO_MINOR_VERSION > (minor)) || \
         (MONGO_MAJOR_VERSION == (major) && MONGO_MINOR_VERSION == (minor) && \
          MONGO_MICRO_VERSION >= (micro)))

#endif /* __MONGO_VERSION_H__ */
