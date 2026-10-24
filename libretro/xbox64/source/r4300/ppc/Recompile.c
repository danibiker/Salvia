/**
 * Wii64 - Recompile.c
 * Copyright (C) 2007, 2008, 2009, 2010 Mike Slegeir
 * 
 * Recompiles a block of MIPS code to PPC
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

/* TODO: Try to conform more to the interface mupen64 uses.
         If we have the entire RAM and recompiled code in memory,
           we'll run out of room, we should implement a recompiled
           code cache (e.g. free blocks which haven't been used lately)
         If it's possible, only use the blocks physical addresses (saves space)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <gccore.h>
#include "Register-Cache.h"
#include "../../gc_memory/memory.h"
#include "../Invalid_Code.h"
#include "../interupt.h"
#include "Recompile.h"
#include "../Recomp-Cache.h"
#include "Wrappers.h"

#include "../../gui/DEBUG.h"
#include "../../xenon64_diag.h"   /* XBOX64_TRAZAS */

static MIPS_instr*    src;
static PowerPC_instr* ppc_dst;
static MIPS_instr*    src_last;
static MIPS_instr*    src_first;
static unsigned int   code_length;
static unsigned int   addr_first;
static unsigned int   addr_last;
static jump_node    jump_table[MAX_JUMPS];
static unsigned int current_jump;
static PowerPC_instr** code_addr;
static unsigned char isJmpDst[1024];

/* [Xbox360] ★★ [Worms Armageddon, excepcion al empezar un entrenamiento,
 * logs 2026-10-24] EL BUFER DE LA CONVERSION SE DESBORDABA.  Wii64 lo tiene
 * de 32K instrucciones y sin vigilar, y asi le basta; pero aqui cada acceso a
 * memoria emite ademas el TLB en linea (partes 1, 2 y 4, XBOX64_TLB_EN_LINEA en
 * MIPS-to-PPC.c), con su volcado, la llamada a dyna_mem y la recarga: decenas
 * de instrucciones por acceso.  La funcion de 0x8005B000 son 1020
 * instrucciones MIPS con ~450 accesos, y `set_next_dst` siguio escribiendo
 * detras de `code_buffer`, encima de las estaticas de este fichero.  MEDIDO
 * con la sonda SEH: `src`=38A00001 (`li r5,1`) y `addr_last`=48000034 (`b`),
 * y el `isJmpDst[src - mips_code]` del bucle de conversion leyo 0x728B8EB8.
 * Ahora 128K instrucciones (512 KB; 128 por instruccion MIPS) y set_next_dst
 * ya no escribe fuera: si aun asi se llenase, se apunta en `xb_desborde`.
 * Medido con la funcion de 0x8005B000: 35735 instrucciones, 2787 saltos. */
#define XB_CODE_BUFFER_INSTR (1024*128)
static PowerPC_instr code_buffer[XB_CODE_BUFFER_INSTR];
static PowerPC_instr* code_addr_buffer[1024];
/* 1 = esta conversion se salio de code_buffer o de jump_table (y se dejo de
 * escribir fuera).  El codigo de esa funcion no vale; ver recompile_block. */
static int xb_desborde = 0;

PowerPC_func* current_func;
static struct func_list {
	PowerPC_func*     func;
	struct func_list* next;
} *freed_funcs = NULL;

static int pass0(PowerPC_block* ppc_block);
static void pass2(PowerPC_block* ppc_block);
//static void genRecompileBlock(PowerPC_block*);
static void genJumpPad(void);
/* [Xbox360] ★ ESTA REDECLARACION SE QUITA, Y NO ES LIMPIEZA.
 *
 * `Recompile.h` -- incluido mas arriba -- ya la declara, y desde la fase
 * 4 la declara DENTRO de `extern "C"`, porque `xbox/savestate_n64.c`
 * (que es C) la llama para tirar el codigo recompilado al cargar un
 * estado.  Dejar aqui una segunda declaracion SIN `extern "C"` en un
 * fichero que se compila como C++ es pedir dos simbolos con el mismo
 * nombre y distinto enlace en la misma unidad.
 *
 * Es literalmente la trampa que costo una oleada de LNK2001 en la fase 3
 * con `blocks`: estaba declarado en DOS cabeceras y arregle la que no
 * ganaba.  Aqui la de la cabecera es la buena y esta sobra. */

MIPS_instr get_next_src(void) { return *(src++); }
MIPS_instr peek_next_src(void){ return *src;     }
int has_next_src(void){ return (src_last-src) > 0; }
// Hacks for delay slots
 // Undoes a get_next_src
 void unget_last_src(void){ --src; }
 // Used for finding how many instructions were generated
 PowerPC_instr* get_curr_dst(void){ return ppc_dst; }
 // Makes sure a branch to a NOP in the delay slot won't crash
 // This should be called ONLY after get_next_src returns a
 //   NOP in a delay slot
 void nop_ignored(void){ if(src<src_last) code_addr[src-1-src_first] = ppc_dst; }
 // Returns whether the current src instruction is branched to
 int is_j_dst(int i){ return isJmpDst[i + ((get_src_pc()&0xfff)>>2)]; }
