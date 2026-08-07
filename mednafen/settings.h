#ifndef MDFN_SETTINGS_H
#define MDFN_SETTINGS_H

#include <boolean.h>

// This should assert() or something if the setting isn't found, since it would
// be a totally tubular error!
bool MDFN_GetSettingB(const char *name);
#endif
