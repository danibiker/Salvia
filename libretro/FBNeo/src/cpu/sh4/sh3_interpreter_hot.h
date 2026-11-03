// Included after the unchanged interpreter instruction functions and execute_one.
// CV1000 samples put roughly half of interpreter fallback steps in 0x60xx.
#ifndef FBNEO_SH3_INTERPRETER_HOT_H
#define FBNEO_SH3_INTERPRETER_HOT_H
#if defined(_MSC_VER)
#define SH3_HOT_INLINE __forceinline
#elif defined(__GNUC__)
#define SH3_HOT_INLINE inline __attribute__((always_inline))
#else
#define SH3_HOT_INLINE inline
#endif
static SH3_HOT_INLINE void sh3_execute_hot_fallback(const UINT16 opcode)
{
#if defined(SH3_PPC_DRC) || defined(SH3_HOT_FALLBACK_TEST)
 // This is still the interpreter: reuse its exact instruction helpers, including
 // all mapped memory handlers, idle-cycle charging and postincrement semantics.
 // Only the high-frequency R0 destination family skips the general decoder.
 if ((opcode & 0xff00) == 0x6000) {
  // Exact-opcode samples identify MOV.L @Rm,R0 as the dominant 0x60 fallback.
  // Keep the original helper, including WaitState/ReadLong and idle accounting;
  // only bypass the remaining multiway decode for this common operation.
  if ((opcode & 0x0f) == 2) {
   MOVLL(opcode);
   return;
  }
  switch (opcode & 0x0f) {
   case 0x00: MOVBL(opcode); break;
   case 0x01: MOVWL(opcode); break;
   case 0x03: MOV(opcode); break;
   case 0x04: MOVBP(opcode); break;
   case 0x05: MOVWP(opcode); break;
   case 0x06: MOVLP(opcode); break;
   case 0x07: NOT(opcode); break;
   case 0x08: SWAPB(opcode); break;
   case 0x09: SWAPW(opcode); break;
   case 0x0a: NEGC(opcode); break;
   case 0x0b: NEG(opcode); break;
   case 0x0c: EXTUB(opcode); break;
   case 0x0d: EXTUW(opcode); break;
   case 0x0e: EXTSB(opcode); break;
   case 0x0f: EXTSW(opcode); break;
  }
  return;
 }
#endif
 execute_one(opcode);
}
#undef SH3_HOT_INLINE
#endif