// Returns the MIPS PC
unsigned int get_src_pc(void){ return addr_first + ((src-1-src_first)<<2); }
/* [Xbox360] [Worms Armageddon] Nunca fuera de code_buffer: ver
 * XB_CODE_BUFFER_INSTR.  Solo se emite durante la conversion, a code_buffer. */
void set_next_dst(PowerPC_instr i){
	if(ppc_dst >= code_buffer && ppc_dst < code_buffer + XB_CODE_BUFFER_INSTR)
		*ppc_dst = i;
	else
		xb_desborde = 1;
	++ppc_dst; ++code_length;
}
// Adjusts the code_addr for the current instruction to account for flushes
void reset_code_addr(void){ if(src<=src_last) code_addr[src-1-src_first] = ppc_dst; }
// Same, but for multiple swallowed source instructions.
void reset_code_addr_burst(int count){ if(src<=src_last) code_addr[src-count-src_first] = ppc_dst; }

static inline unsigned short extractUpper16(void* address){
	unsigned int addr = (unsigned int)address;
	return (addr>>16) + ((addr>>15)&1);
}

static inline short extractLower16(void* address){
	unsigned int addr = (unsigned int)address;
	return addr&0x8000 ? (addr&0xffff)-0x10000 : addr&0xffff;
}


int add_jump(int old_jump, int is_j, int is_call){
	/* [Xbox360] [Worms Armageddon] Tampoco fuera de jump_table: con la tabla
	 * llena se reusa la ultima entrada y la funcion queda marcada. */
	if(current_jump >= MAX_JUMPS){ xb_desborde = 1; current_jump = MAX_JUMPS - 1; }
	int id = current_jump;
	jump_node* jump = &jump_table[current_jump++];
	jump->old_jump  = old_jump;
	jump->new_jump  = 0;     // This should be filled in when known
	jump->src_instr = src-1; // src points to the next
	jump->dst_instr = ppc_dst;   // set_next hasn't happened
	jump->type      = (is_j    ? JUMP_TYPE_J    : 0)
	                | (is_call ? JUMP_TYPE_CALL : 0);
	return id;
}

int add_jump_special(int is_j){
	if(current_jump >= MAX_JUMPS){ xb_desborde = 1; current_jump = MAX_JUMPS - 1; }   /* idem */
	int id = current_jump;
	jump_node* jump = &jump_table[current_jump++];
	jump->new_jump  = 0;     // This should be filled in when known
	jump->dst_instr = ppc_dst;   // set_next hasn't happened
	jump->type      = JUMP_TYPE_SPEC | (is_j ? JUMP_TYPE_J : 0);
	return id;
}

void set_jump_special(int which, int new_jump){
	jump_node* jump = &jump_table[which];
	if(!(jump->type & JUMP_TYPE_SPEC)) return;
	jump->new_jump = new_jump;
}

/* [Xbox360] ★★ FUNCION ANIDADA DE GCC, SACADA A AMBITO DE FICHERO.
 *
 * Estaba declarada DENTRO de recompile_block.  Las funciones anidadas son
 * una extension de GNU C -- no son C89, ni C99, ni C++ -- y MSVC las
 * rechaza con `error C2601: local function definitions are illegal`.
 *
 * ★ Y ESTA **SI** CAPTURA, que es lo que la hace delicada.  De las seis
 * variables del ambito de fuera que toca, dos son estaticas de fichero y
 * siguen funcionando solas:
 *
 *     addr_first, src_first     estaticas (Recompile.c:48,50)
 *
 * y CUATRO son locales de recompile_block.  Esas pasan por parametro, y
 * TRES DE ELLAS SE ESCRIBEN desde aqui, asi que van por PUNTERO o el
 * arreglo seria silencioso y peor que el error:
 *
 *     ppc_block    solo lectura ....... por valor
 *     addr         se escribe (era :205) por puntero
 *     need_pad     se escribe (era :206) por puntero
 *     needInsert   se escribe (era :209) por puntero
 *
 * Con `needInsert` pasado por valor, `insert_func` volveria a insertar una
 * funcion que ya es un agujero de otra: dos entradas para el mismo rango en
 * el arbol de funciones.  No es un fallo que de la cara en el arranque.
 *
 * La recursion se mantiene, y por eso los punteros se propagan tal cual a
 * las dos llamadas de dentro. */
