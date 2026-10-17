/*
 * xbox/savestate_n64.c  --  estados guardados, contra un bufer de memoria.
 *
 * ★ LA LISTA DE QUE SE GUARDA NO ES MIA: sale de `main/savestates_gc.c`,
 * que SI esta en el arbol (aunque no se compila, porque escribe a fichero
 * con gzip y por slots, que es el modelo del menu de Wii64 y no el de
 * libretro).  Acertar QUE estado hay que guardar es la parte dificil y
 * facil de equivocar, asi que se porta la lista entera y se cambia solo
 * el destino: de `gzwrite` a un bufer.
 *
 * Diferencias con la referencia, todas anotadas donde tocan:
 *   - sin cabecera de miniatura ni fecha (eso es del menu de Wii64)
 *   - la lista de TLB va con CUENTA por delante en vez de con centinela
 *   - al CARGAR se tira el codigo recompilado (la referencia no lo hace)
 *   - `init_vi_clock_refresh_rate` se llama al CARGAR, no al guardar
 *
 * C89.
 */

#include "savestate_n64.h"
#include "plataforma.h"

#include <string.h>
#include <stdio.h>                        /* sprintf, para las lineas de log */

#include "../source/main/winlnxdefs.h"
#include "../source/gc_memory/memory.h"
#include "../source/gc_memory/tlb.h"
#include "../source/gc_memory/flashram.h"
#include "../source/r4300/r4300.h"
#include "../source/r4300/macros.h"       /* Status = r4300.reg_cop0[12] */
#include "../source/r4300/interupt.h"
#include "../source/r4300/Invalid_Code.h"
#include "../source/gui/DEBUG.h"
#include "../source/xenon64_diag.h"       /* XBOX64_TRAZAS */

/* ★ `RDRAM_SIZE` NO ES UNA MACRO GLOBAL DEL ARBOL: `savestates_gc.c` se
 * la define para el (lineas 52-56) y nadie mas la exporta.  Se copia la
 * condicion EXACTA, no una equivalente: si un dia se compilara sin
 * USE_EXPANSION y aqui quedara cableado 0x800000, el estado leeria 4 MB
 * de mas de un array de 4 MB. */
#ifdef USE_EXPANSION
#define RDRAM_SIZE (0x800000)
#else
#define RDRAM_SIZE (0x400000)
#endif

#ifdef PPC_DYNAREC
#include "../source/r4300/ppc/Recompile.h"
#include "../source/r4300/ppc/Wrappers.h"   /* [log 38] dynarec_olvida_enlace */
#endif

void init_vi_clock_refresh_rate(void);   /* gc_memory/memory.c:219 */
void set_fpr_pointers(int newStatus);    /* r4300/r4300.c */

/* ====================================================================== */
/* EL FORMATO                                                             */
/* ====================================================================== */

#define XBOX64_STATE_MAGIC   0x58363453u   /* 'X','6','4','S' */
#define XBOX64_STATE_VERSION 1u

/* ★ EL TOPE DE LA TLB, Y POR QUE HAY UNO.
 *
 * `tlb_LUT_r` y `tlb_LUT_w` son dos tablas de 0x100000 entradas de 32
 * bits: 4 MB cada una, 8 MB las dos.  Guardarlas enteras doblaria el
 * estado (de ~9 MB a ~17 MB) para nada, porque casi todas las entradas
 * son cero -- muchos juegos no usan la TLB en absoluto y los que la usan
 * mapean unos pocos miles de paginas.
 *
 * Asi que se guardan SOLO las no nulas, como hace la referencia.  Pero
 * `retro_serialize_size` tiene que devolver un tamanyo CONSTANTE, o sea
 * que hace falta un tope: 65536 entradas por tabla son 512 KB cada una y
 * ~100 veces mas de lo que usa un juego real.
 *
 * ★ SI SE PASA, NO SE TRUNCA: se falla el guardado con una linea en el
 * log.  Un estado truncado carga "bien" y el juego se porta mal mas
 * tarde, que es el peor de los dos mundos. */
#define TLB_MAX_ENTRADAS 65536u

