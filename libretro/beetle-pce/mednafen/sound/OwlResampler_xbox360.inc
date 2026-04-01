//
// Xbox 360 VMX (Vector Multimedia eXtensions) implementation of DoMAC
// Uses native Xbox 360 ppcintrinsics.h intrinsics instead of AltiVec syntax.
// Functionally equivalent to OwlResampler_altivec.inc but with MSVC-compatible intrinsics.
//
static INLINE void DoMAC_AltiVec(float* wave, float* coeffs, int32 count, int32* accum_output)
{
 __vector4 acc0, acc1, acc2, acc3;
 __vector4 zero = __vspltisw(0);

 acc0 = zero;
 acc1 = zero;
 acc2 = zero;
 acc3 = zero;

 count >>= 4;

 if(!((uintptr_t)wave & 0xF))
 {
  // Aligned path - wave is 16-byte aligned
  __vector4 w, c;
  do
  {
   w = __lvx(wave, 0);
   c = __lvx(coeffs, 0);
   acc0 = __vmaddfp(w, c, acc0);

   w = __lvx(wave, 16);
   c = __lvx(coeffs, 16);
   acc1 = __vmaddfp(w, c, acc1);

   w = __lvx(wave, 32);
   c = __lvx(coeffs, 32);
   acc2 = __vmaddfp(w, c, acc2);

   w = __lvx(wave, 48);
   c = __lvx(coeffs, 48);
   acc3 = __vmaddfp(w, c, acc3);

   coeffs += 16;
   wave += 16;
  } while(--count);
 }
 else
 {
  // Unaligned path - use permute to handle misaligned wave data
  __vector4 lperm;
  __vector4 loado;

  lperm = __lvsl(wave, 0);
  loado = __lvx(wave, 0);

  do
  {
   __vector4 tl;
   __vector4 w;
   __vector4 c;

   tl = __lvx(wave, 15);
   w = __vperm(loado, tl, lperm);
   c = __lvx(coeffs, 0);
   loado = tl;
   acc0 = __vmaddfp(w, c, acc0);

   tl = __lvx(wave, 31);
   w = __vperm(loado, tl, lperm);
   c = __lvx(coeffs, 16);
   loado = tl;
   acc1 = __vmaddfp(w, c, acc1);

   tl = __lvx(wave, 47);
   w = __vperm(loado, tl, lperm);
   c = __lvx(coeffs, 32);
   loado = tl;
   acc2 = __vmaddfp(w, c, acc2);

   tl = __lvx(wave, 63);
   w = __vperm(loado, tl, lperm);
   c = __lvx(coeffs, 48);
   loado = tl;
   acc3 = __vmaddfp(w, c, acc3);

   coeffs += 16;
   wave += 16;
  } while(--count);
 }

 // Horizontal sum of all accumulators
 {
  __vector4 sum;
  __vector4 sums0;
  __vector4 sum_i;

  sum = __vaddfp(__vaddfp(acc0, acc1), __vaddfp(acc2, acc3));
  sums0 = __vsldoi(sum, sum, 8);
  sum = __vaddfp(sum, sums0);
  sums0 = __vsldoi(sum, sum, 4);
  sum = __vaddfp(sum, sums0);

  sum_i = __vctsxs(sum, 0);
  __stvewx(sum_i, accum_output, 0);
 }
}
