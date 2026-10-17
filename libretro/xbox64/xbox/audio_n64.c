/*
 * xbox/audio_n64.c  --  el audio del N64 hacia libretro.
 *
 * Sustituye a los nueve puntos de entrada vacios que la fase 1 tenia en
 * plugins_fase1.c.  La referencia es `Wii64/gc_audio/audio.c`, que NO se
 * copio al arbol (FASE0.txt:128 no lista gc_audio): alli el destino es
 * una voz de AESND y aqui es `audio_batch_cb`, asi que lo que se porta es
 * el trato con el N64 -- la formula de la frecuencia y el volcado del DMA
 * -- y no el fichero.
 *
 * C89.
 */

#include "audio_n64.h"
#include "plataforma.h"                   /* xbox64_n_alist */
#include "../source/xenon64_diag.h"       /* XBOX64_TRAZAS */

#include <string.h>                       /* memcpy */

#include "../source/main/winlnxdefs.h"
#include <ppcintrinsics.h>                /* __lwsync: el anillo entre dos hilos */
#include "../source/main/plugin.h"        /* AUDIO_INFO, SYSTEM_* */

/* `audioEnabled` es de plataforma.c (main_gc-menu2.cpp:100-130 en el
 * original) y lo declara main/wii64config.h. */
#include "../source/main/wii64config.h"

/* ====================================================================== */
/* EL FORMATO, Y POR QUE NO HAY CONVERSION                                */
/* ====================================================================== */
/* ★★ SE COPIAN LOS BYTES TAL CUAL.  No es pereza, es que las tres cosas
 * coinciden y eso hay que dejarlo escrito para que nadie "arregle" lo que
 * no esta roto:
 *
 *   1. El bufer del AI en RDRAM son muestras de 16 bits CON SIGNO,
 *      estereo, entrelazadas L,R.
 *   2. `audio_info.MemoryBswaped = TRUE` (plataforma.c:212), igual que en
 *      Wii64: la RDRAM esta en su orden natural, sin voltear por
 *      palabras.  En un anfitrion little-endian habria que deshacerlo;
 *      aqui la consola es big-endian, como el N64.
 *   3. libretro quiere `int16_t` entrelazados L,R en el orden de bytes
 *      NATIVO del anfitrion, que aqui es big-endian.
 *
 * La prueba de que 1 y 2 bastan es la referencia: `gc_audio/audio.c:131`
 * hace `memcpy` crudo del mismo bufer a una voz `VOICE_STEREO16` de
 * AESND, que espera exactamente s16 estereo L,R big-endian.  Si hiciera
 * falta voltear canales o bytes, alli se notaria.
 *
 * ★ SI ALGUN DIA SUENA CON LOS CANALES CAMBIADOS O A RUIDO, el sitio es
 * este y la pregunta es cual de las tres dejo de ser cierta. */

/* ====================================================================== */
/* EL ANILLO                                                              */
/* ====================================================================== */
/* A 48 kHz y 60 fps son 800 frames estereo por frame de video, o sea 3200
 * bytes.  16384 frames son ~20 frames de video de colchon: de sobra para
 * que un tiron del frontend no se coma muestras, y sigue siendo 64 KB.
 *
 * ★ POTENCIA DE DOS a proposito: el indice se envuelve con un AND y no
 * con un modulo, que aqui se paga en cada muestra. */
#define ANILLO_FRAMES  16384u
#define ANILLO_MASCARA (ANILLO_FRAMES - 1u)

/* Un frame estereo = dos shorts. */
static short    s_anillo[ANILLO_FRAMES * 2];
/* ★★★ [etapa 4] `volatile`, Y LAS DOS BARRERAS DE MAS ABAJO: DESDE AHORA SON
 * DOS HILOS DE VERDAD.
 *
 * Con el renderizado con hilo al estilo de mupen, `aiLenChanged` corre en el
 * hilo del EMULADOR (xbox/hilo_emu.cpp, HW2) y `xbox64_audio_drenar` en
 * `retro_run` (HW1).  Sigue siendo un anillo de UN escritor por indice --
 * `s_escribe` solo lo mueve el productor, `s_lee` solo el consumidor --, que
 * es lo que permite no usar cerrojos.  Pero entre dos nucleos con el modelo
 * de memoria debil del PPC hace falta ademas:
 *
 *   productor  copiar las muestras, `__lwsync`, y SOLO ENTONCES publicar
 *              `s_escribe`.  Si no, el consumidor puede ver el indice nuevo
 *              antes que las muestras: ruido.
 *   consumidor leer `s_escribe`, `__lwsync`, y solo entonces leer muestras;
 *              y `__lwsync` antes de publicar `s_lee`, para que el productor
 *              no escriba encima de lo que aun se esta entregando.
 *
 * mupen no pasa por aqui: llama a `audio_batch_cb` directamente desde su
 * hilo del emulador (audio_backend_libretro.c:390-395).  En Salvia eso no
 * vale -- ese callback toca el resampler, el DRC y el MIDI del frontend y
 * no admite otro hilo --, y por eso el audio se sigue entregando en
 * `retro_run`, por este anillo. */
