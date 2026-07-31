#ifndef __WSWAN_COMM_H
#define __WSWAN_COMM_H

#include "../mednafen-types.h"
#include "../state.h"

#ifdef __cplusplus
extern "C" {
#endif

void Comm_Reset(void);
void Comm_Process(void);
uint8 Comm_Read(uint8 A);
void Comm_Write(uint8 A, uint8 V);
int Comm_StateAction(StateMem *sm, int load, int data_only);

#ifdef __cplusplus
}
#endif

#endif