struct cabecera
{
	unsigned int magic;
	unsigned int version;
	unsigned int rdram_bytes;   /* para no cargar un estado de otro build */
	unsigned int n_tlb_r;
	unsigned int n_tlb_w;
	unsigned int eventqueue_len;
};

/* El bufer de la cola de eventos: la referencia usa `char buf[1024]`
 * (savestates_gc.c:157), asi que 1024 es el tope que su propio autor dio
 * por bueno.  Se reserva lo mismo. */
#define EVENTQUEUE_MAX 1024

/* ====================================================================== */
/* UN CURSOR, PARA NO CONTAR DESPLAZAMIENTOS A MANO                       */
/* ====================================================================== */
/* ★ Contar offsets a mano en un formato con 20 campos es como se llega a
 * un estado que carga desplazado un byte y da un fallo irreproducible
 * tres horas despues.  El cursor lleva la cuenta y ademas comprueba que
 * no se sale del bufer, en las dos direcciones. */

struct cursor
{
	unsigned char *base;
	size_t         tam;
	size_t         pos;
	int            error;
};

static void cur_escribe(struct cursor *c, const void *src, size_t n)
{
	if (c->error) return;
	if (c->pos + n > c->tam) { c->error = 1; return; }
	memcpy(c->base + c->pos, src, n);
	c->pos += n;
}

static void cur_lee(struct cursor *c, void *dst, size_t n)
{
	if (c->error) return;
	if (c->pos + n > c->tam) { c->error = 1; return; }
	memcpy(dst, c->base + c->pos, n);
	c->pos += n;
}

/* ====================================================================== */

size_t xbox64_state_tam(void)
{
	size_t t = 0;

	t += sizeof(struct cabecera);
	t += RDRAM_SIZE;
	t += (size_t)TLB_MAX_ENTRADAS * 8u * 2u;   /* (pagina,valor) x r y w */

	t += sizeof(rdram_register);
	t += sizeof(MI_register);
	t += sizeof(pi_register);
	t += sizeof(sp_register);
	t += sizeof(rsp_register);
	t += sizeof(si_register);
	t += sizeof(vi_register);
	t += sizeof(ri_register);
	t += sizeof(ai_register);
	t += sizeof(dpc_register);
	t += sizeof(dps_register);

	t += 0x1000;      /* SP_DMEM */
	t += 0x1000;      /* SP_IMEM */
	t += 0x40;        /* PIF_RAM */
	t += sizeof(flashRAMInfo);
	t += sizeof(r4300);
	t += 32 * 8;      /* fpr_data */
	t += EVENTQUEUE_MAX;

	return t;
}

/* ---------------------------------------------------------------------- */

static void guarda_registros(struct cursor *c)
{
	cur_escribe(c, &rdram_register, sizeof(rdram_register));
	cur_escribe(c, &MI_register,    sizeof(MI_register));
	cur_escribe(c, &pi_register,    sizeof(pi_register));
	cur_escribe(c, &sp_register,    sizeof(sp_register));
	cur_escribe(c, &rsp_register,   sizeof(rsp_register));
	cur_escribe(c, &si_register,    sizeof(si_register));
	cur_escribe(c, &vi_register,    sizeof(vi_register));
	cur_escribe(c, &ri_register,    sizeof(ri_register));
	cur_escribe(c, &ai_register,    sizeof(ai_register));
	cur_escribe(c, &dpc_register,   sizeof(dpc_register));
	cur_escribe(c, &dps_register,   sizeof(dps_register));
}

static void carga_registros(struct cursor *c)
{
	cur_lee(c, &rdram_register, sizeof(rdram_register));
	cur_lee(c, &MI_register,    sizeof(MI_register));
	cur_lee(c, &pi_register,    sizeof(pi_register));
	cur_lee(c, &sp_register,    sizeof(sp_register));
	cur_lee(c, &rsp_register,   sizeof(rsp_register));
	cur_lee(c, &si_register,    sizeof(si_register));
	cur_lee(c, &vi_register,    sizeof(vi_register));
	cur_lee(c, &ri_register,    sizeof(ri_register));
	cur_lee(c, &ai_register,    sizeof(ai_register));
	cur_lee(c, &dpc_register,   sizeof(dpc_register));
	cur_lee(c, &dps_register,   sizeof(dps_register));
}

