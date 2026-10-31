/***************************************************************************
 *   PGXP para pcsxr-360: modo memoria de SwanStation (src/core/pgxp.cpp). *
 *   Copyright (C) 2016 by iCatButler.  GPL v2 o posterior.                *
 ***************************************************************************/

/* Diferencias con upstream, a proposito:
 *   - C89 (el core se compila como C con el compilador del XDK).
 *   - Las banderas por componente se leen y escriben con desplazamientos y
 *     no con compFlags[] / psx_value: upstream da por hecho little-endian
 *     (compFlags[0] = bits 0..7 de flags), y la Xbox 360 es big-endian.
 *   - Sin modo CPU.  Cache de vertices solo en "lite" y mas pequena (ver
 *     "Cache de vertices" abajo).
 *   - RAM de 2 MB (upstream reserva 8 MB por la opcion de RAM ampliada).
 *   - LWL/LWR/SWL/SWR invalidan en vez de pasar por CPU_LW/CPU_SW con un
 *     valor mezclado (que acaba igual de invalido). */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <xtl.h>        /* __lwsync (en PC, la de 360/win32) */
#include "psxcommon.h"
#include "psxmem.h"
#include "pgxp.h"

int g_pgxp_enabled = 0;
int g_pgxp_vertex_cache = 0;
int g_pgxp_memory = 0;
int g_pgxp_culling = 1;
int g_pgxp_texture_correction = 1;

#define PGXP_RAM_WORDS     (0x200000 / 4)
#define PGXP_SCRATCH_WORDS (0x400 / 4)
#define PGXP_MEM_WORDS     (PGXP_RAM_WORDS + PGXP_SCRATCH_WORDS)

#define VALID     1u
#define VALID_0   (VALID << 0)
#define VALID_1   (VALID << 8)
#define VALID_2   (VALID << 16)
#define VALID_3   (VALID << 24)
#define VALID_01  (VALID_0 | VALID_1)
#define VALID_ALL (VALID_0 | VALID_1 | VALID_2 | VALID_3)
#define INV_VALID_ALL (0xFFFFFFFFu ^ VALID_ALL)

typedef struct
{
  float x;
  float y;
  float z;
  u32 flags;   /* byte k = bandera del componente k (x, y, z, w) */
  u32 value;   /* la palabra de 32 bits a la que corresponde */
} PGXP_value;

/* Bandera del componente k (0..3): byte k de flags, por desplazamiento. */
#define GETCF(v, k)      (((v)->flags >> (8 * (k))) & 0xFFu)
#define SETCF(v, k, b)   ((v)->flags = ((v)->flags & ~(0xFFu << (8 * (k)))) | (((u32)(b) & 0xFFu) << (8 * (k))))

static const PGXP_value PGXP_value_invalid = { 0.f, 0.f, 0.f, 0, 0 };

static PGXP_value CPU_reg[34];
static PGXP_value GTE_data_reg[32];
static PGXP_value GTE_ctrl_reg[32];
static PGXP_value* s_mem = NULL;

/* ---------------------------------------------------------------------- *
 * Filtro del modo completo (2026-10-31)
 *
 * El modo completo en la Xbox costaba el doble de CPU (Tomb Raider II: 16 ms
 * de emulacion por frame contra 8,3 en lite): cada LW/LH/SW/SH llamaba a C y
 * tocaba la sombra de 10 MB, que no cabe en la cache.  Pero casi ninguna
 * carga o escritura lleva una coordenada.  Dos mapas de bits que el dynarec
 * mira EN LINEA antes de llamar:
 *
 *   PGXP_mem_bits  1 bit por palabra de RAM (64 KB, cabe en la cache): 0 =
 *                  la sombra de esa palabra seguro que no vale.  El
 *                  scratchpad comparte bit con las 256 primeras palabras de
 *                  RAM (mismo indice (addr >> 2) & 7FFFFh): el bit vale 1 si
 *                  cualquiera de las dos puede valer.
 *   PGXP_reg_bits  1 bit por registro de la CPU: 0 = su sombra no vale.
 *
 *   LW/LH/LHU  bit de la palabra a 0 -> no se llama a C; el dynarec pone a 0
 *              el bit del registro destino.
 *   SW/SH      bit del registro a 0 -> no se llama a C; el dynarec pone a 0
 *              el bit de la palabra.
 *
 * Un bit a 1 solo significa "puede valer": la sombra se sigue validando por
 * valor en C, y si no vale el bit se baja ahi.  Un bit a 0 manda: lo que
 * diga la sombra de esa palabra o registro no se usa (MemGet / RegSrc lo
 * anulan al leerlo, porque el dynarec baja el bit sin tocar la sombra). */
