/**
 * Wii64 - Recompile.h
 * Copyright (C) 2007, 2008, 2009, 2010 Mike Slegeir
 * 
 * Functions and data structures for recompiling blocks of code
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: tehpola@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#ifndef RECOMPILE_H
#define RECOMPILE_H

#include "MIPS-to-PPC.h"

typedef unsigned int uint;

typedef struct hole_node {
	unsigned short addr;
	struct hole_node* next;
} PowerPC_func_hole_node;

struct func;
struct xb_ppc_block;   /* [Xbox360] el dueno de cada funcion: ver PowerPC_func */

typedef struct func_node {
	struct func* function;
	struct func_node* left;
	struct func_node* right;
} PowerPC_func_node;

typedef struct link_node {
	PowerPC_instr*    branch;
	struct func*      func;
	struct link_node* next;
} PowerPC_func_link_node;

typedef struct func {
	unsigned int start_addr;
	unsigned int end_addr;
	PowerPC_instr* code;
	unsigned int   lru;
	PowerPC_func_hole_node* holes;
	PowerPC_func_link_node* links_in;
	PowerPC_func_node*      links_out;
	PowerPC_instr** code_addr;
	/* [Xbox360] ★ [Castlevania, variantes de TLB] EL BLOQUE EN CUYO ARBOL
	 * VIVE.  Antes era siempre `blocks[start_addr >> 12]`; con varias
	 * variantes por pagina virtual (una por pagina fisica, ver Wrappers.c,
	 * "LAS VARIANTES DE TLB") puede ser una apartada.  Lo pone
	 * recompile_block y lo usa free_func para quitarla del arbol bueno. */
	struct xb_ppc_block* xb_owner;
} PowerPC_func;

PowerPC_func* find_func(PowerPC_func_node** root, unsigned int addr);
void insert_func(PowerPC_func_node** root, PowerPC_func* func);
void remove_func(PowerPC_func_node** root, PowerPC_func* func);

typedef struct xb_ppc_block {
	MIPS_instr*     mips_code;     // The code to recompile
	uint            start_address; // The address this code begins for the 64
	//PowerPC_instr** code_addr;     // table of block offsets to code pointer,
	                               //   its length is end_addr - start_addr
	PowerPC_func_node* funcs;      // BST of functions in this block
	/* [Xbox360] ★ [Castlevania] LAS VARIANTES DE TLB (Wrappers.c): solo en
	 * paginas fuera de KSEG0/KSEG1.  Todo a 0 con calloc = bloque normal.
	 *   xb_phys  pagina fisica + 1 de la que salio su codigo (0 = no se sabe)
	 *   xb_pool  indice + 1 en el registro de variantes (0 = no esta)
	 *   xb_var   la siguiente variante APARTADA de la misma pagina virtual;
	 *            la cadena cuelga del bloque activo, `blocks[pagina]` */
	uint               xb_phys;
	uint               xb_pool;
	struct xb_ppc_block* xb_var;
	/* [Bomberman, Logs Salvia (11)] huella de los 4 KB de la fisica al
	 * compilar (0 = sin huella): solo se reusa si sigue igual (var_hash) */
	uint               xb_hash;
} PowerPC_block;

#define MAX_JUMPS        4096
#define JUMP_TYPE_J      1   // uses a long immed & abs addr
#define JUMP_TYPE_CALL   2   // the jump is to a C function
#define JUMP_TYPE_SPEC   4   // special jump, destination precomputed
typedef struct {
	MIPS_instr*    src_instr;
	PowerPC_instr* dst_instr;
	int            old_jump;
	int            new_jump;
	uint           type;
} jump_node;

MIPS_instr get_next_src(void);
MIPS_instr peek_next_src(void);
int        has_next_src(void);
void       set_next_dst(PowerPC_instr);
int        add_jump(int old_jump, int is_j, int is_out);
int        is_j_out(int branch, int is_aa);
// Use these for jumps that won't be known until later in compile time
int        add_jump_special(int is_j);
void       set_jump_special(int which, int new_jump);

int  func_was_freed(PowerPC_func*);
void clear_freed_funcs(void);
/* [Xbox360] [Conker, log 14] Apunta una funcion liberada en `freed_funcs`.
 * La llama `free_func` (Recomp-Cache-Heap.c), por donde pasa TODA liberacion.
 * Ver la nota en Recompile.c. */
void note_freed_func(PowerPC_func*);