static PowerPC_func* handle_overlap(PowerPC_func_node** node, PowerPC_func* func,
                                    PowerPC_block* ppc_block, unsigned int* addr,
                                    int* need_pad, int* needInsert){
	if(!(*node)) return func;
	// Check for any potentially overlapping functions to the left or right
	if((*node)->function->end_addr >= func->start_addr)
		func = handle_overlap(&(*node)->left, func, ppc_block, addr, need_pad, needInsert);
	if((*node)->function->start_addr < func->end_addr)
		func = handle_overlap(&(*node)->right, func, ppc_block, addr, need_pad, needInsert);
	// Check for overlap with this function
	if((*node)->function->start_addr > func->start_addr &&
	   (*node)->function->end_addr == func->end_addr){
		// (*node)->function is a hole in func
		PowerPC_func_hole_node* hole = (PowerPC_func_hole_node*)malloc(sizeof(PowerPC_func_hole_node));
		if(!hole) {
			release(1);
			hole = (PowerPC_func_hole_node*)malloc(sizeof(PowerPC_func_hole_node));
		}
		hole->addr = (*node)->function->start_addr;
		hole->next = func->holes;
		func->holes = hole;
		// Add all holes from the hole
		// Get to the end of this func->holes
		PowerPC_func_hole_node* fhn;
		for(fhn=func->holes; fhn->next; fhn=fhn->next);
		// Add fn->function's holes to the end func->holes
		fhn->next = (*node)->function->holes;
		// Make sure those holes aren't freed
		(*node)->function->holes = NULL;
		// Add it to the freed_funcs list
		struct func_list* freed = (struct func_list*)malloc(sizeof(struct func_list));
		freed->func = (*node)->function, freed->next = freed_funcs;
		freed_funcs = freed;
		// Free the hole
		/* [Xbox360] Por puntero: ver RecompCache_FreeFunc (Recomp-Cache.h). */
		RecompCache_FreeFunc((*node)->function);

	} else if(func->start_addr > (*node)->function->start_addr &&
			  func->end_addr == (*node)->function->end_addr){
		// func is a hole in fn->function
		PowerPC_func_hole_node* hole = (PowerPC_func_hole_node*)malloc(sizeof(PowerPC_func_hole_node));
		if(!hole) {
			release(1);
			hole = (PowerPC_func_hole_node*)malloc(sizeof(PowerPC_func_hole_node));
		}
		hole->addr = func->start_addr&0xffff;
		hole->next = (*node)->function->holes;
		(*node)->function->holes = hole;
		// Free this func since its just a hole now
		free(func);
		// Move all our pointers to the outer function
		func = (*node)->function;
		addr_first = func->start_addr;
		src_first = ppc_block->mips_code + ((addr_first&0xfff)>>2);
		*addr = addr_first;
		*need_pad = pass0(ppc_block);
		RecompCache_Update(func);
		// Make sure we don't insert the old func again
		*needInsert = 0;
		// There cannot be another overlapping function
		//break;

	} else if(func->start_addr < (*node)->function->end_addr &&
			  func->end_addr   > (*node)->function->start_addr){
		// Add it to the freed_funcs list
		struct func_list* freed = (struct func_list*)malloc(sizeof(struct func_list));
		freed->func = (*node)->function, freed->next = freed_funcs;
		freed_funcs = freed;
		// We have some other non-containment overlap
		RecompCache_FreeFunc((*node)->function);   /* [Xbox360] por puntero */
	}
	return func;
}

