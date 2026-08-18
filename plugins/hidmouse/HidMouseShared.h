/*
    HidMouseShared.h — contrato compartido entre el plugin residente de DashLaunch
    (hidmouse.xex) y el consumidor (Salvia / libSDLx360).

    El plugin es un sysdll residente que lee el raton USB HID (hooks de kernel) y
    ACUMULA los deltas en una struct de este tipo (g_hidMouseShared). Salvia, desde
    el titulo actual, LEE esa struct de la memoria del plugin (en RGH/JTAG un titulo
    puede leer memoria de kernel) y empuja los eventos a SDL. Salvia no hookea nada.

    Como el plugin es residente (nunca se descarga), el raton sobrevive a todos los
    XLaunchNewImage: sin crash y sin replug.
*/
#ifndef HIDMOUSE_SHARED_H
#define HIDMOUSE_SHARED_H

#include <stdint.h>

#define HIDMOUSE_SHARED_MAGIC 0x484D5345u   /* 'HMSE' */

/* Version del layout, por si cambia el contrato en el futuro. */
#define HIDMOUSE_SHARED_VERSION 1u

typedef struct HidMouseShared {
    volatile uint32_t magic;     /* == HIDMOUSE_SHARED_MAGIC cuando el plugin esta activo */
    volatile uint32_t version;   /* == HIDMOUSE_SHARED_VERSION */
    volatile uint32_t seq;       /* se incrementa en cada report; "señal de vida" */
    /* Acumuladores CUMULATIVOS (el plugin solo suma, nunca los resetea; envuelven
       a 2^32). El consumidor (Salvia) guarda el ultimo valor leido y calcula el
       delta = actual - ultimo. Asi el consumidor solo LEE esta struct (nunca
       escribe en la memoria del plugin) y cada .xex se "ceba" en su primera
       lectura tras un relanzamiento. */
    volatile int32_t  accumX;    /* X cumulativo */
    volatile int32_t  accumY;    /* Y cumulativo */
    volatile int32_t  accumW;    /* rueda cumulativa */
    volatile uint32_t buttons;   /* bit0=izq, bit1=der, bit2=medio (nivel actual) */
} HidMouseShared;

/*
    Localizacion del canal SIN .map (no hay que ajustar ninguna direccion).

    El plugin es sysdll con base FIJA 0x81F00000 (ver xex.xml). El consumidor
    (Salvia) NO necesita la VA exacta de g_hidMouseShared: escanea la imagen del
    plugin desde su base buscando la firma de 2 palabras (magic seguido de
    version). Asi encuentra la struct este donde este dentro de la imagen, y no
    hay que tocar nada aunque se recompile el plugin y cambie el layout.

    Ver LocateShared() en SDL_xboxhidmouse.cpp (Salvia). Base y tamaño de escaneo:
*/
#define HIDMOUSE_PLUGIN_BASE  0x81F00000u
#define HIDMOUSE_SCAN_BYTES   0x00080000u   /* 512 KB (cota; SEH corta al final real) */

#endif /* HIDMOUSE_SHARED_H */
