/* postal-debug.h
 *
 * Copyright (C) 2012 Christian Hergert <christian@hergert.me>
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

#ifndef POSTAL_DEBUG_H
#define POSTAL_DEBUG_H

#include <glib.h>

G_BEGIN_DECLS

#ifndef POSTAL_LOG_LEVEL_TRACE
#define POSTAL_LOG_LEVEL_TRACE (1 << G_LOG_LEVEL_USER_SHIFT)
#endif

#ifdef POSTAL_TRACE
#define ENTRY                                                 \
    g_log(G_LOG_DOMAIN, POSTAL_LOG_LEVEL_TRACE,               \
          "ENTRY: %s():%d", G_STRFUNC, __LINE__)
#define EXIT                                                  \
    G_STMT_START {                                            \
        g_log(G_LOG_DOMAIN, POSTAL_LOG_LEVEL_TRACE,           \
              " EXIT: %s():%d", G_STRFUNC, __LINE__);         \
        return;                                               \
    } G_STMT_END
#define RETURN(_r)                                            \
    G_STMT_START {                                            \
    	g_log(G_LOG_DOMAIN, POSTAL_LOG_LEVEL_TRACE,             \
              " EXIT: %s():%d", G_STRFUNC, __LINE__);         \
        return _r;                                            \
    } G_STMT_END
#define GOTO(_l)                                              \
    G_STMT_START {                                            \
        g_log(G_LOG_DOMAIN, POSTAL_LOG_LEVEL_TRACE,           \
              " GOTO: %s():%d %s", G_STRFUNC, __LINE__, #_l); \
        goto _l;                                              \
    } G_STMT_END
#else
#define ENTRY
#define EXIT       return
#define RETURN(_r) return _r
#define GOTO(_l)   goto _l
#endif /* POSTAL_TRACE */

G_END_DECLS

#endif /* POSTAL_DEBUG_H */