// Converts a sequence of MIPS instructions to a PowerPC block
PowerPC_func* recompile_block(PowerPC_block* ppc_block, unsigned int addr){
	src_first = ppc_block->mips_code + ((addr&0xfff)>>2);
	addr_first = ppc_block->start_address + (addr&0xfff);
	code_addr = NULL; // Just to make sure this isn't used here
	xb_desborde = 0;   /* [Worms Armageddon] ver XB_CODE_BUFFER_INSTR */

	int need_pad = pass0(ppc_block); // Sets src_last, addr_last

	code_length = 0;

	// Create a PowerPC_func for this function
	/* [Xbox360] casts: fichero compilado como C++ */
	PowerPC_func* func = (PowerPC_func*)calloc(1, sizeof(PowerPC_func));
	if(!func) {
		release(1);
		func = (PowerPC_func*)calloc(1, sizeof(PowerPC_func));
	}
	func->start_addr = addr_first;
	func->end_addr = addr_last;
	// We'll need to insert this func into the block
	int needInsert = 1;

	// Check for and remove any overlapping functions
	func = handle_overlap(&ppc_block->funcs, func,
	                      ppc_block, &addr, &need_pad, &needInsert);
	if(needInsert) insert_func(&ppc_block->funcs, func);
	/* [Xbox360] [Castlevania, variantes de TLB] Su arbol es el de ESTE bloque
	 * (ver xb_owner en Recompile.h).  Tambien si es la de fuera de un hueco:
	 * la de fuera ya era de este bloque. */
	func->xb_owner = ppc_block;
	current_func = func;

	PowerPC_func_hole_node* hole;
	for(hole = func->holes; hole != NULL; hole = hole->next){
		isJmpDst[ (hole->addr&0xfff) >> 2 ] = 1;
	}

	src = src_first;
	ppc_dst = code_buffer; // Use buffer to avoid guessing length
	current_jump = 0;
	code_addr = code_addr_buffer;
	memset(code_addr, 0, addr_last - addr_first);

	start_new_block();
	isJmpDst[src-ppc_block->mips_code] = 1;
	// If the final instruction is a branch delay slot and is branched to
	//   we will need a jump pad so that execution will continue after it
	need_pad |= isJmpDst[src_last-1-ppc_block->mips_code];

	while(has_next_src()){
		unsigned int offset = src - ppc_block->mips_code;

		if(isJmpDst[offset]){
			src++; start_new_mapping(); src--;
		}

		//ppc_block->code_addr[offset] = dst;
		convert();
	}

	// Flush any remaining mapped registers
	flushRegisters(); //start_new_mapping();
	// In case we couldn't compile the whole function, use a pad
	if(need_pad)
		genJumpPad();
#ifdef LAZY_GEN_CALLS
	end_block();
#endif
	/* [Xbox360] [Worms Armageddon] Si aun asi se lleno (ver
	 * XB_CODE_BUFFER_INSTR), no se copia mas de lo que hay en el bufer.  Esa
	 * funcion queda mal, pero ya no pisa nada de este fichero. */
	if(code_length > XB_CODE_BUFFER_INSTR)
		code_length = XB_CODE_BUFFER_INSTR;
	// Allocate the func buffers and copy the code
	if(!func->code){
		// We aren't recompiling from a hole
#ifdef USE_RECOMP_CACHE
		RecompCache_Alloc(code_length * sizeof(PowerPC_instr), addr, func);
#else
		func->code = malloc(code_length * sizeof(PowerPC_instr));
#endif
	} else {
		// We're recompiling from a hole, and we need to adjust the buffer size
		RecompCache_Realloc(func, code_length * sizeof(PowerPC_instr));
		/* [Xbox360] ★★ [Conker, log 12] EL CODIGO VIEJO DE ESTA FUNCION YA NO
		 * EXISTE: que `dynarec()` no enlace desde el.
		 *
		 * `RecompCache_Realloc` libera el codigo de `func` y le da otro.  Si
		 * `func` es la funcion de la que se acaba de salir (`last_func` en
		 * Wrappers.c, que pasa justo cuando se salta a un agujero de la misma
		 * funcion), `link_branch` apunta al codigo VIEJO, y `dynarec()` lo
		 * enlazaba: parcheaba memoria libre y apuntaba el enlace en
		 * `links_in`.  Cuando otra funcion ocupa esa memoria y despues se
		 * deshace el enlace, `unlink_func` escribe ori/ori/blrl en medio de
		 * ella.  La sonda del desenlace lo cogio justo donde Conker se queda
		 * parado para siempre:
		 *     DESENLACE FUERA DE SITIO: branch=84C16D10 op=14 (esperado 18)
		 * Los otros dos casos de `handle_overlap` ya apuntan la funcion en
		 * `freed_funcs`; este era el unico que no.  Se apunta igual, y la
		 * guarda `func_was_freed(last_func)` de `dynarec()` descarta el
		 * enlace.  En Wrappers.c, `[DYNA] enlace descartado` lo cuenta. */
		{
			struct func_list* freed = (struct func_list*)malloc(sizeof(struct func_list));
			freed->func = func, freed->next = freed_funcs;
			freed_funcs = freed;
		}
	}
	memcpy(func->code, code_buffer, code_length * sizeof(PowerPC_instr));
	memcpy(func->code_addr, code_addr_buffer, addr_last - addr_first);

	// Readjusting pointers to the func buffers
	code_addr = func->code_addr;
	ppc_dst = func->code + (ppc_dst - code_buffer);
	int i;
	for(i=0; i<src-src_first; ++i)
		if(code_addr[i])
			code_addr[i] = func->code + (code_addr[i] - code_buffer);
	for(i=0; i<current_jump; ++i)
		jump_table[i].dst_instr = func->code +
		                          (jump_table[i].dst_instr - code_buffer);

	// Here we recompute jumps and branches
	/* [Xbox360] [Worms Armageddon] Con desborde, los saltos apuntados detras
	 * del codigo copiado caerian fuera de func->code: no se parchea ninguno. */
	if(xb_desborde)
		current_jump = 0;
	pass2(ppc_block);

	// Since this is a fresh block of code,
	// Make sure it wil show up in the ICache
	DCFlushRange(func->code, code_length*sizeof(PowerPC_instr));
	ICInvalidateRange(func->code, code_length*sizeof(PowerPC_instr));

	return func;
}

void init_block(PowerPC_block* ppc_block){
	
	// FIXME: Equivalent addresses should point to the same code/funcs?
	if(ppc_block->start_address+0x1000 < 0x80000000 || ppc_block->start_address >= 0xc0000000){
		unsigned long paddr = virtual_to_physical_address(ppc_block->start_address, 2);
		invalid_code_set(paddr>>12, 0);
		if(!blocks[paddr>>12]){
			blocks[paddr>>12] = (PowerPC_block*)calloc(1, sizeof(PowerPC_block));
			if(!blocks[paddr>>12]) {
				release(1);
				blocks[paddr>>12] = (PowerPC_block*)calloc(1, sizeof(PowerPC_block));
			}
			blocks[paddr>>12]->start_address = paddr & ~0xFFF;
			init_block(blocks[paddr>>12]);
		}

		paddr += 0x1000 - 4;
		invalid_code_set(paddr>>12, 0);
		if(!blocks[paddr>>12]){
			blocks[paddr>>12] = (PowerPC_block*)calloc(1, sizeof(PowerPC_block));
			if(!blocks[paddr>>12]) {
				release(1);
				blocks[paddr>>12] = (PowerPC_block*)calloc(1, sizeof(PowerPC_block));
			}
			blocks[paddr>>12]->start_address = paddr & ~0xFFF;
			init_block(blocks[paddr>>12]);
		}
	} else {
		unsigned long paddr = ppc_block->start_address ^ 0x20000000;
		invalid_code_set(paddr>>12, 0);
		if(!blocks[paddr>>12]){
		     blocks[paddr>>12] = (PowerPC_block*)calloc(1, sizeof(PowerPC_block));
			 if(!blocks[paddr>>12]) {
				release(1);
				blocks[paddr>>12] = (PowerPC_block*)calloc(1, sizeof(PowerPC_block));
			}
		     blocks[paddr>>12]->start_address = paddr & ~0xFFF;
		     init_block(blocks[paddr>>12]);
		}
	}

	invalid_code_set(ppc_block->start_address>>12, 0);
}