unsigned int PGXP_mem_bits[PGXP_RAM_WORDS / 32];
unsigned int PGXP_reg_bits = 0;

#define MEMBIT_IDX(a) (((a) >> 2) & (PGXP_RAM_WORDS - 1u))
#define MEMBIT_TEST(a) ((PGXP_mem_bits[MEMBIT_IDX(a) >> 5] >> (MEMBIT_IDX(a) & 31u)) & 1u)

/* ---------------------------------------------------------------------- *
 * Cache de vertices (solo "lite")
 *
 * En lite la sombra solo se mueve por las transferencias de la GTE.  Si el
 * juego copia una coordenada con LW/SW (p. ej. un vertice compartido entre
 * dos poligonos: el primero lo recibe de la GTE con SWC2 y el segundo una
 * copia), un poligono sale con decimales y el vecino con enteras: el borde
 * comun no coincide y se ve una linea negra entre los dos.
 *
 * Arreglo, el "vertex cache" de upstream en pequeno: cada vertice que calcula
 * la GTE (RTPS/RTPT) se apunta aqui indexado por su posicion entera en
 * pantalla; si la busqueda por direccion falla, la GPU busca por la palabra
 * de posicion (que es el mismo SXY de la GTE).  Coste en el hilo de la CPU:
 * una escritura de 16 bytes por vertice transformado (no por cada carga o
 * escritura como el modo completo); la busqueda va en el hilo de GPU.
 *
 * Upstream usa una tabla de 4096x4096 (cientos de MB); aqui 512x512 entradas
 * (4 MB) con la palabra SXY completa como etiqueta: dos vertices en el mismo
 * pixel modulo 512 se pisan y el perdedor sale con enteras, como antes.
 *
 * SOLO x/y, nunca la w.  La primera version devolvia tambien la w y en Tomb
 * Raider II las paredes se deformaban en abanico segun el angulo de camara:
 * la posicion entera no identifica al vertice, y otro punto que la GTE
 * proyecto en ese mismo pixel (otro objeto, un punto de recorte o de
 * visibilidad, o el frame siguiente: el hilo de GPU va por detras) trae una
 * profundidad que no tiene nada que ver.  Con la x/y el error es de menos de
 * un pixel; con la w deforma la textura entera.  Sin w, el poligono que use
 * un vertice de la cache sale con la textura afin (w = 1 en los tres o
 * cuatro), igual que en lite sin cache; los bordes si encajan.
 *
 * Hilos: escribe la CPU emulada, lee el hilo de GPU (va hasta un frame por
 * detras).  La etiqueta se invalida antes de tocar x/y/z y se publica
 * despues, con barreras; quien lee la comprueba antes y despues. */
typedef struct
{
  float x;
  float y;
  float z;
  u32 sxy;
} PGXP_vc;

#define VC_BITS 9
#define VC_MASK ((1u << VC_BITS) - 1u)
#define VC_SIZE (1u << (2 * VC_BITS))
#define VC_IDX(v) (((v) & VC_MASK) | ((((v) >> 16) & VC_MASK) << VC_BITS))
/* Etiqueta "en obras": cambia bits que la GTE nunca produce (satura a 11 bits
 * con signo, la mitad alta de cada coordenada es extension de signo). */
#define VC_BUSY(v) ((v) ^ 0x40004000u)

static PGXP_vc* s_vcache = NULL;

#define SXY0 (GTE_data_reg[12])
#define SXY1 (GTE_data_reg[13])
#define SXY2 (GTE_data_reg[14])
#define SXYP (GTE_data_reg[15])