/* ---------------------------------------------------------------------- */

int xbox64_state_guardar(void *destino, size_t tam)
{
	struct cursor   c;
	struct cabecera h;
	char            evq[EVENTQUEUE_MAX];
	unsigned int    i, n_r = 0, n_w = 0;
	int             len;

	/* [Xbox360] Aqui habia una valla del hilo grafico antiguo (quitado el
	 * 2026-09-30).  Sin ese hilo, nadie lee RDRAM mientras esto corre.
	 *
	 * ★ PARA LA ETAPA 4 (emulador en su propio hilo, como mupen): esto se
	 * llama desde `gen_interupt`, o sea en el hilo del EMULADOR, que es
	 * justo donde tiene que correr.  El traspaso con `retro_serialize` va
	 * en ese lado, no aqui. */

	if (!destino || tam < xbox64_state_tam())
		return 0;

	/* Contar la TLB antes de escribir nada: la cuenta va en la cabecera. */
	for (i = 0; i < 0x100000u; i++) { if (tlb_LUT_r[i]) n_r++; }
	for (i = 0; i < 0x100000u; i++) { if (tlb_LUT_w[i]) n_w++; }

	if (n_r > TLB_MAX_ENTRADAS || n_w > TLB_MAX_ENTRADAS)
	{
#if XBOX64_TRAZAS
		sprintf(txtbuffer,
		        "[xbox64] estado NO guardado: la TLB no cabe (r=%u w=%u, tope %u)",
		        n_r, n_w, TLB_MAX_ENTRADAS);
		DEBUG_print(txtbuffer, DBG_USBGECKO);
#endif
		return 0;
	}

	len = save_eventqueue_infos(evq);
	if (len < 0 || (unsigned)len > EVENTQUEUE_MAX)
	{
#if XBOX64_TRAZAS
		sprintf(txtbuffer,
		        "[xbox64] estado NO guardado: la cola de eventos no cabe (%d)", len);
		DEBUG_print(txtbuffer, DBG_USBGECKO);
#endif
		return 0;
	}

	c.base = (unsigned char *)destino;
	c.tam  = tam;
	c.pos  = 0;
	c.error = 0;

	h.magic          = XBOX64_STATE_MAGIC;
	h.version        = XBOX64_STATE_VERSION;
	h.rdram_bytes    = RDRAM_SIZE;
	h.n_tlb_r        = n_r;
	h.n_tlb_w        = n_w;
	h.eventqueue_len = (unsigned int)len;
	cur_escribe(&c, &h, sizeof(h));

	cur_escribe(&c, (const void *)rdram, RDRAM_SIZE);

	for (i = 0; i < 0x100000u; i++)
		if (tlb_LUT_r[i])
		{
			unsigned int par[2];
			par[0] = i; par[1] = tlb_LUT_r[i];
			cur_escribe(&c, par, 8);
		}
	for (i = 0; i < 0x100000u; i++)
		if (tlb_LUT_w[i])
		{
			unsigned int par[2];
			par[0] = i; par[1] = tlb_LUT_w[i];
			cur_escribe(&c, par, 8);
		}

	guarda_registros(&c);

	cur_escribe(&c, SP_DMEM, 0x1000);
	cur_escribe(&c, SP_IMEM, 0x1000);
	cur_escribe(&c, PIF_RAM, 0x40);
	cur_escribe(&c, &flashRAMInfo, sizeof(flashRAMInfo));
	cur_escribe(&c, &r4300, sizeof(r4300));
	cur_escribe(&c, r4300.fpr_data, 32 * 8);
	cur_escribe(&c, evq, (size_t)len);

	if (c.error)
	{
#if XBOX64_TRAZAS
		DEBUG_print("[xbox64] estado NO guardado: el bufer se quedo corto",
		            DBG_USBGECKO);
#endif
		return 0;
	}
	return 1;
}

