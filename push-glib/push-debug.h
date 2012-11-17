/* push-debug.h
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

#ifndef PUSH_DEBUG_H
#define PUSH_DEBUG_H

#include <glib.h>

#ifndef G_LOG_LEVEL_TRACE
#define G_LOG_LEVEL_TRACE (1 << G_LOG_LEVEL_USER_SHIFT)
#endif

#define ERROR(_d, _f, ...)    g_log(#_d, G_LOG_LEVEL_ERROR, _f, ##__VA_ARGS__)
#define WARNING(_d, _f, ...)  g_log(#_d, G_LOG_LEVEL_WARNING, _f, ##__VA_ARGS__)
#ifdef PUSH_DEBUG
#define DEBUG(_d, _f, ...)    g_log(#_d, G_LOG_LEVEL_DEBUG, _f, ##__VA_ARGS__)
#else
#define DEBUG(_d, _f, ...)
#endif
#define INFO(_d, _f, ...)     g_log(#_d, G_LOG_LEVEL_INFO, _f, ##__VA_ARGS__)
#define CRITICAL(_d, _f, ...) g_log(#_d, G_LOG_LEVEL_CRITICAL, _f, ##__VA_ARGS__)

#ifdef PUSH_TRACE
#define TRACE(_d, _f, ...) g_log(#_d, G_LOG_LEVEL_TRACE, "  MSG: " _f, ##__VA_ARGS__)
#define ENTRY                                                       \
    g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE,                          \
          "ENTRY: %s():%d", G_STRFUNC, __LINE__)
#define EXIT                                                        \
    G_STMT_START {                                                  \
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE,                      \
              " EXIT: %s():%d", G_STRFUNC, __LINE__);               \
        return;                                                     \
    } G_STMT_END
#define RETURN(_r)                                                  \
    G_STMT_START {                                                  \
    	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE,                        \
              " EXIT: %s():%d", G_STRFUNC, __LINE__);               \
        return _r;                                                  \
    } G_STMT_END
#define GOTO(_l)                                                    \
    G_STMT_START {                                                  \
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE,                      \
              " GOTO: %s():%d %s", G_STRFUNC, __LINE__, #_l);       \
        goto _l;                                                    \
    } G_STMT_END
#define CASE(_l)                                                    \
    case _l:                                                        \
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE,                      \
              " CASE: %s():%d %s", G_STRFUNC, __LINE__, #_l)
#define BREAK                                                       \
    g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE,                          \
          "BREAK: %s():%d", G_STRFUNC, __LINE__);                   \
    break
#define DUMP_BYTES(_n, _b, _l)                                      \
    G_STMT_START {                                                  \
        GString *str, *astr;                                        \
        gint _i;                                                    \
        g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE,                      \
              "        %s = %p [%d]", #_n, _b, (gint)_l);           \
        str = g_string_sized_new(80);                               \
        astr = g_string_sized_new(16);                              \
        for (_i = 0; _i < _l; _i++) {                               \
            if ((_i % 16) == 0) {                                   \
                g_string_append_printf(str, "%06x: ", _i);          \
            }                                                       \
            g_string_append_printf(str, " %02x", _b[_i]);           \
            if (g_ascii_isprint(_b[_i])) {                          \
                g_string_append_printf(astr, " %c", _b[_i]);        \
            } else {                                                \
                g_string_append(astr, " .");                        \
            }                                                       \
            if ((_i % 16) == 15) {                                  \
                g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE,              \
                      "%s  %s", str->str, astr->str);               \
                g_string_truncate(str, 0);                          \
                g_string_truncate(astr, 0);                         \
            } else if ((_i % 16) == 7) {                            \
                g_string_append(str, " ");                          \
                g_string_append(astr, " ");                         \
            }                                                       \
        }                                                           \
        if (_i != 16) {                                             \
            g_log(G_LOG_DOMAIN, G_LOG_LEVEL_TRACE,                  \
                  "%-56s   %s", str->str, astr->str);               \
        }                                                           \
        g_string_free(str, TRUE);                                   \
        g_string_free(astr, TRUE);                                  \
    } G_STMT_END
#define DUMP_UINT64(_n, _v) DUMP_BYTES(_n, ((guint8*)&_v), sizeof(guint64))
#define DUMP_UINT(_n, _v) DUMP_BYTES(_n, ((guint8*)&_v), sizeof(guint))
#else
#define TRACE(_d, _f, ...)
#define ENTRY
#define EXIT       return
#define RETURN(_r) return _r
#define GOTO(_l)   goto _l
#define CASE(_l)   case _l:
#define BREAK      break
#define DUMP_BYTES(_n, _b, _l)
#define DUMP_UINT64(_n, _v)
#define DUMP_UINT(_n, _v)
#endif

#define CASE_RETURN_STR(_l) case _l: return #_l

#endif /* PUSH_DEBUG_H */
