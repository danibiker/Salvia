#ifndef __V30MZ_H_
#define __V30MZ_H_

#include <boolean.h>

enum
{
	NEC_PC=1,
   NEC_AW,
   NEC_CW,
   NEC_DW,
   NEC_BW,
   NEC_SP,
   NEC_BP,
   NEC_IX,
   NEC_IY,
	NEC_FLAGS,
   NEC_DS1,
   NEC_PS,
   NEC_SS,
   NEC_DS0
};

#ifdef __cplusplus
extern "C" {
#endif

/* Public variables */
extern int32 v30mz_ICount;

/* Public functions */
void v30mz_execute(int cycles);
unsigned v30mz_get_reg(int regnum);


extern uint32 v30mz_timestamp;

void v30mz_init(uint8 (*readmem20)(uint32), void (*writemem20)(uint32,uint8), uint8 (*readport)(uint32), void (*writeport)(uint32, uint8));
void v30mz_set_reg(int, unsigned);
void v30mz_reset(void);
void v30mz_int(uint32 vector, bool IgnoreIF);
int v30mz_StateAction(StateMem *sm, int load, int data_only);

#ifdef __cplusplus
}
#endif

#endif