void deinit_block(PowerPC_block* ppc_block){

	invalidate_block(ppc_block, 1);

	// We need to mark all equivalent addresses as invalid
	if(ppc_block->start_address+0x1000 < 0x80000000 || ppc_block->start_address >= 0xc0000000){
		unsigned long paddr = virtual_to_physical_address(ppc_block->start_address, 2);
		
		if(blocks[paddr>>12]){
		     invalid_code_set(paddr>>12, 1);
		}

		paddr += 0x1000 - 4;
		if(blocks[paddr>>12]){
		     invalid_code_set(paddr>>12, 1);
		}
	} else {
		unsigned long paddr = ppc_block->start_address ^ 0x20000000;
		if(blocks[paddr>>12]){
		     invalid_code_set(paddr>>12, 1);
		}
	}
}

int is_j_out(int branch, int is_aa){
	if(is_aa)
		return ((branch << 2 | (addr_first & 0xF0000000)) <  addr_first ||
		        (branch << 2 | (addr_first & 0xF0000000)) >= addr_last);
	else {
		int dst_instr = (src-1 - src_first) + branch;
		return (dst_instr < 0 || dst_instr >= (addr_last-addr_first)>>2);
	}
}

// Pass 2 fills in all the new addresses
#if defined(_XBOX)
/* ===========================================================================
 * [Xbox360] ★★ LA ISLA DE TRAMPOLINES.  EL `bl` A C NO LLEGA.
 * ===========================================================================
 *
 * SINTOMA MEDIDO: el primer salto a una funcion de C desde codigo
 * recompilado murio en 0x863F2BB0, que no es ninguna funcion.
 *
 *     0x863F2BB0 - 0x04000000 = 0x823F2BB0
 *
 * o sea el destino REAL, mas EXACTAMENTE un giro completo del campo de 26
 * bits (2^26 = 64 MB).  Esa resta es la firma; no hace falta nada mas.
 *
 * EL `bl` DE PowerPC LLEVA UN DESPLAZAMIENTO DE 24 BITS x4 = +-32 MB
 * (`PPC_LI_MASK` = 0xFFFFFF, PowerPC.h:85).  La arena de codigo esta en
 * 0x84E5xxxx y el .text del XEX en 0x823Fxxxx: 44,4 MB.  `pass2` metia el
 * desplazamiento en el campo CON UN `&` Y SIN COMPROBAR NADA, asi que el
 * salto se iba a la nada en silencio.  Es la trampa nº4 de FASE3.txt:6.
 *
 * ★ POR QUE UNA ISLA Y NO CONVERTIR CADA LLAMADA EN LIS/ORI/MTCTR/BCTRL.
 * Esa es la solucion evidente y es MUCHO mas cara: la llamada pasaria de
 * UNA instruccion a CUATRO, y hay desplazamientos de rama CABLEADOS A MANO
 * alrededor de varias de ellas (`genCheckFP` salta 6 instrucciones por
 * encima, `CHECK_INVALID_CODE` otras tantas) que habria que recalcular uno
 * a uno.  Cada uno de esos numeros es una oportunidad de equivocarse en
 * silencio, y estan repartidos por un fichero de 140 KB que ademas es LA
 * pieza que vigila la puerta de salida de la fase.
 *
 * Con la isla, el codigo emitido NO CAMBIA: sigue siendo un `bl` de una
 * instruccion, solo que a un trampolin que esta DENTRO de la arena.  Como
 * la arena mide menos de 32 MB (16 hoy; lo comprueba xbox_mem.h en
 * compilacion), ese salto cabe siempre.  El trampolin hace el salto
 * largo de verdad.
 *
 * ★ Y LA SEMANTICA ES IDENTICA, que es lo que lo hace seguro: `bl` deja en
 * LR la vuelta al codigo recompilado y `bctr` NO TOCA LR, asi que la
 * funcion de C vuelve directamente a donde tiene que volver.  El trampolin
 * no aparece en la cadena de retorno.
 *
 * ★ r0 COMO REGISTRO DE PASO, y el argumento es cerrado: el destino es una
 * funcion de C, y en el ABI r0 es VOLATIL -- o sea que la llamada ya lo
 * destruia de todos modos.  Ademas `Register-Cache.c:33` nunca lo reparte
 * ("r0 is mostly used for saving/restoring lr: used as a temp").
 *
 * ★ LA ISLA VIVE EN LA CABECERA DE LA ARENA, no en un array aparte: asi la
 * distancia bloque->trampolin esta acotada POR CONSTRUCCION por el tamanyo
 * de la arena, y no por donde el enlazador decida poner dos .bss distintos.
 * `RecompCache_Init` arranca el monton DESPUES de la isla.
 */