/* Decodificacion de la instruccion */
#define PG_RD(i) (((i) >> 11) & 0x1F)
#define PG_RT(i) (((i) >> 16) & 0x1F)

/* ---------------------------------------------------------------------- */

static __inline void Validate(PGXP_value* pV, u32 psxV)
{
  pV->flags &= (pV->value == psxV) ? 0xFFFFFFFFu : INV_VALID_ALL;
}

static __inline void MaskValidate(PGXP_value* pV, u32 psxV, u32 mask, u32 validMask)
{
  pV->flags &= ((pV->value & mask) == (psxV & mask)) ? 0xFFFFFFFFu : (0xFFFFFFFFu ^ validMask);
}

static __inline PGXP_value* GetPtr(u32 addr)
{
  u32 paddr;
  if (!s_mem)
    return NULL;
  /* Scratchpad (1F800000h, tambien por KSEG0) */
  if ((addr & 0x7FFFFC00u) == 0x1F800000u)
    return &s_mem[PGXP_RAM_WORDS + ((addr & 0x3FFu) >> 2)];
  paddr = addr & 0x1FFFFFFFu;
  if (paddr < 0x800000u)   /* RAM y sus espejos */
    return &s_mem[(paddr & 0x1FFFFFu) >> 2];
  return NULL;
}

/* Recalcula el bit de la palabra (RAM y su alias de scratchpad). */
static void MemBitUpdate(u32 addr)
{
  const u32 idx = MEMBIT_IDX(addr);
  int on = 0;
  if (s_mem)
  {
    on = (s_mem[idx].flags & VALID_ALL) != 0;
    if (idx < PGXP_SCRATCH_WORDS)
      on |= (s_mem[PGXP_RAM_WORDS + idx].flags & VALID_ALL) != 0;
  }
  if (on)
    PGXP_mem_bits[idx >> 5] |= (1u << (idx & 31u));
  else
    PGXP_mem_bits[idx >> 5] &= ~(1u << (idx & 31u));
}

/* Sombra de la palabra para leerla o modificarla: con el bit a 0 no vale,
 * aunque sus banderas digan otra cosa (el dynarec baja el bit sin tocarla). */
static __inline PGXP_value* MemGet(u32 addr)
{
  PGXP_value* p = GetPtr(addr);
  if (p && !MEMBIT_TEST(addr))
    *p = PGXP_value_invalid;
  return p;
}

/* Registro r de la CPU: bit = sombra posiblemente valida.  El 0 nunca. */
static __inline void RegBitUpdate(u32 r)
{
  r &= 31u;
  if (r != 0 && (CPU_reg[r].flags & VALID_ALL) != 0)
    PGXP_reg_bits |= (1u << r);
  else
    PGXP_reg_bits &= ~(1u << r);
}

static __inline PGXP_value* RegSrc(u32 r)
{
  r &= 31u;
  if (!((PGXP_reg_bits >> r) & 1u))
    CPU_reg[r] = PGXP_value_invalid;
  return &CPU_reg[r];
}

static __inline void ValidateAndCopyMem(PGXP_value* dest, u32 addr, u32 value)
{
  PGXP_value* pMem = MemGet(addr);
  if (pMem)
  {
    Validate(pMem, value);
    *dest = *pMem;
    MemBitUpdate(addr);
    return;
  }
  *dest = PGXP_value_invalid;
}

static void ValidateAndCopyMem16(PGXP_value* dest, u32 addr, u32 value, int sign)
{
  PGXP_value* pMem = MemGet(addr);
  if (pMem)
  {
    const int high = ((addr & 3u) == 2u);
    const u32 val = high ? ((value & 0xFFFFu) << 16) : (value & 0xFFFFu);
    const u32 mask = high ? 0xFFFF0000u : 0x0000FFFFu;

    /* validar y copiar la palabra entera */
    MaskValidate(pMem, val, mask, high ? VALID_1 : VALID_0);
    *dest = *pMem;
    MemBitUpdate(addr);

    /* media palabra alta: bajarla */
    if (high)
    {
      dest->x = dest->y;
      SETCF(dest, 0, GETCF(dest, 1));
    }

    /* truncar */
    dest->y = (dest->x < 0) ? -1.f * (float)sign : 0.f;
    dest->value = value;
    SETCF(dest, 1, VALID);   /* la mitad alta es valida: 0 */
    return;
  }
  *dest = PGXP_value_invalid;
}

