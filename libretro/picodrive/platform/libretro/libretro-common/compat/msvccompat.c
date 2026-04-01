#if defined(_MSC_VER) && _MSC_VER < 1900
#include <stdio.h>
#include <stdarg.h>

/* Implementación de snprintf para MSVC antiguo */
int c99_snprintf_retro__(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;
    va_start(ap, format);
    count = _vsnprintf(outBuf, size, format, ap);
    va_end(ap);
    return count;
}

/* Implementación de vsnprintf para MSVC antiguo */
int c99_vsnprintf_retro__(char *outBuf, size_t size, const char *format, va_list ap)
{
    return _vsnprintf(outBuf, size, format, ap);
}
#endif
