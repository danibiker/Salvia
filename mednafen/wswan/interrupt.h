#ifndef __WSWAN_INTERRUPT_H
#define __WSWAN_INTERRUPT_H

#include "../mednafen-types.h"
#include "../state.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
 WSINT_SERIAL_SEND = 0,
 WSINT_KEY_PRESS,
 WSINT_RTC_ALARM,
 WSINT_SERIAL_RECV,
 WSINT_LINE_HIT,
 WSINT_VBLANK_TIMER,
 WSINT_VBLANK,
 WSINT_HBLANK_TIMER
};

void WSwan_Interrupt(int);
void WSwan_InterruptAssert(int which, bool asserted);
void WSwan_InterruptWrite(uint32 A, uint8 V);
uint8 WSwan_InterruptRead(uint32 A);
void WSwan_InterruptCheck(void);
int WSwan_InterruptStateAction(StateMem *sm, int load, int data_only);
void WSwan_InterruptReset(void);

#ifdef __cplusplus
}
#endif

#endif
