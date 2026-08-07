#ifndef FUSE_Z80_TYPES_H
#define FUSE_Z80_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Union allowing a register pair to be accessed as bytes or as a word */
typedef union {
#ifdef MSB_FIRST
  struct { uint8_t h,l; } b;
#else
  struct { uint8_t l,h; } b;
#endif
  uint16_t w;
} regpair;

/* What's stored in the main processor */
typedef struct processor {
  regpair af,bc,de,hl;
  regpair af_,bc_,de_,hl_;
  regpair ix,iy;
  uint8_t i;
  uint16_t r;	/* The low seven bits of the R register. 16 bits long
			   so it can also act as an RZX instruction counter */
  uint8_t r7;	/* The high bit of the R register */
  regpair sp,pc;
  uint8_t iff1, iff2, im;
  int halted;

  /* Interrupts were enabled at this time; do not accept any interrupts
     until z80_tstates > this value */
  uint64_t interrupts_enabled_at;

} processor;

#ifdef __cplusplus
}
#endif

#endif
