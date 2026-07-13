/*
   SWI HLE - matematica de punto fijo del Portfolio OS del 3DO.
   Port de opera-libretro (opera_fixedpoint_math.c / opera_swi_hle_0x5XXXX.h).

   HLE (High Level Emulation) de los SWI 0x50000..0x50012: en vez de vectorar a
   0x08 y dejar que el ARM interprete cientos de instrucciones del BIOS (bucles
   de matriz/vector), se ejecuta el equivalente nativo en C. Gran ahorro de CPU
   en juegos con mucha geometria 3D.

   BE-safe: toda la aritmetica es int64 (endian-independiente); los operandos se
   leen de la DRAM (pRam) que en 3dox se guarda host-native = big-endian en la
   Xbox360, exactamente como los ve el ARM del 3DO. Asume punteros alineados a 4
   (el ABI matematico del 3DO los pasa asi; el PPC de Xenon sirve accesos no
   alineados por hardware de todos modos).

   Todas las funciones son static: este header lo incluye SOLO arm.cpp, asi que
   no hace falta anadir ningun .c al proyecto (vcxproj intacto).
*/

#ifndef SWI_HLE_HEAD_DEFINITION
#define SWI_HLE_HEAD_DEFINITION

#include "types.h"

typedef int32  frac16;
typedef frac16 vec3f16[3];
typedef frac16 vec4f16[4];
typedef frac16 mat33f16[3][3];
typedef frac16 mat44f16[4][4];

static frac16 sqrt_frac16(frac16 x_)
{
   frac16 root, remHi, remLo, testDiv, count;

   root  = 0;
   remHi = 0;
   remLo = x_;
   count = 16;

   do
   {
      remHi   = ((remHi << 16) | (remLo >> 16));
      remLo   = (remLo << 16);
      testDiv = ((root << 1) + 1);
      if(remHi >= testDiv)
      {
         remHi -= testDiv;
         root++;
      }
   } while(count-- != 0);

   return root;
}

/* swi 0x50000 */
static void MulVec3Mat33_F16(vec3f16 dest_, vec3f16 vec_, mat33f16 mat_)
{
   vec3f16 tmp;
   tmp[0] = ((((int64)vec_[0]*(int64)mat_[0][0]) + ((int64)vec_[1]*(int64)mat_[1][0]) + ((int64)vec_[2]*(int64)mat_[2][0])) >> 16);
   tmp[1] = ((((int64)vec_[0]*(int64)mat_[0][1]) + ((int64)vec_[1]*(int64)mat_[1][1]) + ((int64)vec_[2]*(int64)mat_[2][1])) >> 16);
   tmp[2] = ((((int64)vec_[0]*(int64)mat_[0][2]) + ((int64)vec_[1]*(int64)mat_[1][2]) + ((int64)vec_[2]*(int64)mat_[2][2])) >> 16);
   dest_[0] = tmp[0];
   dest_[1] = tmp[1];
   dest_[2] = tmp[2];
}

/* swi 0x50001 */
static void MulMat33Mat33_F16(mat33f16 dest_, mat33f16 src1_, mat33f16 src2_)
{
   mat33f16 tmp;
   tmp[0][0] = ((((int64)src1_[0][0]*(int64)src2_[0][0]) + ((int64)src1_[0][1]*(int64)src2_[1][0]) + ((int64)src1_[0][2]*(int64)src2_[2][0])) >> 16);
   tmp[0][1] = ((((int64)src1_[0][0]*(int64)src2_[0][1]) + ((int64)src1_[0][1]*(int64)src2_[1][1]) + ((int64)src1_[0][2]*(int64)src2_[2][1])) >> 16);
   tmp[0][2] = ((((int64)src1_[0][0]*(int64)src2_[0][2]) + ((int64)src1_[0][1]*(int64)src2_[1][2]) + ((int64)src1_[0][2]*(int64)src2_[2][2])) >> 16);
   tmp[1][0] = ((((int64)src1_[1][0]*(int64)src2_[0][0]) + ((int64)src1_[1][1]*(int64)src2_[1][0]) + ((int64)src1_[1][2]*(int64)src2_[2][0])) >> 16);
   tmp[1][1] = ((((int64)src1_[1][0]*(int64)src2_[0][1]) + ((int64)src1_[1][1]*(int64)src2_[1][1]) + ((int64)src1_[1][2]*(int64)src2_[2][1])) >> 16);
   tmp[1][2] = ((((int64)src1_[1][0]*(int64)src2_[0][2]) + ((int64)src1_[1][1]*(int64)src2_[1][2]) + ((int64)src1_[1][2]*(int64)src2_[2][2])) >> 16);
   tmp[2][0] = ((((int64)src1_[2][0]*(int64)src2_[0][0]) + ((int64)src1_[2][1]*(int64)src2_[1][0]) + ((int64)src1_[2][2]*(int64)src2_[2][0])) >> 16);
   tmp[2][1] = ((((int64)src1_[2][0]*(int64)src2_[0][1]) + ((int64)src1_[2][1]*(int64)src2_[1][1]) + ((int64)src1_[2][2]*(int64)src2_[2][1])) >> 16);
   tmp[2][2] = ((((int64)src1_[2][0]*(int64)src2_[0][2]) + ((int64)src1_[2][1]*(int64)src2_[1][2]) + ((int64)src1_[2][2]*(int64)src2_[2][2])) >> 16);
   dest_[0][0]=tmp[0][0]; dest_[0][1]=tmp[0][1]; dest_[0][2]=tmp[0][2];
   dest_[1][0]=tmp[1][0]; dest_[1][1]=tmp[1][1]; dest_[1][2]=tmp[1][2];
   dest_[2][0]=tmp[2][0]; dest_[2][1]=tmp[2][1]; dest_[2][2]=tmp[2][2];
}