static __inline void WriteMem(const PGXP_value* value, u32 addr)
{
  PGXP_value* pMem = GetPtr(addr);
  if (pMem)
  {
    *pMem = *value;
    MemBitUpdate(addr);
  }
}

static void WriteMem16(const PGXP_value* src, u32 addr)
{
  PGXP_value* dest = MemGet(addr);
  if (dest)
  {
    const u32 half = src->value & 0xFFFFu;
    if ((addr & 3u) == 2u)
    {
      dest->y = src->x;
      SETCF(dest, 1, GETCF(src, 0));
      dest->value = (dest->value & 0x0000FFFFu) | (half << 16);
    }
    else
    {
      dest->x = src->x;
      SETCF(dest, 0, GETCF(src, 0));
      dest->value = (dest->value & 0xFFFF0000u) | half;
    }

    /* z/w solo si es valida */
    if (GETCF(src, 2) == VALID)
    {
      dest->z = src->z;
      SETCF(dest, 2, GETCF(src, 2));
    }
    MemBitUpdate(addr);
  }
}

/* ---------------------------------------------------------------------- */

int PGXP_Init(void)
{
  memset(PGXP_mem_bits, 0, sizeof(PGXP_mem_bits));
  PGXP_reg_bits = 0;
  memset(CPU_reg, 0, sizeof(CPU_reg));
  memset(GTE_data_reg, 0, sizeof(GTE_data_reg));
  memset(GTE_ctrl_reg, 0, sizeof(GTE_ctrl_reg));
  g_pgxp_vertex_cache = 0;
  if (!g_pgxp_enabled)
    return 0;
  /* Cache de vertices: solo en lite (el modo completo ya sigue las copias). */
  if (!g_pgxp_memory)
  {
    if (!s_vcache)
      s_vcache = (PGXP_vc*)malloc(sizeof(PGXP_vc) * VC_SIZE);
    if (s_vcache)
    {
      /* etiqueta imposible = vacia */
      u32 i;
      for (i = 0; i < VC_SIZE; i++)
        s_vcache[i].sxy = 0x40004000u;
      g_pgxp_vertex_cache = 1;
    }
  }
  if (!s_mem)
  {
    s_mem = (PGXP_value*)calloc(PGXP_MEM_WORDS, sizeof(PGXP_value));
    if (!s_mem)
    {
      g_pgxp_enabled = 0;
      return -1;
    }
  }
  else
  {
    memset(s_mem, 0, sizeof(PGXP_value) * PGXP_MEM_WORDS);
  }
  return 0;
}

void PGXP_Reset(void)
{
  memset(PGXP_mem_bits, 0, sizeof(PGXP_mem_bits));
  PGXP_reg_bits = 0;
  memset(CPU_reg, 0, sizeof(CPU_reg));
  memset(GTE_data_reg, 0, sizeof(GTE_data_reg));
  memset(GTE_ctrl_reg, 0, sizeof(GTE_ctrl_reg));
  if (s_mem)
    memset(s_mem, 0, sizeof(PGXP_value) * PGXP_MEM_WORDS);
  if (s_vcache)
  {
    u32 i;
    for (i = 0; i < VC_SIZE; i++)
      s_vcache[i].sxy = 0x40004000u;
  }
}

void PGXP_Shutdown(void)
{
  if (s_mem)
  {
    free(s_mem);
    s_mem = NULL;
  }
  if (s_vcache)
  {
    free(s_vcache);
    s_vcache = NULL;
  }
  g_pgxp_vertex_cache = 0;
  memset(PGXP_mem_bits, 0, sizeof(PGXP_mem_bits));
  PGXP_reg_bits = 0;
  memset(CPU_reg, 0, sizeof(CPU_reg));
  memset(GTE_data_reg, 0, sizeof(GTE_data_reg));
  memset(GTE_ctrl_reg, 0, sizeof(GTE_ctrl_reg));
}

