#ifndef GBA_CHEATS_H
#define GBA_CHEATS_H

#ifndef __cplusplus
#include <boolean.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct CheatsData {
  int code;
  int size;
  int status;
  bool enabled;
  uint32_t rawaddress;
  uint32_t address;
  uint32_t value;
  uint32_t oldValue;
  char codestring[20];
  char desc[32];
};
typedef struct CheatsData CheatsData;

void cheatsAdd(const char *codeStr, const char *desc, uint32_t rawaddress, uint32_t address, uint32_t value, int code, int size);
void cheatsAddGSACode(const char *code, const char *desc, bool v3);
void cheatsAddCBACode(const char *code, const char *desc);
void cheatsDelete(int number, bool restore);
void cheatsDeleteAll(bool restore);
int cheatsCheckKeys(uint32_t keys, uint32_t extended);

#ifdef __cplusplus
}
#endif

#endif /* CHEATS_H */
