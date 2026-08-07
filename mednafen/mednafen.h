#ifndef _MEDNAFEN_H
#define _MEDNAFEN_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "git.h"

#include "mednafen-types.h"

extern MDFNGI *MDFNGameInfo;

void MDFN_LoadGameCheats(void);
void MDFN_FlushGameCheats(void);

#endif