static volatile unsigned s_escribe;      /* indice de frame, sin envolver */
static volatile unsigned s_lee;
/* ★ SIN CERROJOS, y es una afirmacion comprobable: `aiLenChanged` la
 * llama `memory.c` desde el hilo de emulacion, y `xbox64_audio_drenar` la
 * llama `retro_run`.  Un solo escritor POR INDICE.  [Hasta la etapa 3 los
 * dos eran el mismo hilo; ver la nota de arriba.]
 *
 * ★★ OJO: YA HAY UN SEGUNDO HILO EN EL CORE, y esta nota decia antes que no.
 * `xenon/gfx_hilo.cpp` corre la display list y `updateScreen` en el nucleo 1.
 * SIGUE SIENDO SEGURO -- ese hilo no toca ni este anillo ni `AUDIO_INFO` ni
 * los registros del AI; lo unico que comparte con el de emulacion es RDRAM,
 * el estado del plugin grafico y el shim de D3D --, pero la razon ya no es
 * "no hay otro hilo": es "el otro hilo no llega hasta aqui".
 *
 * ★★★ Y LO QUE SI HARIA FALSA ESTA NOTA: mover la TAREA DE AUDIO del RSP a
 * un hilo.  Ahi el plugin de audio escribe RDRAM y el invitado programa el
 * DMA del AI, y ademas `rsp_break` (source/rsp_hle/hle.c) levanta el
 * `SP_INT` DESPUES de la tarea -- al reves que en graficos, donde el nucleo
 * lo pone antes y por eso salio gratis.  Retrasar ese `SP_INT` es lo que en
 * Xenon64 hizo que el planificador de libultra perdiera una de cada tres
 * tareas de sonido.  Si se intenta, esto hay que rehacerlo con dos indices
 * de un solo escritor y `__lwsync`, como `xenon/video_handoff.cpp`. */

#define DEFAULT_FREQUENCY 33600          /* gc_audio/audio.c:58 */

static AUDIO_INFO s_info;
static unsigned   s_freq = DEFAULT_FREQUENCY;
static int        s_freq_cambio;

/* ====================================================================== */

static unsigned pendientes(void)
{
	return s_escribe - s_lee;            /* sin signo: envuelve solo */
}

void xbox64_audio_reset(void)
{
	s_escribe = 0;
	s_lee     = 0;
}

void xbox64_audio_habilitar(int si)
{
	char nuevo = si ? 1 : 0;

	/* ★ AL APAGARLO SE TIRA LA COLA.  Si no, al volver a encenderlo se
	 * entregarian muestras de hace rato: un chasquido con sonido viejo,
	 * que parece un fallo del audio y es solo la cola. */
	if (!nuevo && audioEnabled)
		xbox64_audio_reset();

	audioEnabled = nuevo;
}

unsigned xbox64_audio_frecuencia(void)
{
	return s_freq ? s_freq : DEFAULT_FREQUENCY;
}

int xbox64_audio_frecuencia_cambio(void)
{
	int c = s_freq_cambio;
	s_freq_cambio = 0;
	return c;
}

unsigned xbox64_audio_drenar(void)
{
	unsigned hay = pendientes();
	unsigned entregados = 0;

	/* [etapa 4] Ver las muestras que `s_escribe` anuncia antes de leerlas.
	 * Pareja del `__lwsync` de `aiLenChanged`. */
	__lwsync();

	if (hay > ANILLO_FRAMES)             /* desbordo: solo lo ultimo vale */
		hay = ANILLO_FRAMES;

	/* ★ EN DOS TROZOS COMO MUCHO, que es lo que puede partir el anillo.
	 * Se entrega TODO lo que hay: dejar cola aqui es latencia que ya no
	 * se recupera, porque el invitado sigue produciendo al mismo ritmo. */
	while (hay > 0)
	{
		unsigned ini   = s_lee & ANILLO_MASCARA;
		unsigned trozo = ANILLO_FRAMES - ini;
		if (trozo > hay)
			trozo = hay;

		xbox64_retro_audio_batch(&s_anillo[ini * 2], trozo);

		/* [etapa 4] Terminar de leer ANTES de devolver el hueco al
		 * productor. */
		__lwsync();
		s_lee      += trozo;
		hay        -= trozo;
		entregados += trozo;
	}

	return entregados;
}

/* ====================================================================== */
/* LOS PUNTOS DE ENTRADA DEL PLUGIN (main/plugin.h)                       */
/* ====================================================================== */