/* swi 0x50002 */
static void MulManyVec3Mat33_F16(vec3f16 *dest_, vec3f16 *src_, mat33f16 mat_, int32 count_)
{
   int32 i;
   vec3f16 tmp;
   for(i = 0; i < count_; i++)
   {
      tmp[0] = ((((int64)src_[i][0]*(int64)mat_[0][0]) + ((int64)src_[i][1]*(int64)mat_[1][0]) + ((int64)src_[i][2]*(int64)mat_[2][0])) >> 16);
      tmp[1] = ((((int64)src_[i][0]*(int64)mat_[0][1]) + ((int64)src_[i][1]*(int64)mat_[1][1]) + ((int64)src_[i][2]*(int64)mat_[2][1])) >> 16);
      tmp[2] = ((((int64)src_[i][0]*(int64)mat_[0][2]) + ((int64)src_[i][1]*(int64)mat_[1][2]) + ((int64)src_[i][2]*(int64)mat_[2][2])) >> 16);
      dest_[i][0] = tmp[0];
      dest_[i][1] = tmp[1];
      dest_[i][2] = tmp[2];
   }
}

/* swi 0x50005 */
static void MulManyF16(frac16 *dest_, frac16 *src1_, frac16 *src2_, int32 count_)
{
   int32 i;
   for(i = 0; i < count_; i++)
      dest_[i] = (((int64)src1_[i]*(int64)src2_[i]) >> 16);
}

/* swi 0x50006 */
static void MulScalerF16(frac16 *dest_, frac16 *src_, frac16 scaler_, int32 count_)
{
   int32 i;
   for(i = 0; i < count_; i++)
      dest_[i] = (((int64)src_[i]*(int64)scaler_) >> 16);
}

/* swi 0x50007 */
static void MulVec4Mat44_F16(vec4f16 dest_, vec4f16 vec_, mat44f16 mat_)
{
   vec4f16 tmp;
   tmp[0] = ((((int64)vec_[0]*(int64)mat_[0][0]) + ((int64)vec_[1]*(int64)mat_[1][0]) + ((int64)vec_[2]*(int64)mat_[2][0]) + ((int64)vec_[3]*(int64)mat_[3][0])) >> 16);
   tmp[1] = ((((int64)vec_[0]*(int64)mat_[0][1]) + ((int64)vec_[1]*(int64)mat_[1][1]) + ((int64)vec_[2]*(int64)mat_[2][1]) + ((int64)vec_[3]*(int64)mat_[3][1])) >> 16);
   tmp[2] = ((((int64)vec_[0]*(int64)mat_[0][2]) + ((int64)vec_[1]*(int64)mat_[1][2]) + ((int64)vec_[2]*(int64)mat_[2][2]) + ((int64)vec_[3]*(int64)mat_[3][2])) >> 16);
   tmp[3] = ((((int64)vec_[0]*(int64)mat_[0][3]) + ((int64)vec_[1]*(int64)mat_[1][3]) + ((int64)vec_[2]*(int64)mat_[2][3]) + ((int64)vec_[3]*(int64)mat_[3][3])) >> 16);
   dest_[0]=tmp[0]; dest_[1]=tmp[1]; dest_[2]=tmp[2]; dest_[3]=tmp[3];
}

/* swi 0x50008 */
static void MulMat44Mat44_F16(mat44f16 dest_, mat44f16 src1_, mat44f16 src2_)
{
   mat44f16 tmp;
   int32 r, c;
   for(r = 0; r < 4; r++)
      for(c = 0; c < 4; c++)
         tmp[r][c] = ((((int64)src1_[r][0]*(int64)src2_[0][c]) +
                       ((int64)src1_[r][1]*(int64)src2_[1][c]) +
                       ((int64)src1_[r][2]*(int64)src2_[2][c]) +
                       ((int64)src1_[r][3]*(int64)src2_[3][c])) >> 16);
   for(r = 0; r < 4; r++)
      for(c = 0; c < 4; c++)
         dest_[r][c] = tmp[r][c];
}