/* DYNA_STUB_MAX / _WORDS / _BYTES viven en Recompile.h: los necesita
 * tambien Recomp-Cache-Heap.c, que tiene que arrancar el monton DESPUES de
 * la isla. */
static unsigned int dyna_stub_target[DYNA_STUB_MAX];
static int          dyna_stub_count = 0;

static PowerPC_instr* dyna_get_stub(unsigned int target){
	PowerPC_instr* base = (PowerPC_instr*)RECOMP_CACHE_LO;
	PowerPC_instr* s;
	int i;

	for(i = 0; i < dyna_stub_count; i++)
		if(dyna_stub_target[i] == target)
			return base + i*DYNA_STUB_WORDS;

	if(dyna_stub_count >= DYNA_STUB_MAX){
#if XBOX64_TRAZAS
		sprintf(txtbuffer, "[DYNA] isla LLENA (%d trampolines); destino %08X",
		        DYNA_STUB_MAX, target);
		DEBUG_print(txtbuffer, DBG_USBGECKO);
#endif
		return NULL;
	}

	i = dyna_stub_count++;
	dyna_stub_target[i] = target;
	s = base + i*DYNA_STUB_WORDS;

	/* lis r0,hi / ori r0,r0,lo / mtctr r0 / bctr
	 * ★ `lis`+`ori` y no `lis`+`addi`: `ori` es de cero-extension, asi que
	 * no hay que corregir el signo de los 16 bits bajos. */
	_LIS  (s[0], 0, (target >> 16) & 0xFFFF);
	_ORI  (s[1], 0, 0, target & 0xFFFF);
	_MTCTR(s[2], 0);
	_BCTR (s[3]);

	DCFlushRange(s, DYNA_STUB_WORDS*sizeof(PowerPC_instr));
	ICInvalidateRange(s, DYNA_STUB_WORDS*sizeof(PowerPC_instr));

	/* ★ UNA LINEA POR TRAMPOLIN, y no mas: son como mucho DYNA_STUB_MAX en
	 * toda la sesion, asi que no hay nada que acotar.  Es el control
	 * POSITIVO de la isla: si la lista sale vacia, ningun bloque ha llamado
	 * a C todavia y cualquier teoria sobre las llamadas es prematura; si
	 * sale con destinos en el .text, la isla se construyo. */
#if XBOX64_TRAZAS
	sprintf(txtbuffer, "[DYNA] trampolin %d -> %08X  (en %08X)",
	        i, target, (unsigned int)s);
	DEBUG_print(txtbuffer, DBG_USBGECKO);
#endif

	return s;
}

/* ★ LA HERRAMIENTA QUE MENTIA, ARREGLADA.  `pass2` truncaba el
 * desplazamiento con un `&` y seguia como si nada.  Ahora grita.  No es
 * defensa por si acaso: es que este fallo ya costo una sesion, y el
 * sintoma -- salto a una direccion plausible -- no apunta a `pass2` ni de
 * lejos. */
static int dyna_disp_ok(int words, const char* que, unsigned int destino){
	if(words >= -0x800000 && words <= 0x7FFFFF) return 1;
#if XBOX64_TRAZAS
	sprintf(txtbuffer, "[DYNA] ★ DESPLAZAMIENTO FUERA DE RANGO (%s): "
	        "%d palabras (%d MB), destino %08X",
	        que, words, (int)((words*4)/(1024*1024)), destino);
	DEBUG_print(txtbuffer, DBG_USBGECKO);
#endif
	return 0;
}
#endif /* _XBOX */

