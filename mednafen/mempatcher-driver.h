#ifndef __MDFN_MEMPATCHER_DRIVER_H
#define __MDFN_MEMPATCHER_DRIVER_H

#include "mednafen-types.h"
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

int MDFNI_AddCheat(const char *name, uint32 addr, uint64 val, uint64 compare, char type, unsigned int length, bool bigendian);
void MDFN_FlushGameCheats(int nosave);

#ifdef __cplusplus
}
#endif

#endif