/* swi 0x50009 */
static void MulManyVec4Mat44_F16(vec4f16 *dest_, vec4f16 *src_, mat44f16 mat_, int32 count_)
{
   int32 i;
   vec4f16 tmp;
   for(i = 0; i < count_; i++)
   {
      tmp[0] = ((((int64)src_[i][0]*(int64)mat_[0][0]) + ((int64)src_[i][1]*(int64)mat_[1][0]) + ((int64)src_[i][2]*(int64)mat_[2][0]) + ((int64)src_[i][3]*(int64)mat_[3][0])) >> 16);
      tmp[1] = ((((int64)src_[i][0]*(int64)mat_[0][1]) + ((int64)src_[i][1]*(int64)mat_[1][1]) + ((int64)src_[i][2]*(int64)mat_[2][1]) + ((int64)src_[i][3]*(int64)mat_[3][1])) >> 16);
      tmp[2] = ((((int64)src_[i][0]*(int64)mat_[0][2]) + ((int64)src_[i][1]*(int64)mat_[1][2]) + ((int64)src_[i][2]*(int64)mat_[2][2]) + ((int64)src_[i][3]*(int64)mat_[3][2])) >> 16);
      tmp[3] = ((((int64)src_[i][0]*(int64)mat_[0][3]) + ((int64)src_[i][1]*(int64)mat_[1][3]) + ((int64)src_[i][2]*(int64)mat_[2][3]) + ((int64)src_[i][3]*(int64)mat_[3][3])) >> 16);
      dest_[i][0]=tmp[0]; dest_[i][1]=tmp[1]; dest_[i][2]=tmp[2]; dest_[i][3]=tmp[3];
   }
}

/* swi 0x5000C */
static frac16 Dot3_F16(vec3f16 v1_, vec3f16 v2_)
{
   return ((((int64)v1_[0]*(int64)v2_[0]) + ((int64)v1_[1]*(int64)v2_[1]) + ((int64)v1_[2]*(int64)v2_[2])) >> 16);
}

/* swi 0x5000D */
static frac16 Dot4_F16(vec4f16 v1_, vec4f16 v2_)
{
   return ((((int64)v1_[0]*(int64)v2_[0]) + ((int64)v1_[1]*(int64)v2_[1]) + ((int64)v1_[2]*(int64)v2_[2]) + ((int64)v1_[3]*(int64)v2_[3])) >> 16);
}

/* swi 0x5000E */
static void Cross3_F16(vec3f16 dest_, vec3f16 v1_, vec3f16 v2_)
{
   vec3f16 tmp;
   tmp[0] = ((((int64)v1_[1]*(int64)v2_[2]) - ((int64)v1_[2]*(int64)v2_[1])) >> 16);
   tmp[1] = ((((int64)v1_[2]*(int64)v2_[0]) - ((int64)v1_[0]*(int64)v2_[2])) >> 16);
   tmp[2] = ((((int64)v1_[0]*(int64)v2_[1]) - ((int64)v1_[1]*(int64)v2_[0])) >> 16);
   dest_[0]=tmp[0]; dest_[1]=tmp[1]; dest_[2]=tmp[2];
}

/* swi 0x5000F */
static frac16 AbsVec3_F16(vec3f16 vec_)
{
   frac16 rv = ((((int64)vec_[0]*(int64)vec_[0]) + ((int64)vec_[1]*(int64)vec_[1]) + ((int64)vec_[2]*(int64)vec_[2])) >> 16);
   return sqrt_frac16(rv);
}

/* swi 0x50010 */
static frac16 AbsVec4_F16(vec4f16 vec_)
{
   frac16 rv = ((((int64)vec_[0]*(int64)vec_[0]) + ((int64)vec_[1]*(int64)vec_[1]) + ((int64)vec_[2]*(int64)vec_[2]) + ((int64)vec_[3]*(int64)vec_[3])) >> 16);
   return sqrt_frac16(rv);
}

/* swi 0x50011 */
static void MulVec3Mat33DivZ_F16(vec3f16 dest_, vec3f16 vec_, mat33f16 mat_, frac16 n_)
{
   MulVec3Mat33_F16(dest_, vec_, mat_);
   if(dest_[2] != 0)
   {
      int64 mul = (((int64)n_ << 16) / (int64)dest_[2]);
      dest_[0] = (((int64)dest_[0]*mul) >> 16);
      dest_[1] = (((int64)dest_[1]*mul) >> 16);
   }
}

/* swi 0x50012 */
static void MulManyVec3Mat33DivZ_F16(vec3f16 *dest_, vec3f16 *src_, mat33f16 *mat_, frac16 n_, uint32 count_)
{
   uint32 i;
   for(i = 0; i < count_; i++)
      MulVec3Mat33DivZ_F16(dest_[i], src_[i], *mat_, n_);
}

#endif /* SWI_HLE_HEAD_DEFINITION */