/* ---------------------------------------------------------------------- */
/* ★★ TIRAR EL CODIGO RECOMPILADO AL CARGAR                               */
/* ---------------------------------------------------------------------- */
/* LA REFERENCIA NO HACE ESTO, y es una desviacion deliberada con motivo,
 * no por simetria.  El motivo:
 *
 * El dynarec guarda, por pagina de 4 KB, si ya tiene codigo compilado
 * (`invalid_code`) y donde esta (`blocks[]`).  Un estado guardado repone
 * la RDRAM ENTERA.  Si una pagina tenia codigo compilado y el estado
 * trae OTRO codigo en esa pagina, el dynarec seguira ejecutando el viejo:
 * no tiene forma de enterarse, porque para el la pagina sigue siendo
 * valida.
 *
 * ★ POR QUE A WII64 NO LE PASA: casi siempre se carga un estado del
 * MISMO juego en el MISMO sitio, y el codigo en RDRAM es el mismo.  Le
 * funciona por suerte, no por diseno.  Deja de funcionar en cuanto el
 * juego usa OVERLAYS -- carga rutinas distintas en la misma direccion
 * segun el nivel -- que es justo lo que hacen DK64 y Conker, los dos
 * juegos con los que se ha trabajado en este port.
 *
 * ★ Y EL SINTOMA SERIA EL PEOR POSIBLE: ejecutar codigo recompilado que
 * ya no corresponde a la memoria.  Es la misma clase de fallo que costo
 * la fase 3 entera -- calcula mal y corrompe memoria, sin excepcion que
 * apunte a nada.
 *
 * ★ NO SE INVENTA NADA: `invalidate_block(b, 0)` es la funcion del propio
 * arbol (ppc/Recompile.c:719) que libera las funciones de un bloque y lo
 * deja listo para recompilar.  Es la misma que usa la invalidacion por
 * codigo automodificable.  Aqui solo se aplica a todos los bloques. */
static void xbox64_state_tirar_codigo(void)
{
#ifdef PPC_DYNAREC
	unsigned int i;
#if XBOX64_TRAZAS
	unsigned int n = 0;
#endif

	if (dynacore != 1)     /* 1 = dynarec; con el interprete no hay nada */
		return;

	for (i = 0; i < 0x100000u; i++)
		if (blocks[i])
		{
			invalidate_block(blocks[i], 0);
#if XBOX64_TRAZAS
			n++;
#endif
		}
	/* [log 38] `last_func` acaba de liberarse con todo lo demas: que el
	 * primer bloque del estado no se enlace con el. */
	dynarec_olvida_enlace();

#if XBOX64_TRAZAS
	sprintf(txtbuffer, "[xbox64] estado cargado: %u bloques recompilados tirados", n);
	DEBUG_print(txtbuffer, DBG_USBGECKO);
#endif
#endif
}

/* ---------------------------------------------------------------------- */