void aiDacrateChanged(int SystemType)
{
	unsigned nueva = DEFAULT_FREQUENCY;
	unsigned dacrate;

	if (!s_info.AI_DACRATE_REG)
		return;

	dacrate = *s_info.AI_DACRATE_REG;

	/* gc_audio/audio.c:100-111, verbatim en los tres relojes. */
	switch (SystemType)
	{
		case SYSTEM_NTSC: nueva = 48681812u / (dacrate + 1u); break;
		case SYSTEM_PAL:  nueva = 49656530u / (dacrate + 1u); break;
		case SYSTEM_MPAL: nueva = 48628316u / (dacrate + 1u); break;
		default: break;
	}

	/* ★ UN CERO AQUI ENVENENA AL FRONTEND: si el juego escribe un dacrate
	 * absurdo antes de configurarse -- pasa en algunos arranques -- la
	 * division puede dar una frecuencia que ningun dispositivo acepta, y
	 * el frontend se queda sin audio para toda la sesion sin decir por
	 * que.  Se acota a un rango que el N64 puede producir de verdad. */
	if (nueva < 4000u || nueva > 96000u)
		return;

	if (nueva != s_freq)
	{
		s_freq        = nueva;
		s_freq_cambio = 1;
	}
}

void aiLenChanged(void)
{
	const unsigned char *origen;
	unsigned bytes, frames, i;

	if (!audioEnabled || !s_info.RDRAM ||
	    !s_info.AI_DRAM_ADDR_REG || !s_info.AI_LEN_REG)
		return;

	/* gc_audio/audio.c:125-126.  La mascara de 24 bits es la que usa la
	 * referencia; el registro trae una direccion fisica de RDRAM. */
	origen = (const unsigned char *)s_info.RDRAM +
	         (*s_info.AI_DRAM_ADDR_REG & 0xFFFFFF);
	bytes  = *s_info.AI_LEN_REG;

	frames = bytes / 4u;                 /* 4 bytes = un frame estereo */
	if (frames == 0)
		return;

	/* ★ SI NO CABE, SE TIRA LO NUEVO -- igual que la referencia, que ni
	 * siquiera empieza a copiar si no cabe entero
	 * (`if (buffered + length < BUFFER_SIZE)`, audio.c:128).  Tirar es
	 * mejor que pisar lo viejo: pisar parte el sonido por la mitad, y
	 * tirar solo deja un hueco que el resampler del frontend disimula.
	 *
	 * Que esto ocurra significa que el frontend no esta drenando, no que
	 * el anillo sea pequenyo. */
	if (pendientes() + frames > ANILLO_FRAMES)
		return;

	/* ★ `memcpy`, IGUAL QUE LA REFERENCIA (audio.c:131), y en dos trozos
	 * como mucho porque el anillo puede partirse.  Se copian los bytes
	 * sin tocarlos: ver la nota del formato arriba.  Usar `memcpy` y no
	 * un bucle de shorts no es solo velocidad -- deja claro de un vistazo
	 * que aqui NO hay conversion, que es justo lo que hay que saber. */
	i = 0;
	while (i < frames)
	{
		unsigned ini   = (s_escribe + i) & ANILLO_MASCARA;
		unsigned trozo = ANILLO_FRAMES - ini;
		if (trozo > frames - i)
			trozo = frames - i;

		memcpy(&s_anillo[ini * 2u], origen + i * 4u, trozo * 4u);
		i += trozo;
	}

	/* [etapa 4] Las muestras tienen que ser visibles ANTES que el indice
	 * que las anuncia: el consumidor esta en otro nucleo. */
	__lwsync();
	s_escribe += frames;
}

void processAList(void)
{
	/* ★ VACIA A PROPOSITO, como en la referencia (audio.c:183).  La lista
	 * de audio la ejecuta el RSP por HLE (`rsp_hle/`, que SI se compila);
	 * cuando termina, el resultado esta en RDRAM y llega aqui por el DMA
	 * del AI.  Este punto de entrada es para plugins que quisieran hacer
	 * ellos la mezcla. */
#if XBOX64_TRAZAS
	xbox64_n_alist++;
#endif
}

BOOL initiateAudio(AUDIO_INFO Audio_Info)
{
	s_info = Audio_Info;
	s_freq = DEFAULT_FREQUENCY;
	s_freq_cambio = 1;                   /* que el wrapper lo publique */
	xbox64_audio_reset();
	return TRUE;
}

void romOpen_audio(void)   { xbox64_audio_reset(); }
void romClosed_audio(void) { xbox64_audio_reset(); }
void closeDLL_audio(void)  { }

/* gc_audio/audio.c:187,193.  Los llama ROM-Cache.c:67-68 alrededor de la
 * carga, para que el DMA de audio no muerda mientras se lee la ROM.
 * ★ Aqui basta con tirar lo pendiente: no hay una voz que parar, y lo que
 * el invitado produjo a medio cargar no vale. */
void pauseAudio(void)  { xbox64_audio_reset(); }
void resumeAudio(void) { xbox64_audio_reset(); }
