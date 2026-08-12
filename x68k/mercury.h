#ifndef _WINX68K_MERCURY_H
#define _WINX68K_MERCURY_H

#include <stdint.h>

extern uint8_t Mcry_LRTiming;

void FASTCALL Mcry_Update(int16_t *buffer, size_t length);
void FASTCALL Mcry_PreUpdate(uint32_t clock);

void FASTCALL Mcry_Write(uint32_t adr, uint8_t data);
uint8_t FASTCALL Mcry_Read(uint32_t adr);

void Mcry_SetClock(void);
void Mcry_SetVolume(uint8_t vol);

void Mcry_Init(const char* path);
void Mcry_Cleanup(void);
int Mcry_IsReady(void);

void FASTCALL Mcry_Int(void);

#endif /* _WINX68K_MERCURY_H */
