#ifndef _WINX68K_WINCORE_H
#define _WINX68K_WINCORE_H

#include <stdint.h>
#include "common.h"

#define vline HOGEvline /* workaround for redefinition of 'vline' */

#define TOSTR(s) #s
#define PX68KVERSTR TOSTR(PX68K_VERSION)

extern	uint8_t*	FONT;

extern	uint16_t	VLINE_TOTAL;
extern	uint32_t	VLINE;
extern	uint32_t	vline;

extern	char	winx68k_dir[2048];
extern	char	winx68k_ini[2048];

void WinX68k_Reset(void);

#endif /* _WINX68K_WINCORE_H */