/* These functions are used to initialize, recompile, and deinit a block
   init assumes that all pointers in the block fed it it are NULL or allocated
   memory. Deinit frees a block with the same preconditions.
 */
PowerPC_func* recompile_block(PowerPC_block* ppc_block, unsigned int addr);

/* [Xbox360] ★ `extern "C"` SOLO EN LAS QUE CRUZAN LA FRONTERA.
 *
 * Los .c del dynarec se compilan como C++ (son C99), asi que definen
 * simbolos DECORADOS.  `init_block` y `deinit_block` las llama `r4300.c`,
 * que es C y pide el simbolo PELADO:
 *
 *     xbox64.lib(r4300.obj) : error LNK2001: unresolved external symbol
 *                             init_block
 *
 * ★ Y SE HACE SIMBOLO A SIMBOLO, NO ENVOLVIENDO LA CABECERA ENTERA.  Con
 * enlace C estos nombres pasan al espacio global del .exe, y este .lib se
 * enlaza dentro de Salvia.exe junto a SDL, curl y wolfSSL.  Nombres como
 * `release` (Recomp-Cache.h:52), `find_func` o `insert_func` son
 * demasiado genericos para soltarlos ahi sin motivo: las que no cruzan se
 * quedan decoradas, que ademas es gratis.
 *
 * `recompile_block` NO cruza: solo la llaman .c del propio dynarec, que
 * tambien son C++.
 *
 * ★★ `invalidate_block` SI CRUZA DESDE LA FASE 4, y esta linea decia lo
 * contrario.  La premisa cambio, no el razonamiento: ahora la llama
 * `xbox/savestate_n64.c`, que es C, para tirar TODO el codigo
 * recompilado al cargar un estado guardado (si no, el dynarec seguiria
 * ejecutando el codigo de la RDRAM anterior; la nota larga esta alli).
 *
 * Se mueve dentro del bloque en vez de dejar la declaracion fuera y
 * apanyar el `extern "C"` en el que llama: con dos declaraciones del
 * mismo simbolo y distinto enlace, gana la que el compilador vea
 * primero, y eso ya costo una oleada de LNK2001 en la fase 3 con
 * `blocks`.  El nombre es especifico de sobra para el espacio global. */
#ifdef __cplusplus
extern "C" {
#endif
void init_block  (PowerPC_block* ppc_block);
void deinit_block(PowerPC_block* ppc_block);
void invalidate_block(PowerPC_block* ppc_block, int is_deinit);
#ifdef __cplusplus
}
#endif

#if defined(_XBOX)
/* [Xbox360] LA ISLA DE TRAMPOLINES.  El `bl` de PowerPC solo alcanza
 * +-32 MB y las funciones de C estan a 44, asi que el codigo emitido salta
 * a un trampolin dentro de la propia arena y es el trampolin el que hace el
 * salto largo.  El porque completo, con la medida, esta en
 * ppc/Recompile.c junto a `dyna_get_stub`.
 *
 * Vive en la CABECERA de la arena; por eso el tamanyo lo necesita tambien
 * `Recomp-Cache-Heap.c`, que arranca el monton detras de ella. */
#define DYNA_STUB_MAX    64
#define DYNA_STUB_WORDS  4
#define DYNA_STUB_BYTES  (DYNA_STUB_MAX * DYNA_STUB_WORDS * sizeof(PowerPC_instr))
#endif

/* [Xbox360] ★ `blocks` ESTA DECLARADO EN DOS CABECERAS, Y GANA ESTA.
 *
 * La otra es `r4300.h:113`, que ya quedo dentro de su `extern "C"`.  Pero
 * `r4300.h` incluye ESTE fichero JUSTO ANTES (linea 109, y a proposito
 * fuera del bloque, porque aqui se declaran simbolos de C++), asi que en
 * los .c del dynarec esta linea se ve PRIMERO y es la que fija el enlace.
 * MSVC no avisa de la discrepancia: se queda con la primera y pide el
 * nombre decorado.
 *
 *     unresolved external symbol "struct PowerPC_block * * blocks"
 *                                (?blocks@@3QAPAUPowerPC_block@@A)
 *
 * Lo define `r4300.c:64`, que es C.  ★ Que una declaracion este arreglada
 * no basta si hay otra antes: hay que arreglar LAS DOS. */
#ifdef __cplusplus
extern "C" {
#endif
extern PowerPC_block **const blocks;
#ifdef __cplusplus
}
#endif

#endif
