#ifndef MDFN_SETTINGS_H
#define MDFN_SETTINGS_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t setting_ngp_language;

bool MDFN_GetSettingB(const char *name);

#ifdef __cplusplus
}
#endif

#endif