/* ---------------------------------------------------------------------- *
 * GTE
 * ---------------------------------------------------------------------- */

void PGXP_GTE_PushSXYZ2f(float x, float y, float z, unsigned int v)
{
  /* bajar la FIFO */
  SXY0 = SXY1;
  SXY1 = SXY2;

  SXY2.x = x;
  SXY2.y = y;
  SXY2.z = z;
  SXY2.value = v;
  SXY2.flags = VALID_ALL;

  /* Cache de vertices (lite): ver arriba. */
  if (s_vcache)
  {
    PGXP_vc* e = &s_vcache[VC_IDX(v)];
    e->sxy = VC_BUSY(v);
    __lwsync();
    e->x = x;
    e->y = y;
    e->z = z;
    __lwsync();
    e->sxy = v;
  }
}

int PGXP_GTE_NCLIP_valid(unsigned int sxy0, unsigned int sxy1, unsigned int sxy2)
{
  Validate(&SXY0, sxy0);
  Validate(&SXY1, sxy1);
  Validate(&SXY2, sxy2);
  return ((SXY0.flags & SXY1.flags & SXY2.flags & VALID_01) == VALID_01) ? 1 : 0;
}

float PGXP_GTE_NCLIP(void)
{
  float nclip = (SXY0.x * SXY1.y) + (SXY1.x * SXY2.y) + (SXY2.x * SXY0.y) - (SXY0.x * SXY2.y) - (SXY1.x * SXY0.y) -
                (SXY2.x * SXY1.y);

  /* que una fraccion no se redondee a 0 (cara perpendicular descartada) */
  const float nclipAbs = (float)fabs(nclip);
  if ((0.1f < nclipAbs) && (nclipAbs < 1.f))
    nclip += (nclip < 0.f) ? -1.f : 1.f;
  return nclip;
}

static void MTC2_int(const PGXP_value* value, u32 reg)
{
  switch (reg)
  {
    case 15:
      /* empujar la FIFO */
      SXY0 = SXY1;
      SXY1 = SXY2;
      SXY2 = *value;
      SXYP = SXY2;
      break;

    case 31:
      return;
  }
  GTE_data_reg[reg] = *value;
}

void PGXP_CPU_MFC2(unsigned int instr, unsigned int rtVal, unsigned int rdVal)
{
  /* CPU[Rt] = GTE_D[Rd].  El 15 (SXYP) lee SXY2. */
  const u32 rd = PG_RD(instr);
  PGXP_value* src = (rd == 15) ? &SXY2 : &GTE_data_reg[rd];
  Validate(src, rdVal);
  CPU_reg[PG_RT(instr)] = *src;
  CPU_reg[PG_RT(instr)].value = rtVal;
  RegBitUpdate(PG_RT(instr));
}

void PGXP_CPU_MTC2(unsigned int instr, unsigned int rdVal, unsigned int rtVal)
{
  /* GTE_D[Rd] = CPU[Rt] */
  const u32 rd = PG_RD(instr);
  PGXP_value* src = RegSrc(PG_RT(instr));
  Validate(src, rtVal);
  RegBitUpdate(PG_RT(instr));
  MTC2_int(src, rd);
  GTE_data_reg[rd].value = rdVal;
}

void PGXP_CPU_CFC2(unsigned int instr, unsigned int rtVal, unsigned int rdVal)
{
  /* CPU[Rt] = GTE_C[Rd] */
  Validate(&GTE_ctrl_reg[PG_RD(instr)], rdVal);
  CPU_reg[PG_RT(instr)] = GTE_ctrl_reg[PG_RD(instr)];
  CPU_reg[PG_RT(instr)].value = rtVal;
  RegBitUpdate(PG_RT(instr));
}

void PGXP_CPU_CTC2(unsigned int instr, unsigned int rdVal, unsigned int rtVal)
{
  /* GTE_C[Rd] = CPU[Rt] */
  Validate(RegSrc(PG_RT(instr)), rtVal);
  RegBitUpdate(PG_RT(instr));
  GTE_ctrl_reg[PG_RD(instr)] = CPU_reg[PG_RT(instr)];
  GTE_ctrl_reg[PG_RD(instr)].value = rdVal;
}

