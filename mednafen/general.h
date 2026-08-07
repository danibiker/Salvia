#ifndef _GENERAL_H
#define _GENERAL_H

#include <stdint.h>
#include <string.h>

#define MDFNMKF_SAV 1

#ifdef __cplusplus
extern "C" {
#endif
void MDFN_MakeFName(uint8_t type, char *s, size_t len,
      int id1, const char *cd1);
#ifdef __cplusplus
}
#endif

#endif
