/* Implementation of the C99 helpers declared in msvc-compat.h (MSVC 2010 and
 * Xbox 360 XDK builds only). */
#include <mgba-util/common.h>

#ifdef MGBA_MSVC_C89

int mgba_c89_vsnprintf(char* str, size_t size, const char* format, va_list args) {
	int count = -1;
	if (str && size) {
		count = _vsnprintf(str, size, format, args);
		str[size - 1] = '\0';
	}
	if (count < 0 || (size_t) count >= size) {
		/* Truncated (or no buffer): report the full length like C99 does.
		 * va_list is a plain pointer on x86 and PowerPC MSVC, so it can be
		 * reused here without va_copy. */
		count = _vscprintf(format, args);
	}
	return count;
}

int mgba_c89_snprintf(char* str, size_t size, const char* format, ...) {
	int count;
	va_list args;
	va_start(args, format);
	count = mgba_c89_vsnprintf(str, size, format, args);
	va_end(args);
	return count;
}

#endif