static void pass2(PowerPC_block* ppc_block){
	int i;
	PowerPC_instr* current;
	for(i=0; i<current_jump; ++i){
		current = jump_table[i].dst_instr;

		// Special jump, its been filled out
		if(jump_table[i].type & JUMP_TYPE_SPEC){
			if(!(jump_table[i].type & JUMP_TYPE_J)){
				// We're filling in a branch instruction
				*current &= ~(PPC_BD_MASK << PPC_BD_SHIFT);
				PPC_SET_BD(*current, jump_table[i].new_jump);
			} else {
				// We're filling in a jump instrucion
				*current &= ~(PPC_LI_MASK << PPC_LI_SHIFT);
				PPC_SET_LI(*current, jump_table[i].new_jump);
			}
			continue;
		}

		if(jump_table[i].type & JUMP_TYPE_CALL){ // Call to C function code
			// old_jump is the address of the function to call
#if defined(_XBOX)
			/* [Xbox360] El salto NO va a la funcion, va a su trampolin en
			 * la cabecera de la arena.  Ver la nota larga de
			 * `dyna_get_stub`: a la funcion no se llega, son 44 MB y el
			 * campo del `bl` da para 32.
			 *
			 * ★ GliGli, con ESTE MISMO dynarec en Xenon, no toco esta
			 * linea: la dejo igual que Wii64 y acerco el bufer al .text
			 * con `section(".bss.beginning.upper")` y un linker script
			 * (mupen64-360/source/r4300/Recomp-Cache.c:301).  MSVC no
			 * tiene linker script, asi que aqui se acorta el salto en vez
			 * de acortar la distancia. */
			PowerPC_instr* stub = dyna_get_stub((unsigned int)jump_table[i].old_jump);
			int jump_offset = stub ? (int)(stub - current) : 0;
			dyna_disp_ok(jump_offset, "llamada a C",
			             (unsigned int)jump_table[i].old_jump);
#else
			int jump_offset = ((unsigned int)jump_table[i].old_jump -
			                   (unsigned int)current)/4;
#endif
			// We're filling in a jump instrucion
			*current &= ~(PPC_LI_MASK << PPC_LI_SHIFT);
			PPC_SET_LI(*current, jump_offset);

		} else if(!(jump_table[i].type & JUMP_TYPE_J)){ // Branch instruction
			int jump_offset = (unsigned int)jump_table[i].old_jump +
				         ((unsigned int)jump_table[i].src_instr - (unsigned int)src_first)/4;

			jump_table[i].new_jump = code_addr[jump_offset] - current;

#if 0
			// FIXME: Reenable this when blocks are small enough to BC within
			//          Make sure that branch is using BC/B as appropriate
			*current &= ~(PPC_BD_MASK << PPC_BD_SHIFT);
			PPC_SET_BD(*current, jump_table[i].new_jump);
#else
			*current &= ~(PPC_LI_MASK << PPC_LI_SHIFT);
			PPC_SET_LI(*current, jump_table[i].new_jump);
#endif

		} else { // Jump instruction
			// The destination is actually calculated from the delay slot
			unsigned int jump_addr = (jump_table[i].old_jump << 2) |
			                         (ppc_block->start_address & 0xF0000000);

			// We're jumping within this block, find out where
			int jump_offset = (jump_addr - addr_first) >> 2;
			jump_table[i].new_jump = code_addr[jump_offset] - current;

			*current &= ~(PPC_LI_MASK << PPC_LI_SHIFT);
			PPC_SET_LI(*current, jump_table[i].new_jump);

		}
	}
}

static int pass0(PowerPC_block* ppc_block){
	// Scan over the MIPS code for this function and detect any
	//   any local branches and mark their destinations (for flushing).
	//   Determine src_last and addr_last by the end of the block
	//   (if reached) or the end of the function (exclusive of the
	//   delay slot as it will be recompiled ahead of the J/JR and
	//   does not need to be recompiled after the J/JR as it belongs
	//   to the next function if its used in that way.
	// If the function continues into the next block, we'll need to use
	//   a jump pad at the end of the block.
	unsigned int pc = addr_first >> 2;
	// Set this to the end address of the block for is_j_out
	addr_last = ppc_block->start_address+0x1000;
	// Zero out the jump destinations table
	memset(isJmpDst, 0, 1024);
	// Go through each instruction and map every branch instruction's destination
	for(src = src_first; (pc < addr_last >> 2); ++src, ++pc){
		int opcode = MIPS_GET_OPCODE(*src);
		int func = MIPS_GET_FUNC(*src);
		int index = pc - (ppc_block->start_address >> 2);
		if(opcode == MIPS_OPCODE_J || opcode == MIPS_OPCODE_JAL){
			unsigned int li = MIPS_GET_LI(*src);
			src+=2; ++pc;
			if(!is_j_out(li, 1)){
				assert( ((li&0x3FF) >= 0) && ((li&0x3FF) < 1024) );
				isJmpDst[ li & 0x3FF ] = 1;
			}
			--src;
			if(opcode == MIPS_OPCODE_JAL && index + 2 < 1024)
				isJmpDst[ index + 2 ] = 1;
			if(opcode == MIPS_OPCODE_J){ ++src, ++pc; break; }
		} else if(opcode == MIPS_OPCODE_BEQ   ||
		          opcode == MIPS_OPCODE_BNE   ||
		          opcode == MIPS_OPCODE_BLEZ  ||
		          opcode == MIPS_OPCODE_BGTZ  ||
		          opcode == MIPS_OPCODE_BEQL  ||
		          opcode == MIPS_OPCODE_BNEL  ||
		          opcode == MIPS_OPCODE_BLEZL ||
		          opcode == MIPS_OPCODE_BGTZL ||
		          opcode == MIPS_OPCODE_B     ||
		          (opcode == MIPS_OPCODE_COP1 &&
		           MIPS_GET_RS(*src) == MIPS_FRMT_BC)){
			short bd = MIPS_GET_IMMED(*src);
			src+=2; ++pc;
			bd |= (bd & 0x8000) ? 0xFFFF0000 : 0; // sign extend
			if(!is_j_out(bd, 0)){
				assert( index + 1 + bd >= 0 && index + 1 + bd < 1024 );
				isJmpDst[ index + 1 + bd ] = 1;
			}
			--src;
			if(index + 2 < 1024)
				isJmpDst[ index + 2 ] = 1;
		} else if(opcode == MIPS_OPCODE_R &&
		          (MIPS_GET_FUNC(*src) == MIPS_FUNC_JR ||
		           MIPS_GET_FUNC(*src) == MIPS_FUNC_JALR)){
			++src, ++pc;
			if(func == MIPS_FUNC_JALR && index + 2 < 1024)
				isJmpDst[ index + 2 ] = 1;
			if(func == MIPS_FUNC_JR){ ++src, ++pc; break; }
		} else if(opcode == MIPS_OPCODE_COP0 &&
		          func == MIPS_FUNC_ERET){
			++src, ++pc;
			break;
		}
	}
	if(pc < addr_last >> 2){
		src_last = src;
		addr_last = pc << 2;
		return 0;
	} else {
		src_last = ppc_block->mips_code + 1024;
		addr_last = ppc_block->start_address+0x1000;
		return 1;
	}
}

