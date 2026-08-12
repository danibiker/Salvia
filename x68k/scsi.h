#ifndef _WINX68K_SCSI_H
#define _WINX68K_SCSI_H

#include <stdint.h>
#include "common.h"

extern	uint8_t	SCSIIPL[0x2000];

void SCSI_Init(void);
void SCSI_Cleanup(void);

uint8_t FASTCALL SCSI_Read(uint32_t adr);
void FASTCALL SCSI_Write(uint32_t adr, uint8_t data);

#endif /* _WINX68K_SCSI_H */
