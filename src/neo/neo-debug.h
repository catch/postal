#ifndef NEO_DEBUG_H
#define NEO_DEBUG_H

#include <glib.h>

G_BEGIN_DECLS

#ifndef NEO_LOG_LEVEL_TRACE
#define NEO_LOG_LEVEL_TRACE (1 << G_LOG_LEVEL_USER_SHIFT)
#endif

#ifdef NEO_TRACE
#define NEO_ENTRY                                                   \
    g_log(G_LOG_DOMAIN, NEO_LOG_LEVEL_TRACE,                        \
          "ENTRY: %s():%d", G_STRFUNC, __LINE__)
#define NEO_EXIT                                                    \
    G_STMT_START {                                                  \
        g_log(G_LOG_DOMAIN, NEO_LOG_LEVEL_TRACE,                    \
              " EXIT: %s():%d", G_STRFUNC, __LINE__);               \
        return;                                                     \
    } G_STMT_END
#define NEO_RETURN(_r)                                              \
    G_STMT_START {                                                  \
    	g_log(G_LOG_DOMAIN, NEO_LOG_LEVEL_TRACE,                      \
              " EXIT: %s():%d", G_STRFUNC, __LINE__);               \
        return _r;                                                  \
    } G_STMT_END
#define NEO_GOTO(_l)                                                \
    G_STMT_START {                                                  \
        g_log(G_LOG_DOMAIN, NEO_LOG_LEVEL_TRACE,                    \
              " GOTO: %s():%d %s", G_STRFUNC, __LINE__, #_l);       \
        goto _l;                                                    \
    } G_STMT_END
#else
#define NEO_ENTRY
#define NEO_EXIT       return
#define NEO_RETURN(_r) return _r
#define NEO_GOTO(_l)   goto _l
#endif /* NEO_TRACE */

G_END_DECLS

#endif /* NEO_DEBUG_H */