int xbox64_state_cargar(const void *origen, size_t tam)
{
	struct cursor   c;
	struct cabecera h;
	char            evq[EVENTQUEUE_MAX];
	unsigned int    i;

	/* [Xbox360] Aqui habia otra valla del hilo grafico antiguo.  Ver la nota
	 * de `xbox64_state_guardar`. */

	if (!origen || tam < sizeof(struct cabecera))
		return 0;

	/* El cursor lee de un bufer constante; el `const` se quita aqui y en
	 * ningun otro sitio, y `cur_lee` no escribe en el. */
	c.base = (unsigned char *)origen;
	c.tam  = tam;
	c.pos  = 0;
	c.error = 0;

	cur_lee(&c, &h, sizeof(h));
	if (c.error ||
	    h.magic != XBOX64_STATE_MAGIC ||
	    h.version != XBOX64_STATE_VERSION ||
	    h.rdram_bytes != RDRAM_SIZE ||
	    h.n_tlb_r > TLB_MAX_ENTRADAS ||
	    h.n_tlb_w > TLB_MAX_ENTRADAS ||
	    h.eventqueue_len > EVENTQUEUE_MAX)
	{
		/* ★ SE RECHAZA ANTES DE TOCAR NADA.  Si se empezara a cargar y se
		 * abandonase a la mitad, el emulador quedaria con media RDRAM
		 * nueva y medio estado viejo -- irrecuperable y sin sintoma
		 * inmediato.  Aqui, un estado que no cuadra simplemente no se
		 * carga y la partida sigue como estaba. */
#if XBOX64_TRAZAS
		DEBUG_print("[xbox64] estado RECHAZADO: cabecera incompatible",
		            DBG_USBGECKO);
#endif
		return 0;
	}

	cur_lee(&c, (void *)rdram, RDRAM_SIZE);

	/* ★ LAS DOS TABLAS SE BORRAN ENTERAS ANTES DE REPONER LAS NO NULAS.
	 * El estado solo lleva las entradas con valor; sin el borrado, las
	 * que el estado NO trae se quedarian con lo que hubiera de la partida
	 * en curso, y una traduccion de direcciones fantasma no da un error:
	 * da lecturas de la pagina equivocada. */
	memset((void *)tlb_LUT_r, 0, 0x100000u * sizeof(unsigned long));
	memset((void *)tlb_LUT_w, 0, 0x100000u * sizeof(unsigned long));

	for (i = 0; i < h.n_tlb_r; i++)
	{
		unsigned int par[2];
		cur_lee(&c, par, 8);
		if (c.error) break;
		if (par[0] < 0x100000u) tlb_LUT_r[par[0]] = par[1];
	}
	for (i = 0; i < h.n_tlb_w; i++)
	{
		unsigned int par[2];
		cur_lee(&c, par, 8);
		if (c.error) break;
		if (par[0] < 0x100000u) tlb_LUT_w[par[0]] = par[1];
	}

	carga_registros(&c);

	cur_lee(&c, SP_DMEM, 0x1000);
	cur_lee(&c, SP_IMEM, 0x1000);
	cur_lee(&c, PIF_RAM, 0x40);
	cur_lee(&c, &flashRAMInfo, sizeof(flashRAMInfo));
	cur_lee(&c, &r4300, sizeof(r4300));

	/* savestates_gc.c:371.  ★ VA AQUI, ENTRE LEER `r4300` Y LEER LOS FPR:
	 * los punteros `fpr_single`/`fpr_double` apuntan DENTRO de
	 * `fpr_data`, y el bit FR de Status decide como se reparten.  Al leer
	 * la estructura `r4300` entera se han sobrescrito con los punteros de
	 * la maquina que GUARDO el estado.  Si no se rehacen antes de
	 * escribir los datos, se escribe a traves de punteros ajenos. */
	set_fpr_pointers(Status);

	cur_lee(&c, r4300.fpr_data, 32 * 8);

	memset(evq, 0, sizeof(evq));
	cur_lee(&c, evq, h.eventqueue_len);

	if (c.error)
	{
#if XBOX64_TRAZAS
		DEBUG_print("[xbox64] estado a medio cargar: bufer corto", DBG_USBGECKO);
#endif
		return 0;
	}

	load_eventqueue_infos(evq);

	/* savestates_gc.c:385. */
	r4300.stop = 0;

	/* ★ LA REFERENCIA LLAMA A ESTO AL GUARDAR (savestates_gc.c:240), Y
	 * AQUI SE LLAMA AL CARGAR.  Es una desviacion deliberada: la funcion
	 * recalcula el ritmo del VI a partir de `vi_register`, o sea que
	 * cambia estado.  Hacerlo mientras se GUARDA significa que guardar
	 * una partida la altera, aunque sea poco; hacerlo al cargar, justo
	 * despues de reponer `vi_register`, es donde ese recalculo hace
	 * falta.  Es el mismo juego y los mismos registros, asi que el valor
	 * sale igual: la diferencia es cuando, no cuanto. */
	init_vi_clock_refresh_rate();

	/* ★★ Y LO QUE LA REFERENCIA NO HACE Y AQUI ES OBLIGATORIO. */
	xbox64_state_tirar_codigo();

	return 1;
}