void PGXP_CPU_LWC2(unsigned int instr, unsigned int rtVal, unsigned int addr)
{
  /* GTE_D[Rt] = Mem[addr] */
  PGXP_value val;
  ValidateAndCopyMem(&val, addr, rtVal);
  MTC2_int(&val, PG_RT(instr));
}

void PGXP_CPU_SWC2(unsigned int instr, unsigned int rtVal, unsigned int addr)
{
  /* Mem[addr] = GTE_D[Rt].  El 15 lee SXY2. */
  const u32 rt = PG_RT(instr);
  PGXP_value* src = (rt == 15) ? &SXY2 : &GTE_data_reg[rt];
  Validate(src, rtVal);
  WriteMem(src, addr);
}

/* ---------------------------------------------------------------------- *
 * CPU <-> memoria
 * ---------------------------------------------------------------------- */

void PGXP_CPU_LW(unsigned int instr, unsigned int rtVal, unsigned int addr)
{
  ValidateAndCopyMem(&CPU_reg[PG_RT(instr)], addr, rtVal);
  RegBitUpdate(PG_RT(instr));
}

void PGXP_CPU_LBx(unsigned int instr, unsigned int rtVal, unsigned int addr)
{
  (void)rtVal; (void)addr;
  CPU_reg[PG_RT(instr)] = PGXP_value_invalid;
  RegBitUpdate(PG_RT(instr));
}

void PGXP_CPU_LHx(unsigned int instr, unsigned int rtVal, unsigned int addr)
{
  ValidateAndCopyMem16(&CPU_reg[PG_RT(instr)], addr, rtVal, 1);
  RegBitUpdate(PG_RT(instr));
}

void PGXP_CPU_SB(unsigned int instr, unsigned int rtVal, unsigned int addr)
{
  (void)instr; (void)rtVal;
  WriteMem(&PGXP_value_invalid, addr);
}

void PGXP_CPU_SH(unsigned int instr, unsigned int rtVal, unsigned int addr)
{
  PGXP_value* val = RegSrc(PG_RT(instr));
  MaskValidate(val, rtVal & 0xFFFFu, 0xFFFFu, VALID_0);
  RegBitUpdate(PG_RT(instr));
  WriteMem16(val, addr);
}

void PGXP_CPU_SW(unsigned int instr, unsigned int rtVal, unsigned int addr)
{
  PGXP_value* val = RegSrc(PG_RT(instr));
  Validate(val, rtVal);
  RegBitUpdate(PG_RT(instr));
  WriteMem(val, addr);
}

void PGXP_CPU_MOVE(unsigned int rd_and_rs, unsigned int rsVal)
{
  const u32 rs = rd_and_rs & 0x1Fu;
  const u32 rd = (rd_and_rs >> 8) & 0x1Fu;
  Validate(RegSrc(rs), rsVal);
  RegBitUpdate(rs);
  if (rd == 0)
    return;
  CPU_reg[rd] = CPU_reg[rs];
  RegBitUpdate(rd);
}

void PGXP_CPU_InvalidateReg(unsigned int reg)
{
  CPU_reg[reg & 0x1Fu] = PGXP_value_invalid;
  RegBitUpdate(reg);
}

void PGXP_CPU_InvalidateMem(unsigned int addr)
{
  WriteMem(&PGXP_value_invalid, addr & ~3u);
}

/* ---------------------------------------------------------------------- *
 * Ganchos del dynarec
 * ---------------------------------------------------------------------- */

unsigned int PGXP_rec_slot[2];

unsigned int PGXP_RecLW(unsigned int value, unsigned int instr)
{
  PGXP_CPU_LW(instr, value, PGXP_rec_slot[0]);
  return value;
}

unsigned int PGXP_RecLH(unsigned int value, unsigned int instr)
{
  PGXP_CPU_LHx(instr, (u32)(s32)(s16)(value & 0xFFFFu), PGXP_rec_slot[0]);
  return value;
}

