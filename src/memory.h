#ifndef GBA_MEMORY_H
#define GBA_MEMORY_H

#define FLASH_128K_SZ 0x20000

#define EEPROM_IDLE           0
#define EEPROM_READADDRESS    1
#define EEPROM_READDATA       2
#define EEPROM_READDATA2      3
#define EEPROM_WRITEDATA      4

#ifdef __cplusplus
extern "C" {
#endif

enum
{
   IMAGE_UNKNOWN,
   IMAGE_GBA
};

/* save game */
typedef struct
{
   void *address;
   int size;
} variable_desc;

extern int flashSize;

extern bool eepromInUse;
extern int eepromSize;

extern uint8_t *flashSaveMemory;
extern uint8_t *eepromData;

extern void eepromReadGameMem(const uint8_t **data, int version);
extern void eepromSaveGameMem(uint8_t **data);

extern int eepromRead(void);
extern void eepromWrite(uint8_t value);
extern void eepromInit(void);
extern void eepromReset(void);

extern void sramWrite(uint32_t address, uint8_t byte);
extern void sramDelayedWrite(uint32_t address, uint8_t byte);
extern void flashSaveGameMem(uint8_t **data);
extern void flashReadGameMem(const uint8_t **data, int version);

extern uint8_t flashRead(uint32_t address);
extern void flashWrite(uint32_t address, uint8_t byte);
extern void flashDelayedWrite(uint32_t address, uint8_t byte);
extern void flashSaveDecide(uint32_t address, uint8_t byte);
extern void flashReset(void);
extern void flashSetSize(int size);
extern void flashInit(void);

extern uint16_t rtcRead(uint32_t address);
extern bool rtcWrite(uint32_t address, uint16_t value);
extern void rtcEnable(bool);
extern void rtcReset (void);
extern void rtcReadGameMem(const uint8_t **data);
extern void rtcSaveGameMem(uint8_t **data);

extern uint16_t gyroRead(uint32_t address);
extern bool gyroWrite(uint32_t address, uint16_t value);

void utilWriteIntMem(uint8_t **data, int);
void utilWriteMem(uint8_t **data, const void *in_data, unsigned size);
void utilWriteDataMem(uint8_t **data, variable_desc *);

int utilReadIntMem(const uint8_t **data);
void utilReadMem(void *buf, const uint8_t **data, unsigned size);
void utilReadDataMem(const uint8_t **data, variable_desc *);

#ifdef __cplusplus
}
#endif

#endif /* GBA_MEMORY_H */
