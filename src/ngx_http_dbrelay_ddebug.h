#ifndef DDEBUG_H
#define DDEBUG_H

#include <ngx_core.h>

#if defined(DDEBUG) && (DDEBUG)

#   if (NGX_HAVE_VARIADIC_MACROS)

#       define dd(...) fprintf(stderr, "ngx_dbrelay *** %s: ", __func__); \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, " at %s line %d.\n", __FILE__, __LINE__)

#   else

#include <stdarg.h>
#include <stdio.h>

#include <stdarg.h>

static void dd(const char * fmt, ...) {
}

#    endif

#endif

#endif /* DDEBUG_H */

