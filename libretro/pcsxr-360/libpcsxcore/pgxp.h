/***************************************************************************
 *   PGXP (Parallel/Precision Geometry Transform Pipeline) para pcsxr-360.  *
 *                                                                         *
 *   Portado del modo "memoria" de SwanStation (src/core/pgxp.cpp), que a   *
 *   su vez viene del PGXP de iCatButler (Beetle PSX / PCSX-R).            *
 *   Copyright (C) 2016 by iCatButler.  GPL v2 o posterior.                *
 ***************************************************************************/

/* Que hace: la GTE calcula la proyeccion de cada vertice con decimales y la
 * guarda (x, y, w) en una "sombra" de sus registros SXY.  Las instrucciones
 * que mueven esos valores (MFC2/MTC2/SWC2/LWC2, y LW/SW/LH/SH entre memoria y
 * registros) mueven tambien la sombra, que vive en paralelo a la RAM.  Cuando
 * la GPU recibe un poligono por DMA, mira la sombra de la palabra de RAM de
 * cada vertice: si sigue valiendo lo mismo que la palabra (nadie la ha tocado
 * por otro camino), usa la posicion con decimales y la w (perspectiva).
 *
 * Modo memoria solamente (el "CPU mode" de upstream, que sigue tambien las
 * operaciones aritmeticas, no esta portado).  Solo con SwanStation HW. */

#ifndef __PCSXR_PGXP_H__
#define __PCSXR_PGXP_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Motor activo: opcion pcsxr360_pgxp + renderizador SwanStation HW.  Se fija
 * al cargar el juego (el dynarec decide al recompilar). */
extern int g_pgxp_enabled;
/* 1 = modo memoria completo (LW/SW/LH/SH y "move" de la CPU tambien mueven la
 * sombra, como upstream); 0 = "lite": solo la GTE y sus transferencias
 * (MFC2/MTC2/LWC2/SWC2).  Lite cubre lo habitual (las coordenadas van de la
 * GTE al paquete con SWC2) y quita casi todo el coste: en la Xbox cada carga
 * o escritura de la CPU tocaba la sombra de 10 MB, que no cabe en la cache.
 * Init-only como g_pgxp_enabled. */
extern int g_pgxp_memory;
/* 1 = cache de vertices activa (solo lite): si la palabra de un vertice no
 * tiene sombra, se busca por su posicion entera entre los ultimos vertices
 * que calculo la GTE.  Quita las lineas negras entre poligonos de lite.  La
 * pone PGXP_Init. */
extern int g_pgxp_vertex_cache;
/* En caliente (check_game_fixes): NCLIP con decimales (descarte de caras). */
extern int g_pgxp_culling;
/* En caliente: w por vertice -> texturas con perspectiva correcta. */
extern int g_pgxp_texture_correction;

/* Reserva la sombra de memoria (~10 MB) si g_pgxp_enabled.  0 = bien; si no
 * hay memoria deja g_pgxp_enabled a 0 y devuelve -1. */
int  PGXP_Init(void);
void PGXP_Reset(void);
void PGXP_Shutdown(void);

/* --- GTE ---------------------------------------------------------------- */
void  PGXP_GTE_PushSXYZ2f(float x, float y, float z, unsigned int v);
int   PGXP_GTE_NCLIP_valid(unsigned int sxy0, unsigned int sxy1, unsigned int sxy2);
float PGXP_GTE_NCLIP(void);

/* --- Transferencias GTE <-> CPU / memoria ------------------------------- */
void PGXP_CPU_MFC2(unsigned int instr, unsigned int rtVal, unsigned int rdVal);
void PGXP_CPU_MTC2(unsigned int instr, unsigned int rdVal, unsigned int rtVal);
void PGXP_CPU_CFC2(unsigned int instr, unsigned int rtVal, unsigned int rdVal);
void PGXP_CPU_CTC2(unsigned int instr, unsigned int rdVal, unsigned int rtVal);
void PGXP_CPU_LWC2(unsigned int instr, unsigned int rtVal, unsigned int addr);
void PGXP_CPU_SWC2(unsigned int instr, unsigned int rtVal, unsigned int addr);

/* --- CPU <-> memoria ---------------------------------------------------- */
void PGXP_CPU_LW(unsigned int instr, unsigned int rtVal, unsigned int addr);
void PGXP_CPU_LHx(unsigned int instr, unsigned int rtVal, unsigned int addr);
void PGXP_CPU_LBx(unsigned int instr, unsigned int rtVal, unsigned int addr);
void PGXP_CPU_SB(unsigned int instr, unsigned int rtVal, unsigned int addr);
void PGXP_CPU_SH(unsigned int instr, unsigned int rtVal, unsigned int addr);
void PGXP_CPU_SW(unsigned int instr, unsigned int rtVal, unsigned int addr);
/* addu/addiu con un sumando 0 = "move": copia la sombra.  rd_and_rs =
 * (rd << 8) | rs. */
void PGXP_CPU_MOVE(unsigned int rd_and_rs, unsigned int rsVal);
/* LWL/LWR y SWL/SWR: el valor resultante no es una palabra entera de nadie;
 * se invalida el registro o la palabra de memoria. */
void PGXP_CPU_InvalidateReg(unsigned int reg);
void PGXP_CPU_InvalidateMem(unsigned int addr);

/* --- Ganchos del dynarec (ppc/pR3000A.c) -------------------------------
 * El codigo recompilado guarda la direccion (y el valor, al escribir) en
 * PGXP_rec_slot antes del acceso, y despues llama al gancho con la
 * instruccion como inmediato.  Los de carga devuelven el valor tal cual
 * (r3 entra y sale igual). */
extern unsigned int PGXP_rec_slot[2];   /* [0] = direccion, [1] = valor escrito */
/* Filtro en linea del dynarec (ver pgxp.c): 1 bit por palabra de RAM
 * (indice (addr >> 2) & 7FFFFh; el scratchpad comparte bit con las primeras
 * 256) y 1 bit por registro de la CPU.  0 = la sombra seguro que no vale y no
 * hace falta llamar a C. */
extern unsigned int PGXP_mem_bits[0x200000 / 4 / 32];
extern unsigned int PGXP_reg_bits;
unsigned int PGXP_RecLW(unsigned int value, unsigned int instr);
unsigned int PGXP_RecLH(unsigned int value, unsigned int instr);
unsigned int PGXP_RecLHU(unsigned int value, unsigned int instr);
void PGXP_RecSW(unsigned int instr);
void PGXP_RecSH(unsigned int instr);
/* "move" (addiu rt,rs,0 / addu rd,rs,zero): copia la sombra SIN validar (no
 * hay valores a mano); el SW posterior la valida por valor. */
void PGXP_RecMove(unsigned int rd_and_rs);

/* --- GPU ---------------------------------------------------------------- */
/* Direccion de RAM (0..1FFFFFh) de un puntero que apunta dentro de psxM, o
 * 0xFFFFFFFF si no es RAM (escrituras de la CPU al puerto GP0, pila...). */
unsigned int PGXP_RamAddrOf(const void* p);
/* x, y = posicion entera ya con el desplazamiento de dibujo.  Siempre
 * rellena out_x/out_y/out_w (con la entera y w = 1 si no hay nada valido).
 * Devuelve 1 si la w es valida. */
int PGXP_GetPreciseVertex(unsigned int addr, unsigned int value, int x, int y, int xOffs, int yOffs,
                          float* out_x, float* out_y, float* out_w);

#ifdef __cplusplus
}
#endif

#endif /* __PCSXR_PGXP_H__ */
