/***************************************************************************
 *   spu_freeze.h - SPU savestate struct, extracted from plugins.h.
 *
 *   The dfsound plugin needs SPUFreeze_t in freeze.c, but cannot
 *   include plugins.h (which declares the SPU* function-pointer
 *   typedefs that share names with the plugin's actual functions).
 *   Pulling SPUFreeze_t into its own minimal header lets both
 *   libpcsxcore (via plugins.h) and dfsound consume it cleanly.
 ***************************************************************************/

#ifndef __SPU_FREEZE_H__
#define __SPU_FREEZE_H__

#include <stdint.h>
#include "decode_xa.h"  /* xa_decode_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SPUFreeze {
	unsigned char PluginName[8];
	uint32_t PluginVersion;
	uint32_t Size;
	unsigned char SPUPorts[0x200];
	unsigned char SPURam[0x80000];
	xa_decode_t xa;
	unsigned char *SPUInfo;
} SPUFreeze_t;

#ifdef __cplusplus
}
#endif

#endif /* __SPU_FREEZE_H__ */