unsigned int PGXP_RecLHU(unsigned int value, unsigned int instr)
{
  PGXP_CPU_LHx(instr, value & 0xFFFFu, PGXP_rec_slot[0]);
  return value;
}

void PGXP_RecSW(unsigned int instr)
{
  PGXP_CPU_SW(instr, PGXP_rec_slot[1], PGXP_rec_slot[0]);
}

void PGXP_RecSH(unsigned int instr)
{
  PGXP_CPU_SH(instr, PGXP_rec_slot[1] & 0xFFFFu, PGXP_rec_slot[0]);
}

void PGXP_RecMove(unsigned int rd_and_rs)
{
  const u32 rs = rd_and_rs & 0x1Fu;
  const u32 rd = (rd_and_rs >> 8) & 0x1Fu;
  if (rd == 0)
    return;
  if ((PGXP_reg_bits >> rs) & 1u)
  {
    CPU_reg[rd] = CPU_reg[rs];
    PGXP_reg_bits |= (1u << rd);
  }
  else
  {
    PGXP_reg_bits &= ~(1u << rd);
  }
}

/* ---------------------------------------------------------------------- *
 * GPU
 * ---------------------------------------------------------------------- */

/* libpcsxcore/gpu.c: con el hilo de GPU el plugin lee una copia de la lista
 * (el ring); esto devuelve la direccion en RAM de la palabra copiada. */
extern unsigned int gpuRingRamAddrOf(const void* p);

unsigned int PGXP_RamAddrOf(const void* p)
{
  const u8* base = (const u8*)psxM_2;
  const u8* q = (const u8*)p;
  if (q >= base && q < base + 0x200000)
    return (unsigned int)(q - base);
  return gpuRingRamAddrOf(p);
}

/* La GPU solo tiene 11 bits con signo por coordenada: la parte entera da la
 * vuelta igual, la fraccion se conserva. */
static float TruncateVertexPosition(float p)
{
  const s32 int_part = (s32)p;
  const float int_part_f = (float)int_part;
  const s32 wrapped = ((s32)(((u32)int_part) << 21)) >> 21;
  return (float)wrapped + (p - int_part_f);
}

int PGXP_GetPreciseVertex(unsigned int addr, unsigned int value, int x, int y, int xOffs, int yOffs,
                          float* out_x, float* out_y, float* out_w)
{
  /* Con el bit de la palabra a 0 la sombra no vale (ver el filtro). */
  const PGXP_value* vert = (addr != 0xFFFFFFFFu && MEMBIT_TEST(addr)) ? GetPtr(addr) : NULL;
  if (vert && ((vert->flags & VALID_01) == VALID_01) && (vert->value == value))
  {
    /* hay una x/y con decimales para esta palabra */
    *out_x = TruncateVertexPosition(vert->x) + (float)xOffs;
    *out_y = TruncateVertexPosition(vert->y) + (float)yOffs;
    *out_w = vert->z / 32768.0f;
    /* tolerancia de upstream: -1 (sin limite) por defecto */
    return ((vert->flags & VALID_2) == VALID_2) ? 1 : 0;
  }

  /* Lite: buscar por la posicion entera (cache de vertices).  La palabra de
   * posicion del paquete es el SXY que saco la GTE. */
  if (s_vcache)
  {
    const PGXP_vc* e = &s_vcache[VC_IDX(value)];
    if (e->sxy == value)
    {
      float cx, cy, cz;
      __lwsync();
      cx = e->x;
      cy = e->y;
      cz = e->z;
      __lwsync();
      if (e->sxy == value)
      {
        /* x/y si, w no (ver "SOLO x/y" arriba): devolver 0 = w invalida, y el
         * driver pone w = 1 a todo el poligono. */
        (void)cz;
        *out_x = TruncateVertexPosition(cx) + (float)xOffs;
        *out_y = TruncateVertexPosition(cy) + (float)yOffs;
        *out_w = 1.0f;
        return 0;
      }
    }
  }

  /* nada valido: la posicion de la PSX */
  *out_x = (float)x;
  *out_y = (float)y;
  *out_w = 1.0f;
  return 0;
}