inline unsigned long update_invalid_addr(unsigned long addr);
void jump_to(unsigned int address){
	r4300.stop = 1;
}
unsigned long jump_target;

static void genJumpPad(void){
	// noCheckInterrupt = 1
	GEN_LI(R0, 1);
	GEN_STW(R0, offsetof(R4300,noCheckInterrupt), DYNAREG_R4300);
	
	// TODO: I could link to the next block here
	//       When I do, I need to ensure that r4300.noCheckInterrupt is cleared
	
	// Set the next address to the first address in the next block if
	//   we've really reached the end of the block, not jumped to the pad
	GEN_LIS(R3, extractUpper16((void*)(get_src_pc()+4)));
	GEN_ADDI(R3, R3, extractLower16((void*)(get_src_pc()+4)));

	// return destination
	GEN_BLR(0);
}

/* [Xbox360] ★ FUNCION ANIDADA DE GCC, SACADA A AMBITO DE FICHERO.
 *
 * Estaba declarada DENTRO de invalidate_block.  Las funciones anidadas son
 * una extension de GNU C: no son C89, ni C99, ni C++, y MSVC las rechaza
 * con `error C2601: local function definitions are illegal`.
 *
 * ★ ESTA ES DE LAS FACILES: no captura NADA del ambito de fuera, solo
 * recorre el arbol por su parametro.  Sacarla es un movimiento de texto
 * puro, sin cambio de comportamiento.  La otra (`handle_overlap`, mas
 * arriba) si captura, y por eso alli hubo que pasar punteros. */
static PowerPC_func_node* free_tree(PowerPC_func_node* node){
	if(!node) return NULL;
	node->left = free_tree(node->left);
	node->right = free_tree(node->right);
	/* [Xbox360] Por puntero: con variantes de TLB la misma direccion puede
	 * estar en otro arbol (RecompCache_FreeFunc, Recomp-Cache.h). */
	RecompCache_FreeFunc(node->function);
	return NULL;
}

void invalidate_block(PowerPC_block* ppc_block, int is_deinit){
	// Free the code for all the functions in this block
	ppc_block->funcs = free_tree(ppc_block->funcs);
	if(!is_deinit) {
		// Only re-init if this isn't being called from cpu_deinit so we don't want another block.
		init_block(ppc_block);
	}
}

/* [Xbox360] ★★ [Conker, log 14] TODA FUNCION LIBERADA VA A `freed_funcs`.
 *
 * `dynarec()` (Wrappers.c) solo enlaza el salto pendiente `link_branch` de
 * `last_func` si `!func_was_freed(last_func)`.  En Wii64 la lista la llenaba
 * SOLO `handle_overlap`; el arreglo del log 12 anadio el `Realloc`.  Pero hay
 * mas vias que liberan `last_func` en la MISMA vuelta del despachador, entre
 * su salida y el enlace:
 *   - `invalidate_block`: la pagina del destino esta en `invalid_code` y se
 *     tira entera -- y si `last_func` es de esa pagina, se va con ella;
 *   - `release()`: el desalojo LRU cuando se llena la cache (Conker la llena:
 *     el puente libera y recompila miles de funciones);
 *   - `RecompCache_Free` desde `invalidate_func`.
 * En cualquiera, el enlace se hacia sobre codigo liberado y apuntaba el
 * enlace en un `PowerPC_func` liberado.  Medido: con el arreglo del Realloc
 * puesto, el log 14 (Conker, velocidad 1) vuelve a dar
 *     DESENLACE FUERA DE SITIO: branch=84AA2CA4 op=32 (esperado 18)
 * Es la "divergencia 2" que ya hubo que cerrar en Xenon64: alli la lista se
 * lleno en `free_func`, por donde pasa toda liberacion.  Aqui igual.  Una
 * funcion apuntada dos veces (handle_overlap la apunta antes de liberarla) no
 * molesta.  Coste: un nodo por funcion liberada, y la lista se vacia en cada
 * vuelta del despachador. */
void note_freed_func(PowerPC_func* func){
	struct func_list* freed = (struct func_list*)malloc(sizeof(struct func_list));
	if(!freed) return;   /* sin memoria: como antes del arreglo */
	freed->func = func, freed->next = freed_funcs;
	freed_funcs = freed;
}

int func_was_freed(PowerPC_func* func){
	struct func_list* node;
	for(node = freed_funcs; node != NULL; node = node->next)
		if(node->func == func) return 1;
	return 0;
}

void clear_freed_funcs(void){
	struct func_list* node, * next;
	for(node = freed_funcs; node != NULL; node = next){
		next = node->next;
		free(node);
	}
	freed_funcs = NULL;
}
