#ifndef MDFN_SETTINGS_H
#define MDFN_SETTINGS_H

#include <stdint.h>
#include <string.h>

#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t setting_wswan_byear;
extern uint32_t setting_wswan_bmonth;
extern uint32_t setting_wswan_bday;
extern uint32_t setting_wswan_sex;
extern uint32_t setting_wswan_blood;
extern uint32_t setting_wswan_language;
extern char setting_wswan_name[17];

uint64_t MDFN_GetSettingUI(const char *name);
int64_t MDFN_GetSettingI(const char *name);
bool MDFN_GetSettingB(const char *name);
const char *MDFN_GetSettingS(const char *name);

#ifdef __cplusplus
}
#endif

#endif
